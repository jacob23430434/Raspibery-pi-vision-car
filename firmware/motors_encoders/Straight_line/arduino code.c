// Declare Variables
int G;
int vel;
int G1 = 0;
int G2 = 0;
int G3 = 0;
int G4 = 0;
int G5 = 0;

#include <Wire.h>


L3G gyro;
ZumoMotors motors;

void setup()
{
  // Set up reading the gyro
  Serial.begin(9600);
  Wire.begin();

  if (!gyro.init())
  {
    Serial.println("Failed to autodetect gyro type!");
    while (1);
  }

  gyro.enableDefault();

  delay(5000); // Wait 5 seconds before commanding
}

void loop()
{
  gyro.read();     // Read the Gyro

  // Build 5 frame averager filter
  G5 = G4;
  G4 = G3;
  G3 = G2;
  G2 = G1;
  G1 = (int)gyro.g.z;
  // Average the last 5 readings
  G = (G1 + G2 + G3 + G4 + G5) / 5;

  vel = 100 - (G / 35);
  motors.setLeftSpeed((int)vel);

  vel = 100 + (G / 35);
  motors.setRightSpeed(vel);

  delay(50);   // Run at ~20Hz
}


