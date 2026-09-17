#include <MsTimer2.h>
const int pwmPin = 9;       // PWM output pin
const int tableSize = 50;   // Number of points in the sine table
// Sine table with 50 points, range 0–255
const uint8_t sineTable[tableSize] = {
  127, 142, 156, 169, 181, 191, 200, 207, 213, 218,
  221, 224, 225, 226, 225, 224, 221, 218, 213, 207,
  200, 191, 181, 169, 156, 142, 127, 112, 98, 85,
  73, 63, 54, 47, 41, 36, 33, 30, 29, 28,
  29, 30, 33, 36, 41, 47, 54, 63, 73, 85
};
volatile int index = 0; // Current index in the sine table
void setup() {
  pinMode(pwmPin, OUTPUT);
  // Set the Timer2 interrupt
  MsTimer2::set(10, updatePWM);
  MsTimer2::start();
}
void loop() {
  // The main loop can perform other tasks and will not block
  // delay() is no longer needed
}
// Timer2 interrupt function, updates the duty cycle every 20 ms
void updatePWM() {
  analogWrite(pwmPin, sineTable[index]);
  index++;
  if (index >= tableSize) index = 0;
}
