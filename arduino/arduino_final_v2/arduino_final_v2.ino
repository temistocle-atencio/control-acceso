/*
 * =====================================================================================
 * SISTEMA DE CONTROL DE ACCESO - Arduino Uno (VERSIÓN CON MEJORAS)
 * =====================================================================================
 */

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Stepper.h>
#include <Keypad.h>
#include <SPI.h>
#include <MFRC522.h>

// ── LCD ────────────────────────────────────────────────────────
LiquidCrystal_I2C lcd(0x27, 16, 2);
bool lcdEncendido = true;

// ── BUZZER + TRIG COMPARTIDOS ──────────────────────────────────
const int TRIG_BUZZER_PIN = 7;
const int ECHO_PIN        = 6;

// ── RFID ───────────────────────────────────────────────────────
const int SS_PIN  = 2;
const int RST_PIN = -1;
MFRC522 mfrc522(SS_PIN, RST_PIN);

#define TOTAL_UIDS 1
byte UIDS_AUTORIZADOS[TOTAL_UIDS][4] = {
  {0x74, 0x7F, 0xFD, 0x27}
};

// ── TECLADO 3x2 ────────────────────────────────────────────────
const byte FILAS    = 3;
const byte COLUMNAS = 2;
char keys[FILAS][COLUMNAS] = {
  {'1','2'},
  {'4','5'},
  {'7','8'}
};
byte pinesFilas[FILAS]       = {5, 4, 3};
byte pinesColumnas[COLUMNAS] = {A0, A1};
Keypad teclado = Keypad(makeKeymap(keys), pinesFilas, pinesColumnas, FILAS, COLUMNAS);

// ── MOTOR ──────────────────────────────────────────────────────
const int PASOS_POR_VUELTA = 2038;
Stepper motorPuerta(PASOS_POR_VUELTA, 8, 9, 10, A2);
const long POSICION_ABIERTO = 2100;
const long POSICION_CERRADO = 150;
long posicionActual = POSICION_CERRADO;
bool puertaAbierta = false;

// ── CLAVES ─────────────────────────────────────────────────────
String claveActual = "1458";
String claveIngresada = "";
String nuevaClaveTemp = "";
unsigned long tUltimaTeclaSecuencia = 0;
char ultimaTecla = '\0';

// ── TOUCH ──────────────────────────────────────────────────────
const int TOUCH_PIN = A3;
int touchAnterior   = LOW;

// ── ULTRASÓNICO / ALERTAS / LCD ────────────────────────────────
const int DISTANCIA_ALERTA = 20;
const int DISTANCIA_PRESENCIA = 80;
const int DISTANCIA_FORZADO = 10;

unsigned long tUltimaAlerta        = 0;
const unsigned long INTERVALO_ALERTA = 800;
bool alertaEnviada                 = false;

unsigned long tUltimaPresencia = 0;
unsigned long tInicioDeteccionCorta = 0;
bool posibleForzado = false;

// ── ESTADOS ────────────────────────────────────────────────────
enum Estado { 
  ESPERA, 
  MOSTRANDO_MENSAJE, 
  PUERTA_ABIERTA, 
  ANALIZANDO_HUELLA,
  BLOQUEADO,
  CAMBIO_CLAVE_ACTUAL,
  CAMBIO_CLAVE_NUEVA,
  CAMBIO_CLAVE_CONFIRM
};
Estado estadoActual = ESPERA;

unsigned long tEstado          = 0;
const unsigned long DURACION_MENSAJE = 1500;
const unsigned long DURACION_ABIERTA = 5000;
const unsigned long DURACION_BLOQUEO = 30000;
unsigned long tUltimaMedicion  = 0;
bool avisoCierreEmitido        = false;

// ── TIEMPO Y DISPLAY ───────────────────────────────────────────
int horaInt = 0, minInt = 0, segInt = 0;
bool horaSincronizada = false;
unsigned long tUltimoSegundo = 0;

String horaUltimoAcceso = "";
unsigned long tUltimoCambioDisplay = 0;
bool mostrandoHoraActual = true;

// ── CONTADOR INTENTOS FALLIDOS ─────────────────────────────────
int intentosFallidos = 0;

