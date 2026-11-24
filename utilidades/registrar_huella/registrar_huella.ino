/*
 * =====================================================================================
 * REGISTRAR UNA HUELLA  -  NodeMCU ESP8266
 * =====================================================================================
 * Guarda tu dedo como ID #1 en el sensor.
 * Deja SOLO el sensor conectado (VCC->3V3, GND->GND, TX->D4, RX->D0).
 *
 * USO:
 *   1. Sube este sketch.
 *   2. Abre el Monitor Serial a 9600.
 *   3. Sigue las instrucciones: pon el dedo, retíralo, y pon el MISMO dedo otra vez.
 *   4. Cuando diga "Huella guardada como ID #1", listo. No se repite.
 *   5. Despues sube el programa principal.
 * =====================================================================================
 */

#include <SoftwareSerial.h>
#include <Adafruit_Fingerprint.h>

SoftwareSerial mySer(D4, D0);            // RX=D4, TX=D0  (igual que tu sistema)
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySer);

uint8_t id = 1;                          // se registrara como ID #1

void setup() {
  Serial.begin(9600);
  delay(300);
  Serial.println("\n=== REGISTRO DE HUELLA (ESP8266) ===");

  finger.begin(57600);
  delay(50);

  if (finger.verifyPassword()) {
    Serial.println("Sensor OK. Vamos a registrar el ID #1.");
  } else {
    Serial.println("Sensor NO responde. Revisa cables y reinicia (RST).");
    while (1) delay(10);
  }
}

void loop() {
  Serial.println("\n>> Pon el dedo en el sensor para registrarlo...");

  while (!registrarHuella(id)) {
    Serial.println(">> Reintentemos. Pon el dedo de nuevo.");
    delay(1200);
  }

  Serial.println("\n################################################");
  Serial.println(">> LISTO: huella guardada como ID #1.");
  Serial.println(">> Ya puedes subir el programa principal.");
  Serial.println("################################################");
  while (1) delay(1000);                 // termina aqui
}

bool registrarHuella(uint8_t id) {
  int p = -1;

  // Primera captura
  Serial.println("Esperando dedo...");
  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    if (p == FINGERPRINT_NOFINGER) delay(100);
  }
  if (finger.image2Tz(1) != FINGERPRINT_OK) {
    Serial.println("No se pudo leer la imagen (1).");
    return false;
  }

  // Retirar dedo
  Serial.println("Bien. Ahora RETIRA el dedo...");
  delay(2000);
  p = 0;
  while (p != FINGERPRINT_NOFINGER) p = finger.getImage();

  // Segunda captura
  Serial.println("Pon el MISMO dedo otra vez...");
  p = -1;
  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    if (p == FINGERPRINT_NOFINGER) delay(100);
  }
  if (finger.image2Tz(2) != FINGERPRINT_OK) {
    Serial.println("No se pudo leer la imagen (2).");
    return false;
  }

  // Crear y guardar el modelo
  if (finger.createModel() != FINGERPRINT_OK) {
    Serial.println("Las dos lecturas no coinciden. Repite con el mismo dedo.");
    return false;
  }
  if (finger.storeModel(id) != FINGERPRINT_OK) {
    Serial.println("Error al guardar la huella.");
    return false;
  }

  return true;
}
