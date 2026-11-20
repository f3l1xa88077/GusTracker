#include "src/rtc/DFRobot_DS1307.h"

#include "src/eink/EPaper.h"
#include "src/eink/FeederUI.h"

// RTC
DFRobot_DS1307 DS1307;
// #define SET_RTC_TIME

// E-INK
EPaper154G epd;
FeederUI ui(epd);

// ISR
volatile bool buttonPressed = false;
int button_pin = 2;
void ARDUINO_ISR_ATTR  buttonISR();

void setup() {

  // ------------------ SERIAL BEGIN ------------------

  Serial.begin(9600);

  // ------------------ SERIAL END ------------------

  // ------------------ ISR BEGIN ------------------

  pinMode(button_pin, INPUT);
  attachInterrupt(button_pin, buttonISR, RISING);

  // ------------------ ISR END ------------------

  // ------------------ RTC BEGIN ------------------

  // while( !(DS1307.begin()) ){
  //   Serial.println("Communication with device failed, please check connection");
  //   delay(3000);
  // }
  Serial.println("RTC begin ok!");

  // Calibrate time. Must be flashed without calibration afterwards to prevent repeated time reset
  #ifdef SET_RTC_TIME
    uint16_t setTimeBuff[7] = {5, 1, 7, 6, 9, 9, 2021};
    DS1307.setTime(setTimeBuff);
  #endif

  // Start 1 Hz
  DS1307.start();
  DS1307.setSqwPinMode(DS1307.eSquareWave_1Hz);

  // ------------------ RTC END ------------------

  // ------------------ E-INK BEGIN ------------------

  epd.begin(1);
  ui.setTitle("GUS");
  ui.update(BOX_TICK, BOX_EMPTY, BATT_FULL, "7:30 AM", "", true);

  delay(5000);
  ui.update(BOX_TICK, BOX_EMPTY, BATT_HALF, "7:30 AM", "", true);

  delay(5000);
  ui.update(BOX_TICK, BOX_EMPTY, BATT_LOW, "7:30 AM", "", true);

  delay(5000);
  ui.update(BOX_TICK, BOX_TICK, BATT_LOW, "7:30 AM", "", true);

  // ------------------ E-INK END ------------------

}

void loop() {
  
  // Read time
  uint16_t getTimeBuff[7] = {0};
  DS1307.getTime(getTimeBuff);
  // Print time
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
  //Serial.print(outputarr);

  if (buttonPressed) {
    buttonPressed = false;
    Serial.println("BUTTON");
  }

  // Short delay
  delay(50);
}

void ARDUINO_ISR_ATTR buttonISR() {
  buttonPressed = true;
}