// ── BUFFER SERIAL DESDE ESP8266 ────────────────────────────────
String bufferSerial = "";

// ============================================================
//  ENVIAR AL ESP8266
// ============================================================
void enviarESP(String msg) {
  Serial.println(msg);
}

// ============================================================
//  BUZZER (COMPARTIDO CON TRIG)
// ============================================================
void pitidoArranque() {
  noTone(TRIG_BUZZER_PIN);
  tone(TRIG_BUZZER_PIN, 800,  200); delay(250);
  tone(TRIG_BUZZER_PIN, 1200, 200); delay(250);
  tone(TRIG_BUZZER_PIN, 1800, 200); delay(300);
  noTone(TRIG_BUZZER_PIN);
}

void pitidoTecla() {
  noTone(TRIG_BUZZER_PIN);
  tone(TRIG_BUZZER_PIN, 2000, 50);
  delay(60);
  noTone(TRIG_BUZZER_PIN);
}

void pitidoConcedido() {
  noTone(TRIG_BUZZER_PIN);
  tone(TRIG_BUZZER_PIN, 1000, 150); delay(200);
  tone(TRIG_BUZZER_PIN, 1500, 150); delay(200);
  noTone(TRIG_BUZZER_PIN);
}

void pitidoDenegado() {
  noTone(TRIG_BUZZER_PIN);
  tone(TRIG_BUZZER_PIN, 300, 700);
  delay(750);
  noTone(TRIG_BUZZER_PIN);
}

void pitidoTouch() {
  noTone(TRIG_BUZZER_PIN);
  tone(TRIG_BUZZER_PIN, 1200, 200);
  delay(220);
  noTone(TRIG_BUZZER_PIN);
}

void pitidoAlerta() {
  noTone(TRIG_BUZZER_PIN);
  tone(TRIG_BUZZER_PIN, 1500, 100); delay(150);
  tone(TRIG_BUZZER_PIN, 800,  100); delay(150);
  noTone(TRIG_BUZZER_PIN);
}

void pitidoCierre() {
  noTone(TRIG_BUZZER_PIN);
  tone(TRIG_BUZZER_PIN, 1000, 200); delay(250);
  tone(TRIG_BUZZER_PIN, 700,  200); delay(250);
  noTone(TRIG_BUZZER_PIN);
}

void pitidoBloqueo() {
  noTone(TRIG_BUZZER_PIN);
  tone(TRIG_BUZZER_PIN, 200, 2000);
  delay(2100);
  noTone(TRIG_BUZZER_PIN);
}

// ============================================================
//  FALLOS Y BLOQUEO
// ============================================================
void registrarFallo(String motivo) {
  intentosFallidos++;
  enviarESP(motivo);
  if (intentosFallidos >= 3) {
    pitidoBloqueo();
    enviarESP("ALERTA:BLOQUEO");
    cambiarEstado(BLOQUEADO);
  } else {
    pitidoDenegado();
    cambiarEstado(MOSTRANDO_MENSAJE);
  }
}

void resetearFallos() {
  intentosFallidos = 0;
  // Solo se guarda la hora si hay un acceso exitoso y tenemos la hora
  if (horaSincronizada) {
    String h = (horaInt < 10) ? "0" + String(horaInt) : String(horaInt);
    String m = (minInt < 10) ? "0" + String(minInt) : String(minInt);
    horaUltimoAcceso = h + ":" + m; // Guardamos en formato corto HH:MM
  }
}

// ============================================================
//  RELOJ LOCAL (SINCRONIZADO CON ESP8266)
// ============================================================
void actualizarRelojLocal() {
  if (!horaSincronizada) return;
  // Avanza el segundero localmente sin bloquear el código
  while (millis() - tUltimoSegundo >= 1000) {
    tUltimoSegundo += 1000;
    segInt++;
    if (segInt >= 60) {
      segInt = 0;
      minInt++;
      if (minInt >= 60) {
        minInt = 0;
        horaInt++;
        if (horaInt >= 24) horaInt = 0;
      }
    }
  }
}

