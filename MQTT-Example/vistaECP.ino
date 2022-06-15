/*
 *  MQTT example sketch (esp8266/esp32)
 *
 *  Processes the security system status and allows for control of all system aspects using MQTT.
 *
 * 
 * First copy all *.h and *cpp files from the /src/vistaEcpInterface directory to the same location
 * where you placed the sketch or into a subdirectory within your arduino libraries folder.
 * 
 *  Usage:
 *    1. Set the WiFi SSID and password in the sketch.
 *    2. Program the keypad addresses to be used in your panel in programs *190 - *196 and assign them to config parameters kp_addr1, kp_addr2, kp_addr3 for the partitions you need.
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


#include  "vista.h"

//#define MQTT_MAX_PACKET_SIZE 20000
#define MAX_ZONES 32
#define LED 2 //Define blinking LED pin

//zone timeout before resets to closed
#define TTL 30000

//set to true if you want to emulate a long range radio . leave at false if you already have one on the system
#define LRRSUPERVISOR false

/*
  # module addresses:
  # 07 4229 zone expander  zones 9-16
  # 08 4229 zone expander zones 17-24
  # 09 4229 zone expander zones 25-32
  # 10 4229 zone expander zones 33-40
  # 11 4229 zone expander zones 41 48
  
  # 12 4204 relay module  
  # 13 4204 relay module
  # 14 4204 relay module
  # 15 4204 relay module
  */
//if you wish to emulate a zone expander to add zones, set to the address you want to assign to the emulated board
#define ZONEEXPANDER1 0
#define ZONEEXPANDER2 0
#define ZONEEXPANDER3 0
#define ZONEEXPANDER4 0

#define RELAYEXPANDER1 0
#define RELAYEXPANDER2 0
#define RELAYEXPANDER3 0
#define RELAYEXPANDER4 0

//program the keypad addresses below using programs *190 - *196 on your panel
#define KP_ADDR1 17 //partition 1 keypad address
#define KP_ADDR2 0 //partition 2 keypad address
#define KP_ADDR3 0 //partition 3 keypad address
#define DEFAULTPARTITION 1

#ifdef ESP32
// Configures the ECP bus interface with the specified pins 
#define RX_PIN 22
#define TX_PIN 21
#define MONITOR_PIN 18 // pin used to monitor the green TX line . See wiring diagram
#else
// Configures the ECP bus interface with the specified pins 
#define RX_PIN 5 //esp8266: D1, D2, D5 (GPIO 5, 4 , 14)
#define TX_PIN 4
#define MONITOR_PIN 14 // pin used to monitor the green TX line . See wiring diagram
#endif


#define DEBUG 1

// Settings
const char * wifiSSID = ""; //name of wifi access point to connect to
const char * wifiPassword = "";
const char * accessCode = "1234"; // An access code is required to arm (unless quick arm is enabled)
const char * otaAccessCode = "1234"; // Access code for OTA uploading
const char * mqttServer = ""; // MQTT server domain name or IP address
const char * mqttUsername = ""; // Optional, leave blank if not required
const char * mqttPassword = ""; // Optional, leave blank if not required

#define periodicRefresh // Optional, uncomment to have sketch update all topics every minute

#ifdef useMQTTSSL
const int mqttPort = 8883; // MQTT server ssl port
#else
const int mqttPort = 1883; // MQTT server port
#endif

// MQTT topics
const char * mqttClientName = "vistaECPInterface";
const char * mqttZoneTopic = "vista/Get/Zone"; // Sends zone status per zone: vista/Get/Zone1 ... vista/Get/Zone64
const char * mqttRFTopic = "vista/Get/RF";
const char * mqttRelayTopic = "vista/Get/Relay"; // Sends zone status per zone: vista/Get/Zone1 ... vista/Get/Zone64
const char * mqttFireTopic = "vista/Get/Fire"; // Sends fire status per partition: vista/Get/Fire1 ... vista/Get/Fire8
const char * mqttTroubleTopic = "vista/Get/Trouble"; // Sends trouble status
const char * mqttSystemStatusTopic = "vista/Get/SystemStatus"; // Sends online/offline status
const char * mqttStatusTopic = "vista/Get/Status"; // Sends online/offline status
const char * mqttLrrTopic = "vista/Get/LrrMessage"; // send lrr messages
const char * mqttBeepTopic = "vista/Get/Beeps"; // send beep counts
const char * mqttLine1Topic = "vista/Get/DisplayLine1"; // send display line 1
const char * mqttLine2Topic = "vista/Get/DisplayLine2"; // send display line 1
const char * mqttBirthMessage = "online";
const char * mqttLwtMessage = "offline";
const char * mqttCmdSubscribeTopic = "vista/Set/Cmd"; // Receives messages to write to the panel
const char * mqttKeypadSubscribeTopic = "vista/Set/Keypad"; // Receives messages to write to the panel
const char * mqttFaultSubscribeTopic = "vista/Set/Fault"; // Receives messages to write to the panel

const char * FAULT = "FAULT"; //change these to suit your panel language 
const char * BYPAS = "BYPAS";
const char * ALARM = "ALARM";
const char * FIRE = "FIRE";
const char * CHECK = "CHECK";
const char * KLOSED = "CLOSED";
const char * OPEN = "OPEN";
const char * ARMED = "ARMED";
const char * HITSTAR = "Hit *";
// End user defines  

const char * STATUS_PENDING = "pending";
const char * STATUS_ARMED = "armed_away";
const char * STATUS_STAY = "armed_stay";
const char * STATUS_NIGHT = "armed_night";
const char * STATUS_OFF = "disarmed";
const char * STATUS_ONLINE = "online";
const char * STATUS_OFFLINE = "offline";
const char * STATUS_TRIGGERED = "triggered";
const char * STATUS_READY = "ready";
const char * STATUS_NOT_READY = "unavailable"; //ha alarm panel likes to see "unavailable" instead of not_ready when the system can't be armed
const char * MSG_ZONE_BYPASS = "zone_bypass_entered";
const char * MSG_ARMED_BYPASS = "armed_custom_bypass";
const char * MSG_NO_ENTRY_DELAY = "no_entry_delay";
const char * MSG_NONE = "no_messages";

enum sysState {
  soffline,
  sarmedaway,
  sarmedstay,
  sbypass,
  sac,
  schime,
  sbat,
  scheck,
  scanceled,
  sarmednight,
  sdisarmed,
  striggered,
  sunavailable,
  strouble,
  salarm,
  sfire,
  sinstant,
  sready
};

// Initialize components
Stream * OutputStream = & Serial;
Vista vista(OutputStream);

#ifdef useMQTTSSL
WiFiClientSecure wifiClient;
wifiClient.setInsecure();
#else
WiFiClient wifiClient;
#endif

PubSubClient mqtt(mqttServer, mqttPort, wifiClient);
unsigned long mqttPreviousTime;

enum zoneState {
  zclosed,
  zopen,
  zbypass,
  zalarm,
  zfire,
  ztrouble
};

sysState currentSystemState, previousSystemState, emptySystemState;

uint8_t zone;
bool sent, vh;
char p1[18];
char p2[18];
char msg[50];
uint8_t partitions[3];
std::string lastp1;
std::string lastp2;
int lastbeeps;
unsigned long ledTime, refreshTime, refreshLrrTime;
int lastLedState, upCount;

