#include <Wire.h> //wire library  
void setup() {  
Serial.begin(9600); // put your setup code here, to run once: 
Wire.begin(5); //establishing device to be No5  
Wire.onReceive(receiveEvent); //function, expects to receive data as a slave  
pinMode(13,OUTPUT); //LED pin  
digitalWrite(13,LOW); // starting low  
}  
void loop() { //empty loop function as everything is in receive function 
}  
void receiveEvent(int howMany) //function content. Executed when receives data 
{  
while(Wire.available()) //if data is still in the buAer 
{ 
char c = Wire.read(); //read data from buAer 
if(c == 'H')  
{  
digitalWrite(13,HIGH); 
Serial.println("success");
}  
else if(c == 'L')  
{  
digitalWrite(13,LOW); 
Serial.println("success");
}  
}  
}