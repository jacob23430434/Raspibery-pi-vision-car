// digital pin 2 has a pushbutton attached to it. Give it a name:
int pushButton1 = 2;
int pushButton2 = 3;
const int buttonPin = 9;  // the number of the pushbutton pin
int buttonState = 0;  // variable for reading the pushbutton status
int count = 1;


// the setup routine runs once when you press reset:
void setup() {
  // initialize serial communication at 9600 bits per second:
  Serial.begin(9600);
  // make the pushbutton's pin an input:
  pinMode(pushButton1, INPUT);
  pinMode(pushButton2, INPUT);
  pinMode(buttonPin, INPUT);
}

// the loop routine runs over and over again forever:
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
  delay(25);
}
