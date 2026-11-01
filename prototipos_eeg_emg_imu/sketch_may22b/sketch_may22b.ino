#include <SoftwareSerial.h>
SoftwareSerial BT(11, 10);// RX, TX
#define LED 13
#define BAUDRATE 57600
#define DEBUGOUTPUT 0
#define pinRele 2

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

void setup() {
  Serial.begin(BAUDRATE);
  pinMode(LED, OUTPUT);
  pinMode(pinRele, OUTPUT);
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
                ";\n";

  Serial.print(data);
}

void loop() {
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
        attention = 0;
        meditation = 0;

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
        }

#if !DEBUGOUTPUT
        if (bigPacket) {
          if (poorQuality == 0) digitalWrite(LED, HIGH);
          else digitalWrite(LED, LOW);

          // NORMALIZACIÓN CORREGIDA DE ONDAS CEREBRALES
          // Los valores típicos de MindWave van de 0 a ~2,000,000
          // Usamos logaritmo para mejor distribución
          float deltaScaled = constrain(map(delta, 0, 2000000, 0, 100), 0, 100);
          float thetaScaled = constrain(map(theta, 0, 2000000, 0, 100), 0, 100);
          float alphaScaled = constrain(map(lowAlpha + highAlpha, 0, 4000000, 0, 100), 0, 100);
          float betaScaled = constrain(map(lowBeta + highBeta, 0, 4000000, 0, 100), 0, 100);

          // Alternativa con escalado logarítmico para mejor resolución
          // float deltaScaled = constrain(log10(delta + 1) * 10, 0, 100);
          // float thetaScaled = constrain(log10(theta + 1) * 10, 0, 100);
          // float alphaScaled = constrain(log10(lowAlpha + highAlpha + 1) * 10, 0, 100);
          // float betaScaled = constrain(log10(lowBeta + highBeta + 1) * 10, 0, 100);

          Serial.print("Attention:");
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
          Serial.print(100-poorQuality);
          
          // Mostrar valores brutos para debugging
          Serial.print(" [Raw: D:");
          Serial.print(delta);
          Serial.print(" T:");
          Serial.print(theta);
          Serial.print(" A:");
          Serial.print(lowAlpha + highAlpha);
          Serial.print(" B:");
          Serial.print(lowBeta + highBeta);
          Serial.println("]");

          static unsigned long lastSend = 0;
          if (millis() - lastSend > 100) { // Enviar cada 100ms
            sendEEGData(deltaScaled, thetaScaled, alphaScaled, betaScaled);
            lastSend = millis();
          }
        }
#endif
        bigPacket = false;
      }
    }
  }
}
