# 🔐 Sistema de Control de Acceso

Sistema de control de acceso multimétodo basado en **Arduino Uno** y **NodeMCU ESP8266**,
con apertura por **clave, sensor táctil, huella digital, tarjeta RFID** y **WiFi** desde el
celular. Incluye funciones de seguridad como bloqueo por intentos fallidos, detección de
apertura forzada y alertas de proximidad.

---

## ✨ Características

- **Apertura por clave** en teclado matricial (PIN configurable, cambiable desde el teclado).
- **Apertura por sensor táctil** (botón capacitivo).
- **Apertura por huella digital** (sensor AS608/R307).
- **Apertura por tarjeta RFID** (MIFARE, lectura por UID).
- **Apertura remota por WiFi** desde el navegador del celular.
- **Seguridad:**
  - Bloqueo de 30 s tras 3 intentos fallidos.
  - Detección de apertura forzada con sensor ultrasónico.
  - Alertas de proximidad.
  - Cierre automático de la puerta.
- **Reloj en tiempo real** (RTC DS3231) mostrado en el LCD.

---

## 🧠 Arquitectura

El sistema usa **dos microcontroladores** que trabajan en conjunto y se comunican por
puerto serie (9600 baudios):

```
┌──────────────────────────┐        Serial (9600)        ┌──────────────────────────┐
│       ARDUINO UNO         │  <------------------------> │     NodeMCU ESP8266       │
│  (Control principal)      │                             │  (Periféricos + WiFi)     │
│                           │   ESP:"HUELLA:OK:1"  --->   │                           │
│  • Teclado / clave        │   <--- ARD:"OK:PIN"         │  • Sensor de huella       │
│  • Sensor táctil          │                             │  • Reloj RTC              │
│  • Lector RFID            │                             │  • Servidor web WiFi      │
│  • Motor de la puerta     │                             │                           │
│  • LCD + buzzer           │                             │                           │
│  • Sensor ultrasónico     │                             │                           │
└──────────────────────────┘                             └──────────────────────────┘
```

**Ventaja del diseño:** el Arduino es autónomo. Abre con clave y táctil aunque el ESP no
esté. El ESP añade la huella, el reloj y el WiFi **sin necesidad de modificar el código del
Arduino**: para abrir, el ESP simplemente envía el mensaje `HUELLA:OK:<id>` que el Arduino
ya sabe interpretar.

---

## 🔧 Hardware necesario

| Componente | Conectado a |
|---|---|
| Arduino Uno | — |
| NodeMCU ESP8266 | — |
| Módulo LCD 16x2 I2C | Arduino |
| Teclado matricial 3x2 | Arduino |
| Lector RFID RC522 | Arduino |
| Motor paso a paso 28BYJ-48 + driver ULN2003 | Arduino |
| Sensor táctil capacitivo | Arduino |
| Sensor ultrasónico HC-SR04 | Arduino |
| Buzzer | Arduino |
| Sensor de huella AS608 / R307 | ESP8266 |
| Reloj RTC DS3231 | ESP8266 |

---

## 📌 Conexiones (pinout)

### Arduino Uno

| Periférico | Pines |
|---|---|
| LCD I2C | SDA = A4, SCL = A5 (dirección 0x27) |
| Teclado (filas) | 5, 4, 3 |
| Teclado (columnas) | A0, A1 |
| RFID RC522 | SS = 2, MOSI = 11, MISO = 12, SCK = 13 |
| Motor (ULN2003) | 8, 9, 10, A2 |
| Sensor táctil | A3 |
| Ultrasónico (Trig + Buzzer, compartido) | 7 |
| Ultrasónico (Echo) | 6 |
| Enlace al ESP | RX = 0, TX = 1 |

### NodeMCU ESP8266

| Periférico | Pines |
|---|---|
| Sensor de huella | TX sensor → D4, RX sensor → D0 |
| Alimentación huella | VCC → 3V3, GND → GND |
| RTC DS3231 | SDA → D2, SCL → D1 |
| LED indicador | D3 |
| Enlace al Arduino | ESP TX → Arduino RX (0), ESP RX → Arduino TX (1) |

> ⚠️ **GND común:** el Arduino y el ESP deben compartir tierra (GND) para poder
> comunicarse por serial.

---

## 📚 Librerías necesarias

**Arduino:** `Wire`, `LiquidCrystal_I2C`, `Stepper`, `Keypad`, `SPI`, `MFRC522`

**ESP8266:** `ESP8266WiFi`, `ESP8266WebServer`, `SoftwareSerial`, `Adafruit_Fingerprint`,
`Wire`, `RTClib`

> `ESP8266WiFi` y `ESP8266WebServer` vienen incluidas con el paquete de placas ESP8266.

---

## ▶️ Puesta en marcha

1. **Arduino:** carga `arduino/arduino_final_v2/arduino_final_v2.ino`.
2. **ESP8266:** carga la versión que quieras usar (ver más abajo).
3. Conecta el enlace serial entre ambas placas **con GND común**.
4. Da energía a las dos placas.

### ¿Qué versión del ESP cargar?

| Archivo | Para qué sirve |
|---|---|
| `esp8266/esp8266_huella_wifi` | **Versión recomendada.** Huella + apertura remota por WiFi. Sin SD. |
| `esp8266/esp8266_huella_sin_sd` | Solo huella (sin WiFi, sin SD). |
| `esp8266/esp8266_final_v2` | Versión original con registro en tarjeta SD y RTC. |

---

## 🚪 Formas de abrir la puerta

- **Clave:** marca el PIN en el teclado (configurable en el código; se puede cambiar con la
  combinación 7 + 8 en el teclado).
- **Táctil:** un toque en el sensor capacitivo abre; otro toque cierra.
- **Huella:** apoya un dedo registrado en el sensor.
- **WiFi:** ver sección siguiente.

---

## 📲 Apertura remota por WiFi

Con la versión `esp8266_huella_wifi` cargada:

1. En el celular, conéctate a la red WiFi **`ControlAcceso`** (contraseña: **`abrir1234`**).
2. Abre el navegador en **`http://192.168.4.1`**.
3. Pulsa **"Abrir puerta"**.

> El ESP crea su propia red (modo Access Point), así que no necesita router ni internet.

---

## 👆 Registrar una nueva huella

1. Deja **solo el sensor de huella** conectado al ESP.
2. Carga `utilidades/registrar_huella/registrar_huella.ino`.
3. Abre el Monitor Serial a 9600 y sigue las instrucciones (apoya el dedo, retíralo, apóyalo
   de nuevo).
4. Cuando confirme "Huella guardada", vuelve a cargar la versión principal del ESP.

Para **diagnosticar** el sensor (ver si responde y cuántas huellas tiene guardadas), usa
`utilidades/test_huella_esp8266`.

---

## 📁 Estructura del repositorio

```
control-acceso/
├── README.md
├── arduino/
│   └── arduino_final_v2/          → código principal del Arduino
├── esp8266/
│   ├── esp8266_final_v2/          → versión original (con SD + RTC)
│   ├── esp8266_huella_sin_sd/     → solo huella, sin SD
│   └── esp8266_huella_wifi/       → huella + WiFi (recomendada)
└── utilidades/
    ├── registrar_huella/          → registrar una huella nueva
    └── test_huella_esp8266/       → diagnóstico del sensor
```

---

## 🚀 Mejoras futuras

- Registro de accesos en la nube con **Supabase** (reemplazo del log en SD).
- Panel web con historial de entradas.
- Apertura desde internet (fuera de la red local).
