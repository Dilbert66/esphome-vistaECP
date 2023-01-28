/*
    Virtual Keypad/Telegram client for the Honeywell Vista20P alarm system family and variants  (ESP32 Only)
    
    Provides a virtual keypad web interface and push notification  using the ESP32 as a standalone web server using
    AES encrypted web socket communications. All keypad functionality provided.
    
    This sketch uses portions of the code from the VirtualKeypad-Web example for DSC alarm systems found in the
    taligent/VistaECPInterface respository at:
   https://github.com/taligentx/VistaECPInterface/blob/master/examples/esp32/VirtualKeypad-Web/VirtualKeypad-Web.ino.
   
    It was adapted to use the Vista20P alarm system library at: 
  https://github.com/Dilbert66/esphome-vistaECP/tree/dev/src/vistaEcpInterface, with the addition of two way 
    AES encryption for the web socket and push notification capability using the Telegram (https://telegram.org)
    messaging application due to it's ability to also provide control of your system remotely using bots in addition 
    to having push capability.  All this at no cost!

   Usage:
     1. Copy the sketch file VirtualKeypad-Vista.ino, "data" folder and telegram_async.h/.cpp files into a new sketch directory of the same name.
   
   If using the Virtualkeypad (#define  VIRTUALKEYPAD):

     2. Install the following libraries directly from each Github repository:
           ESPAsyncWebServer: https://github.com/me-no-dev/ESPAsyncWebServer
           AsyncTCP: https://github.com/me-no-dev/AsyncTCP
 
     3. Install the filesystem uploader tools to enable uploading web server files:
          https://github.com/me-no-dev/arduino-esp32fs-plugin
 
     4. Install the following libraries, available in the Arduino IDE Library Manager and
        the Platform.io Library Registry:
          AESLib: https://github.com/suculent/thinx-aes-lib
 
     5. If desired, update the DNS hostname in the sketch.  By default, this is set to
        "vistakeypad" and the web interface will be accessible at: http://vistakeypad.local


 

     If not using the VirtualKeypad, start at here step 6. Comment out the #define VIRTUALKEYPAD line.
     
      
     6.  Install the ArduinoJson library available in the Arduino IDE Library Manager and
        the Platform.io Library Registry:  https://github.com/bblanchon/ArduinoJson    
        
     7. Copy all .h and cpp files from the repository at: https://github.com/Dilbert66/esphome-vistaECP/tree/dev/src/vistaEcpInterface
       to your sketch directory or into a subdirectory within your arduino libraries folder.        
        
     8.  Set all configuration variables in the sketch to match your local setup such as WIFI settings, password, teleggramids, aes password,etc.    
     
     9. Compile and upload the sketch. Recommended to use board "ESP32 Dev Module with Minimal SPIFFS partition scheme (190K SPIFFS   partition) to get the maximum flash storage for program storage if using OTA.    

     If using the VirtualKeypad:
     10. Upload the "data" directory (from the same directory as the .ino file) containing the web server files  to the SPIFFS partition:
          Arduino IDE: Tools > ESP32 Sketch Data Upload.  If uploading the sketch using an external app, ensure you do not erase first or you will need to re-upload the data spiffs folder again after.


       
     11. Configure Telegram:   
     
    a. Start a  conversation with @BotFather or go to url: https://telegram.me/botfather
    b. Create a new bot using BotFather: /newbot
    c. Copy the bot token to the telegramBotToken config variable in this sketch
    d. Start a conversation with the newly created bot to open the chat.
    e. Start a conversation with @myidbot, or go to url: https://telegram.me/myidbot to get your chat id.
    f. Get your user chat ID: /getid
    g. Copy the user chat ID to this sketch in the telegramUserID config variable.
    

    12. Once the sketch is loaded and running, any following updates can be done via OTA updates for initial testing.  
        See here for an example:  https://randomnerdtutorials.com/esp8266-ota-updates-with-arduino-ide-over-the-air/
        
        NOTES: 
        a. I do not normally recommended leaving the ability to do OTA updates active on a production system. Once done testing, you should either disable it by commenting out "useOTA" or set a good long passcode.
        Be aware that for uploading sketch data (web server files) via OTA, you cannot have a password set. Once all testing is done, you can then set your password of choice or disable the feature. 
        
        b. You can access the virtual keypad web interface by the IP address displayed through
        the serial output or http://vistakeypad.local (for clients and networks that support mDNS).
        You can also talk to your telegram bot from the bot chat window created above. Send /help for a list of commands. On boot, the system will send all status to your bot channel.
      
       
     
*/

#define VIRTUALKEYPAD //comment if you do not want/need the virtualkeypad functionality
#define useOTA //comment this to disable OTA updates. 

#include <Arduino.h>

#ifdef useOTA

#include <ArduinoOTA.h>

#endif

#if defined(VIRTUALKEYPAD)

#include <ESPmDNS.h>

#include <AsyncTCP.h>

#include <ESPAsyncWebServer.h>

#include <AESLib.h>

#endif

#include <SPIFFS.h>

#include <WiFi.h>

#include <ArduinoJson.h>

#include <string>

#include <list>


#define DEBUG 1

#define ARDUINO_MQTT

#include "vistaalarm.h"

#include "telegram_async.h" //telegram notify full async plugin with inbound bot cmd capability

//start user config


const char * wifiSSID = ""; //name of wifi access point to connect to
const char * wifiPassword = "";
const char * clientName = "vistaKeypad"; //WIFI client name
const char * telegramBotToken=""; // Set the Telegram bot access token
const char * telegramUserID="1234567890"; // Set the default Telegram chat recipient user/group ID
const char * telegramMsgPrefix="[Alarm Panel] "; // Set a prefix for all messages
std::list<String> telegramAllowedIDs = {}; //comma separated list of additional telegram ids with access to bot.  Can include channel id.
std::list<int> notifyZones = {}; //comma separated list of zones that you want push notifications on change
String password = "YourSecretPass"; // login and AES encryption/decryption password. Up to 16 characters accepted.
String accessCode = "1234"; // An access code is required to arm (unless quick arm is enabled)
String otaAccessCode = ""; // Access code for OTA uploading


const int monitorPin = 18;
const int rxPin = 22;
const int txPin = 21;

const int defaultPartition = 1;
const int maxPartitions = 1;
const int maxZones = 48;

// Assign a new virtual keypad address to each active partition that you wish to monitor  using programs *190 - *196 #and enter it below.  For unused partitions, use 0 as the keypad address.
const int keypadAddr1 = 17; //partition 1 virtual keyapd
const int keypadAddr2 = 0; //partition 2 virtual keypad. set to 0 to disable
const int keypadAddr3 = 0; //partition 3 virtual keypad. set to 0 to disable