//add zone ttl array.  zone, last seen (millis)
struct {
  unsigned long time;
  zoneState state;
  uint8_t partition;
}
zones[MAX_ZONES + 1];
uint8_t defaultKpAddr;

unsigned long lowBatteryTime;

struct alarmStatusType {
  unsigned long time;
  bool state;
  uint8_t zone;
  char prompt[17];
};

alarmStatusType fireStatus,
panicStatus,
alarmStatus;

struct {
  unsigned long time;
  bool state;
  uint8_t zone;
  char p1[17];
  char p2[17];
}
systemPrompt;

struct lrrType {
  int code;
  uint8_t qual;
  uint8_t zone;
  uint8_t user;
  uint8_t partition;
};

struct lightStates {
  bool away;
  bool stay;
  bool night;
  bool instant;
  bool bypass;
  bool ready;
  bool ac;
  bool chime;
  bool bat;
  bool alarm;
  bool check;
  bool fire;
  bool canceled;
  bool trouble;
  bool armed;
};

lightStates currentLightState, previousLightState, emptyLightState;
enum lrrtype {
  user_t,
  zone_t
};

struct {
  sysState previousSystemState;
  lightStates previousLightState;
  std::string lastp1;
  std::string lastp2;
  int lastbeeps;
  bool refreshStatus;
  bool refreshLights;
}
partitionStates[3];

std::string previousMsg;

lrrType lrr, previousLrr, emptyLrr;
unsigned long asteriskTime, sendWaitTime;
bool firstRun;

void setup() {

  Serial.setDebugOutput(true);
  Serial.begin(115200);
  Serial.println();
  firstRun = true;
  pinMode(LED_BUILTIN, OUTPUT); // LED pin as output.
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifiSSID, wifiPassword);

  uint8_t checkCount = 20;
  while (WiFi.status() != WL_CONNECTED) {
    Serial.printf("Connecting to Wifi..%d\n", checkCount);
    delay(1000);
    if (checkCount--) continue;
    checkCount = 50;
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
    vista.begin(RX_PIN, TX_PIN,KP_ADDR1, MONITOR_PIN);
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  mqttPublish(mqttStatusTopic, mqttLwtMessage);

  mqtt.setCallback(mqttCallback);

  if (mqttConnect()) mqttPreviousTime = millis();
  else mqttPreviousTime = 0;

  vista.begin(RX_PIN, TX_PIN, KP_ADDR1, MONITOR_PIN);

  vista.lrrSupervisor = LRRSUPERVISOR; //if we don't have a monitoring lrr supervisor we emulate one if set to true
  //set addresses of expander emulators
  vista.zoneExpanders[0].expansionAddr = ZONEEXPANDER1;
  vista.zoneExpanders[1].expansionAddr = ZONEEXPANDER2;
  vista.zoneExpanders[2].expansionAddr = ZONEEXPANDER3;
  vista.zoneExpanders[3].expansionAddr = ZONEEXPANDER4;
  vista.zoneExpanders[4].expansionAddr = RELAYEXPANDER1;
  vista.zoneExpanders[5].expansionAddr = RELAYEXPANDER2;
  vista.zoneExpanders[6].expansionAddr = RELAYEXPANDER3;
  vista.zoneExpanders[7].expansionAddr = RELAYEXPANDER4;
  
  setDefaultKpAddr(DEFAULTPARTITION);
  
  Serial.println(F("Vista ECP Interface is online."));

}

void printPacket(const char * label, char cbuf[], int len) {

  std::string s;
  char s1[4];
  for (int c = 0; c < len; c++) {
    sprintf(s1, "%02X ", cbuf[c]);
    s.append(s1);
  }
  Serial.print(label);
  Serial.print(": ");
  Serial.println(s.c_str());
}

