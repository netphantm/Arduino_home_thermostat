## Home thermostat with touch display 240\*320
> ESP8266 home thermostat

<img src="https://github.com/netphantm/Arduino_home-thermostat/raw/master/pics/pic-01.png" width="300" align="right" />

This is an IoT home thermostat made with a WeMos D1 Mini Pro, solid state relays and a 240\*320 touch display, that's also logging to a webserver which offers a nice gauge and a chart. The hysteresis on my old home thermostat is kaputt and the noises it makes by randomly switching on/off are *very* annoying, especially at night. The scope here was to do these projects on my own, more or less from scratch and not just buy and install an already built one (see [Links](#Links)).

I used tzapu's WiFiManager, so I don't have to hard-code the WiFi credentials and local IP address (reflashing it every time I change my WiFi configuration).

I've used a solid state relay that doesn't make those clicking noises when it turns the heating on or off, and a double one so it interrupts both power lines to the heating system.

I suppose it could be integrated into [Home Assistant](https://hass.io/) (or anonther home automation system), but I didn't want that. I plan to make a nice, resposive web interface for the settings, hosted on my own webserver. For now, the crude php version which also controls my other thermostats, is doing the job just fine.

---

### Features
- **WiFiManager**
- **240\*320 touch Display**
    - It Shows the measured and target temperatures and the heating status (On/Off). I'm working on an info page for the connection and other miscellaneous settings
    - The display dimms down, one minute after using the touch sensor

---

### Libraries needed

- WiFiManager
- ESP8266HTTPClient
- ESP8266WebServer
- ESP8266mDNS
- OneWire
- DallasTemperature
- SSD1306Wire
- ArduinoJson
- Free_Fonts
- time

---

### TODO
- [ ] add info page for connection and other miscellaneous settings
- [ ] add display page for scheduled timers
- [ ] display dimming after 1 minute
- [ ] rearrange standard display, show other useful stuff - like current time, outside temperature, etc.
- [ ] documentation, perhaps 'fritzing'

---

### Links

[16MB WeMos D1 Mini Pro # NodeMcu # ESP8266 ESP-8266EX CP2104 for Arduino NodeMCU](https://www.ebay.de/itm/16MB-WeMos-D1-Mini-Pro-NodeMcu-ESP8266-ESP-8266EX-CP2104-for-Arduino-NodeMCU/272405937539?ssPageName=STRK%3AMEBIDX%3AIT&_trksid=p2057872.m2749.l2649)

[2.8 Inch TFT SPI Serial Port LCD Touch Panel ILI9341 240x320 5V/3.3V BAF](https://www.ebay.de/itm/2-8-Inch-TFT-SPI-Serial-Port-LCD-Touch-Panel-Module-ILI9341-240x320-5V-3-3V-/152643497986?hash=item238a42f002)

[1/2/4 Channel 5v OMRON SSR G3MB-202P Solid State Relay Module For Arduino](https://www.ebay.de/itm/1-2-4-Channel-5v-OMRON-SSR-G3MB-202P-Solid-State-Relay-Module-For-Arduino/253066802045?ssPageName=STRK%3AMEBIDX%3AIT&_trksid=p2057872.m2749.l2649)

[WiFiManager](https://github.com/tzapu/WiFiManager)

[ Details zu  Schwarz WiFi Funk Thermostat Raumthermostat Fußbodenheizung Programmierbar APP](https://www.ebay.de/itm/Schwarz-WiFi-Funk-Thermostat-Raumthermostat-Fusbodenheizung-Programmierbar-APP/163618503022)

[Home Assistant](http://hass.io/)

---

### Images

<img src="https://github.com/netphantm/Arduino_home-thermostat/raw/master/pics/pic-01.png" alt="pic-01" width="440px" align="left"> 

---

### License

[![License](http://img.shields.io/:license-mit-blue.svg?style=flat-square)](http://badges.mit-license.org)

- **[MIT license](http://opensource.org/licenses/mit-license.php)**
- Copyright 2018 © netphantm.

[↑ goto top](#Relay-socket-with-OLED-display)

