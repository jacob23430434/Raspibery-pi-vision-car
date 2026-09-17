#include <SPI.h>
#include "RF24.h"

RF24 radio(9,10); // CE, CSN

const byte address_A[] = "1Node"; // A → B
const byte address_B[] = "2Node"; // B → A

void setup() {
  Serial.begin(9600);

  radio.begin();
  radio.setChannel(93);
  radio.setPALevel(RF24_PA_MAX);
  radio.setDataRate(RF24_2MBPS);

  // B listens on 1Node
  radio.openReadingPipe(1, address_A);
  radio.openWritingPipe(address_B);

  radio.startListening();
  Serial.println("Node B ready!");
}

void loop() {
  if (radio.available()) {
    char incoming[32] = "";
    radio.read(&incoming, sizeof(incoming));
    Serial.print("[B] Got < ");
    Serial.print(incoming);
    Serial.println(" >");

    delay(20);

    // Send reply
    radio.stopListening();
    const char reply[] = "Got it — B";
    radio.write(&reply, sizeof(reply));
    Serial.println("[B] Reply sent");
    radio.startListening();
  }
}
