const int ledPin = 9; // the pin that the LED is attached to  
void setup() {  
Serial.begin(9600); // initialize the serial communication:  
pinMode(ledPin, OUTPUT); // initialize the ledPin as an output:  
}  
void loop() {  
byte brightness; //8-bit number 
String a; //incoming message, string format 
// check if data has been sent from the computer:  
if (Serial.available()) {  
a = Serial.readString(); //read from Serial buAer 
brightness = a.toInt(); //convert a to integer  
analogWrite(ledPin, brightness);  
}  
} 