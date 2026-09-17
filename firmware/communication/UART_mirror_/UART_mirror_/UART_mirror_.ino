// Example 2 - Receive with an end-marker


const byte numChars = 32;
char receivedChars[numChars];   // an array to store the received data
char N_rec=0;
boolean newData = false;

void setup() {
  
    Serial.begin(57600);
    //Serial.println("<Arduino is ready>");
}

void loop() {
    recvWithEndMarker();
    showNewData();
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
    if (newData == true) {
        if (receivedChars[0]=='n')
        {newData = false;}else
        {Serial.println(receivedChars);
        newData = false;}
    }
}