void loop() {

  if (WiFi.status() != WL_CONNECTED) {
    WiFi.begin();
    int loopCount = 0;
    int upCount = 0;
    Serial.println("\nWifi disconnected. Reconnecting...");
    while (WiFi.status() != WL_CONNECTED && loopCount < 200) {
      delay(100);
      WiFi.reconnect();
      Serial.print(".");
      if (upCount >= 60) {
        upCount = 0;
        Serial.println();
      }
      ++upCount;
      ++loopCount;
    }
  }

  ArduinoOTA.handle();
  mqttHandle();

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
  #ifdef periodicRefresh
  if (millis() - refreshTime > 60000 && vista.keybusConnected && !vista.statusFlags.armedAway && !vista.statusFlags.armedStay && !vista.statusFlags.programMode) {
    //refresh all mqtt fields every minute
    previousSystemState = emptySystemState;
    previousLightState = emptyLightState;
    previousLrr = emptyLrr;
    for (int x = 1; x < MAX_ZONES + 1; x++) {
      zoneState zs = zones[x].state;
      if (zs == zopen)
        mqttPublish(mqttZoneTopic, x, OPEN);
      if (zs == zclosed)
        mqttPublish(mqttZoneTopic, x, KLOSED);
      if (zs == zbypass)
        mqttPublish(mqttZoneTopic, x, BYPAS);
      if (zs == zalarm)
        mqttPublish(mqttZoneTopic, x, ALARM);
      if (zs == zfire)
        mqttPublish(mqttZoneTopic, x, FIRE);
      if (zs == ztrouble)
        mqttPublish(mqttZoneTopic, x, "TROUBLE");

    }
    for (uint8_t partition = 1; partition < 4; partition++) {
      partitionStates[partition - 1].refreshStatus = true;
      partitionStates[partition - 1].refreshLights = true;

    }

    refreshTime = millis();
  }

  #endif

  //if data to be sent, we ensure we process it quickly to avoid delays with the F6 cmd
  sendWaitTime = millis();
  vh = vista.handle();
  while (!firstRun && vista.keybusConnected && vista.sendPending() && !vh) {
    if (millis() - sendWaitTime > 5) break;
    vh = vista.handle();
  }

  if (vista.keybusConnected && vh) {

    if (firstRun) {
      mqttPublish(mqttStatusTopic, mqttBirthMessage);
    }

    if (DEBUG > 0 && vista.cbuf[0] && vista.newCmd) {
      printPacket("CMD", vista.cbuf, 13);

    }

    if (vista.newExtCmd) {
      if (DEBUG > 0)
        printPacket("EXT", vista.extcmd, 13);
      vista.newExtCmd = false;
      //format: [0xFA] [deviceid] [subcommand] [channel/zone] [on/off] [relaydata]

      if (vista.extcmd[0] == 0xFA) {
        uint8_t z = vista.extcmd[3];
        zoneState zs;
        if (vista.extcmd[2] == 0xF1 && z > 0 && z <= MAX_ZONES) { // we have a zone status (zone expander address range)
          zs = vista.extcmd[4] ? zopen : zclosed;
          std::string zone_state1 = zs == zopen ? "OPEN" : "CLOSED";
          Serial.printf("Zone %d,state=%d,zs=%d, %s\n", z, zones[z].state, zs, zone_state1.c_str());

          if (zones[z].state != zbypass && zones[z].state != zalarm) {
            mqttPublish(mqttZoneTopic, z, zone_state1.c_str());
            zones[z].time = millis();
            zones[z].state = zs;
          } else {
            std::string zone_state2 = zones[z].state == zbypass ? "BYPASS" : zones[z].state == zalarm ? "ALARM" : "";
            mqttPublish(mqttZoneTopic, z, (zone_state2.append("_").append(zone_state1)).c_str());
          }

        } else if (vista.extcmd[2] == 0x00) { //relay update z = 1 to 4
          if (z > 0) {
            char rc[5];
            sprintf(rc, "%d:%d=%d", vista.extcmd[1], z, vista.extcmd[4]);
            Serial.printf("Got relay address %d channel %d = %d,%s\n", vista.extcmd[1], z, vista.extcmd[4], rc);
            mqttPublish(mqttRelayTopic, rc, vista.extcmd[4] ? true : false);
          }
        } else if (vista.extcmd[2] == 0x0d) { //relay update z = 1 to 4 - 1sec on / 1 sec off
          if (z > 0) {
            char rc[5];
            sprintf(rc, "%d:%d=%d", vista.extcmd[1], z, vista.extcmd[4]);
            //  mqttPublish(mqttRelayTopic, rc, vista.extcmd[4] ? true : false);
            Serial.printf("Got relay address %d channel %d = %d. Cmd 0D. Pulsing 1sec on/ 1sec off", vista.extcmd[1], z, vista.extcmd[4]);
          }
        } else if (vista.extcmd[2] == 0xF7) { //30 second zone expander module status update
          uint8_t faults = vista.extcmd[4];
          for (int x = 8; x > 0; x--) {
            z = getZoneFromChannel(vista.extcmd[1], x); //device id=extcmd[1]
            if (!z) continue;
            zs = faults & 1 ? zopen : zclosed; //check first bit . lower bit = channel 8. High bit= channel 1
            if (zones[z].state != zs) {
              std::string zone_state1 = zs == zopen ? "OPEN" : "CLOSED";
              if (zones[z].state != zbypass && zones[z].state != zalarm) {
                mqttPublish(mqttZoneTopic, z, zone_state1.c_str());
                zones[z].time = millis();
                zones[z].state = zs;
              }
            }
            faults = faults >> 1; //get next zone status bit from field
          }

        }
      } else if (vista.extcmd[0] == 0xFB && vista.extcmd[1] == 4) {
        char rf_serial_char[9];
        // Decode and push new RF sensor data
        uint32_t device_serial = (vista.extcmd[2] << 16) + (vista.extcmd[3] << 8) + vista.extcmd[4];
        Serial.print("RFX: ");
        sprintf(rf_serial_char, "%03d%04d %02X", device_serial / 10000, device_serial % 10000);
        Serial.print(rf_serial_char);
        Serial.print(" Device State: ");
        sprintf(rf_serial_char, "%02x", vista.extcmd[5]);
        Serial.println(rf_serial_char);
        mqttRFPublish(mqttRFTopic, device_serial, rf_serial_char);
      }
    }
    if (vista.cbuf[0] == 0xF7 && vista.newCmd) {
      getPartitions(vista.cbuf[3]);
      memcpy(p1, vista.statusFlags.prompt, 16);
      memcpy(p2, & vista.statusFlags.prompt[16], 16);
      p1[16] = '\0';
      p2[16] = '\0';

      for (uint8_t partition = 1; partition < 4; partition++) {
        if (partitions[partition - 1]) {
          if (partitionStates[partition - 1].lastp1 != p1)
            line1DisplayCallback(p1, partition);
          if (partitionStates[partition - 1].lastp2 != p2)
            line2DisplayCallback(p2, partition);
          if (partitionStates[partition - 1].lastbeeps != vista.statusFlags.beeps)
            beepsCallback(vista.statusFlags.beeps, partition);

          partitionStates[partition - 1].lastp1 = p1;
          partitionStates[partition - 1].lastp2 = p2;
          partitionStates[partition - 1].lastbeeps = vista.statusFlags.beeps;

          if (strstr(vista.statusFlags.prompt, HITSTAR))
            alarm_keypress_partition("*", partition);
        }
      }

      /*
      if (lastp1 != p1)
        mqttPublish(mqttLineTopic, 1, p1);
      if (lastp2 != p2)
        mqttPublish(mqttLineTopic, 2, p2);
      if (lastbeeps != vista.statusFlags.beeps) {
        char tmp[4] = {
          0
        };
        sprintf(tmp, "%d", vista.statusFlags.beeps);
        mqttPublish(mqttBeepTopic, tmp);
      }
      lastbeeps = vista.statusFlags.beeps;
      */
      Serial.print("Prompt1:");
      Serial.println(p1);
      Serial.print("Prompt2:");
      Serial.println(p2);
      Serial.print("Beeps:");
      Serial.println(vista.statusFlags.beeps);
    }

    //publishes lrr status messages
    if ((vista.cbuf[0] == 0xf9 && vista.cbuf[3] == 0x58 && vista.newCmd) || firstRun) { //we show all lrr messages with type 58
      int c, q, z;
      c = vista.statusFlags.lrr.code;
      q = vista.statusFlags.lrr.qual;
      z = vista.statusFlags.lrr.zone;

      std::string qual;

      if (c < 400)
        qual = (q == 3) ? " Cleared" : "";
      else if (c == 570)
        qual = (q == 1) ? " Active" : " Cleared";
      else
        qual = (q == 1) ? " Restored" : "";
      if (c) {
        String lrrString = String(statusText(c));

        char uflag = lrrString[0];
        std::string uf = "user";
        if (uflag == 'Z')
          uf = "zone";
        sprintf(msg, "%d: %s %s %d%s", c, & lrrString[1], uf.c_str(), z, qual.c_str());
        mqttPublish(mqttLrrTopic, msg);
        refreshLrrTime = millis();
      }

    }

    vista.newCmd = false;

    if (!(vista.cbuf[0] == 0xf7 || vista.cbuf[0] == 0xf9 || vista.cbuf[0] == 0xf2)) return;

    currentSystemState = sunavailable;
    currentLightState.stay = false;
    currentLightState.away = false;
    currentLightState.night = false;
    currentLightState.ready = false;
    currentLightState.alarm = false;
    currentLightState.armed = false;
    currentLightState.ac = false;
   // currentLightState.bat = false;
    //currentLightState.trouble = false;
    currentLightState.bypass = false;
    currentLightState.chime = false;

    if (vista.statusFlags.armedAway || vista.statusFlags.armedStay) {
      if (vista.statusFlags.night) {
        currentSystemState = sarmednight;
        currentLightState.night = true;
        currentLightState.stay = true;
      } else if (vista.statusFlags.armedAway) {
        currentSystemState = sarmedaway;
        currentLightState.away = true;
      } else {
        currentSystemState = sarmedstay;
        currentLightState.stay = true;
      }
      currentLightState.armed = true;
    }

    // Publishes ready status
    if (vista.statusFlags.ready) {
      currentSystemState = sdisarmed;
      currentLightState.ready = true;
      /*
      for (int x = 1; x < MAX_ZONES + 1; x++) {
        if ((zones[x].state != zbypass && zones[x].state != zclosed) || (zones[x].state == zbypass && !vista.statusFlags.bypass)) {
          mqttPublish(mqttZoneTopic, x, "CLOSED");
          zones[x].state = zclosed;

        }
      }
      */
    }
    /*
    //system armed prompt type
    if (strstr(p1, ARMED) && vista.statusFlags.systemFlag) {
      strncpy(systemPrompt.p1, p1, 17);
      strncpy(systemPrompt.p2, p2, 17);
      systemPrompt.time = millis();
      systemPrompt.state = true;
    }
    */
    //zone fire status
    if (strstr(p1, FIRE) && !vista.statusFlags.systemFlag) {
      fireStatus.zone = vista.statusFlags.zone;
      fireStatus.time = millis();
      fireStatus.state = true;
      strncpy(fireStatus.prompt, p1, 17);
    }
    //zone alarm status 
    if (strstr(p1, ALARM) && !vista.statusFlags.systemFlag) {
      if (vista.statusFlags.zone <= MAX_ZONES) {
        if (zones[vista.statusFlags.zone].state != zalarm)
          mqttPublish(mqttZoneTopic, vista.statusFlags.zone, "ALARM");
        zones[vista.statusFlags.zone].time = millis();
        zones[vista.statusFlags.zone].state = zalarm;
      } else {
        panicStatus.zone = vista.statusFlags.zone;
        panicStatus.time = millis();
        panicStatus.state = true;
        strncpy(panicStatus.prompt, p1, 17);
      }
    }
    //zone check status 
    if (strstr(p1, CHECK) && !vista.statusFlags.systemFlag) {
      if (zones[vista.statusFlags.zone].state != ztrouble)
        mqttPublish(mqttZoneTopic, vista.statusFlags.zone, "TROUBLE");
      zones[vista.statusFlags.zone].time = millis();
      zones[vista.statusFlags.zone].state = ztrouble;

    }
    //zone fault status 
    if (strstr(p1, FAULT) && !vista.statusFlags.systemFlag) {
      if (zones[vista.statusFlags.zone].state != zopen)
        mqttPublish(mqttZoneTopic, vista.statusFlags.zone, "OPEN");
      zones[vista.statusFlags.zone].time = millis();
      zones[vista.statusFlags.zone].state = zopen;

    }
    //zone bypass status
    if (strstr(p1, BYPAS) && !vista.statusFlags.systemFlag) {
      if (zones[vista.statusFlags.zone].state != zbypass)
        mqttPublish(mqttZoneTopic, vista.statusFlags.zone, "BYPASS");
      zones[vista.statusFlags.zone].time = millis();
      zones[vista.statusFlags.zone].state = zbypass;
      assignPartitionToZone(vista.statusFlags.zone);      
    }

    //trouble lights 
    if (!vista.statusFlags.acPower) {
      currentLightState.ac = false;
    } else currentLightState.ac = true;

    if (vista.statusFlags.lowBattery ) {
      currentLightState.bat = true;
      lowBatteryTime = millis();
    }

    if (vista.statusFlags.fire) {
      currentLightState.fire = true;
      currentSystemState = striggered;
    } else currentLightState.fire = false;

    if (vista.statusFlags.inAlarm) {
      currentSystemState = striggered;
      alarmStatus.zone = 99;
      alarmStatus.time = millis();
      alarmStatus.state = true;
    }

    if (vista.statusFlags.chime) {
      currentLightState.chime = true;
    } else currentLightState.chime = false;

    if (vista.statusFlags.entryDelay) {
      currentLightState.instant = true;
    } else currentLightState.instant = false;

    if (vista.statusFlags.bypass) {
      currentLightState.bypass = true;
    } else currentLightState.bypass = false;

    if (vista.statusFlags.fault) {
      currentLightState.check = true;
    } else currentLightState.check = false;

    if (vista.statusFlags.instant) {
      currentLightState.instant = true;
    } else currentLightState.instant = false;

    //clear alarm statuses  when timer expires
    if ((millis() - fireStatus.time) > TTL) fireStatus.state = false;
    if ((millis() - panicStatus.time) > TTL) panicStatus.state = false;
    if ((millis() - alarmStatus.time) > TTL) alarmStatus.state = false;
    // if ((millis() - systemPrompt.time) > TTL) systemPrompt.state = false;
    if ((millis() - lowBatteryTime) > TTL) currentLightState.bat = false;
    if (currentLightState.ac && !currentLightState.bat)
      currentLightState.trouble = false;
    else
      currentLightState.trouble = true;

    currentLightState.alarm = alarmStatus.state;
   
    for (uint8_t partition = 1; partition < 4; partition++) {
      if (partitions[partition - 1]) {
        //system status message
        bool forceRefresh = partitionStates[partition - 1].refreshStatus;
        if (currentSystemState != partitionStates[partition - 1].previousSystemState || forceRefresh)
          switch (currentSystemState) {
          case striggered:
            systemStatusChangeCallback(STATUS_TRIGGERED, partition);
            break;
          case sarmedaway:
            systemStatusChangeCallback(STATUS_ARMED, partition);
            break;
          case sarmednight:
            systemStatusChangeCallback(STATUS_NIGHT, partition);
            break;
          case sarmedstay:
            systemStatusChangeCallback(STATUS_STAY, partition);
            break;
          case sunavailable:
            systemStatusChangeCallback(STATUS_NOT_READY, partition);
            break;
          case sdisarmed:
            systemStatusChangeCallback(STATUS_OFF, partition);
            break;
          default:
            systemStatusChangeCallback(STATUS_NOT_READY, partition);
          }
        partitionStates[partition - 1].previousSystemState = currentSystemState;
        partitionStates[partition - 1].refreshStatus = false;
      }
    }
    
    
    for (uint8_t partition = 1; partition < 4; partition++) {
      if (partitions[partition - 1]) {

        previousLightState = partitionStates[partition - 1].previousLightState;
        bool forceRefresh = partitionStates[partition - 1].refreshLights;
        
        //publish status on change only - keeps api traffic down
        if (currentLightState.fire != previousLightState.fire || forceRefresh)
          statusChangeCallback("FIRE", currentLightState.fire, partition);
        if (currentLightState.alarm != previousLightState.alarm || forceRefresh)
          statusChangeCallback("ALARM", currentLightState.alarm, partition);
        if (currentLightState.trouble != previousLightState.trouble || forceRefresh)
          statusChangeCallback("TROUBLE", currentLightState.trouble, partition);
        if (currentLightState.chime != previousLightState.chime || forceRefresh)
          statusChangeCallback("CHIME", currentLightState.chime, partition);
        if (currentLightState.away != previousLightState.away || forceRefresh)
          statusChangeCallback("AWAY", currentLightState.away, partition);
        if (currentLightState.ac != previousLightState.ac || forceRefresh)
          statusChangeCallback("AC", currentLightState.ac, partition);
        if ((currentLightState.stay != previousLightState.stay || forceRefresh) && vista.statusFlags.systemFlag)
          statusChangeCallback("STAY", currentLightState.stay, partition);
        if ((currentLightState.night != previousLightState.night || forceRefresh) && vista.statusFlags.systemFlag)
          statusChangeCallback("NIGHT", currentLightState.night, partition);
        if ((currentLightState.instant != previousLightState.instant || forceRefresh) && vista.statusFlags.systemFlag)
          statusChangeCallback("INST", currentLightState.instant, partition);
        if (currentLightState.bat != previousLightState.bat || forceRefresh)
          statusChangeCallback("BATTERY", currentLightState.bat, partition);
        if (currentLightState.bypass != previousLightState.bypass || forceRefresh)
          statusChangeCallback("BYPASS", currentLightState.bypass, partition);
        if (currentLightState.ready != previousLightState.ready || forceRefresh)
          statusChangeCallback("READY", currentLightState.ready, partition);
        if (currentLightState.armed != previousLightState.armed || forceRefresh)
          statusChangeCallback("ARMED", currentLightState.armed, partition);
        partitionStates[partition - 1].previousLightState = currentLightState;
        partitionStates[partition - 1].refreshLights = false;

      }
    }

    //clears restored zones after timeout
    for (int x = 1; x < MAX_ZONES + 1; x++) {
      if (((zones[x].state != zbypass && zones[x].state != zclosed) || (zones[x].state == zbypass && !partitionStates[zones[x].partition].previousLightState.bypass)) && (millis() - zones[x].time) > TTL) {
        mqttPublish(mqttZoneTopic, x, "CLOSED");
        zones[x].state = zclosed;
      }
    }

    previousLrr = lrr;
    if (millis() - refreshLrrTime > 30000) {
      lrrMsgChangeCallback("");
      refreshLrrTime = millis();
    }

    firstRun = false;

  }

}

