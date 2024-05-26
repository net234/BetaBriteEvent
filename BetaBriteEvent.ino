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
    V2.0  (19/05/2024)
    -passage en mode bNode V2.0
     V2.1  (26/05/2024)
    - envois des donnees bNode vers un broker MQTT (PicoMQTT)
*************************************************/

// Definition des constantes pour les IO
#include "ESP8266.h"
//static_assert(sizeof(time_t) == 8, "This version works with time_t 64bit  move to ESP8266 kernel 3.0 or more");

#define APP_NAME "BetaBriteEvent V2.1"


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


//#define NOT_A_DATE_YEAR 2000

// Liste des evenements specifique a ce projet


enum tUserEventCode {
  // evenement utilisateurs
  evBP0 = 100,  // low = low power allowed
  evLed0,
  //evUdp,  // Trame UDP avec un evenement
  evCheckWWW,
  evCheckAPI,
  evNewStatus,
  evEraseUdp,
  evLcdRefresh,
  evHttp,  //demande d'api local
  evMqtt,
  // evenement action
  doReset,
};

#define checkWWW_DELAY (5 * 60 * 1000L)  // toute les 5 minutes
#define checkAPI_DELAY (2 * 60 * 1000L)  // toute les 2 minutes

//#define HTTP_API "http://nimux.frdev.com/net234/api.php"  // URI du serveur api local


/// bHub.h  installe :
//  - une instance "Events" pour gere les evenements
// poussoir "BP0"
// une LED "Led0"
// un clavier "Keyboard"
// un debugger "Debug"
//  MyBP0 genere un evenement evBP0 a chaque pression le poussoir connecté sur D2
//  Led0 genere un evenement evLed0 a chaque clignotement de la led precablée sur la platine
//  Keyboard genere un evenement evChar a char caractere recu et un evenement evString a chaque ligne recue
//  MyDebug permet sur reception d'un "T" sur l'entrée Serial d'afficher les infos de charge du CPU
// bHub.h will create and initiate following global instances
//  Events:  eventsManager
//  bHub:    evHandlerbNode
//  Wifi:    standard Wifi ESP
//  bNodeFS: alias of LittleFS

#define DEBUG_ON
#include "ESP8266.H"  // assignation des pin a l'aide constant #define type XXXXXX_PIN
#include <bHub.h>

// gestionaire MQTT
#include "evHandlerMqtt.h"
evHandlerMqtt MQTT(evMqtt, "mqtt.beta");



//  Info I2C

#define LCD_I2CADR 0x4E / 2  //adresse LCD

#include <Wire.h>  // Gestion I2C

// Instance LCD
#include "LiquidCrystal_PCF8574.h"
LiquidCrystal_PCF8574 lcd(LCD_I2CADR);  // set the LCD address






String messageUDP;  //trame UDP
String openDoors;
bool postInit = false;

//String   message;

bool WWWOk = false;
bool APIOk = false;
//int      currentMonth = -1;
bool sleepOk = true;

String displayText;
String lcdMessage;
String mailSendTo;
// gestion de l'ecran

bool lcdOk = false;