String obtenerHoraFormateada() {
  if (!horaSincronizada) return "--:--:--";
  String h = (horaInt < 10) ? "0" + String(horaInt) : String(horaInt);
  String m = (minInt < 10) ? "0" + String(minInt) : String(minInt);
  String s = (segInt < 10) ? "0" + String(segInt) : String(segInt);
  return h + ":" + m + ":" + s;
}

// ============================================================
//  DISPLAY ESPERA
// ============================================================
void actualizarDisplayEspera() {
  if (estadoActual != ESPERA) return;
  if (!lcdEncendido) return; // No actualizar si esta apagado

  String linea0 = "";
  
  if (claveIngresada.length() > 0) {
    linea0 = "PIN: " + claveIngresada;
  } else {
    // Alternar mensaje de tiempo cada 3 seg
    if (millis() - tUltimoCambioDisplay >= 3000) {
      tUltimoCambioDisplay = millis();
      if (horaUltimoAcceso != "") {
        mostrandoHoraActual = !mostrandoHoraActual;
      } else {
        mostrandoHoraActual = true;
      }
    }

    if (mostrandoHoraActual) {
      if (intentosFallidos > 0) {
        linea0 = "Seg. Activa F:" + String(intentosFallidos);
      } else {
        // Muestra los segundos avanzando en vivo
        linea0 = "Seg. " + obtenerHoraFormateada(); 
      }
    } else {
      linea0 = "Ultimo: " + horaUltimoAcceso;
    }
  }

  // Rellenar espacios
  while (linea0.length() < 16) linea0 += " ";

  // Refresco SIN PARPADEO (solo envía comandos si la línea cambió)
  static String ultimaLinea0 = "";
  if (linea0 != ultimaLinea0) {
    lcd.setCursor(0, 0);
    lcd.print(linea0);
    ultimaLinea0 = linea0;
  }
}

// ============================================================
//  PROCESAR MENSAJES DEL ESP8266
// ============================================================
void procesarMensajeESP(String msg) {
  msg.trim();

  if (msg.startsWith("HORA:")) {
    // Sincroniza el reloj interno a partir del formato HORA:HH:MM:SS
    horaInt = msg.substring(5, 7).toInt();
    minInt  = msg.substring(8, 10).toInt();
    segInt  = msg.substring(11, 13).toInt();
    horaSincronizada = true;
    tUltimoSegundo = millis(); // Reinicia el timer de 1 seg
  }
  else if (msg.startsWith("HUELLA:OK:")) {
    String id = msg.substring(10);
    resetearFallos();
    pitidoConcedido();
    lcd.backlight(); lcdEncendido = true;
    lcd.clear();
    lcd.setCursor(0, 0); lcd.print("Huella OK ID:#" + id);
    lcd.setCursor(0, 1); lcd.print("Abriendo Puerta");
    abrirPuerta();
    enviarESP("OK:HUELLA");
  } 
  else if (msg == "HUELLA:DENEGADO") {
    lcd.backlight(); lcdEncendido = true;
    lcd.clear();
    lcd.setCursor(0, 0); lcd.print("Dedo Incorrecto");
    lcd.setCursor(0, 1); lcd.print("Acceso Denegado");
    registrarFallo("DENEGADO:HUELLA");
  } 
  else if (msg == "HUELLA:LEYENDO") {
    lcd.backlight(); lcdEncendido = true;
    lcd.clear();
    lcd.setCursor(0, 0); lcd.print("Analizando...");
    lcd.setCursor(0, 1); lcd.print("Espere por favor");
    estadoActual = ANALIZANDO_HUELLA;
    tEstado = millis();
  }
}

// ============================================================
//  LEER SERIAL DESDE ESP8266
// ============================================================
void leerSerialESP() {
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\n') {
      procesarMensajeESP(bufferSerial);
      bufferSerial = "";
    } else if (c != '\r') {
      bufferSerial += c;
    }
  }
}

// ============================================================
//  PUERTA
// ============================================================
void moverMotor(long posicionObjetivo) {
  long pasos = posicionObjetivo - posicionActual;
  if (pasos == 0) return;
  motorPuerta.step(pasos);
  posicionActual = posicionObjetivo;
}

void abrirPuerta() {
  moverMotor(POSICION_ABIERTO);
  puertaAbierta = true;
  avisoCierreEmitido = false;
  cambiarEstado(PUERTA_ABIERTA);
}

