/*
 * =====================================================================================
 * TEST INDEPENDIENTE DEL SENSOR DE HUELLA  -  NodeMCU ESP8266
 * =====================================================================================
 * Objetivo: comprobar si el sensor responde y cuantas huellas tiene guardadas,
 * aislado del RTC, la SD y el resto del sistema.
 *
 * NO necesitas reconectar nada: usa los MISMOS pines que tu codigo original
 *   SoftwareSerial mySer(D4, D0)   ->   RX=D4, TX=D0
 *
 * REQUISITO: libreria "Adafruit Fingerprint Sensor" instalada en el IDE.
 *
 * USO: sube este sketch al ESP8266 y abre el Monitor Serial a 9600 baudios.
 *      (Cuando termines, vuelves a subir tu codigo esp8266_final_v2.ino)
 * =====================================================================================
 */

#include <SoftwareSerial.h>
#include <Adafruit_Fingerprint.h>

SoftwareSerial mySer(D4, D0);            // RX=D4 (al TX del sensor), TX=D0 (al RX del sensor)
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySer);

void setup() {
  Serial.begin(9600);
  delay(300);
  Serial.println("\n=== TEST INDEPENDIENTE DE HUELLA (ESP8266) ===");

  finger.begin(57600);                   // mismo baudrate que usabas
  delay(50);

  if (finger.verifyPassword()) {
    Serial.println("Sensor ENCONTRADO  [OK]");
  } else {
    Serial.println("Sensor NO responde [FALLA] -> revisa cableado y voltaje");
    while (1) delay(10);
  }

  finger.getTemplateCount();
  Serial.print("Huellas registradas: ");
  Serial.println(finger.templateCount);
  if (finger.templateCount == 0) {
    Serial.println(">> No hay huellas guardadas: por eso 'no reconoce'.");
  }

  Serial.println("Pon un dedo para probar coincidencia...");
}

void loop() {
  if (finger.getImage() != FINGERPRINT_OK) return;   // no hay dedo todavia
  if (finger.image2Tz()  != FINGERPRINT_OK) return;   // no pudo procesar la imagen

  if (finger.fingerSearch() == FINGERPRINT_OK) {
    Serial.print("COINCIDE  ID #");
    Serial.print(finger.fingerID);
    Serial.print("  confianza ");
    Serial.println(finger.confidence);
  } else {
    Serial.println("Dedo leido pero NO coincide con ninguno guardado.");
  }

  delay(1500);
}
