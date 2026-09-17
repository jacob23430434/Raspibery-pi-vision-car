#include <SoftwareSerial.h>// import the serial library
SoftwareSerial mySerial(10, 11); // TX, RX
int BluetoothData; // the data given from Computer
void setup() {
Serial.begin(115200);
Serial.println("Type AT commands!"); // put your setup code here, to run once:
mySerial.begin(9600);
Serial.println("Bluetooth On please press 1 or 0 blink LED ..");
}
void loop() {
// put your main code here, to run repeatedly:
if (mySerial.available()) {
BluetoothData=mySerial.read();
Serial.println(BluetoothData);
if(BluetoothData=='1') { // if number 1 pressed ….
Serial.println("LED  On D13 ON !");
}
if (BluetoothData=='0') { // if number 0 pressed ….
Serial.println("OFF");
}
}
delay(100);// prepare for next data …
}