/*************************************************
 *************************************************
    Sketch BetaPorteData.ino   // gestion des liste de badge, plage horaire pour betaporte

    les données badges  sont stockée en flash sous forme de fichier : 1 ligne de JSON stringifyed par badges.json
    les donnees config sont stockée en flash sous forme de fichier : 1 ligne unique de JSON stringifyed  config.json

    Copyright 20201 Pierre HENRY net23@frdev.com .
    https://github.com/betamachine-org/BetaPorteV2   net23@frdev.com

  This file is part of BetaPorteV2.

    BetaPorteV2 is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    BetaPorteV2 is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with betaEvents.  If not, see <https://www.gnu.org/licenses/lglp.txt>.



*************************************************/

#define CONFIG_FNAME F("/config.json")


//get a value of a config key
String jobGetConfigStr(const String aKey) {
  String result = "";
  File aFile = MyLittleFS.open(CONFIG_FNAME, "r");
  if (!aFile) return (result);

  JSONVar jsonConfig = JSON.parse(aFile.readStringUntil('\n'));
  aFile.close();
  if (JSON.typeof(jsonConfig[aKey]) == F("string") ) result = (const char*)jsonConfig[aKey];

  return (result);
}


// set a value of a config key
//todo : check if config is realy write ?
bool jobSetConfigStr(const String aKey, const String aValue) {
  // read current config
  JSONVar jsonConfig;  // empry config
  File aFile = MyLittleFS.open(CONFIG_FNAME, "r");
  if (aFile) {
    jsonConfig = JSON.parse(aFile.readStringUntil('\n'));
    aFile.close();
  }
  jsonConfig[aKey] = aValue;
  aFile = MyLittleFS.open(CONFIG_FNAME, "w");
  if (!aFile) return (false);
  D_println(JSON.stringify(jsonConfig));
  aFile.println(JSON.stringify(jsonConfig));
  aFile.close();
  return (true);
}

int jobGetConfigInt(const String aKey) {
  int result = 0;
  File aFile = MyLittleFS.open(CONFIG_FNAME, "r");
  if (!aFile) return (result);
  aFile.setTimeout(5);
  JSONVar jsonConfig = JSON.parse(aFile.readStringUntil('\n'));
  aFile.close();

  D_println(JSON.typeof(jsonConfig[aKey]));
  configOk = JSON.typeof(jsonConfig[aKey]) == F("number");
  if (configOk ) result = jsonConfig[aKey];
  return (result);
}

bool jobSetConfigInt(const String aKey, const int aValue) {
  // read current config
  JSONVar jsonConfig;  // empry config
  File aFile = MyLittleFS.open(CONFIG_FNAME, "r");
  if (aFile) {
    aFile.setTimeout(5);
    jsonConfig = JSON.parse(aFile.readStringUntil('\n'));
    aFile.close();
  }
  jsonConfig[aKey] = aValue;
  aFile = MyLittleFS.open(CONFIG_FNAME, "w");
  if (!aFile) return (false);
  D_println(JSON.stringify(jsonConfig));
  aFile.println(JSON.stringify(jsonConfig));
  aFile.close();
  return (true);
}



void eraseConfig() {
  Serial.println(F("Erase config") );
  MyLittleFS.remove(CONFIG_FNAME);
}







void betaBriteWrite(const String & aMessage) {
  Serial.print(F("Message BetaBrite : "));

  if (lcdOk) {
    lcd.clear();
    String aStr = aMessage;
    aStr.replace(F("\x1c""3"), "  ");
    aStr.replace(F("\x1c""2"), "  ");
    aStr.replace(F("\x1c""1"), "  ");
    lcd.println(nodeName);
    lcd.print(aStr);

  }
  D_println(aMessage);
  for (int N = 0;  N < 8; N++) Serial1.write(0); // setup speed (mandatory)
  Serial1.print(F("\x01""Z00"));  // all display (mandatory)
  Serial1.print(F("\x02""AA"));   // write to A file (mandatory)
  Serial1.print(F("\x1b"" a"));   // mode defilant
  Serial1.print(F("\x1c""3"));    // couleur ambre
  Serial1.print(aMessage);
  //Serial1.print(F("\x1c""1"));    // couleur rouge
  //Serial1.print(number++);
  Serial1.print(F("   "));

  //Serial1.print("\x03");  (only for check sum)
  Serial1.print(F("\x04"));  // fin de transmission
  //• 1CH + “1” (31H) = Red
  //• 1CH + “2” (32H) = Green
  //• 1CH + “3” (33H) = Amber
  //• 1CH + “4” (34H) = Dim red
  //• 1CH + “5” (35H) = Dim green
  //• 1CH + “6” (36H) = Brown
  //• 1CH + “7” (37H) = Orange
  //• 1CH + “8” (38H) = Yellow
  //• 1CH + “9” (39H) = Rainbow 1
  //• 1CH + “A” (41H) = Rainbow 2
  //• 1CH + “B” (42H) = Color mix
  //• 1CH + “C” (43H) = Autocolor
}


