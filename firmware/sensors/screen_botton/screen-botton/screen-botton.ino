// include the library code:
#include <LiquidCrystal.h>
#include <MsTimer2.h>
volatile unsigned int counter = 0;
// digital pin 2 has a pushbutton attached to it. Give it a name:
int pushButton1 = 2;
int pushButton2 = 3;
const int buttonPin = 9;  // the number of the pushbutton pin
int buttonState = 0;  // variable for reading the pushbutton status
int count = 1;

// initialize the library by associating any needed LCD interface pin
// with the arduino pin number it is connected to
const int rs = 12, en = 11, d4 = 4, d5 = 5, d6 = 6, d7 = 7;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

void setup() {
  // initialize serial communication at 9600 bits per second:
  Serial.begin(9600);
  // make the pushbutton's pin an input:
  pinMode(pushButton1, INPUT);
  pinMode(pushButton2, INPUT);
  pinMode(buttonPin, INPUT);
  // set up the LCD's number of columns and rows:
  lcd.begin(16, 2);
  //lcd.leftToRight();
}

void loop() {


  // read the input pin:
  int buttonState1 = digitalRead(pushButton1);
  int buttonState2 = digitalRead(pushButton2);
  buttonState = digitalRead(buttonPin);
  // check if the pushbutton is pressed. If it is, the buttonState is HIGH:
  if (buttonState == HIGH) {
    // turn LED on:
    Serial.println("off");
  } else {
    // turn LED off:
    Serial.println("on");

  }
   //print out the state of the button:
  /*  Serial.println("buttonState1:   ");
  Serial.println(buttonState1);
  Serial.println("buttonState2:   ");
  Serial.println(buttonState2);*/
  if(buttonState1 - buttonState2 == 1)
  {
    //Serial.println("left"); 
    count--;
  }
   if(buttonState2 - buttonState1 == 1)
  {
    //Serial.println("right"); 
    count++;
  }
  Serial.println(count);
  lcd.display();
  lcd.print(count);
  // set the display to automatically scroll:
  delay(200);
  lcd.clear();

}



