/*
 *  MQTT example for the Vista10/20/128/250 sketch (esp8266/esp32)
 *
 *  Processes the security system status and allows for control of all system aspects using MQTT.
 *
 * First copy all *.h and *cpp files from the /src/VistaECPInterface directory to the same location
 * where you placed the sketch or into a subdirectory within your arduino libraries folder.
 * 
 *  Usage:
 *    1. Set the WiFi SSID and password in the sketch.
 *    2. Program the keypad addresses to be used in your panel in programs *190 - *196 and assign them to config parameters keypadAddr1, keypadAddr2, keypadAddr3 for the partitions you need.
 *    3. Set the security system access code to permit disarming through your control software.
 *    4. Set the MQTT server address and other connection options in the sketch.
 *    5. Upload the sketch and monitor using a tool such as MQTT explorer.
 *    6. Setup your home control software to process the MQTT topics
 *
 * 

/*NOTE: Only use SSL with an ESP32.  The ESP8266 will get out of memory errors with the bear ssl library*/

//#define useMQTTSSL /*set this to use SSL with a supported mqtt broker.  */

#ifdef ESP32

#include <WiFi.h>

#include <mDNS.h>

#include <WiFiClientSecure.h>

#define LED_BUILTIN 13

#else

#include <ESP8266WiFi.h>

#include <ESP8266mDNS.h>

#endif

#include <string>

#include <PubSubClient.h>

#include <WiFiUdp.h>

#include <ArduinoOTA.h>
#include <ArduinoJson.h>

#define ARDUINO_MQTT
#include "vistaAlarm.h"

#define DEBUG 2


// Settings
const char * wifiSSID = "<yourwifiaccesspoint>"; //name of wifi access point to connect to
const char * wifiPassword = "<yourwifipassword>";
const char * accessCode = "1234"; // An access code is required to arm (unless quick arm is enabled)
const char * otaAccessCode = "1234"; // Access code for OTA uploading
const char * mqttServer = "<yourmqttserveraddress>"; // MQTT server domain name or IP address
const char * mqttUsername = "<mqttuser>"; // Optional, leave blank if not required
const char * mqttPassword = "<mqttpass>"; // Optional, leave blank if not required

const int monitorPin=18;
const int rxPin=22;
const int txPin=21;

const int defaultPartition=1;
const int maxPartitions=3;
const int maxZones=48;

const int keypadAddr1=17;
const int keypadAddr2=21;
const int keypadAddr3=22;

const int expanderAddr1=0;
const int expanderAddr2=0;

//relay module emulation (4204) addresses. Set to 0 to disable
const int relayAddr1=0;
const int relayAddr2=0;
const int relayAddr3=0;
const int relayAddr4=0;

const int TTL = 30000;
const bool quickArm=false;
const bool lrrSupervisor=true;

const char * rfSerialLookup=  "0019994:66:80,0818433:22:80,0123456:55:80"; //#serial1:zone1:mask1,#serial2:zone2:mask2


// MQTT topics
const char * mqttClientName = "VistaInterface";
const char * mqttRFTopic = "Vista/Get/RF";
const char * mqttLrrTopic = "Vista/Get/LrrMessage"; // send lrr messages

const char * mqttZoneStatusTopic = "Vista/Get/Zone"; // Sends zone status per zone: Vista/Get/Zone1 ... Vista/Get/Zone64
const char * mqttRelayStatusTopic = "Vista/Get/Relay"; //relay status (relay1... relay2)

const char * mqttTroubleMsgTopic = "Vista/Get/Trouble"; // Sends trouble status
const char * mqttPanelTopic = "Vista/Get/Panel"; 
//const char * mqttLightsSuffix = "/StatusLights"; // battery/ac
const char * mqttPartitionTopic = "Vista/Get/Partition"; // Partition1 ... Partition2
const char * mqttStatusSuffix = "/Status"; // alarm/triggered ready/etc Partition1 ... Partition
const char * mqttFireSuffix = "/Fire"; // Sends fire status per partition: Vista/Get/Fire1/(on:off) ... Vista/Get/Fire8/(on:off)
const char * mqttZoneMsgTopic = "Vista/Get/ZoneExtStatus"; //zone message for partition
const char * mqttPartitionMsgSuffix = "/Message"; // Status messages for the partition
const char * mqttBeepSuffix = "/Beep"; // send beep counts for partition
const char * mqttLine1Suffix = "/DisplayLine1"; // send display line 1 for partition
const char * mqttLine2Suffix = "/DisplayLine2"; // send display line 1
const char * mqttBirthMessage = "online";
const char * mqttLwtMessage = "offline";
const char * mqttCmdSubscribeTopic = "Vista/Set"; // Receives messages to write to the panel