void setup() {
  Events.begin();
  Serial.println(F("\r\n\n" APP_NAME));

  Serial1.begin(2400, SERIAL_7E1);  // afficheur betabrite sur serial1 TX (D4)



  //  message.reserve(200);
  messageUDP.reserve(100);
  openDoors.reserve(100);
  displayText.reserve(200);
  lcdMessage.reserve(200);

  //  Serial.println(F("Bonjour ...."));



  bool configErr = false;
  // recuperation de la config dans config.json
  String nodeName = jobGetConfigStr(F("nodename"));
  if (nodeName == "") {
    Serial.println(F("!!! Configurer le nom de la device avec 'NODE=nodename' !!!"));
    configErr = true;
    nodeName = "bNode_";
    nodeName += WiFi.macAddress().substring(12, 14);
    nodeName += WiFi.macAddress().substring(15, 17);
  }
  bHub.nodeName = nodeName;
  DV_println(nodeName);

  // recuperation de la timezone dans la config
  bHub.timeZone = jobGetConfigInt(F("timezone"));
  if (!bHub.configOk) {
    bHub.timeZone = 0;  // par defaut France hivers
    //jobSetConfigInt(F("timezone"),   bHub.timeZone);
    Serial.println(F("!!! timezone !!!"));
  }
  DV_println(bHub.timeZone);

  //Init I2C
  Wire.begin(I2C_SDA, I2C_SCL);

  //  Check LCD
  if (!checkI2C(LCD_I2CADR)) {
    Serial.println(F("No LCD detected"));
  }
  mailSendTo = jobGetConfigStr(F("mailto"));
  if (mailSendTo == "") {
    Serial.println(F("!!! Configurer l'adresse pour le mail  'MAILTO=monAdresseMail' !!!"));
    configErr = true;
  }
  DV_println(mailSendTo);
  // Init LCD

  lcd.begin(20, 4);  // initialize the lcd
  lcd.setBacklight(100);
  lcd.println(F(APP_NAME));
  lcd.println(F("Bonjour"));




  String message = F("Bonjour ...   ");
  message += niceDisplayTime(bHub.currentTime, true);
  betaBriteWrite(message);

  Serial.println("Bonjour ....");
  Serial.println("Tapez '?' pour avoir la liste des commandes");

  DV_println(helperFreeRam());
}

//String niceDisplayTime(const time_t time, bool full = false);