uint8_t getZoneFromChannel(uint8_t deviceAddress, uint8_t channel) {

  switch (deviceAddress) {
  case 7:
    return channel + 8;
  case 8:
    return channel + 16;
  case 9:
    return channel + 24;
  case 10:
    return channel + 32;
  case 11:
    return channel + 40;
  default:
    return 0;
  }

}

void assignPartitionToZone(uint8_t zone) {
    for (int p=1;p<4;p++) {
        if (partitions[p-1]) {
            zones[zone].partition=p-1;
            break;
        }
            
    }
}

void setDefaultKpAddr(uint8_t p) {
    uint8_t a;
    switch (p) {
       case 1:a= KP_ADDR1;break;
       case 2: a= KP_ADDR2;break;
       case 3: a= KP_ADDR3;break;
       default: return;
    }
      if (a > 15 && a < 24)
        vista.setKpAddr(a);
}   



void getPartitions(uint8_t mask) {
  memset(partitions, 0, sizeof(partitions));
  if (KP_ADDR1 > 15 && (mask & (0x01 << (KP_ADDR1 - 16)))) partitions[0] = 1;
  if (KP_ADDR2 > 15 && (mask & (0x01 << (KP_ADDR2 - 16)))) partitions[1] = 1;
  if (KP_ADDR3 > 15 && (mask & (0x01 << (KP_ADDR3 - 16)))) partitions[2] = 1;
}

