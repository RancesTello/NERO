#include <SoftwareSerial.h>
#include <Servo.h>
#include "I2Cdev.h"
#include "MPU6050.h"
#include "Wire.h"

unsigned long previousMillis = 0;
const unsigned long interval = 2000; // 2 segundo

Servo servo;

SoftwareSerial BT(11, 10);// TX, RX
#define LED 13
#define BAUDRATE 57600
#define DEBUGOUTPUT 0
#define pinRele 2

byte  generatedChecksum = 0;
byte  checksum = 0;
int   payloadLength = 0;
byte  payloadData[64] = {0};

struct EEGData {
  byte poorQuality = 0;
  byte attention = 0;
  byte meditation = 0;

  long delta = 0, theta = 0, lowAlpha = 0, highAlpha = 0;
  long lowBeta = 0, highBeta = 0, lowGamma = 0, highGamma = 0;

  float deltaScaled = 0, thetaScaled = 0;
  float alphaScaled = 0, betaScaled = 0;
};

EEGData eeg;

int pos = 0;

enum EstadoBrazo {REPOSO, FLEXION, EXTENSION};
EstadoBrazo estadoActual = REPOSO;
EstadoBrazo estadoAnterior = REPOSO;

long lastReceivedPacket = 0;
boolean bigPacket = false;

unsigned long tiempoDeEspera = 0;
bool band = false;

class Giroscopio {
  public:
    float filtered_x = 0;
    float filtered_y = 0;
    MPU6050 sensor;
    bool initialized = false;

    Giroscopio() {
      // Constructor vacío
    }

    bool begin() {
      Serial.println("Inicializando MPU6050...");
      /*sensor.initialize();

      if (sensor.testConnection()) {
        Serial.println("MPU6050 conectado correctamente");

        sensor.setFullScaleAccelRange(MPU6050_ACCEL_FS_2); // ±2g
        sensor.setFullScaleGyroRange(MPU6050_GYRO_FS_250);  // ±250°/s
        sensor.setDLPFMode(MPU6050_DLPF_BW_20);             // Filtro paso bajo

        Serial.println("Calibrando sensor...");
        for (int i = 0; i < 100; i++) {
          actualizar();
          delay(10);
        }

        initialized = true;
        Serial.println("Calibración completada");
        return true;
      } else {
        Serial.println("Error: No se pudo conectar al MPU6050");
        Serial.println("Verificar conexiones:");
        Serial.println("  VCC -> 5V o 3.3V");
        Serial.println("  GND -> GND");
        Serial.println("  SCL -> A5 (pin 21)");
        Serial.println("  SDA -> A4 (pin 20)");
        return false;
      }*/
    }

    void actualizar() {
      if (!initialized) return;

      int16_t ax, ay, az;
      sensor.getAcceleration(&ax, &ay, &az);

      Serial.print("Raw: ax=");
      Serial.print(ax);
      Serial.print(", ay=");
      Serial.print(ay);
      Serial.print(", az=");
      Serial.println(az);

      if (ax == 0 && ay == 0 && az == 0) {
        Serial.println("Error: Valores del sensor son cero - revisar conexiones");
        return;
      }

      float denom_x = sqrt(float(ay) * ay + float(az) * az);
      float denom_y = sqrt(float(ax) * ax + float(az) * az);

      if (denom_x < 0.1) denom_x = 0.1;
      if (denom_y < 0.1) denom_y = 0.1;

      float accel_ang_x = atan(float(ax) / denom_x) * (180.0 / PI);
      float accel_ang_y = atan(float(ay) / denom_y) * (180.0 / PI);

      Serial.print("Angles: x=");
      Serial.print(accel_ang_x);
      Serial.print("°, y=");
      Serial.print(accel_ang_y);
      Serial.print("°");

      this->filtered_x = 0.8 * filtered_x + 0.2 * accel_ang_x;
      this->filtered_y = 0.8 * filtered_y + 0.2 * accel_ang_y;

      Serial.print(" -> Filtered: x=");
      Serial.print(filtered_x);
      Serial.print("°, y=");
      Serial.print(filtered_y);
      Serial.println("°");
    }