/*
  # module addresses:
  # assign zones in program *56
  Suggested program *56 settings for extended zones:  
    Zone Type: 01
    Partition: 1
    Report Code: 1st 01 2nd 00 10
    Input Type: 2 Aux Wire  
  # 07 4229/4219 zone expander  zones 9-16
  # 08 4229/4219 zone expander zones 17-24
  # 09 4229/4219 zone expander zones 25-32
  # 10 4229/4219 zone expander zones 33-40
  # 11 4229/4219 zone expander zones 41 48
  # 12 4204 relay module  
  # 13 4204 relay module
  # 14 4204 relay module
  # 15 4204 relay module
*/
const int expanderAddr1 = 0;
const int expanderAddr2 = 0;

//relay module emulation (4204) addresses. Set to 0 to disable
const int relayAddr1 = 0;
const int relayAddr2 = 0;
const int relayAddr3 = 0;
const int relayAddr4 = 0;

const int TTL = 30000;
const bool quickArm = false;
const bool lrrSupervisor = false;

/*list of RF serial numbers with associated zone and bitmask. 
  Format: "serial1#:zone1:mask1,serial2#:zone2:mask2" 
  Mask: hex value used to mask out open/close bit from RF returned value
  */
const char * rfSerialLookup = "0019994:66:80,0818433:22:80,0123456:55:80"; //serial1:zone1:mask1,#serial2:zone2:mask2

uint8_t notificationFlag = 1 + 2 + 4; //which events; bit 1=zones, bit 2=status, bit 3= events, bit 4 = messages, bit 5=light states, bit 6 = relay and rf messages
//end user config

const char *
  const telegramMenu[] PROGMEM = {
    "/help - this command",
    "/armstay - arm partition in stay mode",
    "/armaway - arm parition in away mode",
    "/armnight - arm partition in night mode",
    "/disarm - disarm partition",        
    "/bypass - bypass all open zones in partition",
    "/reboot - reboot esp",
    "/!<keys> - send cmds direct to panel",
    "/getstatus - get zone/system/light statuses",
    "/getstats - get memory useage stats",
    "/stopbus - stop vista bus",
    "/startbus - start vista bus",
    "/stopnotify - pause notifications",
    "/startnotify - unpause notifications",
    "/setpartition=<partition> - set active partition",
    "/&<p><keys> - send cmds to partition p",
    "/addzones=<zone>,<zone> - add zones to notify list",
    "/removezones=<zone>,<zone> - remove zones from notify list",
    "/addids=<id>,<id> - add telegram ids to allowed control list",
    "/removeids=<id>,<id> - remove telegram ids from allowed list",
    "/getip - get url of keypad",
    "/getcfg - list notify and telegram ids",
    "/setnotifyflag=<flag> - sum of digits: zones = 1 , status = 2 , messages = 4 , events = 8 , light statuses = 16",
    "/setpassword=<password> - new keypad access password",
    "/setaccesscode=<accesscode> - new panel arm/disarm access code",
    "/setotaaccesscode=<otaaccesscode> - new OTA upload access code"        
  };

#if defined(VIRTUALKEYPAD)
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
#endif

unsigned long pingTime;
bool pauseNotifications;
uint8_t activePartition = defaultPartition;

PushLib pushlib(telegramBotToken, telegramUserID, telegramMsgPrefix);

vistaECPHome * VistaECP;

#if defined(VIRTUALKEYPAD)
AES aes;
char key[16]{
'0',
'0',
'0',
'0',
'0',
'0',
'0',
'0',
'0',
'0',
'0',
'0',
'0',
'0',
'0',
'0'
};
char * aeskey=key;

byte ivaes[N_BLOCK] = {
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0
};
#endif

bool inListTelegramID(String id) {

  auto it = std::find(telegramAllowedIDs.begin(), telegramAllowedIDs.end(), id);
  return it == telegramAllowedIDs.end() ? false : true;

}

bool inListZone(byte zone) {

  auto it = std::find(notifyZones.begin(), notifyZones.end(), zone);
  return it == notifyZones.end() ? false : true;

}

void pushNotification(String text, String receiverid = "") {

  StaticJsonDocument < 300 > doc;
  doc["chat_id"] = receiverid != "" ? receiverid : (String) telegramUserID;
  doc["text"] = telegramMsgPrefix + text;
  pushlib.sendMessageDoc(doc);

}

void publishLcd(char * line1, char * line2, uint32_t id = 0) {
  #if defined(VIRTUALKEYPAD)
  if (ws.count()) {
    char outas[128];
    StaticJsonDocument < 200 > doc;
    if (line1 != NULL)
      doc["lcd_upper"] = line1;
    if (line2 != NULL)
      doc["lcd_lower"] = line2;
    serializeJson(doc, outas);
    String out = encrypt(outas);
    if (id)
      ws.text(id, out.c_str());
    else
      ws.textAll(out.c_str());
  }
  #endif

}

void publishMsg(const char * stateName, String msg, uint32_t id = 0) {
  #if defined(VIRTUALKEYPAD)
  char outas[128];
  StaticJsonDocument < 200 > doc;
  doc[stateName] = msg.c_str();
  serializeJson(doc, outas);
  String out = encrypt(outas);
  if (id)
    ws.text(id, out.c_str());
  else
    ws.textAll(out.c_str());
  #endif
}

void publishStatus(const char * stateName, uint8_t state, uint32_t id = 0) {
  #if defined(VIRTUALKEYPAD)
  if (ws.count()) {
    char outas[128];
    StaticJsonDocument < 200 > doc;
    doc[stateName] = state;
    serializeJson(doc, outas);
    String out = encrypt(outas);
    if (DEBUG > 1)
      Serial.printf("publishstat: %s\n", out.c_str());
    if (id)
      ws.text(id, out.c_str());
    else
      ws.textAll(out.c_str());

  }
  #endif
}