// make an http get on a specific server and get json answer
#include <ESP8266HTTPClient.h>
// passage du json en string
//

bool dialWithPHP(const String& aNode, const String& aAction,  String& jsonParam) {
  //D_println(helperFreeRam() + 000);
  Serial.print(F("Dial With http as '"));
  Serial.print(aNode);
  Serial.print(':');
  Serial.print(aAction);
  Serial.println('\'');
  //{
  String aUri =  HTTP_API;
  aUri += F("?action=");  // my FAI web server
  aUri += encodeUri(aAction);
  aUri += F("&node=");
  aUri += encodeUri(aNode);;

  // les parametres eventuels sont passées en JSON dans le parametre '&json='
  if (jsonParam.length() > 0) {
    aUri += F("&json=");
    //D_println(JSON.stringify(jsonParam));
    aUri += encodeUri(jsonParam);
  }
  //D_println(helperFreeRam() + 0001);
  jsonParam = "";
  WiFiClient client;
  HTTPClient http;  //Declare an object of class HTTPClient
  D_println(aUri);
  http.begin(client, aUri); //Specify request destination

  int httpCode = http.GET();                                  //Send the request
  /*** HTTP client errors
     #define HTTPCLIENT_DEFAULT_TCP_TIMEOUT (5000)
    #define HTTPC_ERROR_CONNECTION_FAILED   (-1)
    #define HTTPC_ERROR_SEND_HEADER_FAILED  (-2)
    #define HTTPC_ERROR_SEND_PAYLOAD_FAILED (-3)
    #define HTTPC_ERROR_NOT_CONNECTED       (-4)
    #define HTTPC_ERROR_CONNECTION_LOST     (-5)
    #define HTTPC_ERROR_NO_STREAM           (-6)
    #define HTTPC_ERROR_NO_HTTP_SERVER      (-7)
    #define HTTPC_ERROR_TOO_LESS_RAM        (-8)
    #define HTTPC_ERROR_ENCODING            (-9)
    #define HTTPC_ERROR_STREAM_WRITE        (-10)
    #define HTTPC_ERROR_READ_TIMEOUT        (-11)
    ****/
  if (httpCode < 0) {
    Serial.print(F("cant get an answer :( http.GET()="));
    Serial.println(httpCode);
    http.end();   //Close connection
    return (false);
  }
  if (httpCode != 200) {
    Serial.print(F("got an error in http.GET() "));
    D_println(httpCode);
    http.end();   //Close connection
    return (false);
  }


  aUri = http.getString();   //Get the request response payload
  //D_println(helperFreeRam() + 1);
  http.end();   //Close connection (restore 22K of ram)
  D_println(aUri.length());             //Print the response payload
  //D_println(bigString);
  // check json string without real json lib  not realy good but use less memory and faster
  int16_t answerPos = aUri.indexOf(F(",\"answer\":{"));
  if ( !aUri.startsWith(F("{\"status\":true,")) || answerPos < 0 ) {
    return (false);
  }
  // hard cut of "answer":{ xxxxxx } //
  jsonParam = aUri.substring(answerPos + 10, aUri.length() - 1);
  D_println(jsonParam.length());
  return (true);
}

#define Hex2Char(X) (char)((X) + ((X) <= 9 ? '0' : ('A' - 10)))

// encode optimisé pour le json
String encodeUri(const String aUri) {
  String answer = "";
  String specialChar = F(".-~_{}[],;:\"\\");
  // uri encode maison :)
  for (uint N = 0; N < aUri.length(); N++) {
    char aChar = aUri[N];
    //TODO:  should I keep " " to "+" conversion ????  save 2 char but oldy
    if (aChar == ' ') {
      answer += '+';
    } else if ( isAlphaNumeric(aChar) ) {
      answer +=  aChar;
    } else if (specialChar.indexOf(aChar) >= 0) {
      answer +=  aChar;
    } else {
      answer +=  '%';
      answer += Hex2Char( aChar >> 4 );
      answer += Hex2Char( aChar & 0xF);
    } // if alpha
  }
  return (answer);
}
