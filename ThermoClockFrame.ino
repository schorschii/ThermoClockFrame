#include <Wire.h>
#include <ds3231.h>

#include <OneWire.h>
#include <DallasTemperature.h>

// set to false to use TM1637 displays on defined pins
// set to true to use HT16K33 displays on I2C
#define HT16K33_insteadof_TM1637   false

#if HT16K33_insteadof_TM1637 == true
  #include <HT16K33.h>
#else
  #include <TM1637Display.h>
#endif

// Pin Definitions
#define DIO_C 4  // clock display DIO
#define CLK_C 5  // clock display CLK
#define DIO_D 8  // date display DIO
#define CLK_D 9  // date display CLK
#define DIO_T A3  // temp display DIO
#define CLK_T A2  // temp display CLK
#define ONE_WIRE_BUS 10  // temp sensor bus
#define BTN_PLUS  15  // button increase minutes (0 = not connected)
#define BTN_MINUS 14  // button decrease minutes (0 = not connected)
#define BTN_MOD   16  // button set day instead of minutes (0 = not connected)

#define TIME_24_HOUR true
#define TIME_ZONE    1 // (0=UTC, 1=MEZ)

// Object init
const uint8_t SEG_DEG = SEG_A | SEG_B | SEG_F | SEG_G;  // °
const uint8_t SEG_CEL = SEG_A | SEG_F | SEG_E | SEG_D;  // C
#if HT16K33_insteadof_TM1637 == true
  HT16K33       display2_c(0x70);
  HT16K33       display2_d(0x71);
  HT16K33       display2_t(0x72);
#else
  TM1637Display display_c(CLK_C, DIO_C);
  TM1637Display display_d(CLK_D, DIO_D);
  TM1637Display display_t(CLK_T, DIO_T);
  const uint8_t SEG_DEG_CEL[] = { SEG_DEG, SEG_CEL };
#endif

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

struct ts t;

// Start off at 0:00:00 as a signal that the time should be read from
// the DS1307 to initialize it.
int years   = 0;
int months  = 0;
int days    = 0;
int hours   = 0;
int minutes = 0;
int seconds = 0;
bool blinkColon = false;
bool saveTime   = false;
unsigned long lastSecUpdate = 0;

// Buttons
bool btnState; // for Debounce
bool lastBtnState  = LOW;
bool lastBtnModState = LOW;
int  debounceDelay = 50;
unsigned long lastDebounceTime = 0;
unsigned long startLow1;          // speichert Millis sobald der Button gedrückt wird
unsigned long startLowEachPlus1;  // speichert Zeit (Millis) wann das nächste mal automatisch erhöht werden soll
bool checkLongPress      = false; // damit der Code für automatisches Hochzählen nur ausgeführt wird, wenn der Button runter gedrückt wird. Ist entweder true oder false
int  waitMillisTillLong  = 300;   // wie lange nach runter drücken gewartet werden soll, bis automatisches hochzählen beginnt
int  waitMillisAfterEach = 100;   // alle wie viel Millis automatisch hochgezählt wird