void setup() {

  Serial.begin(115200);
  delay(1000);
  Serial.println();
  SPIFFS.begin();  
  readConfig();
  memset(key,'0',16);
  for (int x=0;x<password.length() && x < 16;x++)  {
      key[x]=password[x];
  }
  // pinMode(LED_BUILTIN, OUTPUT); // LED pin as output.

  WiFi.mode(WIFI_STA);
  WiFi.begin(wifiSSID, wifiPassword);

  #if defined(VIRTUALKEYPAD)
  aes.setPadMode(paddingMode::CMS);
  aes.set_key((byte * ) aeskey, 128);
  #endif

  uint8_t checkCount = 20;
  while (WiFi.status() != WL_CONNECTED) {
    Serial.printf("Connecting to Wifi..%d\n", checkCount);
    delay(1000);
    if (checkCount--) continue;
    checkCount = 50;
    WiFi.reconnect();

  }
  //WiFi.setAutoReconnect(true);
  //WiFi.persistent(true);

  #if defined(VIRTUALKEYPAD)
  if (!MDNS.begin(clientName)) {
    Serial.println(F("Error setting up MDNS responder."));
    while (1) {
      delay(1000);
    }
  }
  File root = SPIFFS.open("/");

  File file = root.openNextFile();
  Serial.println(F("Opening SPIFFS"));

  while (file) {
    Serial.print(F("FILE: "));
    Serial.println(file.name());
    file = root.openNextFile();

  }
  
  ws.onEvent(onWsEvent);
  server.addHandler( & ws);
  server.serveStatic("/", SPIFFS, "/").setDefaultFile("index.html");
  server.begin();
  MDNS.addService("http", "tcp", 80);
  Serial.print(F("Web server started: http://"));
  Serial.print(clientName);
  Serial.println(F(".local"));
  #endif

  #ifdef useOTA
  // Port defaults to 8266
  ArduinoOTA.setPort(3232); //port 3232 needed for spiffs OTA upload

  // Hostname defaults to esp8266-[ChipID]
  ArduinoOTA.setHostname(clientName);

  // No authentication by default
  ArduinoOTA.setPassword(otaAccessCode.c_str());

  ArduinoOTA.onStart([]() {
    pushlib.stop();
    vista.stop();
    Serial.println(F("Start"));
  });
  ArduinoOTA.onEnd([]() {
    pushlib.begin();
    Serial.println(F("\nEnd"));
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    delay(1);
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println(F("Auth Failed"));
    else if (error == OTA_BEGIN_ERROR) Serial.println(F("Begin Failed"));
    else if (error == OTA_CONNECT_ERROR) Serial.println(F("Connect Failed"));
    else if (error == OTA_RECEIVE_ERROR) Serial.println(F("Receive Failed"));
    else if (error == OTA_END_ERROR) Serial.println(F("End Failed"));
    VistaECP -> begin();
    pushlib.begin();
  });

  ArduinoOTA.begin();
  #endif

  Serial.println(F("Ready"));
  Serial.print(F("IP address: "));
  Serial.println(WiFi.localIP());

  VistaECP = new vistaECPHome(keypadAddr1, rxPin, txPin, monitorPin, maxZones, maxPartitions);
  VistaECP -> partitionKeypads[1] = keypadAddr1;
  VistaECP -> partitionKeypads[2] = keypadAddr2;
  VistaECP -> partitionKeypads[3] = keypadAddr3;
  VistaECP -> rfSerialLookup = rfSerialLookup;
  VistaECP -> defaultPartition = defaultPartition;
  VistaECP -> accessCode = accessCode.c_str();
  VistaECP -> quickArm = quickArm;
  VistaECP -> expanderAddr1 = expanderAddr1;
  VistaECP -> expanderAddr2 = expanderAddr2;
  VistaECP -> relayAddr1 = relayAddr1;
  VistaECP -> relayAddr2 = relayAddr2;
  VistaECP -> relayAddr3 = relayAddr3;
  VistaECP -> relayAddr4 = relayAddr4;
  VistaECP -> lrrSupervisor = lrrSupervisor;
  VistaECP -> TTL = TTL;
  VistaECP -> debug = DEBUG;

  VistaECP -> onSystemStatusChange([ & ](std::string statusCode, uint8_t partition) {
    if (!VistaECP -> forceRefresh && !pauseNotifications && statusCode != "" && (notificationFlag & 2)) {
      char msg[40];
      snprintf(msg, 40, "Partition %d status: %s", partition, statusCode.c_str());
      pushNotification(msg);
    }
  });

  VistaECP -> onStatusChange([ & ](sysState led, bool open, uint8_t partition) {
    if (partition == activePartition || !partition) {
      char msg[50] = "";
      switch (led) {
      case sfire:
        publishStatus("fire_status", open);
        snprintf(msg, 50, "Partition:%d Fire status is %s", partition, open ? "ON" : "OFF");
        break;
      case salarm:
        publishStatus("alarm_status", open);
        snprintf(msg, 50, "Partition:%d Alarm status is %s", partition, open ? "ON" : "OFF");
        break;
      case strouble:
        publishStatus("trouble_status", open);
        snprintf(msg, 50, "Partition:%d Trouble status is %s", partition, open ? "ON" : "OFF");
        break;
      case sarmedstay:
        publishStatus("armedstay_status", open);
        snprintf(msg, 50, "Partition:%d Armed Stay is %s", partition, open ? "ON" : "OFF");
        break;
      case sarmedaway:
        publishStatus("armedaway_status", open);
        snprintf(msg, 50, "Partition:%d Armed Away is %s", partition, open ? "ON" : "OFF");
        break;
      case sinstant:
        publishStatus("instant_status", open);
        snprintf(msg, 50, "Partition:%d Armed Instant is %s", partition, open ? "ON" : "OFF");
        break;
      case sready:
        publishStatus("ready_status", open);
        snprintf(msg, 50, "Partition:%d Ready status is %s", partition, open ? "ON" : "OFF");
        break;
      case sac:
        publishStatus("ac_status", open);
        snprintf(msg, 50, "Partition:%d AC is %s", partition, open ? "ON" : "OFF");
        break;
      case sbypass:
        publishStatus("bypass_status", open);
        snprintf(msg, 50, "Partition:%d Bypass is %s", partition, open ? "ON" : "OFF");
        break;
      case schime:
        publishStatus("chime_status", open);
        snprintf(msg, 50, "Partition:%d Chime is %s", partition, open ? "ON" : "OFF");
        break;
      case sbat:
        publishStatus("battery_status", open);
        snprintf(msg, 50, "Partition:%d Battery status is %s", partition, open ? "ON" : "OFF");
        break;
      case sarmednight:
        publishStatus("armednight_status", open);
        snprintf(msg, 50, "Partition:%d Armed Night is %s", partition, open ? "ON" : "OFF");
        break;
      case sarmed:
        publishStatus("armed_status", open);
        snprintf(msg, 50, "Partition:%d Armed status is %s", partition, open ? "ON" : "OFF");
        break;
      case soffline:
        break;
      case scheck:
        break;
      case sdisarmed:
        break;
      case striggered:
        break;
      case sunavailable:
        break;
      default:
        break;
      }
      if (!VistaECP -> forceRefresh && !pauseNotifications && (notificationFlag & 16))
        pushNotification(msg);
    }

  });

  VistaECP -> onZoneExtendedStatusChange([ & ](std::string msg) {
    publishMsg("zone_info", msg.c_str());
  });
  VistaECP -> onLine1DisplayChange([ & ](std::string msg, uint8_t partition) {
    if (partition == activePartition)
      publishLcd((char * ) msg.c_str(), NULL);
  });
  VistaECP -> onLine2DisplayChange([ & ](std::string msg, uint8_t partition) {
    if (partition == activePartition )
      publishLcd(NULL, (char * ) msg.c_str());

  });
  VistaECP -> onLrrMsgChange([ & ](std::string msg) {

    if (!VistaECP -> forceRefresh && msg != "" && !pauseNotifications && (notificationFlag & 4))
      pushNotification(msg.c_str());
    publishMsg("event_info", msg.c_str());
  });

  VistaECP -> onBeepsChange([ & ](std::string beeps, uint8_t partition) {

  });

  VistaECP -> onRfMsgChange([ & ](std::string msg) {
    if (!VistaECP -> forceRefresh && msg != "" && !pauseNotifications && (notificationFlag & 32))
      pushNotification(msg.c_str());
    publishMsg("event_info", msg.c_str());
  });

  VistaECP -> onZoneStatusChange([ & ](int zone, std::string open) {
    //if (inListZone(zone) && !VistaECP->forceRefresh && !pauseNotifications) {
    // char msg[100];
    //  snprintf(msg, 100, "Zone %d is now %s ", zone,open.c_str()); 
    //  pushNotification(msg);
    //}

  });

  VistaECP -> onZoneStatusChangeBinarySensor([ & ](int zone, bool open) {
    if (inListZone(zone) && !VistaECP -> forceRefresh && !pauseNotifications && (notificationFlag && 1)) {
      char msg[100];
      snprintf(msg, 100, "Zone %d is now %s ", zone, open ? "OPEN" : "CLOSED");
      pushNotification(msg);
    }

  });
  VistaECP -> onRelayStatusChange([ & ](uint8_t addr, int channel, bool open) {
    if (!pauseNotifications && (notificationFlag & 32)) {
      char msg[100];
      snprintf(msg, 100, "Relay %d:%d is now %s ", addr, channel, open ? "OPEN" : "CLOSED");
      pushNotification(msg);
    }
    //zone follower when relayaddress1 , channels 1 to 4 on, sets zones 1 to 4 on
    // when relayaddress2, channels 1 to 4 on sets zones 5 to 8 on
    // switch(addr) {
    //text zone sensor
    //case relayAddr1:  snprintf(msg, 30, "Zone %d is now %s",channel,open?"ON":"OFF");break ;
    //case relayAddr2:  snprintf(msg, 30, "Zone %d is now %s",channel+4,open?"ON":"OFF");break ;

    // }
  });

  VistaECP -> begin();

  pauseNotifications = false;
  #ifdef TELEGRAM_PUSH
  pushlib.addCmdHandler( & cmdHandler);
  #endif
  pushlib.begin();
  pushNotification("System restarted");

}

