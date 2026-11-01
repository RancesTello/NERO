
#include <Servo.h>

Servo myserv;

#define SAMPLE_RATE 500
#define BAUD_RATE 115200
#define INPUT_PIN A0

bool band = false;
int i = 180;

void setup() {
  // Serial connection begin
  Serial.begin(BAUD_RATE);
  myserv.attach(2);
  //myserv.write(100);
  //delay(30000);
}

void loop() {
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
    if (signal <-200)
      if (band == true) {
        for(i=180; i>100 ; i--){
          myserv.write(i);
          //Serial.println("1: "+String(i));
          delay(100);
        }
        band=false;
      } else {
        for(i = 100; i<180 ; i++){
          myserv.write(i);
          //Serial.println("2: "+String(i));
          delay(100);
        }
        band=true;
      }
      
    Serial.println(signal);
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
