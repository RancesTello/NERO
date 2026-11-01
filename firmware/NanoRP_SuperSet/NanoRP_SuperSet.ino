#include <WiFiNINA.h>
#include <WiFiUdp.h>
#include <Servo.h>

#define INPUT_PIN A0
#define SAMPLE_RATE 500

int EMG_SIGNAL = 0;
int EMG_PAST = 0;
int EMG_ULTI = 0;

bool band = false;
unsigned long tiempoDeEspera = 0;

Servo servo;

// Configuración WiFi
#include "secrets.h"   // credenciales (ver secrets.example.h)
const char* ssid = SECRET_WIFI_SSID;
const char* password = SECRET_WIFI_PASSWORD;
const char* host = "172.20.10.6";
const int port = 8088;

// Estados del brazo
enum EstadoBrazo {REPOSO, FLEXION, EXTENSION};
EstadoBrazo estadoActual = REPOSO;
EstadoBrazo estadoAnterior = REPOSO;

// Control del servo
int pos = 90;
int objetivo = 90;
bool moviendoServo = false;
const int paso = 1;
const unsigned long intervaloMovimiento = 20; // Movimiento suave para MG996R
unsigned long ultimoMovimiento = 0;

// UDP
WiFiUDP udp;
const int localPort = 8089;

// Variables EEG
int receivedTheta = 0;
int receivedAlpha = 0;
int receivedDelta = 0;
int receivedBeta = 0;
int receivedAttention = 0;
int receivedMeditation = 0;
int receivedFocusRatio = 0;
int receivedSignalQuality = 0;
int receivedStatus = 0;
bool dataReceived = false;

// Control de envío UDP
unsigned long ultimoEnvioUDP = 0;
const unsigned long intervaloEnvioUDP = 500;

// Verificación de conexión WiFi
unsigned long ultimaVerificacionWiFi = 0;
const unsigned long intervaloVerificacionWiFi = 5000;

void conectarWiFi() {
  //Serial.print("Conectando a WiFi: ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  int intentos = 0;
  while (WiFi.status() != WL_CONNECTED && intentos < 20) {
    delay(500);
    Serial.print(".");
    intentos++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi conectado!");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
    Serial.print("Enviando datos a: ");
    Serial.print(host);
    Serial.print(":");
    Serial.println(port);
  } else {
    Serial.println("\nERROR: No se pudo conectar a WiFi");
  }
}

void setup() {
  Serial1.begin(57600);

  delay(2000);

  Serial.println("\n=== Sistema de Control EEG 1===");
  Serial.println("Iniciando...");

  conectarWiFi();

  if (udp.begin(localPort)) {
    Serial.println("UDP iniciado en puerto " + String(localPort));
  } else {
    Serial.println("ERROR: No se pudo iniciar UDP");
  }

  servo.attach(2);
  pos = 90;
  objetivo = 90;
  servo.write(pos);
  delay(500);

  Serial.println("Sistema listo");
  Serial.println("Esperando datos EEG...\n");
}

void enviarDatosUDP() {
  if (WiFi.status() != WL_CONNECTED) return;

  // Construir mensaje JSON
  String mensaje = "{\"theta\":" + String(receivedTheta) +
                   ",\"alpha\":" + String(receivedAlpha) +
                   ",\"delta\":" + String(receivedDelta) +
                   ",\"beta\":" + String(receivedBeta) +
                   ",\"attention\":" + String(receivedAttention) +
                   ",\"meditation\":" + String(receivedMeditation) +
                   ",\"focusRatio\":" + String(receivedFocusRatio) +
                   ",\"signalQuality\":" + String(receivedSignalQuality) +
                   ",\"status\":" + String(receivedStatus) +
                   ",\"servoPos\":" + String(pos) +
                   ",\"dataValid\":" + String(dataReceived ? "true" : "false") +
                   "}";

  // Enviar UDP
  udp.beginPacket(host, port);
  udp.write((const uint8_t*)mensaje.c_str(), mensaje.length());
  int resultado = udp.endPacket();

  if (resultado == 1) {
    Serial.println("UDP enviado OK: " + mensaje);
  } else {
    Serial.println("ERROR enviando UDP");
  }
}

float EMGFilter(float input)
{
  float output = input;
  {
    static float z1, z2; // filter section state
    float x = output - 0.05159732 * z1 - 0.36347401 * z2;
    output = 0.01856301 * x + 0.03712602 * z1 + 0.01856301 * z2;
    z2 = z1;
    z1 = x;
  }
  {
    static float z1, z2; // filter section state
    float x = output - -0.53945795 * z1 - 0.39764934 * z2;
    output = 1.00000000 * x + -2.00000000 * z1 + 1.00000000 * z2;
    z2 = z1;
    z1 = x;
  }
  {
    static float z1, z2; // filter section state
    float x = output - 0.47319594 * z1 - 0.70744137 * z2;
    output = 1.00000000 * x + 2.00000000 * z1 + 1.00000000 * z2;
    z2 = z1;
    z1 = x;
  }
  {
    static float z1, z2; // filter section state
    float x = output - -1.00211112 * z1 - 0.74520226 * z2;
    output = 1.00000000 * x + -2.00000000 * z1 + 1.00000000 * z2;
    z2 = z1;
    z1 = x;
  }
  return output;
}

