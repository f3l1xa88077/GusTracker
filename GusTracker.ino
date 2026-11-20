#include "src/rtc/DFRobot_DS1307.h"

// RTC
DFRobot_DS1307 DS1307;

void setup() {

  // Serial
  Serial.begin(9600);

  // RTC
  while( !(DS1307.begin()) ){
    Serial.println("Communication with device failed, please check connection");
    delay(3000);
  }
  Serial.println("RTC begin ok!");
  // Calibrate time and 1 Hz tick
  uint16_t setTimeBuff[7] = {5, 1, 7, 6, 9, 9, 2021};
  DS1307.setTime(setTimeBuff);
  DS1307.start();
  DS1307.setSqwPinMode(DS1307.eSquareWave_1Hz);
}

void loop() {
  
  // Read time
  uint16_t getTimeBuff[7] = {0};
  DS1307.getTime(getTimeBuff);
  char outputarr[128];
  sprintf(outputarr, "time: %d/%d/%d-%d %d:%d:%d\r\n",
            getTimeBuff[6],
            getTimeBuff[5],
            getTimeBuff[4],
            getTimeBuff[3],
            getTimeBuff[2],
            getTimeBuff[1],
            getTimeBuff[0]
            );
  Serial.print(outputarr);

  // Short delay
  delay(500);
}
