#include <Wire.h>           // Include the Wire library for I2C communication
#include <SoftwareSerial.h>

unsigned char dataRaw[16];
unsigned int sensorData[8];
SoftwareSerial BT(10, 11); // TX, RX
String bt_cmd = "";

void setup()

{
  BT.begin(9600);
  Wire.begin();        // Join the I2C bus as a master (address optional for master)
  Serial.begin(57600);  // Start serial for output to PC
}

void loop()
{
  readSensorData();

  for(int i = 0;i <= 7;i++){
    Serial.print(sensorData[i]);
    Serial.print(",");
  }
  Serial.println("");
  /*
  BT.println("---------------");
  BT.println(sensorData[0]);
  BT.println(sensorData[1]);
  BT.println(sensorData[2]);
  BT.println(sensorData[3]);
  BT.println(sensorData[4]);
  BT.println(sensorData[5]);
  BT.println(sensorData[6]);
  BT.println(sensorData[7]);
  
  */

  delay(5);
}

void readSensorData(void)
{
  unsigned char n;            // Variable for counter value
  unsigned char dataRaw[16];  // Array for raw data from module
  
  // Request data from the module and store into an array
  n = 0;  // Reset loop variable
  Wire.requestFrom(9, 16);  // Request 16 bytes from slave device #9 (IR Sensor)
  while(Wire.available())  // Loop until all the data has been read
  {
    if (n < 16)
    {
      dataRaw[n] = Wire.read();  // Read a byte and store in raw data array
      n++;
    }
    else
    {
      Wire.read();  // Discard any bytes over the 16 we need
      //n = 0;
    }
  }

  // Loop through and covert two 8 bit values to one 16 bit value
  // Raw data formatted as "MSBs 10 9 8 7 6 5 4 3", "x x x x x x 2 1 LSBs"
  for(n=0;n<8;n++)
  {
    sensorData[n] = dataRaw[n*2]<< 2;   // Shift the 8 MSBs up two places and store in array
    sensorData[n] += dataRaw[(n*2)+1];  // Add the remaining bottom 2 LSBs
  }
}