void loop() {

  static unsigned long previousWifiTime;
  if (WiFi.status() != WL_CONNECTED && millis() - previousWifiTime >= 20000) {
    Serial.println(F("Reconnecting to WIFI network"));
    WiFi.disconnect();
    WiFi.reconnect();
    previousWifiTime = millis();

  }

  #if defined(VIRTUALKEYPAD)
  if (millis() - pingTime > 300000 && ws.count() > 0) {
    ws.pingAll();
    pingTime = millis();
  }
  #endif

  VistaECP -> loop();
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
  pushlib.loop();

  #ifdef useOTA
  if (!pushlib.isSending()) //they conflict (when using ASYNC tasks) so we only check for ota when not pushing out a message
    ArduinoOTA.handle();
  #endif

}
#if defined(VIRTUALKEYPAD)
void onWsEvent(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t * data, size_t len) {

  if (type == WS_EVT_CONNECT) {
    client -> printf("{\"connected_id\": %u}", client -> id());
    Serial.printf("ws[%s][%u] connected\n", server -> url(), client -> id());

    if (vista.keybusConnected && ws.count()) {
      publishLcd((char * )
        "Vista bus connected",(char*)"");
      VistaECP -> forceRefresh = true;
    } else
    if (!vista.keybusConnected && ws.count()) {
      publishLcd((char * )
        "Vista bus disconnected",(char*) "");
    }
    //client->ping();
    pingTime = millis();
  } else if (type == WS_EVT_DISCONNECT) {
    Serial.printf("ws[%s][%u] disconnect\n", server -> url(), client -> id());

  } else if (type == WS_EVT_ERROR) {
    Serial.printf("ws[%s][%u] error(%u): %s\n", server -> url(), client -> id(), *((uint16_t * ) arg), (char * ) data);
  } else if (type == WS_EVT_PONG) {
    Serial.printf("ws[%s][%u] pong[%u]: %s\n", server -> url(), client -> id(), len, (len) ? (char * ) data : "");
  } else if (type == WS_EVT_DATA) {
    AwsFrameInfo * info = (AwsFrameInfo * ) arg;
    String msg = "";
    if (info -> final && info -> index == 0 && info -> len == len) {
      //the whole message is in a single frame and we got all of it's data
      Serial.printf("ws[%s][%u] %s-message[%llu]: ", server -> url(), client -> id(), (info -> opcode == WS_TEXT) ? "text" : "binary", info -> len);
      if (info -> opcode == WS_TEXT) {
        for (size_t i = 0; i < info -> len; i++) {
          msg += (char) data[i];
        }
      }
      Serial.printf("%s\n", msg.c_str());
      if (info -> opcode == WS_TEXT) {
        msg = decrypt(msg);
        StaticJsonDocument < 200 > doc;
        auto err = deserializeJson(doc, msg);
        if (!err) {
          if (doc.containsKey("btn_single_click")) {
            char * tmp = (char * ) doc["btn_single_click"].as <
              const char * > ();
            char *
              const sep_at = strchr(tmp, '_');
            if (sep_at != NULL) {
              * sep_at = '\0';

              char * v = sep_at + 1;
              if (vista.keybusConnected) {

                if (strcmp(v, "s") == 0) {
                  VistaECP -> set_alarm_state("S",std::string(accessCode.c_str()), activePartition);
                } else if (strcmp(v, "w") == 0) {
                  VistaECP -> set_alarm_state("A", std::string(accessCode.c_str()),activePartition);
                } else if (strcmp(v, "c") == 0) {
                  VistaECP -> alarm_keypress_partition(std::string(accessCode.c_str()), activePartition);
                  VistaECP -> alarm_keypress_partition("9", activePartition);
                } else if (strcmp(v, "x") == 0) {
                  VistaECP -> alarm_keypress_partition("#", activePartition);
                } else if (strcmp(v, "p1") == 0) {
                  setActivePartition(1);
                } else if (strcmp(v, "p2") == 0) {
                  setActivePartition(2);
                } else if (strcmp(v, "p3") == 0) {
                  setActivePartition(3);
                } else VistaECP -> alarm_keypress_partition(v, activePartition);
              }
              Serial.printf("got key %s\n", v);
            }
          }
        }
      }
    } else {
      //message is comprised of multiple frames or the frame is split into multiple packets
      if (info -> index == 0) {
        if (info -> num == 0) {
          Serial.printf("ws[%s][%u] %s-message start\n", server -> url(), client -> id(), (info -> message_opcode == WS_TEXT) ? "text" : "binary");
        }
        Serial.printf("ws[%s][%u] frame[%u] start[%llu]\n", server -> url(), client -> id(), info -> num, info -> len);
      }

      Serial.printf("ws[%s][%u] frame[%u] %s[%llu - %llu]: ", server -> url(), client -> id(), info -> num, (info -> message_opcode == WS_TEXT) ? "text" : "binary", info -> index, info -> index + len);

      if (info -> opcode == WS_TEXT) {
        for (size_t i = 0; i < info -> len; i++) {
          msg += (char) data[i];
        }
      } else {
        char buff[3];
        for (size_t i = 0; i < info -> len; i++) {
          sprintf(buff, "%02x ", (uint8_t) data[i]);
          msg += buff;
        }
      }

      Serial.printf("%s\n", msg.c_str());

      if ((info -> index + len) == info -> len) {
        Serial.printf("ws[%s][%u] frame[%u] end[%llu]\n", server -> url(), client -> id(), info -> num, info -> len);
        if (info -> final) {
          Serial.printf("ws[%s][%u] %s-message end\n", server -> url(), client -> id(), (info -> message_opcode == WS_TEXT) ? "text" : "binary");
          if (info -> message_opcode == WS_TEXT) client -> text("I got your text message");
          else client -> binary("I got your binary message");
        }
      }
    }
  }
}

