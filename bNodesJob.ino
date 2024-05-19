/*************************************************
 *************************************************
    jobs for  bHub.ino   check and report by may a box and the wifi connectivity
    Copyright 2021 Pierre HENRY net23@frdev.com All - right reserved.

  This file is part of  bHub.

    bNodes is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    bNodes is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with  bHub.  If not, see <https://www.gnu.org/licenses/lglp.txt>.


  History
    V1.0 (21/10/2021)
    - From betaevent.h + part of betaport V1.2
      to check my box after a problem with sfr :)

  e croquis utilise 334332 octets (32%) de l'espace de stockage de programmes. Le maximum est de 1044464 octets.
  Les variables globales utilisent 28936 octets (35%) de mémoire dynamique, ce qui laisse 52984 octets pour les variables locales. Le maximum est de 81920 octets.

  e croquis utilise 334316 octets (32%) de l'espace de stockage de programmes. Le maximum est de 1044464 octets.
  Les variables globales utilisent 28964 octets (35%) de mémoire dynamique, ce qui laisse 52956 octets pour les variables locales. Le maximum est de 81920 octets.

 *************************************************/

#define HISTO_FNAME F("/histo.json")
#define CONFIG_FNAME F("/config.json")

bool sendHistoTo(const String sendto) {

  Serial.print(F("Send histo to "));
  Serial.println(sendto);

  String smtpServer = jobGetConfigStr(F("smtpserver"));
  String sendFrom = jobGetConfigStr(F("mailfrom"));
  if (smtpServer == "" || sendto == "" || sendFrom == "") {
    Serial.print(F("no mail config"));
    return (false);
  }
  String smtpLogin = jobGetConfigStr(F("smtplogin"));
  String smtpPass = jobGetConfigStr(F("smtppass"));



  WiFiClient tcp;  //Declare an object of Wificlass Client to make a TCP connection
  String aLine;    // to get answer of SMTP
  // Try to find a valid smtp
  if (!tcp.connect(smtpServer, 25)) {
    Serial.print(F("unable to connect with "));
    Serial.print(smtpServer);
    Serial.println(F(":25"));
    return false;
  }


  bool mailOk = false;
  do {
    aLine = tcp.readStringUntil('\n');
    if (aLine.toInt() != 220) break;  //  not good answer
    Serial.println(F("HELO checkmybox"));
    //tcp.print(F("HELO checkmybox \r\n")); // EHLO send too much line
    tcp.println(F("HELO checkmybox"));  // EHLO send too much line
    aLine = tcp.readStringUntil('\n');
    if (aLine.toInt() != 250) break;  //  not good answer
    // autentification
    if (smtpLogin != "") {
      Serial.println(F("AUTH LOGIN"));
      tcp.println(F("AUTH LOGIN"));
      aLine = tcp.readStringUntil('\n');
      if (aLine.toInt() != 334) break;  //  not good answer

      //Serial.println(smtpLogin);
      tcp.println(smtpLogin);
      //tcp.print(F("\r\n"));
      aLine = tcp.readStringUntil('\n');
      if (aLine.toInt() != 334) break;  //  not good answer

      //Serial.println(smtpPass);
      tcp.println(smtpPass);
      //tcp.print(F("\r\n"));
      aLine = tcp.readStringUntil('\n');
      if (aLine.toInt() != 235) break;  //  not good answer
    }

    String aString = F("MAIL FROM: ");
    aString += sendFrom;
    aString.replace(F("NODE"), bHub.nodeName);
    Serial.println(aString);
    tcp.println(aString);
    //tcp.print(F("\r\n"));
    aLine = tcp.readStringUntil('\n');
    if (aLine.toInt() != 250) break;  //  not good answer

    Serial.println("RCPT TO: " + sendto);
    tcp.println("RCPT TO: " + sendto);
    aLine = tcp.readStringUntil('\n');
    if (aLine.toInt() != 250) break;  //  not good answer

    Serial.println(F("DATA"));
    tcp.print(F("DATA\r\n"));
    aLine = tcp.readStringUntil('\n');
    if (aLine.toInt() != 354) break;  //  not goog answer

    //Serial.println( "Mail itself" );
    tcp.print(F("Subject: mail from '"));
    tcp.print(bHub.nodeName);
    tcp.print(F("' " APP_NAME "\r\n"));

    tcp.print(F("\r\n"));  // end of header
    // body
    //    tcp.print("ceci est un mail de test\r\n");
    //    tcp.print("destine a valider la connection\r\n");
    //    tcp.print("au serveur SMTP\r\n");
    //    tcp.print("\r\n");
    tcp.print(F(" == == = histo.json == \r\n"));
    File aFile = bNodeFS.open(HISTO_FNAME, "r");
    if (!aFile) {
      tcp.print(F(" pas de fichier  \r\n"));
    } else {
      aFile.setTimeout(5);
      bool showTime = true;
      while (aFile.available()) {
        String aLine = aFile.readStringUntil('\n');
        //DV_print(aLine);  //'{"teimestamp":xxxxxx"action":"badge ok","info":"0E3F0FFA"}
        JSONVar jsonLine = JSON.parse(aLine);
        time_t aTime = (double)jsonLine["timestamp"];
        uint8_t aPrec = jsonLine["prec"];
        String aAction = (const char*)jsonLine["action"];
        String aInfo = (const char*)jsonLine["info"];
        tcp.print(niceDisplayTime(aTime, showTime, aPrec));
        showTime = false;
        tcp.print('\t');
        tcp.print(aAction);
        tcp.print('\t');
        tcp.print(aInfo);
        tcp.print("\r\n");
      }

      aFile.close();
    }
    tcp.print(F(" == Eof histo == "));
    tcp.print(niceDisplayTime(jobGetHistoTime(), false));
    tcp.print("\r\n");

    // end of body
    tcp.print("\r\n.\r\n");
    aLine = tcp.readStringUntil('\n');
    if (aLine.toInt() != 250) break;  //  not goog answer

    mailOk = true;
    break;
  } while (false);
  DV_println(mailOk);
  DV_println(aLine);
  Serial.println("quit");
  tcp.print("QUIT\r\n");
  aLine = tcp.readStringUntil('\n');
  DV_println(aLine);

  Serial.println(F("Stop TCP connection"));
  tcp.stop();
  return mailOk;
}