//End user config

#ifdef useMQTTSSL
const int mqttPort = 8883; // MQTT server ssl port
#else
const int mqttPort = 1883; // MQTT server port
#endif


#ifdef useMQTTSSL
WiFiClientSecure wifiClient;
wifiClient.setInsecure();
#else
WiFiClient wifiClient;
#endif

PubSubClient mqtt(mqttServer, mqttPort, wifiClient);
unsigned long mqttPreviousTime;
int lastLedState;

vistaECPHome * VistaECP;

void setup() {

  Serial.setDebugOutput(true);
  Serial.begin(115200);
  Serial.println();
  pinMode(LED_BUILTIN, OUTPUT); // LED pin as output.
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifiSSID, wifiPassword);

  uint8_t checkCount = 20;
  while (WiFi.status() != WL_CONNECTED) {
    Serial.printf("Connecting to Wifi..%d\n", checkCount);
    delay(1000);
    if (checkCount--) continue;
    checkCount = 50;
    WiFi.disconnect();
    WiFi.reconnect();

  }
  // Port defaults to 8266
  ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  ArduinoOTA.setHostname(mqttClientName);

  // No authentication by default
  ArduinoOTA.setPassword(otaAccessCode);

  ArduinoOTA.onStart([]() {
    vista.stop();
    Serial.println("Start");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println(F("Auth Failed"));
    else if (error == OTA_BEGIN_ERROR) Serial.println(F("Begin Failed"));
    else if (error == OTA_CONNECT_ERROR) Serial.println(F("Connect Failed"));
    else if (error == OTA_RECEIVE_ERROR) Serial.println(F("Receive Failed"));
    else if (error == OTA_END_ERROR) Serial.println(F("End Failed"));
  });
  ArduinoOTA.begin();
  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  
  mqtt.setCallback(mqttCallback);

  if (mqttConnect()) 
      mqttPreviousTime = millis();
  else 
      mqttPreviousTime = 0;

    VistaECP = new vistaECPHome(keypadAddr1,rxPin,txPin,monitorPin,maxZones,maxPartitions);
    VistaECP->partitionKeypads[1]=keypadAddr1;
    VistaECP->partitionKeypads[2]=keypadAddr2;
    VistaECP->partitionKeypads[3]=keypadAddr3;
    VistaECP->rfSerialLookup=rfSerialLookup;
    VistaECP->defaultPartition=defaultPartition;
    VistaECP->accessCode=accessCode;
    VistaECP->quickArm=quickArm;
    VistaECP->expanderAddr1=expanderAddr1; 
    VistaECP->expanderAddr2=expanderAddr2;
    VistaECP->relayAddr1=relayAddr1; 
    VistaECP->relayAddr2=relayAddr2;
    VistaECP->relayAddr3=relayAddr3; 
    VistaECP->relayAddr4=relayAddr4;     
    VistaECP->lrrSupervisor=lrrSupervisor;
    VistaECP->TTL=TTL;
    VistaECP->debug=DEBUG;
    
    VistaECP->onSystemStatusChange([&](std::string statusCode,uint8_t partition) {
        mqttPublish(mqttPartitionTopic,mqttStatusSuffix,partition,statusCode.c_str());
    }); 
    
    VistaECP->onStatusChange([&](sysState led,bool open,uint8_t partition) {
        
           char psvalue[15];
                strcpy(psvalue,"");
               switch(led) { 
                case sfire:strcat(psvalue,PSTR("/Fire"));break;
                case salarm: strcat(psvalue,PSTR("/Alarm"));break;
                case strouble: strcat(psvalue,PSTR("/Trouble"));break;
                case sarmedstay: strcat(psvalue,PSTR("/ArmedStay"));break;
                case sarmedaway: strcat(psvalue,PSTR("/ArmedAway"));break;
                case sinstant: strcat(psvalue,PSTR("/ArmedInstant"));break; 
                case sready: strcat(psvalue,PSTR("/Ready"));break; 
                case sac: strcat(psvalue,PSTR("/AC"));break;          
                case sbypass: strcat(psvalue,PSTR("/Bypass"));break;  
                case schime: strcat(psvalue,PSTR("/Chime"));break;
                case sbat: strcat(psvalue,PSTR("/Battery"));break;
                case sarmednight: strcat(psvalue,PSTR("/ArmedNight"));break;
                case sarmed: strcat(psvalue,PSTR("/Armed"));break; 
               }
        
      mqttPublish(mqttPartitionTopic,psvalue,partition,open);
    });
    
    
    VistaECP->onLine1DisplayChange([&](std::string msg,uint8_t partition) {
          mqttPublish(mqttPartitionTopic,mqttLine1Suffix,partition,msg.c_str());
    });
    VistaECP->onLine2DisplayChange([&](std::string msg,uint8_t partition) {
          mqttPublish(mqttPartitionTopic,mqttLine2Suffix,partition,msg.c_str());
    });
    
    VistaECP->onLrrMsgChange([&](std::string msg) {
           mqttPublish(mqttLrrTopic,msg.c_str());
    });  
    
    VistaECP->onRfMsgChange([&](std::string msg) {
         mqttPublish(mqttRFTopic,msg.c_str());
    });        

    VistaECP->onBeepsChange([&](std::string beeps,uint8_t partition) {
        mqttPublish(mqttPartitionTopic,mqttBeepSuffix,partition,beeps.c_str());
    });    

    VistaECP->onZoneStatusChange([&](int zone, std::string open) {
        //text zone status O/C/B/A (open/closed/bypassed/alarmed)
        //mqttPublish(mqttZoneStatusTopic,zone,open.c_str());

    });
    VistaECP->onZoneStatusChangeBinarySensor([&](int zone, bool open) {
         //binary zone status ON/OFF
         mqttPublish(mqttZoneStatusTopic,zone,open);
    });     
    
    VistaECP->onZoneExtendedStatusChange([&](std::string msg) {
            //by,al
            mqttPublish(mqttZoneMsgTopic,msg.c_str());
    });
    VistaECP->onRelayStatusChange([&](uint8_t addr,int channel,bool open) {
         char suffix[6];
        strcpy(suffix,"_");
        char c[3];
        itoa(channel,c,10);
        strcat(suffix,c);
        mqttPublish(mqttRelayStatusTopic,suffix,addr,open);
    
    //zone follower when relayaddress1 , channels 1 to 4 on, sets zones 1 to 4 on
    // when relayaddress2, channels 1 to 4 on sets zones 5 to 8 on
    switch(addr) {
      //text zone sensor
     //case relayAddr1: mqttPublish(mqttZoneStatusTopic,channel,open?"O":"C");break;
     //case relayAddr2: mqttPublish(mqttZoneStatusTopic,channel+4,open?"O":"C");break;
     
     //binary zone sensor
     //case relayAddr1: mqttPublish(mqttZoneStatusTopic,channel,open);break;
     //case relayAddr2: mqttPublish(mqttZoneStatusTopic,channel+4,open);break;
    }

    });
   
     VistaECP->begin(); 
}