String encrypt(String msg) {
  char b64data[200];
  byte cipher[200];
  char ivdata[40];
  gen_iv(ivaes);
  base64_encode(ivdata, (char * ) ivaes, 16);

  aes.do_aes_encrypt((byte * ) msg.c_str(), msg.length(), cipher, (byte * ) aeskey, 128, ivaes);
  base64_encode(b64data, (char * ) cipher, aes.get_size());
  char outmsg[200];
  sprintf(outmsg, "{\"iv\":\"%s\",\"data\":\"%s\"}", ivdata, b64data);
  return (String) outmsg;
}

String decrypt(String wsmsg) {
  StaticJsonDocument < 300 > doc;
  auto err = deserializeJson(doc, wsmsg);
  if (!err) {
    std::string eiv, emsg;
    if (doc.containsKey("iv")) {
      eiv = doc["iv"].as < std::string > ();
    }
    if (doc.containsKey("data")) {
      emsg = doc["data"].as < std::string > ();
    }
    char data_decoded[200];
    char iv_decoded[40];
    char out[200];
    int encrypted_length = base64_decode(data_decoded, (char * ) emsg.c_str(), emsg.length());
    base64_decode(iv_decoded, (char * ) eiv.c_str(), eiv.length());
    aes.do_aes_decrypt((byte * ) data_decoded, encrypted_length, (byte * ) out, (byte * ) aeskey, 128, (byte * ) iv_decoded);
    int len = aes.get_size() - out[aes.get_size() - 1]; //remove padding
    out[len] = '\0';
    return out;
  } else return "";
}

uint8_t getrnd() {
  uint8_t rand = (uint8_t) random(0, 0xff);
  return rand;
}

// Generate a random initialization vector
void gen_iv(byte * iv) {
  for (int i = 0; i < N_BLOCK; i++) {
    iv[i] = (byte) getrnd();
  }
}
#endif

void setActivePartition(uint8_t partition) {

  if (partition < 1 || partition > maxPartitions) return;
  char msg[30];
  activePartition = partition;
  publishLcd((char*)VistaECP->partitionStates[partition - 1].lastp1.c_str(),(char*)VistaECP->partitionStates[partition - 1].lastp2.c_str());
  VistaECP -> defaultPartition = partition;
  VistaECP -> forceRefreshGlobal = true;
  publishStatus("armed_status",VistaECP->partitionStates[partition - 1].previousLightState.armed);
  publishStatus("ready_status",VistaECP->partitionStates[partition - 1].previousLightState.ready);
  publishStatus("ac_status",VistaECP->partitionStates[partition - 1].previousLightState.ac);
  publishStatus("trouble_status",VistaECP->partitionStates[partition - 1].previousLightState.trouble);
  publishStatus("chime_status",VistaECP->partitionStates[partition - 1].previousLightState.chime);    
  sprintf(msg, "Partition: %d", partition);
  publishMsg("event_info", msg);
  Serial.printf("%s\n", msg);
}

void readConfig() {

  Serial.println(F("Reading config)"));

  File file = SPIFFS.open("/configFile", "r");
  if (!file || file.isDirectory()) {
    Serial.println(F("Failed to open file for reading"));
    return;
  }
  StaticJsonDocument < 300 > doc;
  DeserializationError error = deserializeJson(doc, file);

  if (error) {
    Serial.println(F("Failed to parse cred file"));
    if (file) file.close();
    return;
  }
  
  if (doc.containsKey("ids")) {
    JsonArray ids = doc["ids"];
    telegramAllowedIDs.clear();
    for (String id: ids) {
      telegramAllowedIDs.push_back(id);
      Serial.printf("IDS %s \n", id.c_str());
    }
  }
  if (doc.containsKey("zones")) {
    JsonArray zones = doc["zones"];
    notifyZones.clear();
    for (int z: zones) {
      notifyZones.push_back(z);
      Serial.printf("Zone read is %d\n", z);
    }
  }
  if (doc.containsKey("notificationflag")) {
    notificationFlag = (uint8_t) doc["notificationflag"];
  }
  if (doc.containsKey("password")) {
    password = doc["password"].as<String>();
  }  
  if (doc.containsKey("otaaccesscode")) {
    otaAccessCode = doc["otaaccesscode"].as<String>();
  } 
  if (doc.containsKey("accesscode")) {
    accessCode = doc["accesscode"].as<String>();
  }    
  if (file) file.close();

}