/*
void jobGetSondeName() {
  String aStr = jobGetConfigStr(F("sondename"));
  aStr.replace("#", "_");
  for (int N = 0; N < MAXDS18b20; N++) {
    String bStr = grabFromStringUntil(aStr, ',');
    bStr.trim();
    if (bStr.length() == 0) {
      bStr = F("DS18_");
      bStr += String(N + 1);
    }
    sondesName[N] = bStr;
  }
}

void jobGetSwitcheName() {
  String aStr = jobGetConfigStr(F("switchename"));
  aStr.replace("#", "_");
  for (int N = 0; N < switchesNumber; N++) {
    String bStr = grabFromStringUntil(aStr, ',');
    bStr.trim();
    if (bStr.length() == 0) {
      bStr = F("switch_");
      bStr += String(N + 1);
    }
    switchesName[N] = bStr;
  }
}
*/

void jobBcastSwitch(const String& aName, const int aValue) {
  String aTxt = "{\"switch\":{\"";
  aTxt += aName;
  aTxt += "\":";
  aTxt += aValue;
  aTxt += "}}";
  DTV_println("BroadCast", aTxt);
  bHubUdp.broadcast(aTxt);
}
/*
// 100 HZ
void jobRefreshLeds(const uint8_t delta) {
  //Serial.print('.');
  ledFixe1.start();
  ledFixe1.write();
  ledFixe2.write();
  ledFixe3.write();
  for (int8_t N = 0; N < ledsMAX; N++) {
    leds[N].write();
  }

  ledFixe1.stop();  // obligatoire
  ledFixe1.anime(delta);
  ledFixe2.anime(delta);
  ledFixe3.anime(delta);
  for (uint8_t N = 0; N < ledsMAX; N++) {
    leds[N].anime(delta);
  }
}
*/
void jobUpdateLed0() {
  //ledFixe2.setcolor((APIOk) ? rvb_green : rvb_orange, 10, 30 * 1000);
  //ledFixe2.setcolor((WWWOk) ? rvb_green : rvb_orange, 10, 30 * 1000);
  uint cpu = 80 * Events.getPercentCPU() / 100 + 2;
  DV_println(cpu);
  if (BP0.isOn()) {
    Led0.setMillisec(500, cpu);
 //   ledLifeColor = rvb_blue;
 //   ledFixe1.setcolor(ledLifeColor, 50, 100, 100);
    return;
  }
  if (bHub.connected) {
    Led0.setMillisec(5000, cpu);
//    ledLifeColor = (bHub.isTimeMaster) ? rvb_purple : rvb_green;
  //  if (!APIOk) ledLifeColor = rvb_orange;
 //   ledFixe1.setcolor(ledLifeColor, 50, 100, (4000 * cpu) / 100);
    return;
  }
  Led0.setMillisec(1000, cpu);
//  ledLifeColor = rvb_red;
//  ledFixe1.setcolor(ledLifeColor, 50, 100, (1000 * cpu) / 100);
}

