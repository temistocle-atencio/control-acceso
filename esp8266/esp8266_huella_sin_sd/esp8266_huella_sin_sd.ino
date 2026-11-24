/*
 * =====================================================================================
 * CONTROL DE ACCESO - ESP8266  (VERSION SIN SD)
 * =====================================================================================
 * - Lee la huella y le avisa al Arduino (HUELLA:OK / HUELLA:DENEGADO / HUELLA:LEYENDO).
 * - El Arduino, con tu codigo actual, abre la puerta al recibir HUELLA:OK. No se toca.
 * - SIN tarjeta SD (se elimino el registro en memoria).
 * - RTC OPCIONAL: si esta conectado, envia la hora; si no, el sistema NO se congela.
 * - La huella que falla ya NO congela el ESP.
 *
 * CONEXIONES:
 *   Sensor huella:  VCC->3V3, GND->GND, TX->D4, RX->D0
 *   Enlace al Arduino:  ESP TX -> Arduino RX(0),  ESP RX -> Arduino TX(1)
 *   *** GND COMUN entre ESP y Arduino (imprescindible) ***
 *   RTC (si lo usas): SDA->D2, SCL->D1
 *
 * CONSEJO: sube este sketch con el enlace al Arduino DESCONECTADO (solo el sensor).
 *          Luego apaga, conecta el enlace ESP<->Arduino + GND comun, y enciende.
 * =====================================================================================
 */

#include <SoftwareSerial.h>
#include <Adafruit_Fingerprint.h>
#include <Wire.h>
#include <RTClib.h>

// ── Huella ─────────────────────────────────────────────────────
SoftwareSerial mySer(D4, D0);            // RX=D4, TX=D0
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySer);

// ── RTC (opcional) ─────────────────────────────────────────────
RTC_DS3231 rtc;
bool rtcOK = false;
unsigned long tUltimoEnvioHora = 0;

// ── LED ────────────────────────────────────────────────────────
const int LED_ROJO = D3;

// ── Buffer serial desde Arduino ────────────────────────────────
String bufferSerial = "";

// ── Contador de huellas fallidas ───────────────────────────────
int huellasFallidasConsecutivas = 0;
unsigned long tUltimaHuellaFallida = 0;

// ============================================================
void enviarHoraArduino() {
  if (!rtcOK) return;
  DateTime ahora = rtc.now();
  String m = "HORA:";
  if (ahora.hour()   < 10) m += "0"; m += ahora.hour();   m += ":";
  if (ahora.minute() < 10) m += "0"; m += ahora.minute(); m += ":";
  if (ahora.second() < 10) m += "0"; m += ahora.second();
  Serial.println(m);
}

void ledRojo() {
  digitalWrite(LED_ROJO, HIGH); delay(50); digitalWrite(LED_ROJO, LOW);
}
void parpadeoRojo(int veces, int tDelay = 100) {
  for (int i = 0; i < veces; i++) {
    digitalWrite(LED_ROJO, HIGH); delay(tDelay);
    digitalWrite(LED_ROJO, LOW);  delay(tDelay);
  }
}

// ============================================================
//  MENSAJES DEL ARDUINO (sin SD: solo feedback por LED)
// ============================================================
void procesarMensajeArduino(String msg) {
  msg.trim();
  if (msg.length() == 0) return;
  if      (msg.startsWith("DENEGADO:")) ledRojo();
  else if (msg == "ALERTA:BLOQUEO")     parpadeoRojo(5, 200);
  else if (msg == "ALERTA:FORZADO")     parpadeoRojo(5, 100);
}

void leerSerialArduino() {
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\n') { procesarMensajeArduino(bufferSerial); bufferSerial = ""; }
    else if (c != '\r') { bufferSerial += c; }
  }
}

// ============================================================
//  HUELLA
// ============================================================
void manejarHuellaFallida() {
  if (millis() - tUltimaHuellaFallida > 60000) huellasFallidasConsecutivas = 0;
  huellasFallidasConsecutivas++;
  tUltimaHuellaFallida = millis();
  Serial.println("HUELLA:DENEGADO");
  if (huellasFallidasConsecutivas >= 3) parpadeoRojo(5, 100);
}

void procesarHuella() {
  int p = finger.getImage();
  if (p == FINGERPRINT_NOFINGER) return;

  Serial.println("HUELLA:LEYENDO");

  if (p != FINGERPRINT_OK)              { manejarHuellaFallida(); return; }
  if (finger.image2Tz() != FINGERPRINT_OK) { manejarHuellaFallida(); return; }

  if (finger.fingerSearch() == FINGERPRINT_OK) {
    huellasFallidasConsecutivas = 0;
    Serial.println("HUELLA:OK:" + String(finger.fingerID));
  } else {
    manejarHuellaFallida();
  }
  delay(1500);   // evita releer el mismo dedo
}

// ============================================================
//  SETUP
// ============================================================
void setup() {
  Serial.begin(9600);
  delay(500);

  pinMode(LED_ROJO, OUTPUT);
  digitalWrite(LED_ROJO, LOW);

  // RTC opcional: si falla, seguimos sin congelar
  Wire.begin();
  if (rtc.begin()) {
    rtcOK = true;
    if (rtc.lostPower()) rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  // Huella: si falla, tampoco congelamos (clave y touch siguen en el Arduino)
  finger.begin(57600);
  delay(50);
  finger.verifyPassword();

  parpadeoRojo(2, 200);   // senal de arranque
}

// ============================================================
//  LOOP
// ============================================================
void loop() {
  leerSerialArduino();
  procesarHuella();

  if (rtcOK && millis() - tUltimoEnvioHora >= 10000) {
    enviarHoraArduino();
    tUltimoEnvioHora = millis();
  }

  if (huellasFallidasConsecutivas > 0 && (millis() - tUltimaHuellaFallida > 60000))
    huellasFallidasConsecutivas = 0;

  delay(50);
}