void printConfig() {
  File file = SPIFFS.open("/configFile", "r");
  if (!file || file.isDirectory()) {
    Serial.println(F("Failed to open file for reading"));
    return;
  }
  Serial.println("Config file contents: ");
  
  while(file.available()){
 
        Serial.write(file.read());
    }

  if (file) file.close();
}

void writeConfig() {
  Serial.println(F("Writing config"));

  File file = SPIFFS.open("/configFile", FILE_WRITE);

  if (!file) {
    Serial.println(F("Failed to open file for writing"));
    return;
  }
  StaticJsonDocument < 300 > doc;
  StaticJsonDocument < 100 > zones;
  StaticJsonDocument < 100 > ids;

  for (int z: notifyZones) {
    Serial.printf("Notify zone: %d\n", z);
    zones.add(z);
  }

  for (String id: telegramAllowedIDs) {
    Serial.printf("Allowed ids: %s\n", id.c_str());
    ids.add(id);
  }

  doc["zones"] = zones;
  doc["ids"] = ids;
  doc["notificationflag"] = notificationFlag;
  doc["password"]=password;
  doc["accesscode"]=accessCode;
  doc["otaaccesscode"]=otaAccessCode;  
  
  String out;
  serializeJson(doc, out);
  Serial.printf("Serialized=%s,size=%d\n", out.c_str(),out.length());

  if (file.print(out)) {
    Serial.println(F("File written"));
  } else {
    Serial.println(F("Write failed"));
  }
  if (file) file.close();

}

#ifdef TELEGRAM_PUSH
//used with telegram to handle incoming cmds

String getZoneStatus() {
  String s = "<b>Zone statuses:</b> \n";
  for (int x = 1; x <= MAX_ZONES; x++) {
    char out[20];
    out[0] = '\0';
    if (VistaECP -> zones[x].open)
      sprintf(out, "zone %d : open\n", x);
    if (VistaECP -> zones[x].bypass)
      sprintf(out, "zone %d : bypass\n", x);
    if (VistaECP -> zones[x].alarm)
      sprintf(out, "zone %d : alarm\n", x);
    //  if (VistaECP->zones[x].trouble)
    //   sprintf(out, "zone %d : trouble\n", x);
    if (VistaECP -> zones[x].fire)
      sprintf(out, "zone %d : fire\n", x);
    s = s + String(out);;
  }
  return s;
}

String getPartitionStatus() {
  String s = "";
  for (int p = 1; p < 4; p++) {
    //if (!VistaECP->partitionStates[p-1].active) continue;     
    s = s + "<b>Partition " + (String) p + " status:</b> \n";

    switch (VistaECP -> partitionStates[p - 1].previousSystemState) {
    case striggered:
      s = s + "Panel alarm triggered\n";
      break;
    case sarmedaway:
      s = s + "Panel armed away\n";
      break;
    case sarmednight:
      s = s + "Panel armed night\n";
      break;
    case sarmedstay:
      s = s + "Panel armed stay\n";
      break;
    case sinstant:
      s = s + "Panel armed instant\n";
      break;
    case sunavailable:
      s = s + "Panel not ready\n";
      break;
    case sdisarmed:
      s = s + "Panel disarmed/ready\n";
      break;
    default:
      s = s + "Panel not ready\n";
      break;
    }
  }
  return s;
}
String getPanelStatus() {
  String s = "";
   s = s + "<b>System lights: </b>\n";
  if (!vista.statusFlags.acPower) 
      s = s + "NOAC|";
    else
      s = s + "ACOK|";
  if (vista.statusFlags.lowBattery ) 
      s = s + "BAT|";
   s=s+"\n";
  s += F("------------------------------\n");   
  for (int p = 1; p < 4; p++) {
    s = s + "<b>Partition " + (String) p + "status lights: </b>\n";
    if (VistaECP -> partitionStates[p - 1].previousLightState.ready)
      s = s + "Ready|";
    else if (VistaECP -> partitionStates[p - 1].previousLightState.armed)
      s = s + "Armed|";
    else
      s = s + "NotReady|";
    if (VistaECP->partitionStates[p-1].previousLightState.trouble)
     s = s + "Trouble|";
    if (VistaECP -> partitionStates[p - 1].previousLightState.fire)
      s = s + "Fire|";
    if (VistaECP -> partitionStates[p - 1].previousLightState.bypass)
      s = s + "Bypass|";
    if (VistaECP -> partitionStates[p - 1].previousLightState.chime)
      s = s + "CHM|";
    if (vista.statusFlags.programMode)
      s = s + "Program|";
    s = s + "\n";
  }
  return s;
}

void sendStatus(JsonDocument & doc) {
    String s = "\n" + getPanelStatus();
    s += F("------------------------------\n");
    s += getPartitionStatus();
    s += F("------------------------------\n");
    s += getZoneStatus();
    s += F("------------------------------\n");
    if (pauseNotifications)
      s += F("<b>Notifications:</b> DISABLED\n");
    else
      s += F("<b>Notifications:</b> ACTIVE\n");
    s += F("------------------------------\n");    
    s = s + F("<b>Notification flag:</b> ") + (String) notificationFlag + "\n";
    s += F("------------------------------\n");    
    s = s + F("<b>Active partition:</b> ") + (String) activePartition + " \n";
    doc["parse_mode"] = "HTML";
    doc["text"] = s;
    doc.remove("reply_markup"); //msg too long for markup        
    pushlib.sendMessageDoc(doc);

}

void sendCurrentConfig(JsonDocument & doc) {
  String config = F("<b>Notification zone list:</b>\n");
  String tmp="";
  for (int z: notifyZones) {
    if (tmp!="") tmp+=", ";    
    tmp += String(z);
  }
  config = config +  tmp + "\n" +  F("------------------------------\n");  
  config = config + F("<b>Allowed access Telegram IDs:</b>\n");
  tmp="";
  for (String id: telegramAllowedIDs) {
    if (tmp!="") tmp+=", ";
    tmp += id ;
  }
  config = config +  tmp+ "\n" +  F("------------------------------\n");  
  config = config +  F("<b>Notification flag:</b> ") + (String) notificationFlag + "\n";
  config = config +  String(F("------------------------------\n"));   
  config = config +  F("<b>Current OTA access code:</b> ")+ otaAccessCode + "\n";
  config = config +  F("------------------------------\n");  
#if defined(useWT32ETHERNET)
    config= config +  F("<b>Local IP address:</b> http://") + ETH.localIP().toString() + "\n";
#else    
    config= config +  F("<b>Local IP address:</b> http://") + WiFi.localIP().toString() + "\n";
#endif 
  doc["text"] = config;
  doc["parse_mode"] = "HTML";  
  pushlib.sendMessageDoc(doc);

}

