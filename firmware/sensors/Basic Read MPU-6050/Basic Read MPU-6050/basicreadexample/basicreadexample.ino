#include <Wire.h>
#define uchar unsigned char
uchar t;
//void send_data(short a1,short b1,short c1,short d1,short e1,short f1);
uchar data[7];
void setup()
{
  Wire.begin();        // join i2c bus (address optional for master)
  Serial.begin(9600);  // start serial for output
  t = 0;
}
void loop()
{
  t = 0;
  Wire.requestFrom(0x1D, 7);    // request 7 bytes from slave device #0x1D
  while (Wire.available())   // slave may send less than requested
  {
    if (t < 7)
    {
      data[t] = Wire.read(); // receive a byte as character
      t++;
    }
    else
      Wire.read(); // dummy read
  }

  //Print the results (first byte is Status register and is ignored)
  Serial.print("X: ");
  Serial.print( (float)((data[1]<<8 | data[2])>>4)/1024 ); //Convert to bytes to a 12bit signed number, divide by 1024 to convert to g value
  Serial.print(" Y: ");
  Serial.print( (float)((data[3]<<8 | data[4])>>4)/1024 );
  Serial.print(" Z: ");
  Serial.println( (float)((data[5]<<8 | data[6])>>4)/1024 );
  delay(500);
}

