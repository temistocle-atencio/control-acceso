/*
 * =====================================================================================
 * SISTEMA DE CONTROL DE ACCESO - NodeMCU ESP8266 (VERSIÓN CON MEJORAS)
 * =====================================================================================
 */

#include <SoftwareSerial.h>
#include <Adafruit_Fingerprint.h>
#include <Wire.h>
#include <RTClib.h>
#include <SPI.h>
#include <SD.h>

// ── SENSOR DE HUELLA ───────────────────────────────────────────
SoftwareSerial mySer(D4, D0);  // RX=D4, TX=D0
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySer);

// ── RTC ────────────────────────────────────────────────────────
RTC_DS3231 rtc;
unsigned long tUltimoEnvioHora = 0;

// ── SD ─────────────────────────────────────────────────────────
const int CS_PIN = D8;

// ── LED ROJO (Único LED) ───────────────────────────────────────
const int LED_ROJO  = D3;  

// ── BUFFER SERIAL DESDE ARDUINO ────────────────────────────────
String bufferSerial = "";

// ── CONTADOR DE HUELLAS FALLIDAS ───────────────────────────────
int huellasFallidasConsecutivas = 0;
unsigned long tUltimaHuellaFallida = 0;

// ============================================================
//  UTILIDADES
// ============================================================
String obtenerFechaHora() {
  DateTime ahora = rtc.now();
  String dt = "";
  dt += ahora.year();  dt += "/";
  if (ahora.month()  < 10) dt += "0"; dt += ahora.month();  dt += "/";
  if (ahora.day()    < 10) dt += "0"; dt += ahora.day();    dt += " ";
  if (ahora.hour()   < 10) dt += "0"; dt += ahora.hour();   dt += ":";
  if (ahora.minute() < 10) dt += "0"; dt += ahora.minute(); dt += ":";
  if (ahora.second() < 10) dt += "0"; dt += ahora.second();
  return dt;
}

void enviarHoraArduino() {
  DateTime ahora = rtc.now();
  String msjHora = "HORA:";
  if (ahora.hour() < 10) msjHora += "0"; msjHora += ahora.hour(); msjHora += ":";
  if (ahora.minute() < 10) msjHora += "0"; msjHora += ahora.minute(); msjHora += ":";
  if (ahora.second() < 10) msjHora += "0"; msjHora += ahora.second();
  
  Serial.println(msjHora);
}

void guardarEnSD(String registro) {
  File archivo = SD.open("/accesos.txt", FILE_WRITE);
  if (archivo) {
    archivo.println(registro);
    archivo.close();
  }
}

void ledRojo(int ms = 1000) {
  digitalWrite(LED_ROJO, HIGH);
  delay(50);
  digitalWrite(LED_ROJO, LOW);
}

void parpadeoRojo(int veces, int tDelay = 100) {
  for (int i = 0; i < veces; i++) {
    digitalWrite(LED_ROJO, HIGH); delay(tDelay);
    digitalWrite(LED_ROJO, LOW);  delay(tDelay);
  }
}

// ============================================================
//  PROCESAR MENSAJES DEL ARDUINO
// ============================================================
void procesarMensajeArduino(String msg) {
  msg.trim();
  if (msg.length() == 0) return;

  String registro = "[" + obtenerFechaHora() + "] ";

  if      (msg == "OK:PIN")          { registro += "ACCESO OK - PIN correcto";           guardarEnSD(registro); }
  else if (msg == "OK:RFID")         { registro += "ACCESO OK - Tarjeta autorizada";     guardarEnSD(registro); }
  else if (msg == "OK:TOUCH")        { registro += "ACCESO OK - Touch interior";         guardarEnSD(registro); }
  else if (msg == "OK:HUELLA")       { registro += "ACCESO OK - Huella reconocida";      guardarEnSD(registro); }
  
  else if (msg == "DENEGADO:PIN")    { registro += "DENEGADO - PIN incorrecto";          guardarEnSD(registro); ledRojo(800); }
  else if (msg == "DENEGADO:RFID")   { registro += "DENEGADO - Tarjeta no autorizada";   guardarEnSD(registro); ledRojo(800); }
  else if (msg == "DENEGADO:HUELLA") { registro += "DENEGADO - Huella no reconocida";    guardarEnSD(registro); ledRojo(800); }
  
  else if (msg.startsWith("ALERTA:SENSOR:")) { 
    String distStr = msg.substring(14);
    registro += "ALERTA - Objeto detectado a " + distStr + " cm"; 
    guardarEnSD(registro); 
    // Omitimos el parpadeo en sensor comun para no bloquear, o lo hacemos muy corto
    digitalWrite(LED_ROJO, HIGH); delay(100); digitalWrite(LED_ROJO, LOW);
  }
  
  else if (msg == "ALERTA:BLOQUEO")  { 
    registro += "ALERTA - Sistema bloqueado por intentos fallidos"; 
    guardarEnSD(registro); 
    parpadeoRojo(5, 200);
  }
  else if (msg == "ALERTA:FORZADO")  { 
    registro += "ALERTA CRITICA - Posible apertura forzada"; 
    guardarEnSD(registro); 
    parpadeoRojo(5, 100); // Parpadeo rápido
  }
  else if (msg == "INFO:CLAVE_CAMBIADA") { 
    registro += "INFO - Clave de acceso modificada"; 
    guardarEnSD(registro); 
  }
  else if (msg == "CERRADO:AUTO")    { registro += "INFO - Puerta cerrada auto";         guardarEnSD(registro); }
  else if (msg == "CERRADO:TOUCH")   { registro += "INFO - Puerta cerrada touch";        guardarEnSD(registro); }
  else                               { registro += "MSG: " + msg;                        guardarEnSD(registro); }
}