void actualizarEstado() {
  EstadoBrazo nuevoEstado = estadoActual;
  Serial.println("ACTU. ESTADO");

  switch (estadoActual) {
    case FLEXION:
      Serial.println("==== EJECUTANDO FLEXION ====");
      objetivo = 90;
      moviendoServo = true;
      break;

    case EXTENSION:
      Serial.println("==== EJECUTANDO EXTENSION ====");
      objetivo = 180;
      moviendoServo = true;
      break;

    case REPOSO:
      Serial.println("==== ESTADO REPOSO ====");
      moviendoServo = false;
      break;
  }
  estadoAnterior = estadoActual;

  if (moviendoServo && millis() - ultimoMovimiento >= intervaloMovimiento) {
    ultimoMovimiento = millis();

    // Verificar interrupción por cambio de estado
    if ((estadoActual == FLEXION && receivedStatus > 1) ||
        (estadoActual == EXTENSION && receivedStatus < -1)) {
      Serial.println("Movimiento interrumpido");
      moviendoServo = false;
      return;
    }

    // Movimiento gradual
    if (pos < objetivo) {
      pos += paso;
      if (pos > objetivo) pos = objetivo;
    } else if (pos > objetivo) {
      pos -= paso;
      if (pos < objetivo) pos = objetivo;
    }
    Serial.println("EJECT. MOV");
    servo.write(pos);

    // Verificar si llegó al objetivo
    if (pos == objetivo) {
      moviendoServo = false;
      Serial.println("Movimiento completado - Posición: " + String(pos));
    }
  }
}




void loop() {
  /*if (millis() - ultimaVerificacionWiFi >= intervaloVerificacionWiFi) {
    ultimaVerificacionWiFi = millis();
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("WiFi desconectado. Reconectando...");
      conectarWiFi();
    }
  }*/

  procesarDatosSerial();

  if (millis() - ultimoEnvioUDP >= intervaloEnvioUDP) {
    ultimoEnvioUDP = millis();
    enviarDatosUDP();
  }
}

void procesarDatosSerial() {
  // Calculate elapsed time
  static unsigned long past = 0;
  unsigned long present = micros();
  unsigned long interval = present - past;
  past = present;

  // Run timer
  static long timer = 0;
  timer -= interval;

  // Sample
  if (timer < 0) {
    timer += 1000000 / SAMPLE_RATE;
    float sensor_value = analogRead(INPUT_PIN);
    float signal = EMGFilter(sensor_value);

    if (signal < -220) {
      EMG_SIGNAL = signal;
      EMG_PAST = signal;
    }

    if ( millis() - EMG_ULTI >= 30000) {
      EMG_ULTI = millis();
      if (EMG_SIGNAL == EMG_PAST) {
        EMG_SIGNAL = 0;
      }

    }

    if (!Serial1.available()) return;

    String data = Serial1.readStringUntil('\n');
    data.trim();

    if (data.length() == 0) return;

    // Parsear datos: &attention?meditation#focusRatio$delta%theta!alpha@beta*quality/status;
    int startA = data.indexOf("&") + 1;
    int endA = data.indexOf("?");
    int startM = endA + 1;
    int endM = data.indexOf("#");
    int startF = endM + 1;
    int endF = data.indexOf("$");
    int startD = endF + 1;
    int endD = data.indexOf("%");
    int startT = endD + 1;
    int endT = data.indexOf("!");
    int startAl = endT + 1;
    int endAl = data.indexOf("@");
    int startB = endAl + 1;
    int endB = data.indexOf("*");
    int startQ = endB + 1;
    int endQ = data.indexOf("/");
    int startE = endQ + 1;
    int endE = data.indexOf(";");

    if (startA > 0 && endA > startA && endM > startM && endF > startF &&
        endD > startD && endT > startT && endAl > startAl &&
        endB > startB && endQ > startQ && endE > startE) {

      receivedAttention = data.substring(startA, endA).toInt();
      receivedMeditation = data.substring(startM, endM).toInt();
      receivedFocusRatio = data.substring(startF, endF).toInt();
      receivedDelta = data.substring(startD, endD).toInt();
      receivedTheta = data.substring(startT, endT).toInt();
      receivedAlpha = data.substring(startAl, endAl).toInt();
      receivedBeta = data.substring(startB, endB).toInt();
      receivedSignalQuality = data.substring(startQ, endQ).toInt();
      receivedStatus = data.substring(startE, endE).toInt();

      dataReceived = true;

      Serial.println("--- Datos EEG recibidos ---");
      Serial.println("Attention: " + String(receivedAttention));
      Serial.println("Meditation: " + String(receivedMeditation));
      Serial.println("Focus Ratio: " + String(receivedFocusRatio));
      Serial.println("Status: " + String(receivedStatus));
      Serial.println("Signal: " + String(EMG_SIGNAL));

      if (receivedAttention > 40 && receivedStatus < -7 && EMG_SIGNAL < -220) {
        if (!band) {
          band = true;
          tiempoDeEspera = millis();
        } else {
          if ((millis() - tiempoDeEspera) > 1000) {
            estadoActual = FLEXION;
          }
        }
      } else if (receivedAttention > 40 && receivedStatus > 7 && EMG_SIGNAL < -220) {
        if (!band) {
          band = true;
          tiempoDeEspera = millis();
        } else {
          if ((millis() - tiempoDeEspera) > 1000) {
            estadoActual = EXTENSION;
          }
        }
      } else {
        tiempoDeEspera = 0;
        band = false;
        estadoActual = REPOSO;
      }



      // Actualizar estado según comando recibido
      actualizarEstado();

    } else {
      Serial.println("ERROR: Formato de datos incorrecto");
      Serial.println("Recibido: " + data);
    }
  }
}
