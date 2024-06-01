/*************************************************
 *************************************************
    lib   evHandlerMqtt.h   implantation de tinyMqtt en mode event pour bNode
    Copyright 2024 Pierre HENRY net23@frdev.com All - betamachine- right reserved.

    This file is part of betaEvents.
    betaEvents is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    betaEvents is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with betaEvents.  If not, see <https://www.gnu.org/licenses/lglp.txt>.

      wraper evenementiel de la lib tinyMqtt pour installer un broker autonome dans un bNode



  History
   
    evHandlerMqtt.h   V1.0 (21/05/2024)
      from bNodes   bNode V2.0H
 *************************************************/
#include "evHandlerMqtt.h"
#include "evHandlerbNodes.h"
#include <PicoMQTT.h>  // https://github.com/mlesniew/PicoMQTT
#define PORT 1883

PicoMQTT::Client mqtt;

String anyTopic = "#";


// void onPublishA(const MqttClient* srce, const Topic& topic, const char* payload, size_t /* length */) {
//   //if (! (srce->id()=="local")  ) {
//   Serial << "--> Client A received msg on topic " << topic.c_str() << ", " << payload << ", " << srce->id() << endl;
//   //}
// }
/*
static String recevedTopic;
static String recevedPayload;


void onPublish(const char *topic, const char *payload) {
  TV_print("MQTT: received", topic);
  V_println(payload);
  if (strcmp("?", payload)) {
    recevedPayload = String(payload);
    recevedTopic = String(topic);
    Events.push(evCode, evxMqReceved);
  }
}
*/
void onMqttReceve(const char *topic, const char *payload);

void evHandlerMqtt::begin() {
  if (!broker.length()) return;
  mqtt.host = broker.c_str();
  mqtt.client_id = bHub.nodeName;
  // mqtt.port = 1883;
  // mqtt.client_id = "esp-" + WiFi.macAddress();
  // mqtt.username = "username";
  // mqtt.password = "secret";


  Serial.print("Broker ready  with ");
  Serial.println(broker);
  // Subscribe to a topic and attach a callback
  mqtt.subscribe(topicPrefix + '#', onMqttReceve);
  // mqtt.subscribe("#", [](const char *topic, const char *payload) {
  //   // payload might be binary, but PicoMQTT guarantees that it's zero-terminated
  //   Serial.printf("MQTT:Received message in topic '%s': %s\n", topic, payload);
  // });
  //recevedTopic.reserve(200);    //MAX 256
  //recevedPayload.reserve(800);  //MAX 1024
  mqtt.begin();
};

void evHandlerMqtt::handle() {
  //Serial.print(".");
  mqtt.loop();
};
bool evHandlerMqtt::publish(const String &topic, const String &payload) {
  String aStr = topicPrefix;
  aStr += topic;
  bool result = mqtt.publish(aStr.c_str(), payload.c_str());
  //#ifndef NO_DEBUG
  aStr += ':';
  aStr += payload;
  TV_println("MQTT: Sending MQTT ", aStr);
  if (!result) T_println("MQTT: erreur");
  //#endif
  return (result);
}