// ============================================================
//  LEER SERIAL DESDE ARDUINO
// ============================================================
void leerSerialArduino() {
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\n') {
      procesarMensajeArduino(bufferSerial);
      bufferSerial = "";
    } else if (c != '\r') {
      bufferSerial += c;
    }
  }
}

// ============================================================
//  PROCESAR HUELLA
// ============================================================
void procesarHuella() {
  int p = finger.getImage();
  if (p == FINGERPRINT_NOFINGER) return;

  Serial.println("HUELLA:LEYENDO");

  if (p != FINGERPRINT_OK) {
    manejarHuellaFallida();
    return;
  }

  p = finger.image2Tz();
  if  (p != FINGERPRINT_OK) {
    manejarHuellaFallida();
    return;
  }
  p = finger.fingerSearch();
  if (p == FINGERPRINT_OK) {
    huellasFallidasConsecutivas = 0; // Resetear fallos de huella
    String id  = String(finger.fingerID);
    String msg = "[" + obtenerFechaHora() + "] HUELLA OK - ID #" + id;
    guardarEnSD(msg);
    Serial.println("HUELLA:OK:" + id);
    // Nota: El Arduino enviará OK:HUELLA al procesar esto y abrir
  } else {
    manejarHuellaFallida();
  }

  delay(1500);  // pausa para no repetir el mismo dedo
}

void manejarHuellaFallida() {
  // Limpiar contador si paso mucho tiempo (>60s)
  if (millis() - tUltimaHuellaFallida > 60000) {
    huellasFallidasConsecutivas = 0;
  }
  
  huellasFallidasConsecutivas++;
  tUltimaHuellaFallida = millis();

  // Enviar comando al Arduino
  Serial.println("HUELLA:DENEGADO");
  
  // Guardar log estándar (el Arduino también nos enviará DENEGADO:HUELLA)
  // Pero aquí verificamos la regla de las múltiples fallas locales:
  if (huellasFallidasConsecutivas >= 3) {
    String msgAlerta = "[" + obtenerFechaHora() + "] ALERTA - Multiples intentos huella: " + String(huellasFallidasConsecutivas) + " en 60s";
    guardarEnSD(msgAlerta);
    parpadeoRojo(5, 100);
  }
}

// ============================================================
//  SETUP
// ============================================================
void setup() {
  Serial.begin(9600);
  delay(1000);

  pinMode(LED_ROJO,  OUTPUT); digitalWrite(LED_ROJO,  LOW);

  // RTC
  if (!rtc.begin()) {
    parpadeoRojo(10);
    while (1) delay(10);
  }
  if (rtc.lostPower()) {
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  // SD
  if (!SD.begin(CS_PIN)) {
    parpadeoRojo(5);
    while (1) delay(10);
  }
  if (!SD.exists("/accesos.txt")) {
    File f = SD.open("/accesos.txt", FILE_WRITE);
    if (f) { f.println("=== LOG DE ACCESOS ==="); f.close(); }
  }

  // Sensor de huella
  finger.begin(57600);
  if (!finger.verifyPassword()) {
    parpadeoRojo(6);
    while (1) delay(10);
  }

  guardarEnSD("[" + obtenerFechaHora() + "] SISTEMA INICIADO");

  // Señal de arranque exitoso
  parpadeoRojo(2, 200); 
}

// ============================================================
//  LOOP
// ============================================================
void loop() {
  leerSerialArduino();
  procesarHuella();

  // Enviar hora cada 10 segundos
  if (millis() - tUltimoEnvioHora >= 10000) {
    enviarHoraArduino();
    tUltimoEnvioHora = millis();
  }

  // Auto-limpieza del contador de huellas si pasa 1 minuto
  if (huellasFallidasConsecutivas > 0 && (millis() - tUltimaHuellaFallida > 60000)) {
    huellasFallidasConsecutivas = 0;
  }

  delay(50);
}