void set_keypad_address(int addr) {
  if (addr > 0 and addr < 24)
    vista.setKpAddr(addr);
}

long int toInt(const char * s) {
  char * p;
  long int li = strtol(s, & p, 10);
  return li;
}

void alarm_keypress_partition(const char * keys, int partition) {
  uint8_t addr = 0;
  switch (partition) {
  case 1:
    addr = KP_ADDR1;
    break;
  case 2:
    addr = KP_ADDR2;
    break;
  case 3:
    addr = KP_ADDR3;
    break;
  default:
    break;
  }
  if (addr > 0 and addr < 24)
    vista.write(keys, addr);
}
// Handles messages received in the mqttSubscribeTopic
void mqttCallback(char * topic, byte * payload, unsigned int length) {

  payload[length] = '\0';

  Serial.printf("mqtt_callback - message arrived - topic [%s] payload [%s]\n", topic, payload);

  if (strcmp(topic, mqttKeypadSubscribeTopic) == 0) {
    int kp = toInt((char * ) payload);
       //set_keypad_address(kp); //not enabled for now
  } else if (strcmp(topic, mqttFaultSubscribeTopic) == 0) {
    //example: zone:fault  18:1 zone 18 with fault active, 18:0 zone 18 reset fault
    char * sep = strchr((char * ) payload, ':');
    if (sep != 0) {
      * sep = 0;
      int zone = atoi((char * ) payload);
      ++sep;
      bool fault = atoi((char * ) sep) > 0 ? 1 : 0;
      if (zone > 0 && zone < MAX_ZONES)
        vista.setExpFault(zone, fault);
    }
  } else if (strcmp(topic, mqttCmdSubscribeTopic) == 0) {

    // command
    if (payload[0] == '!') {
      // command
        //Serial.printf("Send command: %s\n",(char *) &payload[1]);
        vista.write((char * ) & payload[1]);
    } 
    //cmd with partition
    else if (payload[0] == '&') { //&2xxxxxx where 2=partition, xxxxx=cmd for partition 2
         int p=payload[1] - '0';    
         //Serial.printf("Send command: %s,p=%d\n",(char *) &payload[1],p);
        alarm_keypress_partition((char * ) & payload[2], p);

    } 
    // Arm stay
     else if (payload[0] == 'S' && !partitionStates[DEFAULTPARTITION-1].previousLightState.armed) {
      vista.write(accessCode);
      vista.write("3"); // Virtual keypad arm stay
    }

    //Arm away
    else if (payload[0] == 'A' && !partitionStates[DEFAULTPARTITION-1].previousLightState.armed) {
      vista.write(accessCode);
      vista.write("2"); // Virtual keypad arm away
    }

    // Arm night
    else if (payload[0] == 'N' && !partitionStates[DEFAULTPARTITION-1].previousLightState.armed) {
      vista.write(accessCode);
      vista.write("33");
    }

    // Disarm
    else if (payload[0] == 'D' && partitionStates[DEFAULTPARTITION-1].previousLightState.armed) {
      if (length > 4) {
        vista.write((char * ) & payload[1]); //write code
        vista.write('1'); //disarm
      }
    }
  }

}

void mqttHandle() {
  if (!mqtt.connected()) {
    unsigned long mqttCurrentTime = millis();
    if (mqttCurrentTime - mqttPreviousTime > 5000) {
      mqttPreviousTime = mqttCurrentTime;

      if (mqttConnect()) {
        if (vista.keybusConnected) mqtt.publish(mqttStatusTopic, mqttBirthMessage, true);
        Serial.println(F("MQTT disconnected, successfully reconnected."));
        mqttPreviousTime = 0;
      } else {
        Serial.println(F("MQTT disconnected, failed to reconnect."));
        Serial.print("failed, status code =");
        Serial.print(mqtt.state());
      }
    }
  } else mqtt.loop();
}

bool mqttConnect() {
  Serial.print(F("MQTT...."));
  if (mqtt.connect(mqttClientName, mqttUsername, mqttPassword, mqttStatusTopic, 0, true, mqttLwtMessage)) {
    Serial.print(F("connected: "));
    Serial.println(mqttServer);
    mqtt.subscribe(mqttCmdSubscribeTopic);
    mqtt.subscribe(mqttKeypadSubscribeTopic);
    mqtt.subscribe(mqttFaultSubscribeTopic);

  } else {
    Serial.print(F("connection error: "));
    Serial.println(mqttServer);
  }
  return mqtt.connected();
}

void mqttPublish(const char * publishTopic,
  const char * value) {

  mqtt.publish(publishTopic, value);

}

void mqttRFPublish(const char * topic, char * srcNumber, char * value) {

  char publishTopic[strlen(topic) + 10];
  strcpy(publishTopic, topic);
  strcat(publishTopic, "/");
  strcat(publishTopic, srcNumber);
  mqtt.publish(publishTopic, value);

}

