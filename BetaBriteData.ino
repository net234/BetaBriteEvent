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


String grabFromStringUntil(String & aString, const char aKey) {
  String result;
  int pos = aString.indexOf(aKey);
  if ( pos == -1 ) {
    result = aString;
    aString = "";
    return (result);  // not match
  }
  result = aString.substring(0, pos);
  //aString = aString.substring(pos + aKey.length());
  aString = aString.substring(pos + 1);
  return (result);
}



const  IPAddress broadcastIP(255, 255, 255, 255);

bool jobBroadcastMessage(const String & aMessage) {
  Serial.println("Send broadcast ");
  String message = F("event\tbetaBrite\talive");

  message += '\n';

  if ( !MyUDP.beginPacket(broadcastIP, localUdpPort) ) return false;
  MyUDP.write(message.c_str(), message.length());

  MyUDP.endPacket();

  delay(100);

  if ( !MyUDP.beginPacket(broadcastIP, localUdpPort) ) return false;
  MyUDP.write(message.c_str(), message.length());

  MyUDP.endPacket();

  delay(100);
  if ( !MyUDP.beginPacket(broadcastIP, localUdpPort) ) return false;
  MyUDP.write(message.c_str(), message.length());

  MyUDP.endPacket();

  Serial.print(message);
  return true;
}


void handleUdpPacket() {
  int packetSize = MyUDP.parsePacket();
  if (packetSize) {
    Serial.print("Received packet UDP");
    Serial.printf("Received packet of size %d from %s:%d\n    (to %s:%d, free heap = %d B)\n",
                  packetSize,
                  MyUDP.remoteIP().toString().c_str(), MyUDP.remotePort(),
                  MyUDP.destinationIP().toString().c_str(), MyUDP.localPort(),
                  ESP.getFreeHeap());
    const int UDP_MAX_SIZE = 200;  // we handle short messages
    char udpPacketBuffer[UDP_MAX_SIZE + 1]; //buffer to hold incoming packet,
    int size = MyUDP.read(udpPacketBuffer, UDP_MAX_SIZE);

    // read the packet into packetBufffer
    if (packetSize > UDP_MAX_SIZE) {
      Serial.printf("UDP too big ");
      return;
    }

    //TODO: clean this   cleanup line feed
    if (size > 0 && udpPacketBuffer[size - 1] == '\n') size--;

    udpPacketBuffer[size] = 0;

    String aStr = udpPacketBuffer;
    D_println(aStr);

    // Broadcast
    if  ( MyUDP.destinationIP() == broadcastIP ) {
      // it is a reception broadcast
      String bStr = grabFromStringUntil(aStr, '\t');
      //aStr => 'cardreader  BetaPorte_2B  cardid  1626D989  user  Pierre H'

      if ( bStr.equals(F("cardreader")) ) {
        messageUUID = grabFromStringUntil(aStr, '\t'); // door
        bStr = grabFromStringUntil(aStr, '\t'); // 'cardid'
        bStr = grabFromStringUntil(aStr, '\t');  // cardid (value)
        bStr = grabFromStringUntil(aStr, '\t');  // 'user'
        messageUUID += " : ";
        messageUUID += aStr;
        D_println(messageUUID);
        Events.delayedPush(1000, evUDPEvent);

        return;
      }
    }


  }
}

void betaBriteWrite(const String & aMessage) {
  Serial.println(F("Message BetaBrite"));
  Serial.println(aMessage);
  for (int N = 0;  N < 8; N++) Serial1.write(0); // setup speed (mandatory)
  Serial1.print(F("\x01""Z00"));  // all display (mandatory)
  Serial1.print(F("\x02""AA"));   // write to A file (mandatory)
  Serial1.print(F("\x1b"" a"));   // mode defilant
  Serial1.print(F("\x1c""3"));    // couleur ambre
  Serial1.print(aMessage);
  //Serial1.print(F("\x1c""1"));    // couleur ambre
  //Serial1.print(number++);
  Serial1.print(F("          "));

  //Serial1.print("\x03");  (only for check sum)
  Serial1.print(F("\x04"));  // fin de transmission

}