void loop() {

  Events.get(sleepOk);
  Events.handle();
  switch (Events.code) {
    case evInit:
      {
        Serial.println("Init");
        String aStr = bHub.nodeName;

        aStr += F(" " APP_NAME);

        writeHisto((bHub.coldBoot) ? F("ColdBoot") : F("Boot"), aStr);

        jobUpdateLed0();
        //retarde le post init de 10 seconde pour obtenis un check WWWW et un Check API avant l'affichage

        Events.delayedPushSeconds(5, evCheckAPI);
        Events.delayedPushSeconds(7, evCheckWWW);
        Events.delayedPushSeconds(10, evPostInit);
      }
      break;



    case evPostInit:
      postInit = true;

      //bHubUdp.broadcastInfo("InitDone");
      T_println("PostInit done");


      Events.push(evNewStatus);
      jobUpdateLed0();
      break;

    case evTimeMasterGrab:
    case evTimeMasterSyncr:
      jobUpdateLed0();  //synchro de la led de vie

      break;




    case ev24H:
      {
        String newDateTime = niceDisplayTime(bHub.currentTime, true);
        DV_println(newDateTime);
      }
      break;

    case ev1Hz:
      {
        // check lcd
        if (lcdOk != checkI2C(LCD_I2CADR)) {
          lcdOk = !lcdOk;
          DV_println(lcdOk);
        }
        if (lcdOk and postInit) {
          lcd.setCursor(0, 0);
          lcd.println(APP_NAME);
          String aStr = niceDisplayTime(bHub.currentTime, true);
          lcd.println(aStr);
        }


        //rafraichissement de l'affichage toute les minutes (affichage HH:MM)
        static uint8_t lastMinute = minute();
        if (lastMinute != minute()) {
          lastMinute = minute();
          Events.push(evNewStatus);
        }

        // If we are not connected we warn the user every 30 seconds that we need to update credential
        if (!bHub.connected && second() % 30 == 15) {
          // every 30 sec
          Serial.print(F("module non connecté au Wifi local "));
          DV_println(WiFi.SSID());
          Serial.println(F("taper WIFI= pour configurer le Wifi"));
        }
      }

      break;
      /*
        case evUdp:
          {
            if (Events.ext == evxUdpRxMessage) {
              DTV_print("from", myUdp.rxFrom);
              DTV_println("got an Event UDP", myUdp.rxJson);


              if (lcdOk) {
                lcd.setCursor(0, 2);
                //lcd.print(myUdp.rxHeader);
                //lcd.print(' ');
                lcd.println(myUdp.rxFrom);
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
                DV_println(temperatures);
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
                  DV_println(aCmd);
                  if (aCmd.startsWith(F("NODE=")) and !nodeName.equals(dest)) break;  // NODE= not allowed on aliases
                  if (aCmd.length()) Keyboard.setInputString(aCmd);
                } else {
                  DTV_println("CMD not for me.", dest);
                }
              }


              String action = (const char*)jsonData["action"];
              DV_println(action);
              if (action.equals(F("badge"))) {
                //eceived packet UDPmyUdp.rxHeader => 'EVENT', myUdp.rxNode => 'Betaporte_2B', myUdp.rxJson => '{"action":"badge","userid":"Test_5"}'

                String aStr = myUdp.rxFrom;

                aStr += " : ";
                aStr += (const char*)jsonData["userid"];  //
                DV_println(aStr);

                if (messageUDP.indexOf(aStr) < 0) {
                  messageUDP += "    ";
                  messageUDP += aStr;
                }
                Events.delayedPushMilli(500, evNewStatus);
                Events.delayedPushMilli(5 * 60 * 1000, evEraseUdp);
                return;
              }



              if (action.equals(F("porte"))) {
                //Received packet UDPmyUdp.bcast => '1', myUdp.rxHeader => 'EVENT', myUdp.rxNode => 'Betaporte_2', myUdp.rxJson => '{"action":"porte","close":false}'
                bool closed = (bool)jsonData["close"];
                String aStr = myUdp.rxFrom + ' ';
                if (closed) {
                  openDoors.replace(aStr, "");
                  //openDoors.trim();
                } else {
                  if (openDoors.indexOf(aStr) < 0) {

                    openDoors += aStr;
                  }
                }

                Events.delayedPushMilli(500, evNewStatus);
                //Events.delayedPushMilli(3 * 60 * 1000, evEraseUdp);
                return;
              }
            }
          }
          break;
    */
    case evUdp:
      if (Events.ext == evxUdpRxMessage) {
        DTV_print("from", bHubUdp.rxFrom);
        DTV_println("got an Event UDP (2)", bHubUdp.rxJson);
        JSONVar rxJson = JSON.parse(bHubUdp.rxJson);

        //11:28:34.247 -> "UDP" => 'BetaporteHall', "DATA" => '{"action":"porte","close":true}'
        //11:28:11.712 -> "UDP" => 'BetaporteHall', "DATA" => '{"action":"porte","close":false}'
        //11:28:02.677 -> "UDP" => 'bNode03', "DATA" => '{"temperature":{"hallFond":12.06}}'
        //11:27:49.495 -> "UDP" => 'BetaporteHall', "DATA" => '{"action":"badge","userid":"Pierre H."}'
        //11:27:00.218 -> "UDP" => 'bLed256B', "DATA" => '{"event":"evMasterSyncr"}'
        // Received from BetaporteHall (10.11.12.52) TRAME1 245 : {"action":"badge","userid":"Pierre H."} Got 1 trames !!!
        // Received from BetaporteHall (10.11.12.52) TRAME1 246 : {"action":"porte","close":false}


        /*
        //CMD
        //./bNodeCmd.pl bNode FREE -n
        //bNodeCmd.pl  V1.4
        //broadcast:BETA82	net234	{"CMD":{"bNode":"FREE"}}
        rxJson2 = rxJson["CMD"];
        if (JSON.typeof(rxJson2).equals("object")) {
          String dest = rxJson2.keys()[0];  //<nodename called>
          // Les CMD acceptée doivent etre adressé a ce module
          if (dest.equals("ALL") or (dest.length() > 3 and nodeName.startsWith(dest))) {
            String aCmd = rxJson2[dest];
            aCmd.trim();
            DV_println(aCmd);
            if (aCmd.startsWith("NODE=") and !nodeName.equals(dest)) break;  // NODE= not allowed on aliases
            if (aCmd.length()) Keyboard.setInputString(aCmd);
          } else {
            DTV_println("CMD not for me.", dest);
          }
          break;
        }

        */

        // temperature
        JSONVar rxJson2 = rxJson["temperature"];
        if (JSON.typeof(rxJson2).equals("object")) {
          String aName = rxJson2.keys()[0];
          //DV_println(aName);
          double aValue = rxJson2[aName];
          //DV_println(aValue);
          //bHub.meshDevices[bHubUdp.rxFrom]["temperature"][aName] = aValue;
          DTV_println("grab temperature", aValue);

          MQTT.publish("beta/temperature/" + aName, String(aValue));
          break;
        }

        //{"switch":{"FLASH":0}}
        // temperature
        rxJson2 = rxJson["switch"];
        if (JSON.typeof(rxJson2).equals("object")) {
          String aName = rxJson2.keys()[0];
          //DV_println(aName);
          double aValue = rxJson2[aName];
          //DV_println(aValue);
          bHub.meshDevices[bHubUdp.rxFrom]["switch"][aName] = aValue;
          DTV_println("grab switch", aValue);
          MQTT.publish("beta/temperature/" + aName, String(aValue));
          break;
        }

        String action = (const char*)rxJson["action"];
        DV_println(action);
        if (action.equals(F("badge"))) {
          //eceived packet UDPmyUdp.rxHeader => 'EVENT', myUdp.rxNode => 'Betaporte_2B', myUdp.rxJson => '{"action":"badge","userid":"Test_5"}'
          String aStr = F("beta/badge/");
          aStr += bHubUdp.rxFrom;
          //aStr += "/badge";
          MQTT.publish(aStr, rxJson["userid"]);
          aStr = bHubUdp.rxFrom;

          aStr += " : ";
          aStr += (const char*)rxJson["userid"];  //
          DV_println(aStr);

          if (messageUDP.indexOf(aStr) < 0) {
            messageUDP += "    ";
            messageUDP += aStr;
          }
          Events.delayedPushMillis(500, evNewStatus);
          Events.delayedPushMillis(5 * 60 * 1000, evEraseUdp);
          return;
        }



        if (action.equals(F("porte"))) {
          //Received packet UDPmyUdp.bcast => '1', myUdp.rxHeader => 'EVENT', myUdp.rxNode => 'Betaporte_2', myUdp.rxJson => '{"action":"porte","close":false}'
          bool closed = (bool)rxJson["close"];
          String aStr = F("beta/porte/");
          aStr += bHubUdp.rxFrom;
          //aStr += "/etat/";
          MQTT.publish(aStr,  (closed)?"close":"open");
           aStr = bHubUdp.rxFrom + ' ';
          if (closed) {
            openDoors.replace(aStr, "");
            //openDoors.trim();
          } else {
            if (openDoors.indexOf(aStr) < 0) {

              openDoors += aStr;
            }
          }

          Events.delayedPushMillis(500, evNewStatus);
          //Events.delayedPushMilli(3 * 60 * 1000, evEraseUdp);
          return;
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
        aMessage += niceDisplayTime(bHub.currentTime, true);
        aMessage = aMessage.substring(0, aMessage.length() - 3);

        if (bHub.connected && WWWOk && APIOk) {
          aMessage += F("\x1c"
                        "2"
                        " Infra Ok    ");
        } else {
          if (!bHub.connected) aMessage += F("\x1c"
                                             "1"
                                             " WIFI Err ");
          if (!WWWOk) aMessage += F("\x1c"
                                    "1"
                                    " WWW Err ");
          if (!APIOk) aMessage += F("\x1c"
                                    "1"
                                    " API Err ");
        }
        // recuperation de toute les temperature
        //liste les capteurs du type 'temperature'    api.Json?temperature listera les capteurs temperature
        String temperatures = "";
        int tempCnt = 0;
        if (bHub.localDevices.hasOwnProperty("temperature")) {

          JSONVar aJson = bHub.localDevices["temperature"];
          JSONVar keys = aJson.keys();
          for (int i = 0; i < keys.length(); i++) {
            temperatures += (String)keys[i];
            temperatures += ':';
            temperatures += String((double)aJson[keys[i]], 1);
            temperatures += "  ";
            tempCnt++;
          }
        }
        // recherche dans le mesh
        JSONVar key0 = bHub.meshDevices.keys();
        for (int i = 0; i < key0.length(); i++) {
          String aKey0 = key0[i];
          DV_println(aKey0);
          JSONVar aJson = bHub.meshDevices[aKey0]["temperature"];
          JSONVar keys = aJson.keys();
          for (int i = 0; i < keys.length(); i++) {
            temperatures += (String)keys[i];
            temperatures += ':';
            temperatures += String((double)aJson[keys[i]], 1);
            temperatures += "  ";
            tempCnt++;
          }
        }

        if (tempCnt) {
          aMessage += F(("\x1c"
                         "2"
                         "   Temperature"));
          if (tempCnt > 1) aMessage += 's';
          aMessage += ' ';
          aMessage += temperatures;
        }
        if (displayText.length()) {
          aMessage += F("\x1c"
                        "8");  //Yellow
          aMessage += displayText;

          aMessage += "  ";
        }

        if (postInit) {
          betaBriteWrite(aMessage);
          if (bHub.connected) bHubUdp.broadcastInfo(F("TXT=") + lcdMessage);
          MQTT.publish("beta/domo/message", lcdMessage);
        }
        if (lcdOk) Events.delayedPushMillis(300, evLcdRefresh, 0);
      }
      break;

    case evLcdRefresh:
      {
        if (!lcdOk) break;

        uint displayStep = Events.ext;
        //DV_print(displayStep);
        lcd.setCursor(0, 2);
        // lcd.print(LCD_CLREOL "\r\n" LCD_CLREOL);
        // lcd.setCursor(0, 2);
        String aStr = lcdMessage;
        //aStr += F("                                            ");
        aStr = aStr.substring(displayStep, displayStep + 39);
        lcd.print(aStr);
        lcd.print(LCD_CLREOL);


        displayStep++;

        if (displayStep > lcdMessage.length() - 10) displayStep = 0;
        Events.delayedPushMillis(500, evLcdRefresh, displayStep);
      }
      break;


    case evEraseUdp:
      {
        messageUDP = "";
      }
      break;

    case evCheckWWW:
      Serial.println("evCheckWWW");
      if (bHub.connected) {
        if (WWWOk != checkFAI()) {
          WWWOk = !WWWOk;
          DV_println(WWWOk);
          writeHisto(WWWOk ? F("WWW Ok") : F("WWW Err"), "checkFAI");
          jobUpdateLed0();
          if (WWWOk and postInit) {
            Serial.println("send a mail");
            bool sendOk = sendHistoTo(mailSendTo);
            if (sendOk) {
              //eraseHisto();
              writeHisto(F("Mail send ok"), mailSendTo);
            } else {
              writeHisto(F("Mail erreur"), mailSendTo);
            }
          }
        }
        Events.delayedPushMillis(checkWWW_DELAY, evCheckWWW);
      }
      break;


    case evCheckAPI:
      {
        Serial.println("evCheckAPI");
        if (!bHub.connected) break;
        JSONVar jsonData = bHub.localDevices;
        jsonData["timeZone"] = bHub.timeZone;
        jsonData["timestamp"] = (double)bHub.currentTime;

        String jsonStr = JSON.stringify(jsonData);
        if (APIOk != dialWithPHP(bHub.nodeName, "timezone", jsonStr)) {
          APIOk = !APIOk;
          DV_println(APIOk);
          writeHisto(APIOk ? F("API Ok") : F("API Err"), jobGetConfigStr("API"));
          jobUpdateLed0();
        }
        if (APIOk) {
          jsonData = JSON.parse(jsonStr);
          time_t aTimeZone = (double)jsonData["timezone"];
          DV_println(aTimeZone);
          if (bHub.timeZone != aTimeZone) {
            String aStr = String(bHub.timeZone);
            aStr += " -> ";
            aStr += String(aTimeZone);
            writeHisto(F("Wrong TimeZone"), aStr);
          }
        }
        Events.delayedPushMillis(checkAPI_DELAY, evCheckAPI);
      }
      break;

    case doReset:
      helperReset();
      break;


    case evInChar:
      {
        if (Debug.trackTime < 2) {
          //char aChar = Keyboard.inputChar;
          //          if (isPrintable(aChar)) {
          //            DV_println(aChar);
          //          } else {
          //            DV_println(int(aChar));
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
        DV_println(Keyboard.inputString);
      }

      if (Keyboard.inputString.startsWith(F("NODE="))) {
        Serial.println(F("SETUP NODENAME : 'NODE=nodename'  ( this will reset)"));
        String aStr = Keyboard.inputString;
        grabFromStringUntil(aStr, '=');
        aStr.replace(" ", "_");
        aStr.trim();

        if (aStr != "") {
          bHub.nodeName = aStr;
          DV_println(bHub.nodeName);
          jobSetConfigStr(F("nodename"), bHub.nodeName);
          delay(1000);
          helperReset();
        }
      }

      if (Keyboard.inputString.startsWith(F("DSP="))) {
        Serial.println(F("DISPLAY : 'DSP= atext "));
        String aStr = Keyboard.inputString;
        grabFromStringUntil(aStr, '=');
        aStr.trim();
        DV_println(aStr);
        displayText = aStr;
        Events.push(evNewStatus);
      }


      if (Keyboard.inputString.startsWith(F("WIFI="))) {
        Serial.println(F("SETUP WIFI : 'WIFI=WifiName,password"));
        String aStr = Keyboard.inputString;
        grabFromStringUntil(aStr, '=');
        String ssid = grabFromStringUntil(aStr, ',');
        ssid.trim();
        DV_println(ssid);
        if (ssid != "") {
          String pass = aStr;
          pass.trim();
          DV_println(pass);
          bool result = WiFi.begin(ssid, pass);
          Serial.print(F("WiFi begin "));
          V_println(result);
        }
      }

      if (Keyboard.inputString.equals(F("RESET"))) {
        Serial.println(F("RESET"));
        Events.push(doReset);
      }
      if (Keyboard.inputString.equals(F("FREE"))) {
        DV_println(helperFreeRam());
        String aStr = F("FREE=");
        aStr += String(helperFreeRam());
        aStr += F(",APP=");
        aStr += String(F(APP_NAME));
        bHubUdp.broadcastInfo(aStr);
      }
      if (Keyboard.inputString.equals(F("INFO"))) {
        String aStr = F(" CPU=");
        aStr += String(Events.getPercentCPU());
        bHubUdp.broadcastInfo(aStr);
        DV_print(aStr)
      }
      if (Keyboard.inputString.equals("OTA")) {
        Events.push(evOta, evxOn);
        T_println("Start OTA");
      }


      if (Keyboard.inputString.equals("S")) {
        sleepOk = !sleepOk;
        DV_println(sleepOk);
      }
      if (Keyboard.inputString.equals(F("RAZCONF"))) {
        Serial.println(F("RAZCONF this will reset"));
        eraseConfig();
        delay(1000);
        helperReset();
      }
      if (Keyboard.inputString.equals(F("HIST"))) {
        DT_println("HIST");

        printHisto();
      }
      if (Keyboard.inputString.equals(F("CONF"))) {
        jobShowConfig(true);
      }
      if (Keyboard.inputString.equals(F("CONFSHOW"))) {
        jobShowConfig(true);
      }
      if (Keyboard.inputString.equals(F("ERASEHISTO"))) {
        eraseHisto();
      }
      if (Keyboard.inputString.startsWith(F("SETAPI="))) {
        Serial.println(F("SETUP api key : 'SETAPI=api uri (api.frdev.com)'"));
        String aStr = Keyboard.inputString;
        grabFromStringUntil(aStr, '=');
        aStr.trim();
        jobSetConfigStr(F("API"), aStr);
      }
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
  helperReset();
}


void beep(const uint16_t frequence, const uint16_t duree) {
  tone(BEEP_PIN, frequence, duree);
}


/*
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
*/