void mqttPublish(const char * topic, uint32_t srcNumber,
  const char * value) {

  char publishTopic[strlen(topic) + 3];
  char dstNumber[2];
  strcpy(publishTopic, topic);
  itoa(srcNumber, dstNumber, 10);
  strcat(publishTopic, "/");
  strcat(publishTopic, dstNumber);
  if (!mqtt.publish(publishTopic, value)) {
    Serial.print("Error with publish, ");
    Serial.println("status code =");
    Serial.println(mqtt.state());
  }

}

void mqttRFPublish(const char * topic, uint32_t srcNumber, char * value) {

  char publishTopic[strlen(topic) + 10];
  char dstNumber[9];
  strcpy(publishTopic, topic);
  sprintf(dstNumber, "%07d", srcNumber);
  strcat(publishTopic, "/");
  strcat(publishTopic, dstNumber);
  mqtt.publish(publishTopic, value);
}

void mqttPublish(const char * topic,
  const char * source, bool vValue) {
  const char * value = vValue ? "ON" : "OFF";
  char publishTopic[strlen(topic) + 10];
  strcpy(publishTopic, topic);
  strcat(publishTopic, "/");
  strcat(publishTopic, source);
  mqtt.publish(publishTopic, value);

}

void mqttPublish(const char * topic, char * source,
  const char * value) {
  char publishTopic[strlen(topic) + 10];
  strcpy(publishTopic, topic);
  strcat(publishTopic, "/");
  strcat(publishTopic, source);
  mqtt.publish(publishTopic, value);

}

//esphome migration equivalent functions
void zoneStatusChangeCallback(uint8_t zone,
  const char * msg) {};

void systemStatusChangeCallback(const char * status, uint8_t partition) {
  char publishTopic[strlen(mqttSystemStatusTopic) + 10];
  strcpy(publishTopic, mqttSystemStatusTopic);
  strcat(publishTopic, "/");
  char tmp[4] = {
    0
  };
  sprintf(tmp, "%d", partition);
  strcat(publishTopic, tmp);
  mqtt.publish(publishTopic, status);

};

void statusChangeCallback(const char * msg, bool isOpen, uint8_t partition) {

  char publishTopic[strlen(mqttStatusTopic) + 10];
  strcpy(publishTopic, mqttStatusTopic);
  strcat(publishTopic, "/");
  strcat(publishTopic, msg);
  strcat(publishTopic, "/");
  char tmp[4] = {
    0
  };
  sprintf(tmp, "%d", partition);
  strcat(publishTopic, tmp);
  const char * value = isOpen ? "ON" : "OFF";
  mqtt.publish(publishTopic, value);

};

void systemMsgChangeCallback(const char * msg, uint8_t partition) {};
void lrrMsgChangeCallback(const char * msg) {};
void rfMsgChangeCallback(const char * msg) {};

void line1DisplayCallback(const char * msg, uint8_t partition) {
  char publishTopic[strlen(mqttLine1Topic) + 10];
  strcpy(publishTopic, mqttLine1Topic);
  strcat(publishTopic, "/");
  char tmp[4] = {
    0
  };
  sprintf(tmp, "%d", partition);
  strcat(publishTopic, tmp);
  mqtt.publish(publishTopic, msg);

};
void line2DisplayCallback(const char * msg, uint8_t partition) {
  char publishTopic[strlen(mqttLine2Topic) + 10];
  strcpy(publishTopic, mqttLine2Topic);
  strcat(publishTopic, "/");
  char tmp[4] = {
    0
  };
  sprintf(tmp, "%d", partition);
  strcat(publishTopic, tmp);
  mqtt.publish(publishTopic, msg);

};
void beepsCallback(char beeps, uint8_t partition) {
  char publishTopic[strlen(mqttBeepTopic) + 10];
  strcpy(publishTopic, mqttBeepTopic);
  strcat(publishTopic, "/");
  char tmp[4] = {
    0
  };
  sprintf(tmp, "%d", partition);
  strcat(publishTopic, tmp);
  sprintf(tmp, "%d", beeps);
  mqtt.publish(publishTopic, tmp);

};
void zoneExtendedStatusCallback(std::string zoneExtendedStatus) {};
void relayStatusChangeCallback(uint8_t addr, uint8_t zone, bool state) {};

