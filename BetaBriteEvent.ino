/*************************************************
 *************************************************
    Sketch BetaBriteEvent.ino   Affichage des evenement udp sur un afficheur
    Copyright 20201 Pierre HENRY net234@frdev.com
    https://github.com/betamachine-org/BetaBriteEvent   net234@frdev.com

  This file is part of BetaBriteEvent.

    BetaBriteEvent is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    BetaBriteEvent is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with betaEvents.  If not, see <https://www.gnu.org/licenses/lglp.txt>.


  History
    V1.0 (18/08/2022)
    - build from BetaPorte V2.1.0 (13/08/2022) et checkMyBox V1.2
    V1.1 (22/08/2022)
    - refonte du protocole betaDomo (ajout json)
    - ajout surveillance du wifi  acces web  et acces intanet
    - correction pour avoir la timeZone automatiquement
   V1.2  (07/09/2022)
    _ ajout d'un afficheur LCD 4x 20
    - creation d'un module evenementiel udpEvent pour une gestion des messgae en UDP
   V1.3  (16/02/2024)

*************************************************/

// Definition des constantes pour les IO
#include "ESP8266.h"
static_assert(sizeof(time_t) == 8, "This version works with time_t 64bit  move to ESP8266 kernel 3.0 or more");

#define APP_NAME "BetaBriteEvent V1.3"


//
/* Evenements du Manager (voir EventsManager.h)
  evNill = 0,      // No event  about 1 every milisecond but do not use them for delay Use pushDelay(delay,event)
  ev100Hz,         // tick 100HZ    non cumulative (see betaEvent.h)
  ev10Hz,          // tick 10HZ     non cumulative (see betaEvent.h)
  ev1Hz,           // un tick 1HZ   cumulative (see betaEvent.h)
  ev24H,           // 24H when timestamp pass over 24H
  evInit,
  evInChar,
  evInString,
*/


#define NOT_A_DATE_YEAR 2000

// Liste des evenements specifique a ce projet


enum tUserEventCode {
  // evenement utilisateurs
  evBP0 = 100,  // low = low power allowed
  evLed0,
  evUdp,  // Trame UDP avec un evenement
  evCheckWWW,
  evCheckAPI,
  evNewStatus,
  evEraseUdp,
  evPostInit,
  evLcdRefresh,
  // evenement action
  doReset,
};

#define checkWWW_DELAY (60 * 60 * 1000L)  // toute les heures
#define checkAPI_DELAY (5 * 60 * 1000L)   // toute les 5 minutes

#define HTTP_API "http://nimux.frdev.com/net234/api.php"  // URI du serveur api local


// instance betaEvent

//  une instance "Events" avec un poussoir "BP0" une LED "Led0" un clavier "Keyboard"
//  MyBP0 genere un evenement evBP0 a chaque pression le poussoir connecté sur pinBP0
//  Led0 genere un evenement evLed0 a chaque clignotement de la led precablée sur la platine
//  Keyboard genere un evenement evChar a char caractere recu et un evenement evString a chaque ligne recue
//  Debug permet sur reception d'un "T" sur l'entrée Serial d'afficher les infos de charge du CPU

//#define DEFAULT_PIN
// les sortie pour la led et le poussoir sont definis dans esp8266.h avec BP0_PIN  et LED0_PIN
#define DEBUG_ON
#include <BetaEvents.h>

//  Info I2C

#define LCD_I2CADR 0x4E / 2  //adresse LCD

#include <Wire.h>  // Gestion I2C

// Instance LCD
#include <LiquidCrystal_PCF8574.h>
LiquidCrystal_PCF8574 lcd(LCD_I2CADR);  // set the LCD address




// littleFS
#include <LittleFS.h>  //Include File System Headers
#define MyLittleFS LittleFS


//WiFI
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>  // acces web API
#include <Arduino_JSON.h>


// rtc memory to keep date
//struct __attribute__((packed))
struct {
  // all these values are keep in RTC RAM
  uint8_t crc8;            // CRC for savedRTCmemory
  time_t actualTimestamp;  // time stamp restored on next boot Should be update in the loop() with setActualTimestamp
} savedRTCmemory;




// Variable d'application locale
String nodeName = "NODE_NAME";  // nom de  la device (a configurer avec NODE=)"
bool WiFiConnected = false;