void cerrarPuerta() {
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print("Cerrando...");
  moverMotor(POSICION_CERRADO);
  puertaAbierta = false;
}

// ============================================================
//  SETUP
// ============================================================
void setup() {
  Serial.begin(9600);

  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Sistema Listo");

  pinMode(TRIG_BUZZER_PIN, OUTPUT);
  digitalWrite(TRIG_BUZZER_PIN, LOW);
  pinMode(ECHO_PIN, INPUT);
  pinMode(TOUCH_PIN, INPUT);

  SPI.begin();
  mfrc522.PCD_Init();
  motorPuerta.setSpeed(10);

  tUltimaPresencia = millis();

  pitidoArranque();
  delay(800);
  cambiarEstado(ESPERA);
}

// ============================================================
//  LOOP
// ============================================================
void loop() {
  leerSerialESP();
  actualizarRelojLocal(); // Avanza el reloj interno

  // El touch se lee en varios estados (para no quedar atrapado)
  if (estadoActual == ESPERA || estadoActual == PUERTA_ABIERTA || estadoActual == BLOQUEADO) {
    leerTouch();
  }

  // El sensor de distancia controla alertas, forzado y LCD
  mostrarYControlarDistancia();

  switch (estadoActual) {

    case ESPERA:
      leerTeclado();
      leerRFID();
      actualizarDisplayEspera();
      break;

    case ANALIZANDO_HUELLA:
      if (millis() - tEstado >= 5000) cambiarEstado(ESPERA);
      break;

    case MOSTRANDO_MENSAJE:
      if (millis() - tEstado >= DURACION_MENSAJE) cambiarEstado(ESPERA);
      break;
      
    case BLOQUEADO:
      lcd.setCursor(0,0); lcd.print("BLOQUEADO 30s   ");
      lcd.setCursor(0,1); lcd.print("Espere...       ");
      if (millis() - tEstado >= DURACION_BLOQUEO) {
        intentosFallidos = 0; // Se resetean tras el castigo
        cambiarEstado(ESPERA);
      }
      break;

    case PUERTA_ABIERTA:
      if (!avisoCierreEmitido && millis() - tEstado >= (DURACION_ABIERTA - 1000)) {
        avisoCierreEmitido = true;
        pitidoCierre();
      }
      if (millis() - tEstado >= DURACION_ABIERTA) {
        cerrarPuerta();
        enviarESP("CERRADO:AUTO");
        cambiarEstado(ESPERA);
      }
      break;

    case CAMBIO_CLAVE_ACTUAL:
      leerTecladoClave(1);
      break;
    case CAMBIO_CLAVE_NUEVA:
      leerTecladoClave(2);
      break;
    case CAMBIO_CLAVE_CONFIRM:
      leerTecladoClave(3);
      break;
  }
}

// ============================================================
//  TOUCH
// ============================================================
void leerTouch() {
  static unsigned long tUltimoToque = 0;
  int touchActual = digitalRead(TOUCH_PIN);

  if (touchActual == HIGH && touchAnterior == LOW && (millis() - tUltimoToque > 200)) {
    tUltimoToque = millis();
    lcd.backlight(); lcdEncendido = true;

    if (!puertaAbierta) {
      pitidoTouch();
      lcd.clear(); lcd.setCursor(0, 0); lcd.print("Touch: Abrir");
      lcd.setCursor(0, 1); lcd.print("Abriendo Puerta");
      enviarESP("OK:TOUCH");
      // Touch es un acceso exitoso (salida), resetea fallos
      resetearFallos();
      abrirPuerta();
    } else {
      pitidoTouch();
      lcd.clear(); lcd.setCursor(0, 0); lcd.print("Touch: Cerrar");
      enviarESP("CERRADO:TOUCH");
      cerrarPuerta();
      cambiarEstado(ESPERA);
    }
  }
  touchAnterior = touchActual;
}

