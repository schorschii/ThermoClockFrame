# ThermoClockFrame
<img align="right" style="width:280px" src=".github/example.jpg">

The ThermoClockFrame is an Arduino (Pro Micro) based digital clock with temperature display.

It is assembled on the back of a picture frame, so that the LED displays are shining through the picture. When it is powered off, it looks like a normal picture frame.

Daylight saving time (DST) is automatically applied if the year, month, day and hour reported by the real-time clock is within the European CEST range.

By long-pressing/holding a button, the minutes are increased/decreased automatically in 150ms interval. By pressing the 3rd (mod) button, the day will be changed instead of minutes.

## Components
- DS3231 real-time clock module
- 1, 2 or 3 TM1637 or HT16K33 LED displays for date, time and/or temperature
- (optional) Dallas temperature sensor (over OneWire bus)
- (optional) 3x buttons for manual clock adjustment

## Libraries
In your Arduino IDE, install the following libs:
- OneWire (by Jim Studt et al.)
- DallasTemperature (by Miles Burton et al.)
- TM1637 (by Avishay Orpaz)
- HT16K33 (by Rob Tillaart)
- ds3231FS (by Petre Rodan)

## Assembly
If you want to get one exemplar pre-assembled, [contact me](https://georg-sieber.de/blog-thermoclockframe).

1. Connect DS3231 to I2C bus
   - SDA: pin 2
   - SCL: pin 3
2. Connect displays:
   - TM1637
     - time: pin 4 & 5
     - date: pin 8 & 9
     - temp: pin A3 & A2
   - HT16K33 to I2C bus with addresses:
     - time: 0x70 (no jumper)
     - date: 0x71 (jumper A0)
     - temp: 0x72 (jumper A1)
3. Buttons:
   - Plus: pin 15 & GND
   - Minus: pin 14 & GND
   - Mod: pin 16 & GND
4. Temperature sensor:
   - pin 10 (+ 4,7K to VCC)