time_t currentTime;
int8_t timeZone = -2;  //les heures sont toutes en localtimes
bool configOk = true;  // global used by getConfig...

String messageUDP;  //trame UDP
String openDoors;
bool postInit = false;

//String   message;

bool WWWOk = false;
bool APIOk = false;
//int      currentMonth = -1;
bool sleepOk = true;
JSONVar temperatures;
String displayText;
String lcdMessage;

// gestion de l'ecran

bool lcdOk = false;

// init UDP
#include "evHandlerUdp.h"
const unsigned int localUdpPort = 23423;  // local port to listen on
evHandlerUdp myUdp(evUdp, localUdpPort, nodeName);




void setup() {
  enableWiFiAtBootTime();  // obligatoire pour lekernel ESP > 3.0
  Serial.begin(115200);
  Serial1.begin(2400, SERIAL_7E1);  // afficheur betabrite sur serial1 TX (D4)
  Serial.println(F("\r\n\n" APP_NAME));

  // Start instance
  Events.begin();

  D_println(WiFi.getMode());
  // normaly not needed
  if (WiFi.getMode() != WIFI_STA) {
    Serial.println(F("!!! Force WiFi to STA mode !!!"));
    WiFi.mode(WIFI_STA);
    //WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  }

  //  message.reserve(200);
  messageUDP.reserve(100);
  openDoors.reserve(100);
  displayText.reserve(200);
  lcdMessage.reserve(200);

  //  Serial.println(F("Bonjour ...."));


  // System de fichier
  if (!MyLittleFS.begin()) {
    Serial.println(F("erreur MyLittleFS"));
    fatalError(3);
  }

  // recuperation de l'heure dans la static ram de l'ESP
  if (!getRTCMemory()) {
    savedRTCmemory.actualTimestamp = 0;
  }
  // little trick to leave timeStatus to timeNotSet
  // TODO: see with https://github.com/PaulStoffregen/Time to find a way to say timeNeedsSync
  adjustTime(savedRTCmemory.actualTimestamp);
  currentTime = savedRTCmemory.actualTimestamp;

  // recuperation de la config dans config.json
  nodeName = jobGetConfigStr(F("nodename"));
  if (nodeName == "") {
    Serial.println(F("!!! Configurer le nom de la device avec 'NODE=nodename' !!!"));
    //   configErr = true;
  }
  D_println(nodeName);

  // recuperation de la timezone dans la config
  timeZone = jobGetConfigInt(F("timezone"));
  if (!configOk) {
    timeZone = -2;  // par defaut France hivers
    jobSetConfigInt(F("timezone"), timeZone);
    Serial.println(F("!!! timezone !!!"));
  }
  D_println(timeZone);

  //Init I2C
  Wire.begin(I2C_SDA, I2C_SCL);

  //  Check LCD
  if (!checkI2C(LCD_I2CADR)) {
    Serial.println(F("No LCD detected"));
  }

  // Init LCD

  lcd.begin(20, 4);  // initialize the lcd
  lcd.setBacklight(100);
  lcd.println(F(APP_NAME));
  lcd.println(F("Bonjour"));




  String message = F("Bonjour ...   ");
  message += niceDisplayTime(currentTime, true);
  betaBriteWrite(message);

  Serial.println("Bonjour ....");
  Serial.println("Tapez '?' pour avoir la liste des commandes");

  D_println(Events.freeRam());
}

String niceDisplayTime(const time_t time, bool full = false);

void loop() {
  Events.get(sleepOk);
  Events.handle();
  switch (Events.code) {
      //    case evNill:
      //      break;


    case evInit:
      Serial.println("Init");
      Events.delayedPush(10000, evPostInit);
      break;

    case evPostInit:
      postInit = true;
      Events.push(evNewStatus);
      break;

    case ev24H:
      {
        String newDateTime = niceDisplayTime(currentTime, true);
        D_println(newDateTime);
      }
      break;



    case ev1Hz:
      {

        // check for connection to local WiFi  1 fois par seconde c'est suffisant
        static uint8_t oldWiFiStatus = 99;
        uint8_t WiFiStatus = WiFi.status();
        if (oldWiFiStatus != WiFiStatus) {
          oldWiFiStatus = WiFiStatus;
          D_println(WiFiStatus);
          //    WL_IDLE_STATUS      = 0,
          //    WL_NO_SSID_AVAIL    = 1,
          //    WL_SCAN_COMPLETED   = 2,
          //    WL_CONNECTED        = 3,
          //    WL_CONNECT_FAILED   = 4,
          //    WL_CONNECTION_LOST  = 5,
          //    WL_DISCONNECTED     = 6
          WiFiConnected = (WiFiStatus == WL_CONNECTED);
          static bool wasConnected = false;
          if (wasConnected != WiFiConnected) {
            wasConnected = WiFiConnected;
            Led0.setFrequence(WiFiConnected ? 1 : 2);
            if (WiFiConnected) {
              setSyncProvider(getWebTime);
              setSyncInterval(6 * 3600);
              // lisen UDP 23423
              Serial.println("Listen broadcast");
              myUdp.begin();
              Events.delayedPush(5000, evCheckWWW);
              Events.delayedPush(7000, evCheckAPI);

            } else {
              WWWOk = false;
            }
            Events.push(evNewStatus);
            D_println(WiFiConnected);
          }
        }

        // check lcd
        if (lcdOk != checkI2C(LCD_I2CADR)) {
          lcdOk = !lcdOk;
          D_println(lcdOk);
        }
        if (lcdOk and postInit) {
          lcd.setCursor(0, 0);
          lcd.println(APP_NAME);
          String aStr = niceDisplayTime(currentTime,true);
          lcd.println(aStr);
        }




        // Save current time in RTC memory (not erased by a reset)
        currentTime = now();
        savedRTCmemory.actualTimestamp = currentTime;  // save time in RTC memory
        saveRTCmemory();
        static uint8_t lastMinute = minute();
        if (lastMinute != minute()) {
          lastMinute = minute();
          Events.push(evNewStatus);



          if (WiFiConnected) myUdp.broadcast("{\"init\":\"afficheur\"}");
        }

        // If we are not connected we warn the user every 30 seconds that we need to update credential
        if (!WiFiConnected && second() % 30 == 15) {
          // every 30 sec
          Serial.print(F("module non connecté au Wifi local "));
          D_println(WiFi.SSID());
          Serial.println(F("taper WIFI= pour configurer le Wifi"));
        }
      }

      break;
    case evUdp:
      {
        if (Events.ext == evxUdpRxMessage) {
          D_print(myUdp.bcast);
          D_print(myUdp.rxHeader);
          D_print(myUdp.rxNode);
          D_println(myUdp.rxJson);
          if (lcdOk) {
            lcd.setCursor(0, 2);
            lcd.print(myUdp.rxHeader);
            lcd.print(' ');
            lcd.println(myUdp.rxNode);
            lcd.print(myUdp.rxJson);
          }
          if (!myUdp.bcast) return;
          JSONVar jsonData = JSON.parse(myUdp.rxJson);
          // temperature
          JSONVar rxJson2 = jsonData["temperature"];
          if (JSON.typeof(rxJson2).equals("object")) {
            String aName = rxJson2.keys()[0];
            //DV_println(aName);
            double aValue = rxJson2[aName];
            //DV_println(aValue);
            temperatures[aName] = aValue;
            D_println(temperatures);
            return;
          }
          //CMD
          rxJson2 = jsonData["CMD"];
          if (JSON.typeof(rxJson2).equals("object")) {
            String dest = rxJson2.keys()[0];
            // Les CMD acceptée doivent etre adressé a ce module
            if (dest.equals("ALL") or (dest.length() > 3 and nodeName.startsWith(dest))) {
              String aCmd = rxJson2[dest];
              aCmd.trim();
              D_println(aCmd);
              if (aCmd.startsWith(F("NODE=")) and !nodeName.equals(dest)) break;  // NODE= not allowed on aliases
              if (aCmd.length()) Keyboard.setInputString(aCmd);
            } else {
              TD_println("CMD not for me.", dest);
            }
          }


          String action = (const char*)jsonData["action"];
          D_println(action);
          if (action.equals(F("badge"))) {
            //eceived packet UDPmyUdp.rxHeader => 'EVENT', myUdp.rxNode => 'Betaporte_2B', myUdp.rxJson => '{"action":"badge","userid":"Test_5"}'

            String aStr = myUdp.rxNode;

            aStr += " : ";
            aStr += (const char*)jsonData["userid"];  //
            D_println(aStr);

            if (messageUDP.indexOf(aStr) < 0) {
              messageUDP += "    ";
              messageUDP += aStr;
            }
            Events.delayedPush(500, evNewStatus);
            Events.delayedPush(5 * 60 * 1000, evEraseUdp);
            return;
          }



          if (action.equals(F("porte"))) {
            //Received packet UDPmyUdp.bcast => '1', myUdp.rxHeader => 'EVENT', myUdp.rxNode => 'Betaporte_2', myUdp.rxJson => '{"action":"porte","close":false}'
            bool closed = (bool)jsonData["close"];
            String aStr = myUdp.rxNode + ' ';
            if (closed) {
              openDoors.replace(aStr, "");
              //openDoors.trim();
            } else {
              if (openDoors.indexOf(aStr) < 0) {

                openDoors += aStr;
              }
            }

            Events.delayedPush(500, evNewStatus);
            //Events.delayedPush(3 * 60 * 1000, evEraseUdp);
            return;
          }
        }
      }
      break;


      /*
1CH+“1”(31H)=Red
• 1CH + “2” (32H) = Green
• 1CH + “3” (33H) = Amber
• 1CH + “4” (34H) = Dim red
• 1CH + “5” (35H) = Dim green
• 1CH+“6”(36H)=Brown
• 1CH + “7” (37H) = Orange
• 1CH + “8” (38H) = Yellow
• 1CH + “9” (39H) = Rainbow 1
• 1CH + “A” (41H) = Rainbow 2
• 1CH + “B” (42H) = Color mix
• 1CH+“C”(43H)=Autocolor
• 1CH+“ZRRGGBB”=(Alpha3.0protocol only.) Change the font color to this RGB value (“RRGGBB” = Red, Green, and Blue color intensities in ASCII hexadecimal from “00” to “FF”.)
• 1CH+“YRRGGBB”=(Alpha3.0protocol only.) Change the color of the shaded portion of the font to this RGB value (“RRGGBB” = Red, Green, and Blue color intensities in ASCII hexadecimal from “00” to “FF”.)
*/
    case evNewStatus:
      {
        String aMessage = "";
        if (openDoors.length() > 0) {
          aMessage += F("  \x1c"
                        "9"
                        " porte ouverte : ");
          aMessage += openDoors;
        }
        if (messageUDP.length() > 0) {
          aMessage += F("\x1c"
                        "9");
          aMessage += messageUDP;
          aMessage += "    ";
        }
        aMessage += F("\x1c"
                      "3");
        aMessage += niceDisplayTime(currentTime, true);
        aMessage = aMessage.substring(0, aMessage.length() - 3);

        if (WiFiConnected && WWWOk && APIOk) {
          aMessage += F("\x1c"
                        "2"
                        " Infra Ok    ");
        } else {
          if (!WiFiConnected) aMessage += F("\x1c"
                                            "1"
                                            " WIFI Err ");
          if (!WWWOk) aMessage += F("\x1c"
                                    "1"
                                    " WWW Err ");
          if (!APIOk) aMessage += F("\x1c"
                                    "1"
                                    " API Err ");
        }

        JSONVar keys = temperatures.keys();
        if (keys.length() > 1) {
          aMessage += F(("\x1c"
                         "2"
                         "   Temperatures "));
          for (int i = 0; i < keys.length(); i++) {
            aMessage += (String)keys[i];
            aMessage += ':';
            aMessage += String((double)temperatures[keys[i]], 1);
            aMessage += "  ";
          }
        }
        if (displayText.length()) {
          aMessage += F("\x1c"
                        "8");  //Yellow
          aMessage += displayText;

          aMessage += "  ";
        }

        if (postInit) betaBriteWrite(aMessage);
        if (lcdOk) Events.delayedPush(300, evLcdRefresh, 0);
      }
      break;

    case evLcdRefresh:
      {
        if (!lcdOk) break;

        uint displayStep = Events.intExt;
        //D_print(displayStep);
        lcd.setCursor(0, 2);
        // lcd.print(LCD_CLREOL "\r\n" LCD_CLREOL);
        // lcd.setCursor(0, 2);
        String aStr = lcdMessage;
        //aStr += F("                                            ");
        aStr = aStr.substring(displayStep, displayStep+39);
        lcd.print(aStr);
        lcd.print(LCD_CLREOL);


        displayStep++;

        if (displayStep > lcdMessage.length() - 10) displayStep = 0;
        Events.delayedPush(500, evLcdRefresh, displayStep);
      }
      break;


    case evEraseUdp:
      {
        messageUDP = "";
      }
      break;


    case evCheckWWW:
      Serial.println("evCheckWWW");
      if (WiFiConnected) {
        if (WWWOk != (getWebTime() > 0)) {
          WWWOk = !WWWOk;
          D_println(WWWOk);
          Events.delayedPush(1000, evNewStatus);
        }
        Events.delayedPush(checkWWW_DELAY, evCheckWWW);
      }
      break;

    case evCheckAPI:
      {
        Serial.println("evCheckAPI");
        if (WiFiConnected) {
          JSONVar jsonData;
          jsonData["timeZone"] = timeZone;
          jsonData["timestamp"] = (double)currentTime;
          //         jsonData["sonde1"] = sonde1;
          //         jsonData["sonde2"] = sonde2;
          String jsonStr = JSON.stringify(jsonData);
          if (APIOk != dialWithPHP(nodeName, "timezone", jsonStr)) {
            APIOk = !APIOk;
            D_println(APIOk);
            Events.delayedPush(1000, evNewStatus);
            //           writeHisto( APIOk ? F("API Ok") : F("API Err"), "magnus2.frdev" );
          }
          if (APIOk) {
            jsonData = JSON.parse(jsonStr);
            time_t aTimeZone = (const double)jsonData["timezone"];
            D_println(aTimeZone);
            if (aTimeZone != timeZone) {
              //             writeHisto( F("Old TimeZone"), String(timeZone) );
              timeZone = aTimeZone;
              jobSetConfigInt("timezone", timeZone);
              // force recalculation of time
              setSyncProvider(getWebTime);
              currentTime = now();
              //            writeHisto( F("New TimeZone"), String(timeZone) );
            }
          }
        }
        Events.delayedPush(checkAPI_DELAY, evCheckAPI);
      }
      break;



    case doReset:
      Events.reset();
      break;


    case evInChar:
      {
        if (Debug.trackTime < 2) {
          //char aChar = Keyboard.inputChar;
          //          if (isPrintable(aChar)) {
          //            D_println(aChar);
          //          } else {
          //            D_println(int(aChar));
          //          }
        }
        switch (Keyboard.inputChar) {
          case '0': delay(10); break;
          case '1': delay(100); break;
          case '2': delay(200); break;
          case '3': delay(300); break;
          case '4': delay(400); break;
          case '5': delay(500); break;
        }
      }
      break;



    case evInString:
      if (Debug.trackTime < 2) {
        D_println(Keyboard.inputString);
      }

      if (Keyboard.inputString.startsWith(F("NODE="))) {
        Serial.println(F("SETUP NODENAME : 'NODE=nodename'  ( this will reset)"));
        String aStr = Keyboard.inputString;
        grabFromStringUntil(aStr, '=');
        aStr.replace(" ", "_");
        aStr.trim();

        if (aStr != "") {
          nodeName = aStr;
          D_println(nodeName);
          jobSetConfigStr(F("nodename"), nodeName);
          delay(1000);
          Events.reset();
        }
      }

      if (Keyboard.inputString.startsWith(F("DSP="))) {
        Serial.println(F("DISPLAY : 'DSP= atext "));
        String aStr = Keyboard.inputString;
        grabFromStringUntil(aStr, '=');
        aStr.trim();
        D_println(aStr);
        displayText = aStr;
        Events.push(evNewStatus);
      }


      if (Keyboard.inputString.startsWith(F("WIFI="))) {
        Serial.println(F("SETUP WIFI : 'WIFI=WifiName,password"));
        String aStr = Keyboard.inputString;
        grabFromStringUntil(aStr, '=');
        String ssid = grabFromStringUntil(aStr, ',');
        ssid.trim();
        D_println(ssid);
        if (ssid != "") {
          String pass = aStr;
          pass.trim();
          D_println(pass);
          bool result = WiFi.begin(ssid, pass);
          Serial.print(F("WiFi begin "));
          D1_println(result);
        }
      }

      if (Keyboard.inputString.equals(F("RESET"))) {
        Serial.println(F("RESET"));
        Events.push(doReset);
      }
      if (Keyboard.inputString.equals(F("FREE"))) {
        D_println(Events.freeRam());
        String aStr = F("FREE=");
        aStr += String(Events.freeRam());
        aStr += F(",APP=");
        aStr += String(F(APP_NAME));
        myUdp.broadcastInfo(aStr);
      }
      if (Keyboard.inputString.equals(F("INFO"))) {
        String aStr = F(" CPU=");
        aStr += String(Events._percentCPU);
        myUdp.broadcastInfo(aStr);
        D_print(aStr)
      }

      /*
      if (Keyboard.inputString.equals("S")) {
        sleepOk = !sleepOk;
        D_println(sleepOk);
      }
      if (Keyboard.inputString.equals(F("RAZCONF"))) {
        Serial.println(F("RAZCONF this will reset"));
        eraseConfig();
        delay(1000);
        Events.reset();
      }
      */
  }
}


// helpers

// Check I2C
bool checkI2C(const uint8_t i2cAddr) {
  Wire.beginTransmission(i2cAddr);
  return (Wire.endTransmission() == 0);
}

// fatal error
// flash led0 same number as error 10 time then reset
//
void fatalError(const uint8_t error) {
  Serial.print(F("Fatal error "));
  Serial.println(error);

  // display error on LED_BUILTIN
  for (uint8_t N = 1; N <= 5; N++) {
    for (uint8_t N1 = 1; N1 <= error; N1++) {
      delay(150);
      Led0.setOn(true);
      beep(988, 100);
      delay(150);
      Led0.setOn(false);
    }
    delay(500);
  }
  delay(2000);
  Events.reset();
}


void beep(const uint16_t frequence, const uint16_t duree) {
  tone(BEEP_PIN, frequence, duree);
}



String niceDisplayTime(const time_t time, bool full) {

  String txt;
  // we supose that time < NOT_A_DATE_YEAR is not a date
  if (year(time) < NOT_A_DATE_YEAR) {
    txt = "          ";
    txt += time / (24 * 3600);
    txt += ' ';
    txt = txt.substring(txt.length() - 10);
  } else {

    txt = Digit2_str(day(time));
    txt += '/';
    txt += Digit2_str(month(time));
    txt += '/';
    txt += year(time);
  }

  static String date;
  if (!full && txt == date) {
    txt = "";
  } else {
    date = txt;
    txt += " ";
  }
  txt += Digit2_str(hour(time));
  txt += ':';
  txt += Digit2_str(minute(time));
  txt += ':';
  txt += Digit2_str(second(time));
  return txt;
}

// helper to save and restore RTC_DATA
// this is ugly but we need this to get correct sizeof()
#define RTC_DATA(x) (uint32_t*)&x, sizeof(x)

bool saveRTCmemory() {
  setCrc8(&savedRTCmemory.crc8 + 1, sizeof(savedRTCmemory) - 1, savedRTCmemory.crc8);
  //system_rtc_mem_read(64, &savedRTCmemory, sizeof(savedRTCmemory));
  return ESP.rtcUserMemoryWrite(0, RTC_DATA(savedRTCmemory));
}


bool getRTCMemory() {
  ESP.rtcUserMemoryRead(0, RTC_DATA(savedRTCmemory));
  //Serial.print("CRC1="); Serial.println(getCrc8( (uint8_t*)&savedRTCmemory,sizeof(savedRTCmemory) ));
  return (setCrc8(&savedRTCmemory.crc8 + 1, sizeof(savedRTCmemory) - 1, savedRTCmemory.crc8));
}

/////////////////////////////////////////////////////////////////////////
//  crc 8 tool
// https://www.nongnu.org/avr-libc/user-manual/group__util__crc.html


//__attribute__((always_inline))
inline uint8_t _crc8_ccitt_update(uint8_t crc, const uint8_t inData) {
  uint8_t i;
  crc ^= inData;

  for (i = 0; i < 8; i++) {
    if ((crc & 0x80) != 0) {
      crc <<= 1;
      crc ^= 0x07;
    } else {
      crc <<= 1;
    }
  }
  return crc;
}

bool setCrc8(const void* data, const uint16_t size, uint8_t& refCrc) {
  uint8_t* dataPtr = (uint8_t*)data;
  uint8_t crc = 0xFF;
  for (uint8_t i = 0; i < size; i++) crc = _crc8_ccitt_update(crc, *(dataPtr++));
  //Serial.print("CRC "); Serial.print(refCrc); Serial.print(" / "); Serial.println(crc);
  bool result = (crc == refCrc);
  refCrc = crc;
  return result;
}
