/*
 * =====================================================================================
 * CONTROL DE ACCESO - ESP8266   (HUELLA + WIFI, SIN SD)
 * =====================================================================================
 * Agrega control remoto por WiFi SIN tocar el Arduino.
 *
 * COMO ABRE SIN TOCAR EL ARDUINO:
 *   Al pulsar "Abrir" en la pagina web, el ESP envia "HUELLA:OK:WEB" por el serial.
 *   Tu Arduino ya abre la puerta al recibir HUELLA:OK:, asi que funciona igual.
 *   (El LCD mostrara "Huella OK ID:#WEB")
 *
 * MODO WIFI: Punto de Acceso (AP) propio -> no necesita router.
 *   1. Enciende el sistema.
 *   2. En el celular, conectate a la red WiFi:   ControlAcceso    (clave: abrir1234)
 *   3. Abre el navegador en:   http://192.168.4.1
 *   4. Pulsa "Abrir puerta".
 *
 * CONEXIONES:
 *   Sensor huella:  VCC->3V3, GND->GND, TX->D4, RX->D0
 *   Enlace Arduino: ESP TX -> Arduino RX(0),  ESP RX -> Arduino TX(1),  GND COMUN
 *   RTC (opcional): SDA->D2, SCL->D1
 *
 * NOTA: sube este sketch con el enlace al Arduino DESCONECTADO; luego conecta y enciende.
 * =====================================================================================
 */

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <SoftwareSerial.h>
#include <Adafruit_Fingerprint.h>
#include <Wire.h>
#include <RTClib.h>

// ── WiFi (modo AP) ─────────────────────────────────────────────
const char* AP_SSID = "ControlAcceso";
const char* AP_PASS = "abrir1234";       // minimo 8 caracteres
ESP8266WebServer server(80);

// ── Huella ─────────────────────────────────────────────────────
SoftwareSerial mySer(D4, D0);            // RX=D4, TX=D0
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySer);

// ── RTC (opcional) ─────────────────────────────────────────────
RTC_DS3231 rtc;
bool rtcOK = false;
unsigned long tUltimoEnvioHora = 0;

// ── LED / buffers ──────────────────────────────────────────────
const int LED_ROJO = D3;
String bufferSerial = "";
int huellasFallidasConsecutivas = 0;
unsigned long tUltimaHuellaFallida = 0;

// ── Pagina web ─────────────────────────────────────────────────
const char PAGINA[] PROGMEM = R"HTML(
<!DOCTYPE html><html lang="es"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Control de Acceso</title>
<style>
 body{font-family:system-ui,Arial,sans-serif;margin:0;background:#0f172a;color:#e2e8f0;
   display:flex;min-height:100vh;align-items:center;justify-content:center}
 .card{text-align:center;padding:32px}
 h1{font-size:20px;font-weight:600;margin-bottom:24px}
 button{font-size:22px;padding:22px 40px;border:0;border-radius:16px;background:#22c55e;
   color:#052e13;font-weight:700;cursor:pointer;width:100%;max-width:320px}
 button:active{transform:scale(.98)}
 #msg{margin-top:20px;font-size:15px;min-height:22px;color:#94a3b8}
</style></head><body>
<div class="card">
 <h1>Control de Acceso</h1>
 <button onclick="abrir()">Abrir puerta</button>
 <div id="msg"></div>
</div>
<script>
function abrir(){
 document.getElementById('msg').textContent='Enviando...';
 fetch('/abrir').then(r=>r.text()).then(t=>{
   document.getElementById('msg').textContent=t;
 }).catch(function(){document.getElementById('msg').textContent='Error de conexion';});
}
</script></body></html>
)HTML";

void handleRoot()  { server.send_P(200, "text/html", PAGINA); }
void handleAbrir() {
  Serial.println("HUELLA:OK:WEB");       // el Arduino abre con esto, sin tocarlo
  server.send(200, "text/plain", "Puerta abierta");
}

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

void ledRojo() { digitalWrite(LED_ROJO, HIGH); delay(50); digitalWrite(LED_ROJO, LOW); }
void parpadeoRojo(int veces, int tDelay = 100) {
  for (int i = 0; i < veces; i++) {
    digitalWrite(LED_ROJO, HIGH); delay(tDelay);
    digitalWrite(LED_ROJO, LOW);  delay(tDelay);
  }
}

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

  if (p != FINGERPRINT_OK)                 { manejarHuellaFallida(); return; }
  if (finger.image2Tz() != FINGERPRINT_OK) { manejarHuellaFallida(); return; }

  if (finger.fingerSearch() == FINGERPRINT_OK) {
    huellasFallidasConsecutivas = 0;
    Serial.println("HUELLA:OK:" + String(finger.fingerID));
  } else {
    manejarHuellaFallida();
  }
  delay(1500);
}

// ============================================================
void setup() {
  Serial.begin(9600);
  delay(500);

  pinMode(LED_ROJO, OUTPUT);
  digitalWrite(LED_ROJO, LOW);

  // RTC opcional (no congela si falta)
  Wire.begin();
  if (rtc.begin()) {
    rtcOK = true;
    if (rtc.lostPower()) rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  // Huella (no congela si falta)
  finger.begin(57600);
  delay(50);
  finger.verifyPassword();

  // WiFi como punto de acceso propio
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASS);
  server.on("/", handleRoot);
  server.on("/abrir", handleAbrir);
  server.begin();

  parpadeoRojo(2, 200);   // arranque
}

void loop() {
  server.handleClient();
  leerSerialArduino();
  procesarHuella();

  if (rtcOK && millis() - tUltimoEnvioHora >= 10000) {
    enviarHoraArduino();
    tUltimoEnvioHora = millis();
  }
  if (huellasFallidasConsecutivas > 0 && (millis() - tUltimaHuellaFallida > 60000))
    huellasFallidasConsecutivas = 0;

  delay(5);
}
