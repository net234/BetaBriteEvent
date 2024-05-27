
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

      //wraper evenementiel de la lib tinyMqtt pour installer un broker autonome dans un bNode
      V1.1   wraper de PicoMQTT avec un broker externe 


  History
   
    evHandlerMqtt.h   V1.0 (21/05/2024)
      from bNodes   bNode V2.0H and https://github.com/hsaturn/TinyMqtt

 *************************************************/


/*

//#include <TinyMqtt.h>  // https://github.com/hsaturn/TinyMqtt
. Variables and constants in RAM (global, static), used 34152 / 80192 bytes (42%)
║   SEGMENT  BYTES    DESCRIPTION
╠══ DATA     1524     initialized variables
╠══ RODATA   4676     constants       
╚══ BSS      27952    zeroed variables
. Instruction RAM (IRAM_ATTR, ICACHE_RAM_ATTR), used 62223 / 65536 bytes (94%)
║   SEGMENT  BYTES    DESCRIPTION
╠══ ICACHE   32768    reserved space for flash instruction cache
╚══ IRAM     29455    code in IRAM    
. Code in flash (default, ICACHE_FLASH_ATTR), used 503620 / 1048576 bytes (48%)
║   SEGMENT  BYTES    DESCRIPTION
╚══ IROM     503620   code in flash   

#include <PicoMQTT.h>  // https://github.com/mlesniew/PicoMQTT (lient)

#define PICOMQTT_MAX_TOPIC_SIZE 256
#define PICOMQTT_MAX_MESSAGE_SIZE 1024

. Variables and constants in RAM (global, static), used 32512 / 80192 bytes (40%)
║   SEGMENT  BYTES    DESCRIPTION
╠══ DATA     1528     initialized variables
╠══ RODATA   3232     constants       
╚══ BSS      27752    zeroed variables
. Instruction RAM (IRAM_ATTR, ICACHE_RAM_ATTR), used 62223 / 65536 bytes (94%)
║   SEGMENT  BYTES    DESCRIPTION
╠══ ICACHE   32768    reserved space for flash instruction cache
╚══ IRAM     29455    code in IRAM    
. Code in flash (default, ICACHE_FLASH_ATTR), used 501088 / 1048576 bytes (47%)
║   SEGMENT  BYTES    DESCRIPTION
╚══ IROM     501088   code in flash   
*/



#pragma once
#include <Arduino.h>



enum tevMqtt : uint8_t { evxMqReceve,
                         evxMqError };

#include <EventsManagerESP.h>
//#define PICOMQTT_MAX_TOPIC_SIZE 256 (default)
//#define PICOMQTT_MAX_MESSAGE_SIZE 1024 (default)
//#include <PicoMQTT.h>  // https://github.com/mlesniew/PicoMQTT

class evHandlerMqtt : private evHandler {
public:
  evHandlerMqtt(const uint8_t evCode,const String& broker = "",const String& topicPrefix = "")
    : evCode(evCode), broker(broker){};

  virtual void begin() override;
  virtual void handle() override;

  bool publish(const String &topic, const String &payload);
  void setBroker(const String& aBroker,const String& aTopicPrefix) {broker=aBroker;topicPrefix=aTopicPrefix+'/';begin();};
private:
  uint8_t evCode;
  String broker;
  String topicPrefix;
};
