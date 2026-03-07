#include <Wire.h>
#include <ds3231.h>
#include <TM1637Display.h>

#include <OneWire.h>
#include <DallasTemperature.h>

// Pin Definitions
#define CLK_C 4  // clock display CLK
#define DIO_C 5  // clock display DIO
#define CLK_D 6  // date display CLK
#define DIO_D 7  // date display DIO
#define CLK_T 8  // temp display CLK
#define DIO_T 9  // temp display DIO
#define ONE_WIRE_BUS 12  // temp sensor bus
#define BTN_PLUS  0  // button increase minutes
#define BTN_MINUS 0  // button decrease minutes

#define TIME_24_HOUR true
#define TIME_ZONE    1 // (0=UTC, 1=MEZ)

TM1637Display display_d(CLK_D, DIO_D);
TM1637Display display_c(CLK_C, DIO_C);
TM1637Display display_t(CLK_T, DIO_T);

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

struct ts t;

const uint8_t SEG_DEG[] = {
    SEG_A | SEG_B | SEG_F | SEG_G,  // °
    SEG_A | SEG_F | SEG_E | SEG_D   // C
};

// Start off at 0:00:00 as a signal that the time should be read from
// the DS1307 to initialize it.
int year    = 0;
int month   = 0;
int day     = 0;
int hours   = 0;
int minutes = 0;
int seconds = 0;
bool blinkColon = false;
bool saveTime   = false;
unsigned long lastSecUpdate = 0;

// Buttons
bool btnState; // for Debounce
bool lastBtnState  = LOW;
int  debounceDelay = 50;
unsigned long lastDebounceTime = 0;
unsigned long startLow1;          // speichert Millis sobald der Button gedrückt wird
unsigned long startLowEachPlus1;  // speichert Zeit (Millis) wann das nächste mal automatisch erhöht werden soll
bool checkLongPress      = false; // damit der Code für automatisches Hochzählen nur ausgeführt wird, wenn der Button runter gedrückt wird. Ist entweder true oder false
int  waitMillisTillLong  = 300;   // wie lange nach runter drücken gewartet werden soll, bis automatisches hochzählen beginnt
int  waitMillisAfterEach = 150;   // alle wie viel Millis automatisch hochgezählt wird

void setup() {
  Serial.begin(9600);
  Serial.println("ThermoClockFrame starting!");

  // Setup buttons
  if(BTN_PLUS)  pinMode(BTN_PLUS, INPUT_PULLUP);
  if(BTN_MINUS) pinMode(BTN_MINUS, INPUT_PULLUP);

  // Setup the displays
  display_d.setBrightness(6);
  display_d.clear();
  display_d.showNumberDecEx(8888, 0b00000000, false, 4, 0);
  display_c.setBrightness(6);
  display_c.clear();
  display_c.showNumberDecEx(8888, 0b00000000, false, 4, 0);
  display_t.setBrightness(6);
  display_t.clear();
  display_t.showNumberDecEx(8888, 0b00000000, false, 4, 0);
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
    t.hour=23; t.min=3; t.sec=50;
    t.mday=7; t.mon=3; t.year=2026;
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
    Serial.print("Date : ");
    Serial.print(t.mday);
    Serial.print("/");
    Serial.print(t.mon);
    Serial.print("/");
    Serial.print(t.year);
    Serial.print("\t Hour : ");
    Serial.print(t.hour);
    Serial.print(":");
    Serial.print(t.min);
    Serial.print(".");
    Serial.println(t.sec);
    // Now set the hours and minutes
    year    = t.year;
    month   = t.mon;
    day     = t.mday;
    hours   = t.hour;
    minutes = t.min;
    seconds = t.sec;
    // Apply CET daylight saving time (CEST)
    if(isInDst(year, month, day, hours)) {
      hours += 1;
    }
    // Validity check
    if(t.year != 0) return;
    Serial.println("Got invalid date from RTC... trying again.");
    delay(1000);
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

        if(readingPlus) minutes += 1;
        else minutes -= 1;
        seconds = 1;
        saveTime = true;
      }

      if(!btnState) {
        checkLongPress = false; // Modus zum prüfen ob lange gedrückt wurde wieder ausschalten
      }
    }
  }
  lastBtnState = reading; // debounce

  if(checkLongPress == true) { // wenn der Button runter gedrückt wurde, wird angefangen zu prüfen
    if(millis() - startLow1 > waitMillisTillLong) {  // nach der eingestellten Zeit wird wird etwas erstmals ausgeführt
      if(millis() - startLowEachPlus1 > waitMillisAfterEach) { // nach eingestellter Zeit für automatisches Hochzählen wird das hier ausgeführt
        //startLowEachPlus1 = startLowEachPlus1 + waitMillisAfterEach;  // Zeit hochsetzen für nächtes Automatisches erhöhen

        if(readingPlus) minutes += 1;
        else minutes -= 1;
        seconds = 1;
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
      day += 1;
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
      day -= 1;
    }

  // Show the time on the display by turning it into a numeric
  // value, like 3:30 turns into 330, by multiplying the hour by
  // 100 and then adding the minutes.
  int displayValue = hours*100 + minutes;
  // Do 24 hour to 12 hour format conversion when required.
  if(!TIME_24_HOUR) {
    // Handle when hours are past 12 by subtracting 12 hours (1200 value).
    if(hours > 12) {
      displayValue -= 1200;
    }
    // Handle hour 0 (midnight) being shown as 12.
    else if(hours == 0) {
      displayValue += 1200;
    }
  }
  // Now print the date value to the display.
  display_d.showNumberDecEx(day,   0b01010000, true, 2, 0);
  display_d.showNumberDecEx(month, 0b01010000, true, 2, 2);

  // Now print the time value to the display.
  display_c.showNumberDecEx(displayValue, 0b11100000, true, 4, 0);

  // Store updated time in RTC if changed via button
  if(saveTime) {
    saveTime = false;
    // Check CET daylight saving time (CEST)
    if(isInDst(year, month, day, hours)) {
      t.hour = hours -1;
    } else {
      t.hour = hours;
    }
    t.min  = minutes;
    t.sec  = seconds;
    DS3231_set(t);
  }

  // Now print the temp value to the display.
  display_t.showNumberDecEx(sensors.getTempCByIndex(0), 0b00000000, false, 2, 0);
  display_t.setSegments(SEG_DEG, 2, 2);
}