// ============================================================
//  TECLADO Y SECUENCIAS
// ============================================================
void leerTeclado() {
  char tecla = teclado.getKey();
  if (!tecla) return;

  lcd.backlight(); lcdEncendido = true;
  tUltimaPresencia = millis();

  // Deteccion rapida para cambio de clave (7 y 8)
  if ((ultimaTecla == '7' && tecla == '8') || (ultimaTecla == '8' && tecla == '7')) {
      if (millis() - tUltimaTeclaSecuencia < 500) {
          cambiarEstado(CAMBIO_CLAVE_ACTUAL);
          claveIngresada = "";
          ultimaTecla = '\0';
          return;
      }
  }
  ultimaTecla = tecla;
  tUltimaTeclaSecuencia = millis();

  pitidoTecla();
  claveIngresada += tecla;

  if (claveIngresada.length() == 4) {
    if (claveIngresada == claveActual) {
      resetearFallos();
      pitidoConcedido();
      lcd.clear(); lcd.setCursor(0, 0); lcd.print("Acceso Concedido");
      lcd.setCursor(0, 1); lcd.print("Abriendo Puerta");
      enviarESP("OK:PIN");
      abrirPuerta();
    } else {
      lcd.clear(); lcd.setCursor(0, 0); lcd.print("Acceso Denegado");
      registrarFallo("DENEGADO:PIN");
    }
    claveIngresada = "";
    ultimaTecla = '\0';
  }
}

// Logica separada para el wizard de cambio de clave
void leerTecladoClave(int paso) {
  char tecla = teclado.getKey();
  if (!tecla) return;
  pitidoTecla();
  claveIngresada += tecla;

  lcd.setCursor(0, 1);
  lcd.print(claveIngresada);
  lcd.print("    ");

  if (claveIngresada.length() == 4) {
    delay(300); // Pequeña pausa para ver el ultimo digito
    
    if (paso == 1) { // Validando actual
      if (claveIngresada == claveActual) {
        cambiarEstado(CAMBIO_CLAVE_NUEVA);
      } else {
        lcd.clear(); lcd.print("Clave Incorrecta");
        cambiarEstado(MOSTRANDO_MENSAJE);
      }
    } 
    else if (paso == 2) { // Guardando nueva temporal
      nuevaClaveTemp = claveIngresada;
      cambiarEstado(CAMBIO_CLAVE_CONFIRM);
    }
    else if (paso == 3) { // Confirmando
      if (claveIngresada == nuevaClaveTemp) {
        claveActual = nuevaClaveTemp;
        lcd.clear(); lcd.print("Clave Cambiada!");
        enviarESP("INFO:CLAVE_CAMBIADA");
      } else {
        lcd.clear(); lcd.print("No coinciden!");
      }
      cambiarEstado(MOSTRANDO_MENSAJE);
    }
    claveIngresada = "";
  }
}

// ============================================================
//  RFID
// ============================================================
void leerRFID() {
  if (!(mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial())) return;

  lcd.backlight(); lcdEncendido = true;
  tUltimaPresencia = millis();
  
  bool autorizado = uidAutorizado(mfrc522.uid.uidByte, mfrc522.uid.size);
  mfrc522.PICC_HaltA();

  if (autorizado) {
    resetearFallos();
    pitidoConcedido();
    lcd.clear(); lcd.setCursor(0, 0); lcd.print("Tarjeta OK");
    lcd.setCursor(0, 1); lcd.print("Abriendo Puerta");
    enviarESP("OK:RFID");
    abrirPuerta();
  } else {
    lcd.clear(); lcd.setCursor(0, 0); lcd.print("Tarjeta Invalida");
    registrarFallo("DENEGADO:RFID");
  }
}

bool uidAutorizado(byte *uid, byte len) {
  if (len != 4) return false;
  for (int i = 0; i < TOTAL_UIDS; i++) {
    bool coincide = true;
    for (int j = 0; j < 4; j++) {
      if (uid[j] != UIDS_AUTORIZADOS[i][j]) { coincide = false; break; }
    }
    if (coincide) return true;
  }
  return false;
}

