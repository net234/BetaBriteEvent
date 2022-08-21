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
    V1.0 (18/07/2022)
    - build from BetaPorte V2.1.0 (13/08/2022)

  port


*************************************************/

// Definition des constantes pour les IO
#include "ESP8266.h"
static_assert(sizeof(time_t) == 8, "This version works with time_t 64bit  move to ESP8266 kernel 3.0 or more");

#define APP_NAME "BetaBriteEvent V1.0"


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


#define NOT_A_DATE_YEAR   2000

// Liste des evenements specifique a ce projet


enum tUserEventCode {
  // evenement utilisateurs
  evBP0 = 100,      // low = low power allowed
  evLed0,
  evLowPower,
  evUDPEvent,         // Trame UDP avec un evenement
  // evenement action
  doReset,
};



// instance betaEvent

//  une instance "Events" avec un poussoir "BP0" une LED "Led0" un clavier "Keyboard"
//  MyBP0 genere un evenement evBP0 a chaque pression le poussoir connecté sur pinBP0
//  Led0 genere un evenement evLed0 a chaque clignotement de la led precablée sur la platine
//  Keyboard genere un evenement evChar a char caractere recu et un evenement evString a chaque ligne recue
//  Debug permet sur reception d'un "T" sur l'entrée Serial d'afficher les infos de charge du CPU

//#define DEFAULT_PIN
// les sortie pour la led et le poussoir sont definis dans esp8266.h avec BP0_PIN  et LED0_PIN
#include <BetaEvents.h>

// littleFS
#include <LittleFS.h>  //Include File System Headers 
#define MyLittleFS  LittleFS


//WiFI
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <Arduino_JSON.h>

bool sleepOk = true;
int  multi = 0; // nombre de clic rapide

// gestion de l'ecran


// rtc memory to keep date
//struct __attribute__((packed))
struct  {
  // all these values are keep in RTC RAM
  uint8_t   crc8;                 // CRC for savedRTCmemory
  time_t    actualTimestamp;      // time stamp restored on next boot Should be update in the loop() with setActualTimestamp
} savedRTCmemory;




// Variable d'application locale
bool     WiFiConnected = false;

time_t   currentTime;
int8_t   timeZone = -2;          //les heures sont toutes en localtimes
bool     configOk = true; // global used by getConfig...
String   messageUUID;
String   message;

#include <WiFiUdp.h>
// port d'ecoute UDP
const unsigned int localUdpPort = 23423;      // local port to listen on
//Objet UDP pour la liaison avec la console
WiFiUDP MyUDP;


void setup() {
  enableWiFiAtBootTime();   // obligatoire pour lekernel ESP > 3.0
  Serial.begin(115200);
  Serial1.begin(2400, SERIAL_7E1);
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

  message.reserve(200);
  messageUUID.reserve(16);

  Serial.println(F("Bonjour ...."));


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


  // recuperation de la timezone dans la config
  timeZone = jobGetConfigInt(F("timezone"));
  if (!configOk) {
    timeZone = -2; // par defaut France hivers
    jobSetConfigInt(F("timezone"), timeZone);
    Serial.println(F("!!! timezone !!!"));
  }
    timeZone = -2; // par defaut France hivers
  D_println(timeZone);

  message = F("Bonjour ...   ");
  message += niceDisplayTime(currentTime, true);
  betaBriteWrite(message);


  D_println(Events.freeRam());
}

String niceDisplayTime(const time_t time, bool full = false);

