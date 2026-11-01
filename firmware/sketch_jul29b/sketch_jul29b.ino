#include <WiFi.h>
#include <WiFiUdp.h>
#include <Servo.h>

Servo servo;

#include "secrets.h"   // credenciales (ver secrets.example.h)
const char* ssid = SECRET_WIFI_SSID;
const char* password = SECRET_WIFI_PASSWORD;
const char* host = "172.20.10.6";
const int port = 8888;

enum EstadoBrazo {REPOSO, FLEXION, EXTENSION};
EstadoBrazo estadoActual = REPOSO;
EstadoBrazo estadoAnterior = REPOSO;

int pos = 90;

WiFiUDP udp;

// Variables globales para datos EEG
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

// Variables de movimiento suave
unsigned long ultimoMovimiento = 0;
int objetivo = 90;
bool moviendoServo = false;
int paso = 1;
unsigned long intervaloMovimiento = 50;

// (Opcional) Simulación de ángulo X para lógica de detención basada en MPU6050
int anguloX = 0; // <- Si lo usas, actualízalo con tus lecturas reales

void setup() {
  Serial.begin(57600);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nA-Conectado a WiFi");
  servo.attach(2);
  pos = 180;
  servo.write(pos);
}

void loop() {
  int theta, alpha, delta, beta, attention, meditation;

  // Verificar si hay datos seriales disponibles
  if (Serial.available()) {
    String data = Serial.readStringUntil('\n');
    data.trim();

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

      Serial.println("Datos recibidos correctamente:");
      Serial.println("Raw data: " + data);
      Serial.println("Attention: " + String(receivedAttention));
      Serial.println("Meditation: " + String(receivedMeditation));
      Serial.println("Focus Ratio: " + String(receivedFocusRatio));
      Serial.println("Delta: " + String(receivedDelta));
      Serial.println("Theta: " + String(receivedTheta));
      Serial.println("Alpha: " + String(receivedAlpha));
      Serial.println("Beta: " + String(receivedBeta));
      Serial.println("Signal Quality: " + String(receivedSignalQuality));
      Serial.println("Status: " + String(receivedStatus));
      Serial.println("---");

      switch (receivedStatus) {
        case 1:  // FLEXION
          if (estadoActual != FLEXION) {
            Serial.println("==== EJECUTANDO FLEXION ====");
            estadoActual = FLEXION;
            objetivo = 90;
            moviendoServo = true;
          }
          break;

        case 2:  // EXTENSION
          if (estadoActual != EXTENSION) {
            Serial.println("==== EJECUTANDO EXTENSIÓN ====");
            estadoActual = EXTENSION;
            objetivo = 180;
            moviendoServo = true;
          }
          break;

        case 0:  // REPOSO
          estadoActual = REPOSO;
          moviendoServo = false;
          break;
      }

      estadoAnterior = estadoActual;
    } else {
      Serial.println("Error en formato de datos");
    }
  }

  // Movimiento suave del servo
  if (moviendoServo && millis() - ultimoMovimiento >= intervaloMovimiento) {
    ultimoMovimiento = millis();

    // Interrupción por cambio de estado (opcional: añade condición de anguloX)
    if ((estadoActual == FLEXION && receivedStatus != 1) ||
        (estadoActual == EXTENSION && receivedStatus != 2)) {
      Serial.println("Movimiento interrumpido por cambio de estado");
      moviendoServo = false;
    }

    // Movimiento gradual
    if (pos < objetivo) {
      pos += paso;
      if (pos > objetivo) pos = objetivo;
      servo.write(pos);
    } else if (pos > objetivo) {
      pos -= paso;
      if (pos < objetivo) pos = objetivo;
      servo.write(pos);
    }

    if (pos == objetivo) {
      moviendoServo = false;
      Serial.println("Movimiento completado");
    }
  }

  // Si no se han recibido datos reales, simula
  if (!dataReceived) {
    theta = random(10, 100);
    alpha = random(10, 100);
    delta = random(10, 100);
    beta = random(10, 100);
    attention = random(0, 100);
    meditation = random(0, 100);
  } else {
    theta = receivedTheta;
    alpha = receivedAlpha;
    delta = receivedDelta;
    beta = receivedBeta;
    attention = receivedAttention;
    meditation = receivedMeditation;
  }

  // Enviar mensaje UDP
  String mensaje = "{\"theta\":" + String(theta) +
                   ",\"alpha\":" + String(alpha) +
                   ",\"delta\":" + String(delta) +
                   ",\"beta\":" + String(beta) +
                   ",\"attention\":" + String(attention) +
                   ",\"meditation\":" + String(meditation);

  if (dataReceived) {
    mensaje += ",\"focusRatio\":" + String(receivedFocusRatio) +
               ",\"signalQuality\":" + String(receivedSignalQuality);
  }

  mensaje += "}";

  udp.beginPacket(host, port);
  udp.write((const uint8_t*)mensaje.c_str(), mensaje.length());
  udp.endPacket();

  String status = dataReceived ? "REAL" : "SIMULADO";
  Serial.println("Enviado (" + status + "): " + mensaje);

  delay(500);
}