//telegram callback to handle bot commands
void cmdHandler(rx_message_t * msg) {

  if (!inListTelegramID(msg -> chat_id) && strcmp(msg -> chat_id.c_str(), telegramUserID) != 0) {
    Serial.printf("Chat ID %s not allowed to send cmds", msg -> chat_id.c_str());
    return;
  }
  const char * markup PROGMEM = "{'reply_markup':{'inline_keyboard':[[{'text': '1','callback_data':'1'},{'text': '2','callback_data':'2'},{'text': '3','callback_data':'3'}],      [{'text':'4','callback_data':'4'},{'text': '5','callback_data':'5'},{'text':'6','callback_data':'6'}],      [{'text':'7','callback_data':'7'},{'text': '8','callback_data':'8'},{'text':'9','callback_data':'9'}], [ { 'text': '*', 'callback_data' : '*' }, {'text' :'0', 'callback_data' : '0' },{ 'text' : '#', 'callback_data' : '#' }] , [ { 'text' :'<', 'callback_data' : '<' }, { 'text' : 'ENTER', 'callback_data' : 'ENTER' },{ 'text' : '>', 'callback_data' : '>' }] ]}}";

  static bool firstRun = true;
  StaticJsonDocument < 2000 > doc;
  doc["chat_id"] = msg -> chat_id;
  String sub = msg -> text.substring(0, 2);
  static String command = "";
  if (firstRun) {
      firstRun=false;
      if (msg -> text != "/reboot") {
        doc["text"] = String(F("First command ignored on initial start. Please send your command again: ")) +  msg->text;
        pushlib.sendMessageDoc(doc); 
      }      
      return;
  }
  if (msg -> is_callback) {
    Serial.printf("Callback message=%s\n", msg -> text.c_str());
    if (msg -> text == "ENTER") {
      // doc["text"]=command;
      // doc["message_id"]=msg->message_id;
      // pushlib.sendMessageDoc(doc,"/editMessageText");
      command = "";
    } else {
      std::string s(msg -> text.c_str());
      VistaECP -> alarm_keypress_partition(s, activePartition);
      command = command + msg -> text;
      deserializeJson(doc, markup);
      doc["chat_id"] = msg -> chat_id;
      doc["text"] = command;
      doc["message_id"] = msg -> message_id;
      pushlib.sendMessageDoc(doc, "/editMessageText");
      //doc["callback_query_id"]=msg->id;
      //pushlib.sendMessageDoc(doc,"/answerCallbackQuery");
    }
    
  } else if (sub == "/#") {
    command = "";
    deserializeJson(doc, markup);
    doc["chat_id"] = msg -> chat_id;
    doc["text"] = "Enter keys";
    pushlib.sendMessageDoc(doc);

  } else
  if (msg -> text == "/armstay") {
      doc["text"] = F("setting armed stay...");
      pushlib.sendMessageDoc(doc);
      VistaECP -> set_alarm_state("S", std::string(accessCode.c_str()), activePartition);

  } else if (msg -> text == "/armaway") {
      doc["text"] = F("setting armed away...");
      pushlib.sendMessageDoc(doc);
      VistaECP ->set_alarm_state("A", std::string(accessCode.c_str()), activePartition);
    
  } else if (msg -> text == "/armnight") {
      doc["text"] = F("setting armed night...");
      pushlib.sendMessageDoc(doc);
      VistaECP -> set_alarm_state("n", std::string(accessCode.c_str()), activePartition);
    
  } else if (msg -> text == "/disarm") {
     if (VistaECP -> partitionStates[activePartition - 1].previousLightState.armed) {
        doc["text"] = F("disarming...");
        VistaECP -> alarm_keypress_partition(std::string(accessCode.c_str()), activePartition);
     }  else
         doc["text"] = F("partition is not armed");
        pushlib.sendMessageDoc(doc);       
  } else if (msg -> text == "/bypass") {
     if (VistaECP -> partitionStates[activePartition - 1].previousLightState.armed) {
      doc["text"] = F("Setting bypass...");
      pushlib.sendMessageDoc(doc);
      VistaECP -> alarm_keypress_partition("*199#", activePartition);
    }
#if defined(VIRTUALKEYPAD)
  } else if (msg -> text.startsWith("/setpassword")) {
    String pstr = msg -> text.substring(msg -> text.indexOf('=') + 1, msg -> text.length());
    if (strcmp(pstr.c_str(),"") !=0) {
      password=pstr;
      memset(key,'0',16);
      for (int x=0;x<password.length() && x < 16;x++)  {
        key[x]=password[x];
      }
      aes.set_key((byte * ) aeskey, 128);
      writeConfig();
      char out[40];
      sprintf(out, "Keypad password is now set to %s", password.c_str());
      doc["text"] = String(out);
      pushlib.sendMessageDoc(doc);
    } 
#endif    
  } else if (msg -> text.startsWith("/setaccesscode")) {
    String pstr = msg -> text.substring(msg -> text.indexOf('=') + 1, msg -> text.length());
    if (strcmp(pstr.c_str(),"") !=0) {
      accessCode=pstr;
      VistaECP -> accessCode = accessCode.c_str();      
      writeConfig();
      char out[40];
      sprintf(out, "Panel access code is now set to %s", pstr.c_str());
      doc["text"] = String(out);
      pushlib.sendMessageDoc(doc);
    }   
    
   } else if (msg -> text.startsWith("/setotaaccesscode")) {
    String pstr = msg -> text.substring(msg -> text.indexOf('=') + 1, msg -> text.length());
      otaAccessCode=pstr;
      ArduinoOTA.setPassword(otaAccessCode.c_str());      
      writeConfig();
      char out[40];
      sprintf(out, "OTA access code is now set to %s", pstr.c_str());
      doc["text"] = String(out);
      pushlib.sendMessageDoc(doc);
   
  } else if (msg -> text == "/reboot" && !firstRun) {
    ESP.restart();
    
  } else if (msg -> text == "/getstatus") {
        sendStatus(doc);

  } else if (sub == "/!") {
    String cmd = msg -> text.substring(2, msg -> text.length());
    std::string s(cmd.c_str());
    Serial.printf("cmd = %s\n", cmd.c_str());
    VistaECP -> alarm_keypress_partition(s, activePartition);

  } else if (sub == "/&") {
    int p;
    String pstr = msg -> text.substring(2, 3);
    sscanf(pstr.c_str(), "%d", & p);
    String cmd = msg -> text.substring(3, msg -> text.length());
    std::string s(cmd.c_str());
    Serial.printf("cmd = %s,partition=%d,pstr=%s\n", cmd.c_str(), p, pstr.c_str());
    VistaECP -> alarm_keypress_partition(s, p);

  } else if (msg -> text == "/stopbus") {
    vista.stop();
    doc["text"] = F("Vista bus stopped...");
    pushlib.sendMessageDoc(doc);

  } else if (msg -> text == "/startbus") {
    vista.stop();
    VistaECP -> begin();
    doc["text"] = F("Vista bus started..");
    pushlib.sendMessageDoc(doc);

  } else if (msg -> text == "/stopnotify") {
    pauseNotifications = true;
    doc["text"] = F("Notifications paused..");
    pushlib.sendMessageDoc(doc);

  } else if (msg -> text == "/startnotify") {
    pauseNotifications = false;
    doc["text"] = F("Notifications un-paused..");
    pushlib.sendMessageDoc(doc);

  } else if (msg -> text == "/getstats") {
    char buf[100];
    snprintf(buf, 100,"\n<b>Memory Useage</b>\nFreeheap=%d\nMinFreeHeap=%d\nHeapSize=%d\nMaxAllocHeap=%d\n", ESP.getFreeHeap(), ESP.getMinFreeHeap(), ESP.getHeapSize(), ESP.getMaxAllocHeap());
    doc["parse_mode"] = "HTML";
    doc["text"] = String(buf);
    pushlib.sendMessageDoc(doc);

  } else if (msg -> text.startsWith("/setpartition")) {
    String pstr = msg -> text.substring(msg -> text.indexOf('=') + 1, msg -> text.length());
    int p;
    sscanf(pstr.c_str(), "%d", & p);
    if (p > 0 && p < 4) {
      setActivePartition(p);
      char out[40];
      sprintf(out, "Partition is now set to %d\n", p);
      doc["text"] = String(out);
      pushlib.sendMessageDoc(doc);
    }
    
  } else if (msg -> text.startsWith("/setnotifyflag")) {
    String pstr = msg -> text.substring(msg -> text.indexOf('=') + 1, msg -> text.length());
    int p;
    sscanf(pstr.c_str(), "%d", & p);
    if (p >= 0 && p < 256) {
      notificationFlag = p;
      writeConfig();
      char out[40];
      sprintf(out, "Nofitification flag set to %d\n", p);
      doc["text"] = String(out);
      pushlib.sendMessageDoc(doc);
    }

  } else if (msg -> text.startsWith("/addzones")) {
    String pstr = msg -> text.substring(msg -> text.indexOf('=') + 1, msg -> text.length());
    char * token = strtok((char * ) pstr.c_str(), ",");
    // loop through the string to extract all other tokens
    while (token != NULL) {
      int z;
      sscanf(token, "%d", & z);
      if (z > 0 && z <= maxZones) {
        if (!inListZone(z)) {
          notifyZones.push_back(z);
          writeConfig();
          char out[40];
          sprintf(out, "Added zone %d to notify list\n", z);
          doc["text"] = String(out);
          pushlib.sendMessageDoc(doc);
        }
      }
      token = strtok(NULL, ",");
    }
    sendCurrentConfig(doc);

  } else if (msg -> text.startsWith("/removezones")) {
    String pstr = msg -> text.substring(msg -> text.indexOf('=') + 1, msg -> text.length());
    char * token = strtok((char * ) pstr.c_str(), ",");
    // loop through the string to extract all other tokens
    while (token != NULL) {
      int z;
      sscanf(token, "%d", & z);
      if (z > 0 && z <= maxZones) {
        if (inListZone(z)) {
          notifyZones.remove(z);
          writeConfig();
          char out[40];
          sprintf(out, "Removed zone %d from notify list\n", z);
          doc["text"] = String(out);
          pushlib.sendMessageDoc(doc);
        }
      }
      token = strtok(NULL, ",");
    }
    sendCurrentConfig(doc);

  } else if (msg -> text.startsWith("/removeids")) {
    String pstr = msg -> text.substring(msg -> text.indexOf('=') + 1, msg -> text.length());
    char * token = strtok((char * ) pstr.c_str(), ",");
    // loop through the string to extract all other tokens
    while (token != NULL) {
      if (strcmp(token, "") != 0) {
        if (inListTelegramID(String(token))) {
          telegramAllowedIDs.remove(String(token));
          writeConfig();
          char out[40];
          sprintf(out, "Removed id %s from ID list\n", token);
          doc["text"] = String(out);
          pushlib.sendMessageDoc(doc);
        }
      }
      token = strtok(NULL, ",");
    }
    sendCurrentConfig(doc);

  } else if (msg -> text.startsWith("/addids")) {
    String pstr = msg -> text.substring(msg -> text.indexOf('=') + 1, msg -> text.length());
    char * token = strtok((char * ) pstr.c_str(), ",");
    // loop through the string to extract all other tokens
    while (token != NULL) {
      if (strcmp(token, "") != 0) {
        if (!inListTelegramID(String(token))) {
          telegramAllowedIDs.push_back(String(token));
          writeConfig();
          char out[40];
          sprintf(out, "Added id %s to ID list\n", token);
          doc["text"] = String(out);
          pushlib.sendMessageDoc(doc);
        }
      }
      token = strtok(NULL, ",");
    }
    sendCurrentConfig(doc);

  } else if (msg -> text.startsWith("/getcfg")) {
    sendCurrentConfig(doc);
    printConfig();

  } else if (msg -> text.startsWith("/getip")) {
    char out[50];
#if defined(useWT32ETHERNET)
    sprintf(out, "Local IP address http://%s\n", ETH.localIP().toString().c_str());
#else    
    sprintf(out, "Local IP address http://%s\n", WiFi.localIP().toString().c_str());
#endif
    doc["text"] = String(out);
    pushlib.sendMessageDoc(doc);

  } else if (msg -> text == "/help") {
    String menu = "";
    int x = 1;
    for (auto s: telegramMenu) {
      menu = menu + String(x) + ". " + String(FPSTR(s)) + "\n";
      x++;
    }
    Serial.printf("menu is %s\n",menu.c_str());
    doc["text"] = menu;
    doc.remove("reply_markup"); //msg too long for markup          
    pushlib.sendMessageDoc(doc);

  }
  firstRun = false;
  doc.clear();
}

#endif