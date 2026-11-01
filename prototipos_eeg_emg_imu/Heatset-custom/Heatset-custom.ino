#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>
#include <SoftwareSerial.h>

Adafruit_PWMServoDriver servos = Adafruit_PWMServoDriver(0x40);
SoftwareSerial BT(11, 10);

unsigned int pos0 = 172; // ancho de pulso en cuentas para pocicion 0°
unsigned int pos180 = 565; // ancho de pulso en cuentas para la pocicion 180°

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

long    lastReceivedPacket = 0;
boolean bigPacket = false;

void setup() {
  pinMode(LED, OUTPUT);
  pinMode(pinRele, OUTPUT);
  BT.begin(BAUDRATE);
  Serial.begin(BAUDRATE);
  servos.begin();
  servos.setPWMFreq(60); //Frecuecia PWM de 60Hz o T=16,66ms
  Serial.println("---Inicializando---");
  /*delay(1000);
  for (int duty = pos0; duty < pos180; duty = duty + 10) {
    for (int n = 0; n < 16; n++)
    {
      servos.setPWM(n, 0, duty);
    }
  }
  delay(1000);
  for (int duty = pos180; duty > pos0; duty = duty - 10) {
    for (int n = 0; n < 16; n++)
    {
      servos.setPWM(n, 0, duty);
    }
  }*/
}

////////////////////////////////
// Read data from Serial UART //
////////////////////////////////
byte ReadOneByte() {
  int ByteRead;
  while (!BT.available());
  ByteRead = BT.read();

#if DEBUGOUTPUT
  Serial.print((char)ByteRead);   // echo the same byte out the USB serial (for debug purposes)
#endif

  return ByteRead;
}

/////////////
//MAIN LOOP//
/////////////
void loop() {
  // Look for sync bytes
  if (ReadOneByte() == 170) {
    if (ReadOneByte() == 170) {
      payloadLength = ReadOneByte();
      if (payloadLength > 169)                     //Payload length can not be greater than 169
        return;

      generatedChecksum = 0;
      for (int i = 0; i < payloadLength; i++) {
        payloadData[i] = ReadOneByte();            //Read payload into memory
        generatedChecksum += payloadData[i];
      }

      checksum = ReadOneByte();                      //Read checksum byte from stream
      generatedChecksum = 255 - generatedChecksum;   //Take one's compliment of generated checksum

      if (checksum == generatedChecksum) {

        poorQuality = 200;
        attention = 0;
        meditation = 0;

        for (int i = 0; i < payloadLength; i++) {   // Parse the payload
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
            case 0x83:
              i = i + 25;
              break;
            default:
              break;
          } // switch
        } // for loop

#if !DEBUGOUTPUT
        // *** Add your code here ***
        if (bigPacket) {
          if (poorQuality == 0)  digitalWrite(LED, HIGH);
          else  digitalWrite(LED, LOW);
          Serial.print("Attention: ");
          Serial.print(attention);
          Serial.print("\n");


#endif
          bigPacket = false;
        }
        else {
          // Checksum Error
        }  // end if else for checksum
      } // end if read 0xAA byte
    } // end if read 0xAA byte
  }
}
