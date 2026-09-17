// the setup routine runs once when you press reset: 
int LED_pin1 = 10;
int LED_pin2 = 11;
void setup() { // initialize serial communication at 9600 bits per second: 
Serial.begin(9600); 
} 
void loop() { 
int sensorValue = analogRead(A0);// read the input on analog pin 0: 
float voltage = sensorValue * (5.0 / 1023.0); // Convert the analog reading (which goes from 0 - 1023) to a voltage (0 - 5V):
if (voltage >= 4.32){//50% - fully charged, all light on
  digitalWrite(LED_pin1, HIGH);
  digitalWrite(LED_pin2, HIGH);
}
if ((voltage >= 3.6) && (voltage <= 4.32)) {// 10% - 50%, one LED on
  digitalWrite(LED_pin1, LOW);
  digitalWrite(LED_pin2, HIGH);
}
if (voltage <= 3.6) {// 0% - 10%, one LED blink
  digitalWrite(LED_pin1, LOW);
  digitalWrite(LED_pin2, LOW);
  delay(500);
  digitalWrite(LED_pin2, HIGH);
}
Serial.println(voltage); 
delay(1000);
} 