#include <ESP8266HTTPClient.h>
#define FAI "www.free.fr"

bool checkFAI() {
  // connect to an internet API to
  // check WWW connection
  // get unix time
  // get timezone
  //http://worldtimeapi.org/api/timezone/Europe/Paris

  String urlServer = F("http://" FAI);
  DV_println(urlServer);
  WiFiClient aWiFI;  // Wificlient doit etre declaré avant HTTPClient
  HTTPClient aHttp;  //Declare an object of class HTTPClient
  aHttp.setTimeout(100);
  aHttp.begin(aWiFI, urlServer);  //Specify request destination

  int httpCode = aHttp.GET();                 //Send the request
  if (httpCode != 200 and httpCode != 301) {  //302
    DTV_println(F("got an error in http.GET() "), httpCode);
    aHttp.end();  //Close connection
    return (false);
  }
  aHttp.end();  //Close connection
  return (true);
}


// get full time with a standard web server
#include <ESP8266HTTPClient.h>
#define LOCATION "Europe/Paris"  // adjust to yout location
#define URI "worldtimeapi.org/api/timezone/"

time_t getFullwwwTime() {
  // connect to an internet API to
  // check WWW connection
  // get unix time
  // get timezone
  //http://worldtimeapi.org/api/timezone/Europe/Paris

  String urlServer = F("http://" URI LOCATION);
  DV_println(urlServer);
  WiFiClient aWiFI;  // Wificlient doit etre declaré avant HTTPClient
  HTTPClient aHttp;  //Declare an object of class HTTPClient
  aHttp.setTimeout(100);
  aHttp.begin(aWiFI, urlServer);  //Specify request destination

  int httpCode = aHttp.GET();  //Send the request
  if (httpCode != 200) {
    DTV_println(F("got an error in http.GET() "), httpCode);
    aHttp.end();  //Close connection
    return (0);
  }

  String answer = aHttp.getString();  //Get the request response payload
  //DV_print(helperFreeRam() + 1);
  aHttp.end();  //Close connection (restore 22K of ram)
  //DV_println(answer);  //Print the response payload
  //  23:02:48.615 -> answer => '{"abbreviation":"CET","client_ip":"82.66.229.100","datetime":"2024-03-15T23:02:48.596866+01:00","day_of_week":5,"day_of_year":75,"dst":false,"dst_from":null,"dst_offset":0,"dst_until":null,"raw_offset":3600,"timezone":"Europe/Paris","unixtime":1710540168,"utc_datetime":"2024-03-15T22:02:48.596866+00:00","utc_offset":"+01:00","week_number":11}'
  if (!answer.startsWith(F("{\"abbreviation\":"))) return (0);
  //DV_println(helperFreeRam());
  JSONVar aJson = JSON.parse(answer);
  //DV_println(helperFreeRam());
  int atimezone = aJson["raw_offset"];
  atimezone += (int)aJson["dst_offset"];
  time_t unixtime = (double)aJson["unixtime"] + atimezone;
  unixtime++;  //we are a little late
  atimezone = atimezone / -3600;
  V_println(atimezone);

  V_println(unixtime);
  bHub.clockDelta = unixtime - bHub.currentTime;
  if (bHub.clockDelta) {
    Events.push(evWifi, evxTimestampChanged, bHub.clockDelta);
  }
  if (bHub.timeZone != atimezone) {
    bHub.timeZone = atimezone;
    Events.push(evWifi, evxTimeZoneChanged, atimezone);
  }

  DV_println(bHub.clockDelta);
  bHub.clockLastTry = unixtime;
  return (unixtime);
}


