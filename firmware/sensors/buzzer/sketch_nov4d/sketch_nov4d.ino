int buzzerPin = 9;
int buttonPin = 10;
int buttonState;
int delayTime = 500;

void setup() {
  pinMode(buzzerPin, OUTPUT);
  pinMode(buttonPin, INPUT_PULLUP);
}

void loop() {
  buttonState = digitalRead(buttonPin);
  if(buttonState == LOW){
    tone(buzzerPin, 440); // A4
    delay(delayTime);
    tone(buzzerPin, 494); // B4
    delay(delayTime);
    tone(buzzerPin, 523); // C4
    delay(delayTime);
    tone(buzzerPin, 587); // D4
    delay(delayTime);
    tone(buzzerPin, 659); // E4
    delay(delayTime);
    noTone(buzzerPin);
    delay(delayTime);
    tone(buzzerPin, 659); // E4
    delay(delayTime);
    tone(buzzerPin, 587); // D4
    delay(delayTime);
    tone(buzzerPin, 523); // C4
    delay(delayTime);
    tone(buzzerPin, 494); // B4
    delay(delayTime);
    tone(buzzerPin, 440); // A4
    delay(delayTime);
  } else{
    noTone(buzzerPin);
  }
}