const __FlashStringHelper * statusText(int statusCode) {
  switch (statusCode) {

  case 100:
    return F("ZMedical");
  case 101:
    return F("ZPersonal Emergency");
  case 102:
    return F("ZFail to report in");
  case 110:
    return F("ZFire");
  case 111:
    return F("ZSmoke");
  case 112:
    return F("ZCombustion");
  case 113:
    return F("ZWater Flow");
  case 114:
    return F("ZHeat");
  case 115:
    return F("ZPull Station");
  case 116:
    return F("ZDuct");
  case 117:
    return F("ZFlame");
  case 118:
    return F("ZNear Alarm");
  case 120:
    return F("ZPanic");
  case 121:
    return F("UDuress");
  case 122:
    return F("ZSilent");
  case 123:
    return F("ZAudible");
  case 124:
    return F("ZDuress – Access granted");
  case 125:
    return F("ZDuress – Egress granted");
  case 126:
    return F("UHoldup suspicion print");
  case 127:
    return F("URemote Silent Panic");
  case 129:
    return F("ZPanic Verifier");
  case 130:
    return F("ZBurglary");
  case 131:
    return F("ZPerimeter");
  case 132:
    return F("ZInterior");
  case 133:
    return F("Z24 Hour (Safe)");
  case 134:
    return F("ZEntry/Exit");
  case 135:
    return F("ZDay/Night");
  case 136:
    return F("ZOutdoor");
  case 137:
    return F("ZTamper");
  case 138:
    return F("ZNear alarm");
  case 139:
    return F("ZIntrusion Verifier");
  case 140:
    return F("ZGeneral Alarm");
  case 141:
    return F("ZPolling loop open");
  case 142:
    return F("ZPolling loop short");
  case 143:
    return F("ZExpansion module failure");
  case 144:
    return F("ZSensor tamper");
  case 145:
    return F("ZExpansion module tamper");
  case 146:
    return F("ZSilent Burglary");
  case 147:
    return F("ZSensor Supervision Failure");
  case 150:
    return F("Z24 Hour NonBurglary");
  case 151:
    return F("ZGas detected");
  case 152:
    return F("ZRefrigeration");
  case 153:
    return F("ZLoss of heat");
  case 154:
    return F("ZWater Leakage");

  case 155:
    return F("ZFoil Break");
  case 156:
    return F("ZDay Trouble");
  case 157:
    return F("ZLow bottled gas level");
  case 158:
    return F("ZHigh temp");
  case 159:
    return F("ZLow temp");
  case 160:
    return F("ZAwareness Zone Response");
  case 161:
    return F("ZLoss of air flow");
  case 162:
    return F("ZCarbon Monoxide detected");
  case 163:
    return F("ZTank level");
  case 168:
    return F("ZHigh Humidity");
  case 169:
    return F("ZLow Humidity");
  case 200:
    return F("ZFire Supervisory");
  case 201:
    return F("ZLow water pressure");
  case 202:
    return F("ZLow CO2");
  case 203:
    return F("ZGate valve sensor");
  case 204:
    return F("ZLow water level");
  case 205:
    return F("ZPump activated");
  case 206:
    return F("ZPump failure");
  case 300:
    return F("ZSystem Trouble");
  case 301:
    return F("ZAC Loss");
  case 302:
    return F("ZLow system battery");
  case 303:
    return F("ZRAM Checksum bad");
  case 304:
    return F("ZROM checksum bad");
  case 305:
    return F("ZSystem reset");
  case 306:
    return F("ZPanel programming changed");
  case 307:
    return F("ZSelftest failure");
  case 308:
    return F("ZSystem shutdown");
  case 309:
    return F("ZBattery test failure");
  case 310:
    return F("ZGround fault");
  case 311:
    return F("ZBattery Missing/Dead");
  case 312:
    return F("ZPower Supply Overcurrent");
  case 313:
    return F("UEngineer Reset");
  case 314:
    return F("ZPrimary Power Supply Failure");

  case 315:
    return F("ZSystem Trouble");
  case 316:
    return F("ZSystem Tamper");

  case 317:
    return F("ZControl Panel System Tamper");
  case 320:
    return F("ZSounder/Relay");
  case 321:
    return F("ZBell 1");
  case 322:
    return F("ZBell 2");
  case 323:
    return F("ZAlarm relay");
  case 324:
    return F("ZTrouble relay");
  case 325:
    return F("ZReversing relay");
  case 326:
    return F("ZNotification Appliance Ckt. # 3");
  case 327:
    return F("ZNotification Appliance Ckt. #4");
  case 330:
    return F("ZSystem Peripheral trouble");
  case 331:
    return F("ZPolling loop open");
  case 332:
    return F("ZPolling loop short");
  case 333:
    return F("ZExpansion module failure");
  case 334:
    return F("ZRepeater failure");
  case 335:
    return F("ZLocal printer out of paper");
  case 336:
    return F("ZLocal printer failure");
  case 337:
    return F("ZExp. Module DC Loss");
  case 338:
    return F("ZExp. Module Low Batt.");
  case 339:
    return F("ZExp. Module Reset");
  case 341:
    return F("ZExp. Module Tamper");
  case 342:
    return F("ZExp. Module AC Loss");
  case 343:
    return F("ZExp. Module selftest fail");
  case 344:
    return F("ZRF Receiver Jam Detect");

  case 345:
    return F("ZAES Encryption disabled/ enabled");
  case 350:
    return F("ZCommunication  trouble");
  case 351:
    return F("ZTelco 1 fault");
  case 352:
    return F("ZTelco 2 fault");
  case 353:
    return F("ZLong Range Radio xmitter fault");
  case 354:
    return F("ZFailure to communicate event");
  case 355:
    return F("ZLoss of Radio supervision");
  case 356:
    return F("ZLoss of central polling");
  case 357:
    return F("ZLong Range Radio VSWR problem");
  case 358:
    return F("ZPeriodic Comm Test Fail /Restore");

  case 359:
    return F("Z");

  case 360:
    return F("ZNew Registration");
  case 361:
    return F("ZAuthorized  Substitution Registration");
  case 362:
    return F("ZUnauthorized  Substitution Registration");
  case 365:
    return F("ZModule Firmware Update Start/Finish");
  case 366:
    return F("ZModule Firmware Update Failed");

  case 370:
    return F("ZProtection loop");
  case 371:
    return F("ZProtection loop open");
  case 372:
    return F("ZProtection loop short");
  case 373:
    return F("ZFire trouble");
  case 374:
    return F("ZExit error alarm (zone)");
  case 375:
    return F("ZPanic zone trouble");
  case 376:
    return F("ZHoldup zone trouble");
  case 377:
    return F("ZSwinger Trouble");
  case 378:
    return F("ZCrosszone Trouble");

  case 380:
    return F("ZSensor trouble");
  case 381:
    return F("ZLoss of supervision  RF");
  case 382:
    return F("ZLoss of supervision  RPM");
  case 383:
    return F("ZSensor tamper");
  case 384:
    return F("ZRF low battery");
  case 385:
    return F("ZSmoke detector Hi sensitivity");
  case 386:
    return F("ZSmoke detector Low sensitivity");
  case 387:
    return F("ZIntrusion detector Hi sensitivity");
  case 388:
    return F("ZIntrusion detector Low sensitivity");
  case 389:
    return F("ZSensor selftest failure");
  case 391:
    return F("ZSensor Watch trouble");
  case 392:
    return F("ZDrift Compensation Error");
  case 393:
    return F("ZMaintenance Alert");
  case 394:
    return F("ZCO Detector needs replacement");
  case 400:
    return F("UOpen/Close");
  case 401:
    return F("UArmed AWAY");
  case 402:
    return F("UGroup O/C");
  case 403:
    return F("UAutomatic O/C");
  case 404:
    return F("ULate to O/C (Note: use 453 or 454 instead )");
  case 405:
    return F("UDeferred O/C (Obsolete do not use )");
  case 406:
    return F("UCancel");
  case 407:
    return F("URemote arm/disarm");
  case 408:
    return F("UQuick arm");
  case 409:
    return F("UKeyswitch O/C");
  case 411:
    return F("UCallback request made");
  case 412:
    return F("USuccessful  download/access");
  case 413:
    return F("UUnsuccessful access");
  case 414:
    return F("USystem shutdown command received");
  case 415:
    return F("UDialer shutdown command received");

  case 416:
    return F("ZSuccessful Upload");
  case 418:
    return F("URemote Cancel");
  case 419:
    return F("URemote Verify");
  case 421:
    return F("UAccess denied");
  case 422:
    return F("UAccess report by user");
  case 423:
    return F("ZForced Access");
  case 424:
    return F("UEgress Denied");
  case 425:
    return F("UEgress Granted");
  case 426:
    return F("ZAccess Door propped open");
  case 427:
    return F("ZAccess point Door Status Monitor trouble");
  case 428:
    return F("ZAccess point Request To Exit trouble");
  case 429:
    return F("UAccess program mode entry");
  case 430:
    return F("UAccess program mode exit");
  case 431:
    return F("UAccess threat level change");
  case 432:
    return F("ZAccess relay/trigger fail");
  case 433:
    return F("ZAccess RTE shunt");
  case 434:
    return F("ZAccess DSM shunt");
  case 435:
    return F("USecond Person Access");
  case 436:
    return F("UIrregular Access");
  case 441:
    return F("UArmed STAY");
  case 442:
    return F("UKeyswitch Armed STAY");
  case 443:
    return F("UArmed with System Trouble Override");
  case 450:
    return F("UException O/C");
  case 451:
    return F("UEarly O/C");
  case 452:
    return F("ULate O/C");
  case 453:
    return F("UFailed to Open");
  case 454:
    return F("UFailed to Close");
  case 455:
    return F("UAutoarm Failed");
  case 456:
    return F("UPartial Arm");
  case 457:
    return F("UExit Error (user)");
  case 458:
    return F("UUser on Premises");
  case 459:
    return F("URecent Close");
  case 461:
    return F("ZWrong Code Entry");
  case 462:
    return F("ULegal Code Entry");
  case 463:
    return F("URearm after Alarm");
  case 464:
    return F("UAutoarm Time Extended");
  case 465:
    return F("ZPanic Alarm Reset");
  case 466:
    return F("UService On/Off Premises");

  case 501:
    return F("ZAccess reader disable");
  case 520:
    return F("ZSounder/Relay  Disable");
  case 521:
    return F("ZBell 1 disable");
  case 522:
    return F("ZBell 2 disable");
  case 523:
    return F("ZAlarm relay disable");
  case 524:
    return F("ZTrouble relay disable");
  case 525:
    return F("ZReversing relay disable");
  case 526:
    return F("ZNotification Appliance Ckt. # 3 disable");
  case 527:
    return F("ZNotification Appliance Ckt. # 4 disable");
  case 531:
    return F("ZModule Added");
  case 532:
    return F("ZModule Removed");
  case 551:
    return F("ZDialer disabled");
  case 552:
    return F("ZRadio transmitter disabled");
  case 553:
    return F("ZRemote  Upload/Download disabled");
  case 570:
    return F("ZZone/Sensor bypass");
  case 571:
    return F("ZFire bypass");
  case 572:
    return F("Z24 Hour zone bypass");
  case 573:
    return F("ZBurg. Bypass");
  case 574:
    return F("UGroup bypass");
  case 575:
    return F("ZSwinger bypass");
  case 576:
    return F("ZAccess zone shunt");
  case 577:
    return F("ZAccess point bypass");
  case 578:
    return F("ZVault Bypass");
  case 579:
    return F("ZVent Zone Bypass");
  case 601:
    return F("ZManual trigger test report");
  case 602:
    return F("ZPeriodic test report");
  case 603:
    return F("ZPeriodic RF transmission");
  case 604:
    return F("UFire test");
  case 605:
    return F("ZStatus report to follow");
  case 606:
    return F("ZListenin to follow");
  case 607:
    return F("UWalk test mode");
  case 608:
    return F("ZPeriodic test  System Trouble Present");
  case 609:
    return F("ZVideo Xmitter active");
  case 611:
    return F("ZPoint tested OK");
  case 612:
    return F("ZPoint not tested");
  case 613:
    return F("ZIntrusion Zone Walk Tested");
  case 614:
    return F("ZFire Zone Walk Tested");
  case 615:
    return F("ZPanic Zone Walk Tested");
  case 616:
    return F("ZService Request");
  case 621:
    return F("ZEvent Log reset");
  case 622:
    return F("ZEvent Log 50% full");
  case 623:
    return F("ZEvent Log 90% full");
  case 624:
    return F("ZEvent Log overflow");
  case 625:
    return F("UTime/Date reset");
  case 626:
    return F("ZTime/Date inaccurate");
  case 627:
    return F("ZProgram mode entry");

  case 628:
    return F("ZProgram mode exit");
  case 629:
    return F("Z32 Hour Event log marker");
  case 630:
    return F("ZSchedule change");
  case 631:
    return F("ZException schedule change");
  case 632:
    return F("ZAccess schedule change");
  case 641:
    return F("ZSenior Watch Trouble");
  case 642:
    return F("ULatchkey Supervision");
  case 643:
    return F("ZRestricted Door Opened");
  case 645:
    return F("ZHelp Arrived");
  case 646:
    return F("ZAddition Help Needed");
  case 647:
    return F("ZAddition Help Cancel");
  case 651:
    return F("ZReserved for Ademco Use");
  case 652:
    return F("UReserved for Ademco Use");
  case 653:
    return F("UReserved for Ademco Use");
  case 654:
    return F("ZSystem Inactivity");
  case 655:
    return F("UUser Code X modified by Installer");
  case 703:
    return F("ZAuxiliary #3");
  case 704:
    return F("ZInstaller Test");
  case 750:
    return F("ZUser Assigned");
  case 751:
    return F("ZUser Assigned");
  case 752:
    return F("ZUser Assigned");
  case 753:
    return F("ZUser Assigned");
  case 754:
    return F("ZUser Assigned");
  case 755:
    return F("ZUser Assigned");
  case 756:
    return F("ZUser Assigned");
  case 757:
    return F("ZUser Assigned");
  case 758:
    return F("ZUser Assigned");
  case 759:
    return F("ZUser Assigned");
  case 760:
    return F("ZUser Assigned");
  case 761:
    return F("ZUser Assigned");
  case 762:
    return F("ZUser Assigned");
  case 763:
    return F("ZUser Assigned");
  case 764:
    return F("ZUser Assigned");
  case 765:
    return F("ZUser Assigned");
  case 766:
    return F("ZUser Assigned");
  case 767:
    return F("ZUser Assigned");
  case 768:
    return F("ZUser Assigned");
  case 769:
    return F("ZUser Assigned");
  case 770:
    return F("ZUser Assigned");
  case 771:
    return F("ZUser Assigned");
  case 772:
    return F("ZUser Assigned");
  case 773:
    return F("ZUser Assigned");
  case 774:
    return F("ZUser Assigned");
  case 775:
    return F("ZUser Assigned");
  case 776:
    return F("ZUser Assigned");
  case 777:
    return F("ZUser Assigned");
  case 778:
    return F("ZUser Assigned");
  case 779:
    return F("ZUser Assigned");
  case 780:
    return F("ZUser Assigned");
  case 781:
    return F("ZUser Assigned");
  case 782:
    return F("ZUser Assigned");
  case 783:
    return F("ZUser Assigned");
  case 784:
    return F("ZUser Assigned");
  case 785:
    return F("ZUser Assigned");
  case 786:
    return F("ZUser Assigned");
  case 787:
    return F("ZUser Assigned");
  case 788:
    return F("ZUser Assigned");
  case 789:
    return F("ZUser Assigned");

  case 796:
    return F("ZUnable to output signal (Derived Channel)");
  case 798:
    return F("ZSTU Controller down (Derived Channel)");
  case 900:
    return F("ZDownload Abort");
  case 901:
    return F("ZDownload Start/End");
  case 902:
    return F("ZDownload Interrupted");
  case 903:
    return F("ZDevice Flash Update Started/ Completed");
  case 904:
    return F("ZDevice Flash Update Failed");
  case 910:
    return F("ZAutoclose with Bypass");
  case 911:
    return F("ZBypass Closing");
  case 912:
    return F("ZFire Alarm Silence");
  case 913:
    return F("USupervisory Point test Start/End");
  case 914:
    return F("UHoldup test Start/End");
  case 915:
    return F("UBurg. Test Print Start/End");
  case 916:
    return F("USupervisory Test Print Start/End");
  case 917:
    return F("ZBurg. Diagnostics Start/End");
  case 918:
    return F("ZFire Diagnostics Start/End");
  case 919:
    return F("ZUntyped diagnostics");
  case 920:
    return F("UTrouble Closing (closed with burg. during exit)");
  case 921:
    return F("UAccess Denied Code Unknown");
  case 922:
    return F("ZSupervisory Point Alarm");
  case 923:
    return F("ZSupervisory Point Bypass");
  case 924:
    return F("ZSupervisory Point Trouble");
  case 925:
    return F("ZHoldup Point Bypass");
  case 926:
    return F("ZAC Failure for 4 hours");
  case 927:
    return F("ZOutput Trouble");
  case 928:
    return F("UUser code for event");
  case 929:
    return F("ULogoff");
  case 954:
    return F("ZCS Connection Failure");
  case 961:
    return F("ZRcvr Database Connection Fail/Restore");
  case 962:
    return F("ZLicense Expiration Notify");
  case 999:
    return F("Z1 and 1/3 Day No Read Log");
  default:
    return F("ZUnknown");
  }
}