#if HT16K33_insteadof_TM1637 == true
  // With the HT16K33 library, it does not seem to be possible to write
  // custom chars and decimal points while using the pre-defined displayInt(),
  // as it is possible in TM1637 with showNumberDecEx().
  // That's why, the following helper functions are necessary for this display type.
  void HT16K33_date(int days, int months, uint8_t *ar) {
    uint8_t d[2];
    HT16K33_2digit(days, d);
    uint8_t m[2];
    HT16K33_2digit(months, m);
    ar[0] = d[0]; ar[1] = d[1]|SEG_DP;
    ar[2] = m[0]; ar[3] = m[1]|SEG_DP;
  }
  void HT16K33_temp(int temp, uint8_t *ar) {
    uint8_t t[2];
    HT16K33_2digit(temp, t);
    ar[0] = t[0]; ar[1] = t[1];
    ar[2] = SEG_DEG; ar[3] = SEG_CEL;
  }
  void HT16K33_2digit(uint8_t val, uint8_t *ar2) {
    if(val <= 9) {
      ar2[0] = HT16K33_digit(0);
      ar2[1] = HT16K33_digit(val);
    } else {
      String strVal = String(val);
      ar2[0] = HT16K33_digit(String(strVal[0]).toInt());
      ar2[1] = HT16K33_digit(String(strVal[1]).toInt());
    }
  }
  uint8_t HT16K33_digit(uint8_t val) {
    if(val == 0)      return SEG_A|SEG_B|SEG_C|SEG_D|SEG_E|SEG_F;
    else if(val == 1) return SEG_B|SEG_C;
    else if(val == 2) return SEG_A|SEG_B|SEG_G|SEG_E|SEG_D;
    else if(val == 3) return SEG_A|SEG_B|SEG_C|SEG_D|SEG_G;
    else if(val == 4) return SEG_F|SEG_G|SEG_B|SEG_C;
    else if(val == 5) return SEG_A|SEG_F|SEG_G|SEG_C|SEG_D;
    else if(val == 6) return SEG_A|SEG_F|SEG_E|SEG_D|SEG_C|SEG_G;
    else if(val == 7) return SEG_A|SEG_B|SEG_C;
    else if(val == 8) return SEG_A|SEG_B|SEG_C|SEG_D|SEG_E|SEG_F|SEG_G;
    else if(val == 9) return SEG_A|SEG_B|SEG_C|SEG_D|SEG_F|SEG_G;
  }
#endif

void setup() {
  Serial.begin(9600);
  while(!Serial);  // wait for Serial

  // Setup buttons
  if(BTN_PLUS)  pinMode(BTN_PLUS, INPUT_PULLUP);
  if(BTN_MINUS) pinMode(BTN_MINUS, INPUT_PULLUP);
  if(BTN_MOD)   pinMode(BTN_MOD, INPUT_PULLUP);

  // Setup the displays
  #if HT16K33_insteadof_TM1637 == true
    Serial.println("ThermoClockFrame HT16K33 starting!");
    display2_c.begin();
    display2_c.displayOn();
    display2_c.displayInt(8888);
    display2_d.begin();
    display2_d.displayOn();
    display2_d.displayInt(8888);
    display2_t.begin();
    display2_t.displayOn();
    display2_t.displayInt(8888);
  #else
    Serial.println("ThermoClockFrame TM1637 starting!");
    display_c.setBrightness(6);
    display_c.clear();
    display_c.showNumberDecEx(8888, 0b00000000, false, 4, 0);
    display_d.setBrightness(6);
    display_d.clear();
    display_d.showNumberDecEx(8888, 0b00000000, false, 4, 0);
    display_t.setBrightness(6);
    display_t.clear();
    display_t.showNumberDecEx(8888, 0b00000000, false, 4, 0);
  #endif
  delay(250);

  // Setup the temp sensor
  sensors.begin();

  // Setup the real-time clock
  Wire.begin();
  DS3231_init(DS3231_INTCN);

  // Set the clock if it hasn't been set before
  bool setClockTime = false; ///////////////////
  if(setClockTime) {
    Serial.println("Setting real-time clock time!");
    t.hour=18; t.min=54; t.sec=10;
    t.mday=9; t.mon=3; t.year=2026;
    DS3231_set(t);
  }

  // Initial time read
  readTime();
  lastSecUpdate = millis();

  // Initial temp read
  sensors.requestTemperatures();
}

void readTime() {
  for(int i = 0; i < 3; i ++) {
    // Get the time from the clock
    DS3231_get(&t);
    // Now set the hours and minutes
    Serial.println(
      String(t.mday)+"."+String(t.mon)+"."+String(t.year)
      +" "+String(t.hour)+":"+String(t.min)+":"+String(t.sec)
    );
    years   = t.year;
    months  = t.mon;
    days    = t.mday;
    hours   = t.hour;
    minutes = t.min;
    seconds = t.sec;
    // Apply CET daylight saving time (CEST)
    if(isInDst(years, months, days, hours)) {
      hours += 1;
    }
    // Validity check
    if(t.year > 2000 && t.year < 2165) return;
    Serial.println("Got invalid date from RTC... trying again.");
    delay(500);
  }
  if(t.year == 1900) {
    // RTC was resetted (e.g. battery removed)
    // jump to min year
    t.year = 2026;
    DS3231_set(t);
    Serial.println("Setted year to "+String(t.year));
  }
}

