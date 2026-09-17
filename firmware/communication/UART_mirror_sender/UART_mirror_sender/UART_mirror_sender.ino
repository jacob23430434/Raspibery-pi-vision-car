// Example 2 - Receive with an end-marker
#define LED_blink 13

const byte numChars = 32;
char receivedChars[numChars];   // an array to store the received data
char receivedChars1[numChars];   // an array to store the received data
int count=0;
boolean newData = false;

void setup() {
   pinMode(LED_blink, OUTPUT);
    Serial.begin(57600);
    delay(1000);
    Serial.println("<Arduino is ready>");
}

void loop() {
    delay(1000);
    count++;
    Serial.println(count);
    delay(50);
    recvWithEndMarker();
    showNewData();
    snprintf(receivedChars1, sizeof(receivedChars1), "n%s", receivedChars);
    Serial.println(receivedChars1);
}

void recvWithEndMarker() {
    static byte ndx = 0;
    char endMarker = '\n';
    char rc;
    
    while (Serial.available() > 0 && newData == false) {
        rc = Serial.read();

        if (rc != endMarker) {
            receivedChars[ndx] = rc;
            ndx++;
            if (ndx >= numChars) {
                ndx = numChars - 1;
            }
        }
        else {
            receivedChars[ndx] = '\0'; // terminate the string
            ndx = 0;
            newData = true;
        }
    }
}

void showNewData() {
  static unsigned char flag_LED = 0;
  int pos=0;
  if (newData == true) {
    //Serial.print("This just in ... ");
    //Serial.println(receivedChars);
    pos = atoi(&receivedChars[0]);
    if (pos == count) 
    {
      if (flag_LED == 0) 
      {
        flag_LED = 1;
        digitalWrite(LED_blink, HIGH);  // turn the LED on (HIGH is the voltage level)
      } else 
      {
        flag_LED = 0;
        digitalWrite(LED_blink, LOW);  // turn the LED off by making the voltage LOW
      }
    }
    newData = false;
  }
}