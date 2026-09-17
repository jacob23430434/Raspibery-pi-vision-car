#include <SPI.h>
#include "RF24.h"

RF24 rf24(9,10); // CE, CSN

const byte addr[] = "1Node";
const byte pipe = 1;

void setup() {
  Serial.begin(9600);
  rf24.begin();
  rf24.setChannel(93);
  rf24.setPALevel(RF24_PA_MAX);
  rf24.setDataRate(RF24_2MBPS);
  rf24.openReadingPipe(pipe, addr);
  rf24.startListening();
  Serial.println("nRF24L01 ready!");
}

void loop() {
  if (rf24.available(&pipe)) {
    char msg[32] = "";
    rf24.read(&msg, sizeof(msg));
    Serial.println(msg);
  }
}