bool isInDst(int year, int month, int day, int hour) {
  if(month < 3 || month > 10)
    return false; // keine Sommerzeit in Jan, Feb, Nov, Dez
  if(month > 3 && month < 10)
    return true;  // Sommerzeit in Apr, Mai, Jun, Jul, Aug, Sep

  if(month == 3 && (hour + 24 * day) >= (1 + TIME_ZONE + 24 * (31 - (5 * year / 4 + 4) % 7))
  || month == 10 && (hour + 24 * day) < (1 + TIME_ZONE + 24 * (31 - (5 * year / 4 + 1) % 7)))
    return true;
  else
    return false;
}

int maxMonthDay(int month) {
  if(month == 1 || month == 3 || month == 5 || month == 7 || month == 9 || month == 11)
    return 31;
  else
    return 30;
}

void loop() {
  // Buttons
  int readingPlus = digitalRead(BTN_PLUS);
  int readingMinus = digitalRead(BTN_MINUS);
  boolean reading = readingPlus==LOW || readingMinus==LOW;
  if(reading != lastBtnState) { // debounce
    lastDebounceTime = millis();
  }
  if((millis() - lastDebounceTime) > debounceDelay) { // debounce
    if(reading != btnState) { // debounce
      btnState = reading;  // debounce
      if(btnState) {  // debounce. LOW wenn Pull-Up genutzt wird. Bei Pull-Down muss es auf HIGH geändert werden

        startLow1 = millis(); // Millis zum Zeitpunkt, wenn der Button gedrückt wird
        startLowEachPlus1 = (millis() + waitMillisTillLong);  // Millis, wann das erste mal automatisch hochgezählt werden soll
        checkLongPress = true; // schaltet den "Modus" ein, so dass geprüft wird, ob der Button länger als x millis gedrückt wurde

        if(digitalRead(BTN_MOD) == HIGH) {
          if(readingPlus) minutes += 1;
          else minutes -= 1;
          seconds = 1;
        } else {
          if(readingPlus) days += 1;
          else days -= 1;
        }
        saveTime = true;

      } else {

        checkLongPress = false; // Modus zum prüfen ob lange gedrückt wurde wieder ausschalten

      }
    }
  }
  lastBtnState = reading; // debounce

  if(checkLongPress == true) { // wenn der Button runter gedrückt wurde, wird angefangen zu prüfen
    if(millis() - startLow1 > waitMillisTillLong) {  // nach der eingestellten Zeit wird wird etwas erstmals ausgeführt
      if(millis() - startLowEachPlus1 > waitMillisAfterEach) { // nach eingestellter Zeit für automatisches Hochzählen wird das hier ausgeführt
        startLowEachPlus1 = startLowEachPlus1 + waitMillisAfterEach;  // Zeit hochsetzen für nächtes Automatisches erhöhen

        if(digitalRead(BTN_MOD) == HIGH) {
          if(readingPlus) minutes += 1;
          else minutes -= 1;
          seconds = 1;
        } else {
          if(readingPlus) days += 1;
          else days -= 1;
        }
        saveTime = true;
      }
    }
  }


  // Check if it's the top of the hour and get a new time reading
  // from the DS1307.  This helps keep the clock accurate by fixing
  // any drift.
  ///if(minutes == 0 && seconds == 0) {
  ///  readTime();
  ///}

  unsigned long currentMillis = millis();
  if(currentMillis - lastSecUpdate >= 2000) {
    lastSecUpdate = currentMillis;
    // Now increase the seconds by one.
    // Unfortunately, this is way too unprecise...
    ///seconds += 1;

    // ...so we query RTC every ~2 secs
    if(!checkLongPress) {
      readTime();

      sensors.requestTemperatures();
    }

    // Blink the colon by flipping its value every loop iteration
    // (which happens every second).
    ///blinkColon = !blinkColon;
    ///clockDisplay.drawColon(blinkColon);
  }

  // If the seconds go above 59 then the minutes should increase and
  // the seconds should wrap back to 0.
  if(seconds > 59) {
    seconds = 0;
    minutes += 1;
  }
    // Again if the minutes go above 59 then the hour should increase and
    // the minutes should wrap back to 0.
    if(minutes > 59) {
      minutes = 0;
      hours += 1;
    }
    // Note that when the minutes are 0 (i.e. it's the top of a new hour)
    // then the start of the loop will read the actual time from the DS1307
    // again.  Just to be safe though we'll also increment the hour and wrap
    // back to 0 if it goes above 23 (i.e. past midnight).
    if(hours > 23) {
      hours = 0;
      days += 1;
    }
    if(days > maxMonthDay(months)) {
      days = 1;
      months += 1;
    }
    if(months > 12) {
      months = 1;
      years += 1;
    }

  // If the seconds go below 0 then the minutes should decrease and
  // the seconds should wrap back to 59.
  if(seconds < 0) {
    seconds = 59;
    minutes -= 1;
  }
    // Again if the minutes go below 0 then the hour should decrease and
    // the minutes should wrap back to 59.
    if(minutes < 0) {
      minutes = 59;
      hours -= 1;
    }
    if(hours < 0) {
      hours = 23;
      days -= 1;
    }
    if(days < 1) {
      days = maxMonthDay(months - 1);
      months -= 1;
    }
    if(months < 1) {
      months = 12;
      years -= 1;
    }

  int displayHours = hours;
  int displayMinutes = minutes;
  // Do 24 hour to 12 hour format conversion when required.
  if(!TIME_24_HOUR) {
    // Handle when hours are past 12 by subtracting 12 hours (1200 value).
    if(displayHours > 12) {
      displayHours -= 12;
    }
    // Handle hour 0 (midnight) being shown as 12.
    else if(displayHours == 0) {
      displayHours += 12;
    }
  }
  if(lastBtnModState == LOW && digitalRead(BTN_MOD) == HIGH) {
    // Now print the year value to the display after releasing the MOD button.
    #if HT16K33_insteadof_TM1637 == true
      display2_d.displayInt(years);
    #else
      display_d.showNumberDecEx(years, 0b00000000, true, 4, 0);
    #endif
    delay(1500);
  } else {
    // Now print the date value to the display.
    #if HT16K33_insteadof_TM1637 == true
      uint8_t ar[4];
      HT16K33_date(days, months, ar);
      display2_d.displayRaw(ar);
    #else
      display_d.showNumberDecEx(days,   0b01010000, true, 2, 0);
      display_d.showNumberDecEx(months, 0b01010000, true, 2, 2);
    #endif
  }
  lastBtnModState = digitalRead(BTN_MOD);

  // Now print the time value to the display.
  #if HT16K33_insteadof_TM1637 == true
    display2_c.displayTime(displayHours, displayMinutes, true, false);
  #else
    display_c.showNumberDecEx(displayHours, 0b01000000, true, 2, 0);
    display_c.showNumberDecEx(displayMinutes, 0b00000000, true, 2, 2);
  #endif

  // Store updated time in RTC if changed via button
  if(saveTime) {
    saveTime = false;
    // Check CET daylight saving time (CEST)
    if(isInDst(years, months, days, hours)) {
      t.hour = hours -1;
    } else {
      t.hour = hours;
    }
    t.min  = minutes;
    t.sec  = seconds;
    t.mday = days;
    t.mon  = months;
    t.year = years;
    DS3231_set(t);
  }

  // Now print the temp value to the display.
  #if HT16K33_insteadof_TM1637 == true
    uint8_t ar[4];
    HT16K33_temp(sensors.getTempCByIndex(0), ar);
    display2_t.displayRaw(ar);
  #else
    display_t.showNumberDecEx(sensors.getTempCByIndex(0), 0b00000000, false, 2, 0);
    display_t.setSegments(SEG_DEG_CEL, 2, 2);
  #endif
}
