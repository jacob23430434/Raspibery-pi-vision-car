#include <SPI.h>
#include "RF24.h"

RF24 rf24(9,10); // CE, CSN

const byte addr[] = "1Node";
const char msg[] = "Happy Hacking!";

void setup() {
  rf24.begin();
  Serial.begin(9600);
  rf24.setChannel(93);       
  rf24.openWritingPipe(addr); 
  rf24.setPALevel(RF24_PA_MAX);   
  rf24.setDataRate(RF24_2MBPS); 
  rf24.stopListening();       
}

void loop() {
  rf24.write(&msg, sizeof(msg));  
  delay(1000);
  Serial.println("oK!");
}