void loop() {
  static int loopFlag = 0;
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.begin();
    if (loopFlag==0)
        Serial.println(F("\nWifi disconnected. Reconnecting..."));
    WiFi.disconnect();
    WiFi.reconnect();
    delay(1000);    
    loopFlag=1;
  } else 
      loopFlag=0;

  if (WiFi.status() == WL_CONNECTED) {
    ArduinoOTA.handle();
    mqttHandle();
  }
  /*
  static unsigned long ledTime;
  if (millis() - ledTime > 1000) {
    if (lastLedState) {
      digitalWrite(LED_BUILTIN, LOW);
      lastLedState = 0;
    } else {
      digitalWrite(LED_BUILTIN, HIGH);
      lastLedState = 1;

    }
    ledTime = millis();
  }
  */
  VistaECP->loop();
  
} //loop


// Handles messages received in the mqttSubscribeTopic
void mqttCallback(char * topic, byte * payload, unsigned int length) {

  payload[length] = '\0';

  StaticJsonDocument <256> doc;
  deserializeJson(doc,payload);
       int p=defaultPartition;
       const char * s="";
       const char * c="";
      if (strcmp(topic, mqttCmdSubscribeTopic) == 0) { 
        if (doc.containsKey("partition"))
          p=doc["partition"];
        if (doc.containsKey("code"))
            c=doc["code"];     
        if (doc.containsKey("state") )  {
            s=doc["state"];  
            VistaECP->set_alarm_state(s,c,p); 
        } else if (doc.containsKey("keys")) {
            s=doc["keys"]; 
            VistaECP->alarm_keypress_partition(s,p);
        } else if (doc.containsKey("fault") && doc.containsKey("zone")) {
          int z=doc["zone"];
          bool f=doc["fault"] > 0?1:0;
          VistaECP->set_zone_fault(z,f);
        } else if (doc.containsKey("stop")) {
           disconnectVista(); 
        } else if (doc.containsKey("start") || doc.containsKey("restart")) {
            ESP.restart();
        }
        
      }    
}