bool buildApiAnswer(JSONVar& answer, const String& action, const String& value) {
  DTV_print("api call", action);
  DV_println(value);
  if (!action.length()) {
    answer["node"]["app"] = APP_NAME;
    answer["node"]["name"] = bHub.nodeName;
    answer["node"]["date"] = niceDisplayTime(bHub.currentTime, true);
    answer["node"]["booted"] = niceDisplayDelay(bHub.bootedSecondes);
    answer["devices"] = bHub.meshDevices;
    answer["devices"][bHub.nodeName] = bHub.localDevices;
    ServerHttp.sendHeader("refresh", "60", true);
    return true;
  }
  if (action.equals("CMD") and value.length()) {
    Keyboard.setInputString(value);
    answer["cmd"] = value;
    DTV_println("API CMD", value);
    ServerHttp.sendHeader("refresh", "5;url=api.json", true);
    return true;
  }


  //liste les capteurs du type 'action'='name'    api.Json?temperature=interieur listera le PREMIER capteurs temperature nomé interieur
  if (value.length()) {

    if (bHub.localDevices.hasOwnProperty(action) and bHub.localDevices[action].hasOwnProperty(value)) {
      DV_println(bHub.localDevices[action][value]);
      JSONVar aJson = bHub.localDevices[action][value];
      answer[action] = aJson;
      return (true);
    }
    // recherche dans le mesh
    JSONVar keys = bHub.meshDevices.keys();
    for (int i = 0; i < keys.length(); i++) {
      String aKey = keys[i];
      if (bHub.meshDevices[aKey].hasOwnProperty(action) and bHub.meshDevices[aKey][action].hasOwnProperty(value)) {
        JSONVar aJson = bHub.meshDevices[aKey][action][value];
        answer[action] = aJson;
        return (true);
      }
    }
    answer[action] = null;
    return true;
  }


  //liste les capteurs du type 'action'    api.Json?temperature listera les capteurs temperature
  bool matched = false;
  if (bHub.localDevices.hasOwnProperty(action)) {
    DV_println(bHub.localDevices[action]);
    JSONVar aJson = bHub.localDevices[action];
    answer[bHub.nodeName][action] = aJson;
    matched = true;
  }
  // recherche dans le mesh
  JSONVar keys = bHub.meshDevices.keys();
  for (int i = 0; i < keys.length(); i++) {
    String aKey = keys[i];
    DV_println(aKey);
    if (bHub.meshDevices[aKey].hasOwnProperty(action)) {
      JSONVar aJson = bHub.meshDevices[aKey][action];
      answer[aKey][action] = aJson;
      matched = true;
    }
  }
  if (matched) {
    ServerHttp.sendHeader("refresh", "60", true);
    return true;
  }


  return false;
}