    void getAngles(float &x, float &y) {
      x = filtered_x;
      y = filtered_y;
    }

    void resetFilter() {
      filtered_x = 0;
      filtered_y = 0;
    }
};

Giroscopio Mov;

void setup() {
  Serial.begin(BAUDRATE);
  Serial.println("=== Iniciando sistema ===");

  Wire.begin();           // Iniciando I2C
  Serial.println("I2C inicializado");

  if (!Mov.begin()) {
    Serial.println("FALLO CRÍTICO: No se pudo inicializar el MPU6050");
    // Aquí podrías entrar en un bucle de error o usar valores simulados
  }

  servo.attach(9);
  pinMode(LED, OUTPUT);
  pinMode(pinRele, OUTPUT);
  BT.begin(BAUDRATE);

  Serial.println("Servo y Bluetooth inicializados");
  pos = 180;
  servo.write(180);
  delay(2000);

  Serial.println("=== Sistema listo ===");
}

byte ReadOneByte() {
  int ByteRead;
  while (!BT.available());
  ByteRead = BT.read();
#if DEBUGOUTPUT
  Serial.print((char)ByteRead);
#endif
  return ByteRead;
}

void sendEEGData(float deltaScaled, float thetaScaled, float alphaScaled, float betaScaled) {
  // Calcular focus ratio
  float focusRatio = (eeg.highBeta > 0) ? (float)eeg.highBeta / max(1L, eeg.theta + eeg.delta) * 10 : 0;
  focusRatio = constrain(focusRatio, 0, 100);

  // Calcular calidad de señal (invertir poorQuality para obtener calidad)
  int signalQuality = 100 - eeg.poorQuality;
  signalQuality = constrain(signalQuality, 0, 100);

  // Formato del protocolo:
  String data = "&" + String(eeg.attention) +
                "?" + String(eeg.meditation) +
                "#" + String(focusRatio, 2) +
                "$" + String(deltaScaled, 2) +
                "%" + String(thetaScaled, 2) +
                "!" + String(alphaScaled, 2) +
                "@" + String(betaScaled, 2) +
                "*" + String(signalQuality) +
                ";\n";

  Serial.print(data);
}

void loop() {
  readBrain(eeg);
  /*
    unsigned long tiempoActual = millis();

    // Actualizar sensor MPU6050
    Mov.actualizar();

    // Generar valor random para testing
    int valorRan = random(38, 80);

    // Obtener ángulos filtrados
    float angle_x, angle_y;
    Mov.getAngles(angle_x, angle_y);

    Serial.println("=== ESTADO ===");
    Serial.println("Random: " + String(valorRan) +
                 " - MPU_X: " + String(angle_x, 2) +
                 "° - MPU_Y: " + String(angle_y, 2) + "°");
*/
    //eeg.attention
/*
    if (valorRan eeg.attention > 40 && angle_x < -7 ) {
    if (!band) {
      band = true;
      tiempoDeEspera = millis();
    } else {
      if ((millis() - tiempoDeEspera) > 1000) {
        Serial.println(">>> COMANDO: FLEXION <<<");
        estadoActual = FLEXION;
      }
    }
  } else if (valorRan egg.attention > 40 && angle_x > 7) {
  if (!band) {
      band = true;
      tiempoDeEspera = millis();
    } else {
      if ((millis() - tiempoDeEspera) > 1000) {
        Serial.println(">>> COMANDO: EXTENSION <<<");
        estadoActual = EXTENSION;
      }
    }
  }  else {
    tiempoDeEspera = 0;
    band = false;
    //Serial.println(">>> VALOR RANDOM BAJO - REPOSO <<<");
    //estadoActual = REPOSO;
  }*/

  // Control del servo con temporización
  
  /*if (tiempoActual - previousMillis >= interval) {
  previousMillis = tiempoActual;

  if (estadoActual != estadoAnterior) {
      switch (estadoActual) {
        case FLEXION:
          Serial.println("==== EJECUTANDO FLEXION ====");
          for (pos; pos > 90; pos -= 5) { // Movimiento más suave
            Mov.actualizar();
            if (Mov.filtered_x > 1)
              break;
            servo.write(pos);
            delay(50);
          }
          break;

        case EXTENSION:
          Serial.println("==== EJECUTANDO EXTENSION ====");
          for (pos; pos < 180; pos += 5) { // Movimiento más suave
            Mov.actualizar();
            if (Mov.filtered_x < -1)
              break;
            servo.write(pos);
            delay(50);
          }
          break;

        case REPOSO:
          Serial.println("==== EJECUTANDO REPOSO ====");
          servo.write(90);
          break;
      }
      estadoAnterior = estadoActual;
    }
  }*/

  delay(100); // Pequeña pausa para estabilidad
}

