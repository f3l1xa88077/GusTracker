#include "src/rtc/DFRobot_DS1307.h"

#include "src/eink/EPaper.h"
#include "src/eink/FeederUI.h"

// RTC
DFRobot_DS1307 DS1307;
// #define SET_RTC_TIME

// E-INK
BoxState d1, d2;
Battery d3;
EPaper154G epd;
FeederUI ui(epd);

// VOLTAGE
int vsense = 2;

// ISR
#define WAKE_GPIO GPIO_NUM_3
RTC_DATA_ATTR volatile int buttonPressed = 0;
int button_pin = 3;

void setup() {

  // ------------------ SERIAL ------------------

  Serial.begin(9600);

  delay(3000);

  // ------------------ ISR ------------------

  esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
  bool buttonWake = (cause == ESP_SLEEP_WAKEUP_GPIO);
  bool firstBoot  = (cause == ESP_SLEEP_WAKEUP_UNDEFINED);   // power-on or reset


  pinMode(button_pin, INPUT);

  Serial.print(buttonPressed);
  Serial.print(",");

  // ------------------ RTC ------------------

  while( !(DS1307.begin()) ){
    Serial.println("Communication with device failed, please check connection");
    delay(3000);
  }
  Serial.println("RTC begin ok!");
  // Calibrate module. Must be flashed without calibration afterwards to prevent repeated time reset
  #ifdef SET_RTC_TIME
    uint16_t setTimeBuff[7] = {5, 1, 7, 6, 9, 9, 2021};
    DS1307.setTime(setTimeBuff);
    // Start 1 Hz
    DS1307.start();
    DS1307.setSqwPinMode(DS1307.eSquareWave_1Hz);
  #endif

  // Print time
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

  // ------------------ VSENSE BEGIN ------------------

  analogReadResolution(12);
  pinMode(vsense, INPUT);

  // Battery Voltage
  long vbat = analogReadMilliVolts(vsense) * 1.36; // Reverse voltage divider
  int percentage = map(vbat, 3500, 4000, 10, 90); // Map to a battery percentage 10-100%
  Serial.print(vbat);
  Serial.print(",");
  Serial.println(percentage);

  if (percentage > 65) d3 = BATT_FULL;
  else if (percentage > 25) d3 = BATT_HALF;
  else d3 = BATT_LOW;

  // ------------------ E-INK BEGIN ------------------

  epd.begin(1);
  ui.setTitle("GUS");
  if (buttonWake || firstBoot) {
    if (buttonWake) buttonPressed = (buttonPressed + 1) % 3;
    d1 = (buttonPressed != 0) ? BOX_TICK : BOX_EMPTY;
    d2 = (buttonPressed == 2) ? BOX_TICK : BOX_EMPTY;
    ui.update(d1, d2, d3, "", "", true);
  }

  // ------------------ SLEEP ------------------

  while (digitalRead(button_pin)) delay(10);   // don't sleep while the button is still held
  esp_deep_sleep_enable_gpio_wakeup((1ULL << WAKE_GPIO), ESP_GPIO_WAKEUP_GPIO_HIGH);
  esp_deep_sleep_start();

}

void loop() {
  Serial.println("SHOULD NEVER REACH HERE");
  delay(1000);
}