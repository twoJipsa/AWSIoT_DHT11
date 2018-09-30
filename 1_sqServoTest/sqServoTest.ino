#include <Servo.h>

Servo sgServo;

void setup() {
  // put your setup code here, to run once:
  sgServo.attach(D5);
}

void loop() {
  // put your main code here, to run repeatedly:
  sgServo.write(0);
  delay(1000); //delay 100ms
  sgServo.write(180);
  delay(1000); //delay 100ms
}