// Función de lectura del cerebro (sin cambios)
void readBrain(EEGData &eeg) {
  if (ReadOneByte() == 170) {
    if (ReadOneByte() == 170) {
      payloadLength = ReadOneByte();
      if (payloadLength > 169) return;

      generatedChecksum = 0;
      for (int i = 0; i < payloadLength; i++) {
        payloadData[i] = ReadOneByte();
        generatedChecksum += payloadData[i];
      }

      checksum = ReadOneByte();
      generatedChecksum = 255 - generatedChecksum;

      if (checksum == generatedChecksum) {
        eeg.poorQuality = 200;
        eeg.attention = 0;
        eeg.meditation = 0;

        for (int i = 0; i < payloadLength; i++) {
          switch (payloadData[i]) {
            case 2:
              i++;
              eeg.poorQuality = payloadData[i];
              bigPacket = true;
              break;

            case 4:
              i++;
              eeg.attention = payloadData[i];
              break;

            case 5:
              i++;
              eeg.meditation = payloadData[i];
              break;

            case 0x80:
              i = i + 3;
              break;

            case 0x83:
              i++;
              eeg.delta = ((long)payloadData[i] << 16) | ((long)payloadData[i + 1] << 8) | payloadData[i + 2]; i += 3;
              eeg.theta = ((long)payloadData[i] << 16) | ((long)payloadData[i + 1] << 8) | payloadData[i + 2]; i += 3;
              eeg.lowAlpha = ((long)payloadData[i] << 16) | ((long)payloadData[i + 1] << 8) | payloadData[i + 2]; i += 3;
              eeg.highAlpha = ((long)payloadData[i] << 16) | ((long)payloadData[i + 1] << 8) | payloadData[i + 2]; i += 3;
              eeg.lowBeta = ((long)payloadData[i] << 16) | ((long)payloadData[i + 1] << 8) | payloadData[i + 2]; i += 3;
              eeg.highBeta = ((long)payloadData[i] << 16) | ((long)payloadData[i + 1] << 8) | payloadData[i + 2]; i += 3;
              eeg.lowGamma = ((long)payloadData[i] << 16) | ((long)payloadData[i + 1] << 8) | payloadData[i + 2]; i += 3;
              eeg.highGamma = ((long)payloadData[i] << 16) | ((long)payloadData[i + 1] << 8) | payloadData[i + 2]; i += 3;
              break;

            default:
              break;
          }
        }

#if !DEBUGOUTPUT
        if (bigPacket) {
          if (eeg.poorQuality == 0) digitalWrite(LED, HIGH);
          else digitalWrite(LED, LOW);

          eeg.deltaScaled = constrain(map(eeg.delta, 0, 2000000, 0, 100), 0, 100);
          eeg.thetaScaled = constrain(map(eeg.theta, 0, 2000000, 0, 100), 0, 100);
          eeg.alphaScaled = constrain(map(eeg.lowAlpha + eeg.highAlpha, 0, 4000000, 0, 100), 0, 100);
          eeg.betaScaled = constrain(map(eeg.lowBeta + eeg.highBeta, 0, 4000000, 0, 100), 0, 100);

          Serial.print("Attention:");
          Serial.println(eeg.attention);
          Serial.print("Meditation:");
          Serial.println(eeg.meditation);
          Serial.print("Delta:");
          Serial.print(eeg.deltaScaled, 2);
          Serial.print(",Theta:");
          Serial.print(eeg.thetaScaled, 2);
          Serial.print(",Alpha:");
          Serial.print(eeg.alphaScaled, 2);
          Serial.print(",Beta:");
          Serial.print(eeg.betaScaled, 2);
          Serial.print(",Quality:");
          Serial.print(100 - eeg.poorQuality);

          Serial.print(" [Raw: D:");
          Serial.print(eeg.delta);
          Serial.print(" T:");
          Serial.print(eeg.theta);
          Serial.print(" A:");
          Serial.print(eeg.lowAlpha + eeg.highAlpha);
          Serial.print(" B:");
          Serial.print(eeg.lowBeta + eeg.highBeta);
          Serial.println("]");

          unsigned long tiempoActual = millis();

          // Actualizar sensor MPU6050
          Mov.actualizar();

          float angle_x, angle_y;
          Mov.getAngles(angle_x, angle_y);

          Serial.println("=== ESTADO ===");
          /*Serial.println("Random: " + String(valorRan) +
                         " - MPU_X: " + String(angle_x, 2) +
                         "° - MPU_Y: " + String(angle_y, 2) + "°");*/

          //eeg.attention

          if (/*valorRan*/eeg.attention > 40 && angle_x < -7 ) {
            if (!band) {
              band = true;
              tiempoDeEspera = millis();
            } else {
              if ((millis() - tiempoDeEspera) > 1000) {
                Serial.println(">>> COMANDO: FLEXION <<<");
                estadoActual = FLEXION;
              }
            }
          } else if (/*valorRan*/ eeg.attention > 40 && angle_x > 7) {
            if (!band) {
              band = true;
              tiempoDeEspera = millis();
            } else {
              if ((millis() - tiempoDeEspera) > 1000) {
                Serial.println(">>> COMANDO: EXTENSION <<<");
                estadoActual = EXTENSION;
              }
            }
          }  else {
            tiempoDeEspera = 0;
            band = false;
            //Serial.println(">>> VALOR RANDOM BAJO - REPOSO <<<");
            //estadoActual = REPOSO;
          }

          // Control del servo con temporización
          if (tiempoActual - previousMillis >= interval) {
            previousMillis = tiempoActual;

            if (estadoActual != estadoAnterior) {
              switch (estadoActual) {
                case FLEXION:
                  Serial.println("==== EJECUTANDO FLEXION ====");
                  for (pos; pos > 90; pos -= 5) { // Movimiento más suave
                    Mov.actualizar();
                    if (Mov.filtered_x > 1)
                      break;
                    servo.write(pos);
                    delay(50);
                  }
                  break;

                case EXTENSION:
                  Serial.println("==== EJECUTANDO EXTENSION ====");
                  for (pos; pos < 180; pos += 5) { // Movimiento más suave
                    Mov.actualizar();
                    if (Mov.filtered_x < -1)
                      break;
                    servo.write(pos);
                    delay(50);
                  }
                  break;

                case REPOSO:
                  Serial.println("==== EJECUTANDO REPOSO ====");
                  servo.write(90);
                  break;
              }
              estadoAnterior = estadoActual;
            }
          }
        }
#endif
        bigPacket = false;
      }
    }
  }
}