// ============================================================
//  ULTRASONIDO (Distancia, Presencia, Alerta, Forzado)
// ============================================================
void mostrarYControlarDistancia() {
  if (millis() - tUltimaMedicion < 500) return;
  tUltimaMedicion = millis();

  int distancia = leerDistanciaCM();

  // Control de Backlight por presencia
  if (distancia > 0 && distancia <= DISTANCIA_PRESENCIA) {
    tUltimaPresencia = millis();
    if (!lcdEncendido) {
      lcd.backlight();
      lcdEncendido = true;
    }
  } else {
    if (lcdEncendido && (millis() - tUltimaPresencia > 15000)) {
      lcd.noBacklight();
      lcdEncendido = false;
      // Reseteamos display visual al apagar
      claveIngresada = "";
    }
  }

  // Display en LCD si estamos en espera y no ingresando clave
  if (estadoActual == ESPERA && claveIngresada.length() == 0 && lcdEncendido) {
    String linea1 = "Dist: ";
    if (distancia == 0 || distancia >= 400) {
      linea1 += "-- cm";
    } else {
      linea1 += String(distancia) + " cm";
    }
    // Rellenamos el resto de la linea para sobreescribir fantasmas
    while(linea1.length() < 16) linea1 += " ";
    
    // Antiparpadeo en la línea de distancia
    static String ultimaLinea1 = "";
    if (linea1 != ultimaLinea1) {
      lcd.setCursor(0, 1);
      lcd.print(linea1);
      ultimaLinea1 = linea1;
    }
  }

  // Si estamos en un estado sensible a lectura de distancia
  if (distancia > 0 && distancia < 400) {
    
    // Alerta de proximidad (<20cm) en ESPERA o BLOQUEADO
    if (distancia <= DISTANCIA_ALERTA && (estadoActual == ESPERA || estadoActual == BLOQUEADO)) {
      if (millis() - tUltimaAlerta >= INTERVALO_ALERTA) {
        tUltimaAlerta = millis();
        pitidoAlerta();
        if (!alertaEnviada) {
          enviarESP("ALERTA:SENSOR:" + String(distancia));
          alertaEnviada = true;
        }
      }
    } else {
      alertaEnviada = false;
    }

    // Detección de Apertura Forzada (<10cm por >2s estando cerrada)
    if (distancia <= DISTANCIA_FORZADO && !puertaAbierta) {
      if (tInicioDeteccionCorta == 0) {
        tInicioDeteccionCorta = millis();
      } else if (millis() - tInicioDeteccionCorta > 2000 && !posibleForzado) {
        posibleForzado = true;
        lcd.backlight(); lcdEncendido = true;
        lcd.clear();
        lcd.setCursor(0,0); lcd.print("ALERTA FORZADO!");
        enviarESP("ALERTA:FORZADO");
        // No cambiamos el estado bloqueante, pero alertamos
      }
    } else {
      tInicioDeteccionCorta = 0;
      posibleForzado = false;
    }
  }
}

int leerDistanciaCM() {
  noTone(TRIG_BUZZER_PIN);
  digitalWrite(TRIG_BUZZER_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_BUZZER_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_BUZZER_PIN, LOW);
  long duracion = pulseIn(ECHO_PIN, HIGH, 30000);
  if (duracion == 0) return 0;
  return (int)(duracion * 0.034 / 2);
}

// ============================================================
//  CAMBIO DE ESTADO
// ============================================================
void cambiarEstado(Estado nuevo) {
  estadoActual  = nuevo;
  tEstado       = millis();
  alertaEnviada = false;
  
  if (nuevo == ESPERA) {
    noTone(TRIG_BUZZER_PIN);
    claveIngresada = "";
    ultimaTecla = '\0';
    if(lcdEncendido){
      lcd.clear();
      actualizarDisplayEspera();
    }
  }
  else if (nuevo == CAMBIO_CLAVE_ACTUAL) {
    lcd.clear();
    lcd.setCursor(0,0); lcd.print("Clave actual:");
    claveIngresada = "";
  }
  else if (nuevo == CAMBIO_CLAVE_NUEVA) {
    lcd.clear();
    lcd.setCursor(0,0); lcd.print("Nueva clave:");
    claveIngresada = "";
  }
  else if (nuevo == CAMBIO_CLAVE_CONFIRM) {
    lcd.clear();
    lcd.setCursor(0,0); lcd.print("Confirmar:");
    claveIngresada = "";
  }
}