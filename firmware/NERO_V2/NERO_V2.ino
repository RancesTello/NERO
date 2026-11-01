#include <SoftwareSerial.h>
#include "I2Cdev.h"
#include "MPU6050.h"
#include "Wire.h"

SoftwareSerial BT(11, 10);// TX, RX
#define BAUDRATE 57600
#define DEBUGOUTPUT 0

unsigned long previousMillis = 0;
const unsigned long interval = 2000; // 2 segundo

byte  generatedChecksum = 0;
byte  checksum = 0;
int   payloadLength = 0;
byte  payloadData[64] = {0};
byte  poorQuality = 0;
byte  attention = 0;
byte  meditation = 0;

// Variables para ondas cerebrales
long delta = 0;
long theta = 0;
long lowAlpha = 0;
long highAlpha = 0;
long lowBeta = 0;
long highBeta = 0;
long lowGamma = 0;
long highGamma = 0;

long lastReceivedPacket = 0;
boolean bigPacket = false;

enum EstadoBrazo {REPOSO, FLEXION, EXTENSION};
EstadoBrazo estadoActual = REPOSO;
EstadoBrazo estadoAnterior = REPOSO;

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
      //Serial.println("Inicializando MPU6050...");
      sensor.initialize();

      if (sensor.testConnection()) {
        //Serial.println("MPU6050 conectado correctamente");

        sensor.setFullScaleAccelRange(MPU6050_ACCEL_FS_2); // ±2g
        sensor.setFullScaleGyroRange(MPU6050_GYRO_FS_250);  // ±250°/s
        sensor.setDLPFMode(MPU6050_DLPF_BW_20);             // Filtro paso bajo

        //Serial.println("Calibrando sensor...");
        for (int i = 0; i < 100; i++) {
          actualizar();
          delay(10);
        }

        initialized = true;
        //Serial.println("Calibración completada");
        return true;
      } else {
        /*Serial.println("Error: No se pudo conectar al MPU6050");
          Serial.println("Verificar conexiones:");
          Serial.println("  VCC -> 5V o 3.3V");
          Serial.println("  GND -> GND");
          Serial.println("  SCL -> A5 (pin 21)");
          Serial.println("  SDA -> A4 (pin 20)");*/
        return false;
      }
    }

    void actualizar() {
      if (!initialized) return;

      int16_t ax, ay, az;
      sensor.getAcceleration(&ax, &ay, &az);

      /*Serial.print("Raw: ax=");
        Serial.print(ax);
        Serial.print(", ay=");
        Serial.print(ay);
        Serial.print(", az=");
        Serial.println(az);*/

      if (ax == 0 && ay == 0 && az == 0) {
        //Serial.println("Error: Valores del sensor son cero - revisar conexiones");
        return;
      }

      float denom_x = sqrt(float(ay) * ay + float(az) * az);
      float denom_y = sqrt(float(ax) * ax + float(az) * az);

      if (denom_x < 0.1) denom_x = 0.1;
      if (denom_y < 0.1) denom_y = 0.1;

      float accel_ang_x = atan(float(ax) / denom_x) * (180.0 / PI);
      float accel_ang_y = atan(float(ay) / denom_y) * (180.0 / PI);

      /*Serial.print("Angles: x=");
        Serial.print(accel_ang_x);
        Serial.print("°, y=");
        Serial.print(accel_ang_y);
        Serial.print("°");*/

      this->filtered_x = 0.8 * filtered_x + 0.2 * accel_ang_x;
      this->filtered_y = 0.8 * filtered_y + 0.2 * accel_ang_y;

      /*Serial.print(" -> Filtered: x=");
        Serial.print(filtered_x);
        Serial.print("°, y=");
        Serial.print(filtered_y);
        Serial.println("°");*/
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
  //Serial.println("=== Iniciando sistema ===");

  Wire.begin();           // Iniciando I2C
  //Serial.println("I2C inicializado");

  if (!Mov.begin()) {
    Serial.println("FALLO CRÍTICO: No se pudo inicializar el MPU6050");
    //Aquí podrías entrar en un bucle de error o usar valores simulados
  }
  delay(1000);
  BT.begin(BAUDRATE);
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
  Mov.actualizar();
  float focusRatio = (highBeta > 0) ? (float)highBeta / max(1L, theta + delta) * 10 : 0;
  focusRatio = constrain(focusRatio, 0, 100);

  // Calcular calidad de señal (invertir poorQuality para obtener calidad)
  int signalQuality = 100 - poorQuality;
  signalQuality = constrain(signalQuality, 0, 100);

  // Formato del protocolo:
  // &[attention]?[meditation]#[focusRatio]$[delta]%[theta]![alpha]@[beta]*[signalQuality];
  String data = "&" + String(attention) +
                "?" + String(meditation) +
                "#" + String(focusRatio, 2) +
                "$" + String(deltaScaled, 2) +
                "%" + String(thetaScaled, 2) +
                "!" + String(alphaScaled, 2) +
                "@" + String(betaScaled, 2) +
                "*" + String(signalQuality) +
                "/" + String(Mov.filtered_x) +
                ";\n";

  Serial.print(data);
}