void loop() {
  Events.get(sleepOk);
  Events.handle();
  switch (Events.code)
  {
    case evNill:
      handleUdpPacket();        // handle UDP connection other betaporte
      break;


    case evInit:
      Serial.println("Init");
      break;


    case ev24H:
      Serial.println("---- 24H ---");
      break;


    case ev10Hz: {


      }
      break;

    case ev1Hz: {

        // check for connection to local WiFi  1 fois par seconde c'est suffisant
        static uint8_t oldWiFiStatus = 99;
        uint8_t  WiFiStatus = WiFi.status();
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
          static bool wasConnected = WiFiConnected;
          if (wasConnected != WiFiConnected) {
            wasConnected = WiFiConnected;
            Led0.setFrequence(WiFiConnected ? 1 : 2);
            if (WiFiConnected) {
              setSyncProvider(getWebTime);
              setSyncInterval(6 * 3600);
              // lisen UDP 23423
              Serial.println("Listen broadcast");
              MyUDP.begin(localUdpPort);
              message = F("Wifi ok   ");
              message += niceDisplayTime(currentTime, true);
              betaBriteWrite(message);
            }
            D_println(WiFiConnected);
          }
        }

        // Save current time in RTC memory (not erased by a reset)
        currentTime = now();
        savedRTCmemory.actualTimestamp = currentTime;  // save time in RTC memory
        saveRTCmemory();
        static uint8_t lastMinute = minute();
        if (lastMinute != minute()) {
          lastMinute = minute();

         
             
          String aMessage = niceDisplayTime(currentTime, true);
          aMessage = aMessage.substring(0,aMessage.length()-3);
          betaBriteWrite(aMessage);
          jobBroadcastMessage("");
        }

        // If we are not connected we warn the user every 30 seconds that we need to update credential
        if ( !WiFiConnected && second() % 30 ==  15) {
          // every 30 sec
          Serial.print(F("module non connecté au Wifi local "));
          D_println(WiFi.SSID());
          Serial.println(F("taper WIFI= pour configurer le Wifi"));
        }

      }

      break;


    // Arrivee d'un badge via trame distante (UUID dans messageUUID)
    case evUDPEvent: {

        D_println(messageUUID);
        message = "event ";
        message += messageUUID;
        message += "          ";

        betaBriteWrite(message);

      }
      break;




    case doReset:
      Events.reset();
      break;


    case evInChar: {
        if (Debug.trackTime < 2) {
          char aChar = Keyboard.inputChar;
          //          if (isPrintable(aChar)) {
          //            D_println(aChar);
          //          } else {
          //            D_println(int(aChar));
          //          }
        }
        switch (Keyboard.inputChar)
        {
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
          D_println(result);
        }

      }

      if (Keyboard.inputString.equals(F("RESET"))) {
        Serial.println(F("RESET"));
        Events.push(doReset);
      }
      if (Keyboard.inputString.equals(F("FREE"))) {
        D_println(Events.freeRam());
      }
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


  }
}


// helpers


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
  if ( year(time) < NOT_A_DATE_YEAR ) {
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
#define  RTC_DATA(x) (uint32_t*)&x,sizeof(x)

bool saveRTCmemory() {
  setCrc8(&savedRTCmemory.crc8 + 1, sizeof(savedRTCmemory) - 1, savedRTCmemory.crc8);
  //system_rtc_mem_read(64, &savedRTCmemory, sizeof(savedRTCmemory));
  return ESP.rtcUserMemoryWrite(0, RTC_DATA(savedRTCmemory) );
}


bool getRTCMemory() {
  ESP.rtcUserMemoryRead(0, RTC_DATA(savedRTCmemory));
  //Serial.print("CRC1="); Serial.println(getCrc8( (uint8_t*)&savedRTCmemory,sizeof(savedRTCmemory) ));
  return ( setCrc8( &savedRTCmemory.crc8 + 1, sizeof(savedRTCmemory) - 1, savedRTCmemory.crc8 ) );
}

/////////////////////////////////////////////////////////////////////////
//  crc 8 tool
// https://www.nongnu.org/avr-libc/user-manual/group__util__crc.html


//__attribute__((always_inline))
inline uint8_t _crc8_ccitt_update  (uint8_t crc, const uint8_t inData)   {
  uint8_t   i;
  crc ^= inData;

  for ( i = 0; i < 8; i++ ) {
    if (( crc & 0x80 ) != 0 ) {
      crc <<= 1;
      crc ^= 0x07;
    } else {
      crc <<= 1;
    }
  }
  return crc;
}

bool  setCrc8(const void* data, const uint16_t size, uint8_t &refCrc ) {
  uint8_t* dataPtr = (uint8_t*)data;
  uint8_t crc = 0xFF;
  for (uint8_t i = 0; i < size; i++) crc = _crc8_ccitt_update(crc, *(dataPtr++));
  //Serial.print("CRC "); Serial.print(refCrc); Serial.print(" / "); Serial.println(crc);
  bool result = (crc == refCrc);
  refCrc = crc;
  return result;
}