void mqttHandle() {

  if (!mqtt.connected()) {
    unsigned long mqttCurrentTime = millis();
    if (mqttCurrentTime - mqttPreviousTime > 5000) {
      mqttPreviousTime = mqttCurrentTime;

      if (mqttConnect()) {
        Serial.println(F("MQTT disconnected, successfully reconnected."));
        mqttPreviousTime = 0;
      } else {
        Serial.println(F("MQTT disconnected, failed to reconnect."));
        Serial.print(F("Status code ="));
        Serial.print(mqtt.state());
      }
    }
  } else mqtt.loop();
}

bool mqttConnect() {
  Serial.print(F("MQTT...."));
  //if (mqtt.connect(mqttClientName, mqttUsername, mqttPassword, mqttPanelTopic, 0, true, mqttLwtMessage)) {
  if (mqtt.connect(mqttClientName, mqttUsername, mqttPassword)) {      
    Serial.print(F("connected: "));
    Serial.println(mqttServer);
    mqtt.subscribe(mqttCmdSubscribeTopic);
  } else {
    Serial.print(F("connection error: "));
    Serial.println(mqttServer);
  }
  return mqtt.connected();
}

void mqttPublish(const char * publishTopic,  const char * value) {
  mqtt.publish(publishTopic, value);
}

void mqttPublish(const char * topic, const char * suffix,  const char * value) {
  char publishTopic[strlen(topic) +strlen(suffix) + 2];
  strcpy(publishTopic, topic);
  strcat(publishTopic, suffix);
  mqtt.publish(publishTopic, value);
}

void mqttPublish(const char * topic,const char * suffix , byte srcNumber,  const char * value) {
  char publishTopic[strlen(topic) + 5 + strlen(suffix)];
  char dstNumber[3];
  strcpy(publishTopic, topic);
  itoa(srcNumber, dstNumber, 10);
  strcat(publishTopic, dstNumber);  
  strcat(publishTopic, suffix);  
  mqtt.publish(publishTopic, value);
}

void mqttPublish(const char * topic,const char * suffix , byte srcNumber,  bool bvalue) {
  const char * value = bvalue ? "ON" : "OFF";
   mqttPublish(topic,suffix,srcNumber,value);
}

void mqttPublish(const char * topic, byte srcNumber, const char * value) {
  char publishTopic[strlen(topic) + 10];
  char dstNumber[3];
  strcpy(publishTopic, topic);
  itoa(srcNumber, dstNumber, 10);
  strcat(publishTopic, dstNumber);
  mqtt.publish(publishTopic, value);
}

void mqttPublish(const char * topic, byte srcNumber,  bool bvalue) {
  const char * value = bvalue ? "ON" : "OFF";
  mqttPublish(topic,srcNumber,value);
}