void loop() {
  /*static bool ejecutado = false;
    if (!ejecutado) {
    servo.write(90);
    Serial.println("9000");
    ejecutado = true;
    }
    delay(100);*/

  static unsigned long past = 0;
  unsigned long present = micros();
  unsigned long interval = present - past;
  past = present;

    //if (signal != 0/*< -450*/) {
      //Serial.println("Movimiento detectado!!...\n\n\n\n\n\n\n\n\n\n...........................................");
      while (true) {
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
              poorQuality = 200;
              //attention = 0;
              //meditation = 0;

              for (int i = 0; i < payloadLength; i++) {
                switch (payloadData[i]) {
                  case 2:
                    i++;
                    poorQuality = payloadData[i];
                    bigPacket = true;
                    break;

                  case 4:
                    i++;
                    attention = payloadData[i];
                    //Serial.println("ATTENTIOON: "+String(attention));
                    break;

                  case 5:
                    i++;
                    meditation = payloadData[i];
                    break;

                  case 0x80:
                    i = i + 3;
                    break;

                  case 0x83: // ONDAS CEREBRALES
                    i++;
                    // Leer los 8 valores de 3 bytes cada uno (24 bytes total)
                    delta = ((long)payloadData[i] << 16) | ((long)payloadData[i + 1] << 8) | payloadData[i + 2]; i += 3;
                    theta = ((long)payloadData[i] << 16) | ((long)payloadData[i + 1] << 8) | payloadData[i + 2]; i += 3;
                    lowAlpha = ((long)payloadData[i] << 16) | ((long)payloadData[i + 1] << 8) | payloadData[i + 2]; i += 3;
                    highAlpha = ((long)payloadData[i] << 16) | ((long)payloadData[i + 1] << 8) | payloadData[i + 2]; i += 3;
                    lowBeta = ((long)payloadData[i] << 16) | ((long)payloadData[i + 1] << 8) | payloadData[i + 2]; i += 3;
                    highBeta = ((long)payloadData[i] << 16) | ((long)payloadData[i + 1] << 8) | payloadData[i + 2]; i += 3;
                    lowGamma = ((long)payloadData[i] << 16) | ((long)payloadData[i + 1] << 8) | payloadData[i + 2]; i += 3;
                    highGamma = ((long)payloadData[i] << 16) | ((long)payloadData[i + 1] << 8) | payloadData[i + 2]; i += 3;
                    break;

                  default:
                    break;
                }
              //}

#if !DEBUGOUTPUT
              if (bigPacket) {

                // NORMALIZACIÓN CORREGIDA DE ONDAS CEREBRALES
                // Rangos finales ajustados basados en valores reales observados
                float deltaScaled = constrain(map(delta, 0, 5000000, 0, 100), 0, 100);
                float thetaScaled = constrain(map(theta, 0, 15000000, 0, 100), 0, 100);
                float alphaScaled = constrain(map(lowAlpha + highAlpha, 0, 30000000, 0, 100), 0, 100);
                float betaScaled = constrain(map(lowBeta + highBeta, 0, 30000000, 0, 100), 0, 100);

                // Alternativa con escalado logarítmico para mejor resolución
                // float deltaScaled = constrain(log10(delta + 1) * 10, 0, 100);
                // float thetaScaled = constrain(log10(theta + 1) * 10, 0, 100);
                // float alphaScaled = constrain(log10(lowAlpha + highAlpha + 1) * 10, 0, 100);
                // float betaScaled = constrain(log10(lowBeta + highBeta + 1) * 10, 0, 100);


                /*Serial.print("Attention:");
                  Serial.print(attention);
                  Serial.print(",Meditation:");
                  Serial.print(meditation);
                  Serial.print(",Delta:");
                  Serial.print(deltaScaled, 2);
                  Serial.print(",Theta:");
                  Serial.print(thetaScaled, 2);
                  Serial.print(",Alpha:");
                  Serial.print(alphaScaled, 2);
                  Serial.print(",Beta:");
                  Serial.print(betaScaled, 2);
                  Serial.print(",Quality:");
                  Serial.print(100 - poorQuality);

                  // Mostrar valores brutos para debugging
                  Serial.print(" [Raw: D:");
                  Serial.print(delta);
                  Serial.print(" T:");
                  Serial.print(theta);
                  Serial.print(" A:");
                  Serial.print(lowAlpha + highAlpha);
                  Serial.print(" B:");
                  Serial.print(lowBeta + highBeta);
                  Serial.println("]");*/


                unsigned long tiempoActual = millis();
                Mov.actualizar();
                if (attention > 40 && Mov.filtered_x < -7 /*&& signal < EMG_UMBRAL*/) {
                  if (!band) {
                    band = true;
                    tiempoDeEspera = millis();
                  } else {
                    if ((millis() - tiempoDeEspera) > 1000) {
                      //Serial.println(">>> COMANDO: FLEXION <<<");
                      estadoActual = FLEXION;
                    }
                  }
                } else if (attention > 40 && Mov.filtered_x > 7 /*&& signal < EMG_UMBRAL*/) {
                  if (!band) {
                    band = true;
                    tiempoDeEspera = millis();
                  } else {
                    if ((millis() - tiempoDeEspera) > 1000) {
                      //Serial.println(">>> COMANDO: EXTENSION <<<");
                      estadoActual = EXTENSION;
                    }
                  }
                }  else {
                  tiempoDeEspera = 0;
                  band = false;
                  //Serial.println(">>> VALOR RANDOM BAJO - REPOSO <<<");

                  estadoActual = REPOSO;
                }

                // Control del servo con temporización
                if (tiempoActual - previousMillis >= interval) {
                  previousMillis = tiempoActual;
                }
                static unsigned long lastSend = 0;
                if (millis() - lastSend > 100) { // Enviar cada 100ms
                  sendEEGData(deltaScaled, thetaScaled, alphaScaled, betaScaled);
                  lastSend = millis();
                }
                if (estadoActual != estadoAnterior) {
                  switch (estadoActual) {
                    case FLEXION:
                      //Serial.println("==== EJECUTANDO FLEXION ====");
                      /*if (pos < 90 || pos > 180) pos = 135;

                        for (; pos >= 90; pos -= 1) {
                        Mov.actualizar();
                        if (Mov.filtered_x > 1) break;

                        servo.attach(3);    // Conectar
                        servo.write(pos);   // Mover
                        delay(50);          // Esperar movimiento
                        servo.detach();     // Desconectar
                        delay(10);          // Pausa entre movimientos
                        }*/
                      break;
                    case EXTENSION:
                      //Serial.println("==== EJECUTANDO EXTENSION ====");
                      /*if(pos != 180) {
                        pos = 180;
                        servo.write(pos);
                        }*/
                      /*for (pos; pos <= 180; pos += 1) { // Movimiento más suave
                        Mov.actualizar();
                        if (Mov.filtered_x < -1)
                          break;
                        if (pos != lastPos) {
                          servo.write(pos);
                          lastPos = pos;
                        }
                        delay(50);
                        }*/
                      break;

                    case REPOSO:
                      //Serial.println("==== EJECUTANDO REPOSO ====");
                      break;
                  }
                  estadoAnterior = estadoActual;
                } else {
                  //servo.detach();
                }
              }
#endif
              bigPacket = false;
            }
          }
        }
      }
    }
  }
//}
