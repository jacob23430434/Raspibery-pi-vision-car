#include <Wire.h>  //upload library code 
void setup() {  
Serial.begin(9600); // put your setup code here, to run once:  
Wire.begin(); //initialize I2C communication 
}  
void loop() {  
while(Serial.available()) // put your main code here, to run repeatedly:  
{  
char c = Serial.read();  
if(c == 'H')  
{  
Wire.beginTransmission(5); // could be any number. defines address,     
//but we don't know address of the slave. taken care by library  
Wire.write('H'); // write letter H, when we type H, switches LED ON.  
Wire.endTransmission(); //indicate that this cycle is finished 
Serial.println("successfully send");
}  
else if(c == 'L')  
{  
Wire.beginTransmission(5); //start new cycle 
Wire.write('L'); //send data 
Wire.endTransmission(); //indicate that this cycle is finished 
Serial.println("finish");
}  
}  
}  