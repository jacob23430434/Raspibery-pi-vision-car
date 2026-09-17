#include <SPI.h>
#include "RF24.h"

RF24 radio(9,10); // CE, CSN

const byte address_A[] = "1Node";  // A → B
const byte address_B[] = "2Node";  // B → A

void setup() {
  Serial.begin(9600);

  radio.begin();
  radio.setChannel(93);
  radio.setPALevel(RF24_PA_MAX);
  radio.setDataRate(RF24_2MBPS);

  radio.openWritingPipe(address_A);
  radio.openReadingPipe(1, address_B);

  Serial.println("Node A ready!");
}

void loop() {
  // 1) Send a message
  radio.stopListening();
  const char text[] = "Hello from A!";
  bool ok = radio.write(&text, sizeof(text));

  if (ok) Serial.println("[A] Sent OK");
  else   Serial.println("[A] Send FAIL");

  delay(30);

  // 2) Listen for reply
  radio.startListening();
  unsigned long start = millis();
  while (millis() - start < 150) {
    if (radio.available()) {
      char msg[32] = "";
      radio.read(&msg, sizeof(msg));
      Serial.print("[A] Reply < ");
      Serial.print(msg);
      Serial.println(" >");
      break;
    }
  }

  delay(500);
}
