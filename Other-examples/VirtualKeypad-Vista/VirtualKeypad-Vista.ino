/*
    Virtual Keyapd for the Honeywell Vista alarm systems (esp8266/esp32)
    Provides a virtual keypad web interface using the esp32/esp8266 as a standalone web server using
    AES encrypted web socket communications. All keypad functionality provided as well as zone display.
    This sketch uses portions of the code from the VirtualKeypad-Web example for DSC alarm systems found in the taligent/dscKeybusInterface respository at:
    https://github.com/taligentx/dscKeybusInterface/blob/master/examples/esp32/VirtualKeypad-Web/VirtualKeypad-Web.ino.
    It was adapted to use the Vista alarm system library at: 
    https://github.com/Dilbert66/esphome-vistaECP/tree/master/src/vistaEcpInterface, with the addition of two way AES encryption for the web socket.
 
   Usage:
     1. Install the following libraries directly from each Github repository:
           ESPAsyncWebServer: https://github.com/me-no-dev/ESPAsyncWebServer
         i. For the ESP32 install:
          AsyncTCP: https://github.com/me-no-dev/AsyncTCP
         ii. For the ESP8266 install:
          ESPAsyncTCP: https://github.com/me-no-dev/ESPAsyncTCP
 
     2. Install the filesystem uploader tools to enable uploading web server files:
         i. For the ESP32 install:
          https://github.com/me-no-dev/arduino-esp32fs-plugin
         ii. For the ESP8266 install the ESP8266FS tool:
          https://arduino-esp8266.readthedocs.io/en/latest/filesystem.html#uploading-files-to-file-system 
 
     3. Install the following libraries, available in the Arduino IDE Library Manager and
        the Platform.io Library Registry:
          ArduinoJson: https://github.com/bblanchon/ArduinoJson
          Chrono: https://github.com/SofaPirate/Chrono
          AESLib: https://github.com/suculent/thinx-aes-lib

     4. Set the WiFi SSID and password as well as OTA, keypad access code.
         Also set the AES encryption password that will be used to login to the panel and also used as a key
         for encrypting all web socket communications between the ESP and the browser.
 
     5. If desired, update the DNS hostname in the sketch.  By default, this is set to
        "vistakeypad" and the web interface will be accessible at: http://vistakeypad.local
       
      6. Copy all .h and cpp files from the https://github.com/Dilbert66/esphome-vistaECP/tree/master/src/vistaEcpInterface repository location to the same directory where you placed the sketch or into a subdirectory within your arduino libraries folder.
 
     6. Upload the sketch.
 
     7. Upload the SPIFFS data containing the web server files (the "data" subdirectory contents):
         i. For the ESP32:
          Arduino IDE: Tools > ESP32 Sketch Data Upload
         ii. For the ESP8266:
          Arduino IDE: Tools > ESP8266 Sketch Data Upload 
 
     8. Access the virtual keypad web interface by the IP address displayed through
        the serial output or http://vistakeypad.local (for clients and networks that support mDNS).
 
     9. Once the sketch is loaded and running, any following updates can be done via OTA updates for initial testing.  
        See here for an example:  https://randomnerdtutorials.com/esp8266-ota-updates-with-arduino-ide-over-the-air/
        
        NOTE: I do not recommended leaving the ability to do OTA updates active on a production system. Once done
        testing, you should either disable it by commenting out "useOTA" or set a good passcode.
        Be aware that for uploading sketch data via OTA, you cannot have a password set. Once all testing is done, you can 
        then set your password of choice or disable the feature.. 
 

*/

#ifdef ESP32
#include <WiFi.h>
#include <ESPmDNS.h>
#include <AsyncTCP.h>
#include <SPIFFS.h>
#include <SPIFFSEditor.h>
#else
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>    
#include <ESPAsyncTCP.h>
#endif

#include <ESPAsyncWebServer.h>
#include <FS.h>
#include <ArduinoJson.h>
#include <Chrono.h>
#include <string>
#include <AESLib.h>

#include <ArduinoOTA.h>
#include  "vista.h"


#define useOTA //comment this out to disable OTA updates

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

//keypad address
#define KP_ADDR 17
#define DEBUG 1

// Settings
const char * wifiSSID = ""; //name of wifi access point to connect to
const char * wifiPassword = "";
const char * accessCode = "1234"; // An access code is required to arm (unless quick arm is enabled)
const char * otaAccessCode = ""; // Access code for OTA uploading
const char * clientName = "vistaKeypad";
const char * password="abcd";  // login and AES encryption/decryption password. 




const char *
  const FAULT = "FAULT"; //change these to suit your panel language 
const char *
  const BYPAS = "BYPAS";
const char *
  const ALARM = "ALARM";
const char *
  const FIRE = "FIRE";
const char *
  const CHECK = "CHECK";
const char *
  const KLOSED = "CLOSED";
const char *
  const OPEN = "OPEN";
const char *
  const ARMED = "ARMED";
const char *
  const HITSTAR = "Hit *";
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

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
Chrono ws_ping_pong(Chrono::SECONDS);

AES aes;

std::string key = std::string(password).append(16 - key.length(), '0');
char  *aeskey = &k[0];
byte ivaes[N_BLOCK]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

sysState currentSystemState, previousSystemState, emptySystemState;

uint8_t zone;
bool sent, vh;
char p1[18];
char p2[18];
char msg[50];
std::string lastp1;
std::string lastp2;
int lastbeeps;
unsigned long ledTime,refreshTime;
int lastLedState, upCount;


struct zoneDef {
  unsigned long time;
  bool open;
  bool fire;
  bool bypass;
  bool alarm;
  bool trouble;

};

//zoneDef zones[MAX_ZONES + 1];
zoneDef * zones=(zoneDef *) malloc((MAX_ZONES+1) * sizeof(zoneDef));

unsigned long lowBatteryTime;

struct alarmStatus {
  unsigned long time;
  bool state;
  uint8_t zone;
  char prompt[17];
};

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

std::string previousMsg;

alarmStatus fireStatus, panicStatus;
lrrType lrr, previousLrr,emptyLrr;
unsigned long asteriskTime, sendWaitTime;
bool forceZoneUpdate;


void setup() {

//Serial.setDebugOutput(true);

  Serial.begin(115200);
  delay(1000);
  Serial.println();
  forceZoneUpdate = true;
  pinMode(LED_BUILTIN, OUTPUT); // LED pin as output.
  vista.setKpAddr(KP_ADDR);
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifiSSID, wifiPassword);
  
  aes.setPadMode(paddingMode::CMS); 
  aes.set_key((byte*)aeskey, 128);

  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Connection Failed! Rebooting...");
    delay(500);
    // ESP.restart();
  }
  
  
  if (!MDNS.begin(clientName)) {
    Serial.println("Error setting up MDNS responder.");
    while (1) {
      delay(1000);
    }
  }

  SPIFFS.begin();
  ws.onEvent(onWsEvent);
  server.addHandler(&ws);
  server.serveStatic("/", SPIFFS, "/").setDefaultFile("index.html");
  server.begin();
  MDNS.addService("http", "tcp", 80);
  Serial.print(F("Web server started: http://"));
  Serial.print(clientName);
  Serial.println(F(".local"));

  // Port defaults to 8266
  ArduinoOTA.setPort(3232); //port 3232 needed for spiffs upload

  // Hostname defaults to esp8266-[ChipID]
 ArduinoOTA.setHostname(clientName);

  // No authentication by default
  ArduinoOTA.setPassword(otaAccessCode);

  ArduinoOTA.onStart([]() {
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
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  
  #ifdef useOTA
  ArduinoOTA.begin();
  #endif


  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());


  vista.begin(RX_PIN, TX_PIN, KP_ADDR, MONITOR_PIN);

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
  Serial.println(F("Vista ECP Interface is online."));

  ws_ping_pong.stop();

}




void publishLcd(char * line1, char * line2) {

      if (ws.count()) {
        char strZoneGroup[12];
        char outas[128];
        StaticJsonDocument<200> doc;
        JsonObject root = doc.to<JsonObject>();        
        root["lcd_upper"]=line1;
        root["lcd_lower"]=line2;
        serializeJson(root, outas);
        ws.textAll(encrypt(outas).c_str());
      }

  
}

void publishLcd(char * line1, char * line2, uint32_t id) {

      if (ws.count()) {
        char strZoneGroup[12];
        char outas[128];
        StaticJsonDocument<200> doc;
        JsonObject root = doc.to<JsonObject>();        
        root["lcd_upper"]=line1;
        root["lcd_lower"]=line2;
        serializeJson(root, outas);
        std::string out=encrypt(outas);       
        ws.text(id,out.c_str());        
      }

  
}

void publishMsg(std::string msg) {
        char outas[200];
        StaticJsonDocument<200> doc;
        JsonObject root = doc.to<JsonObject>(); 
        root["event_info"]=msg.c_str();
        serializeJson(root, outas);
        ws.textAll(encrypt(outas).c_str());        
}

void publishMsg(std::string msg,uint32_t id) {
        char outas[200];
        StaticJsonDocument<200> doc;
        JsonObject root = doc.to<JsonObject>(); 
        root["event_info"]=msg.c_str();
        serializeJson(root, outas);
        std::string out=encrypt(outas);
        ws.text(id,out.c_str());
}


void publishStatus(const char * stateName, uint8_t state) {

      if (ws.count()) {
        char strZoneGroup[12];
        char outas[128];
        StaticJsonDocument<200> doc;
        JsonObject root = doc.to<JsonObject>();        
        root[stateName]=state;
       serializeJson(root, outas);
        //ws.textAll(outas);
        ws.textAll(encrypt(outas).c_str());               
}

  
}



void publishZones(const char * zoneType, uint8_t * zoneGroup, int size) {
      if (ws.count()) {
       char outas[512];
      StaticJsonDocument<200> doc;
      JsonObject root = doc.to<JsonObject>();  
      char strZoneGroup[12];      
      for (int x=0; x <= size; x++) {
        sprintf(strZoneGroup,"%s_%d",zoneType,x);
       // Serial.printf("Zone: %s,%02x\n",strZoneGroup,zoneGroup[x]);
        root[strZoneGroup] = zoneGroup[x];
      }
        serializeJson(root, outas);
       // ws.textAll(outas);
        ws.textAll(encrypt(outas).c_str());              
    
      }
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

  if ( WiFi.status() !=  WL_CONNECTED ) 
  {
     WiFi.begin();
    int loopCount = 0;
    int upCount=0;
    Serial.println("\nWifi disconnected. Reconnecting...");
    while (WiFi.status() != WL_CONNECTED && loopCount < 200 ) 
    {
      delay( 100 );
         Serial.print(".");
         if (upCount >= 60)  
         {
            upCount = 0;
            Serial.println();
         }
         ++upCount;
      ++loopCount;
    }
  }

    //ping-pong WebSocket to keep connection open
  if (ws_ping_pong.isRunning() && ws_ping_pong.elapsed() > 5 * 60) {
    ws.pingAll();
    ws_ping_pong.restart();
    Serial.printf("Sending pong...\n");
  }
  
  ArduinoOTA.handle();
 //AsyncElegantOTA.loop();

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
  
  
  
  //if data to be sent, we ensure we process it quickly to avoid delays with the F6 cmd
  sendWaitTime = millis();
  vh = vista.handle();
  while (!forceZoneUpdate && vista.keybusConnected && vista.sendPending() && !vh) {
    if (millis() - sendWaitTime > 5) break;
    vh = vista.handle();
  }

  if (vista.keybusConnected && vh) {

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
        //zoneState zs;
        if (vista.extcmd[2] == 0xF1 && z > 0 && z <= MAX_ZONES) { // we have a zone status (zone expander address range)
                //Serial.printf("Zone %d,zs=%d, %s\n",z,zs,zone_state1.c_str());
               zones[z].open=vista.extcmd[4];
               zones[z].time = millis();
               forceZoneUpdate=true;               
               //publish alert for this zone
        } else if (vista.extcmd[2] == 0x00 ) { //relay update z = 1 to 4
          if (z > 0) {
            char rc[5];
            sprintf(rc, "RLY: %d/%d", vista.extcmd[1],z);
            Serial.printf("Got relay address %d channel %d = %d,%s\n",vista.extcmd[1],z,vista.extcmd[4],rc);
            //mqttPublish(mqttRelayTopic, rc, vista.extcmd[4] ? true : false);
            publishMsg(rc);
            refreshTime=millis();
          }
        } else if (vista.extcmd[2] == 0x0d) { //relay update z = 1 to 4 - 1sec on / 1 sec off
            if (z > 0) {
                char rc[5];
                sprintf(rc, "RLTL %d/%d", vista.extcmd[1],z);
                publishMsg(rc);
                refreshTime=millis();                
                //  mqttPublish(mqttRelayTopic, rc, vista.extcmd[4] ? true : false);
                Serial.printf("Got relay address %d channel %d = %d. Cmd 0D. Pulsing 1sec on/ 1sec off", vista.extcmd[1], z, vista.extcmd[4]);
            }
        } else if (vista.extcmd[2] == 0xF7) { //30 second zone expander module status update
          uint8_t faults = vista.extcmd[4];
          for (int x = 8; x > 0; x--) {
            bool zs=faults & 1;
            z = getZoneFromChannel(vista.extcmd[1], x); //device id=extcmd[1]
            if (!z) continue;
                 if (zs!=zones[z].open)  {               
                    //publish alert for this zone
                 }
                  zones[z].time = millis();
                  zones[z].open=zs;      
                  faults = faults >> 1; //get next zone status bit from field
                  forceZoneUpdate=true;                   

          }


        }
      } else if (vista.extcmd[0] == 0xFB && vista.extcmd[1] == 4) {
        char rf_serial_char[9];
        // Decode and push new RF sensor data
        uint32_t device_serial = (vista.extcmd[2] << 16) + (vista.extcmd[3] << 8) + vista.extcmd[4];
        sprintf(rf_serial_char, "RFX: %03d%04d %02X State: %02x", device_serial / 10000, device_serial % 10000,vista.extcmd[5]);
        publishMsg(rf_serial_char);
        refreshTime=millis();
      }
    }
    if (vista.cbuf[0] == 0xF7 && vista.newCmd) {
      memcpy(p1, vista.statusFlags.prompt, 16);
      memcpy(p2, & vista.statusFlags.prompt[16], 16);
      p1[16] = '\0';
      p2[16] = '\0';
      if (lastp1 != p1 || lastp2 !=p2) {
        publishLcd (p1,p2);
       }
      if (lastbeeps != vista.statusFlags.beeps) {
        char tmp[4] = {
          0
        };
        sprintf(tmp, "%d", vista.statusFlags.beeps);
       // mqttPublish(mqttBeepTopic, tmp);
      }
      lastbeeps = vista.statusFlags.beeps;
      Serial.print("Prompt1:");
      Serial.println(p1);
      Serial.print("Prompt2:");
      Serial.println(p2);
      Serial.print("Beeps:");
      Serial.println(vista.statusFlags.beeps);
    }

    //publishes lrr status messages
    if ((vista.cbuf[0] == 0xf9 && vista.cbuf[3] == 0x58 && vista.newCmd)) { //we show all lrr messages with type 58
      int c, q, z;
      c = vista.statusFlags.lrr.code;
      q = vista.statusFlags.lrr.qual;
      z = vista.statusFlags.lrr.zone;

      std::string qual;

      if (c < 400)
        qual = (q == 3) ? " clr" : "";
      else if (c == 570)
        qual = (q == 1) ? " act" : " clr";
      else
        qual = (q == 1) ? " clr" : "";
      if (c) {
        String lrrString = String(statusText(c));

        char uflag = lrrString[0];
        std::string uf = "user";
        if (uflag == 'Z')
          uf = "zone";
        //sprintf(msg, "%d: %s %s %d%s", c, &lrrString[1], uf.c_str(), z, qual.c_str());
        sprintf(msg, "%s %s %d%s",&lrrString[1], uf.c_str(), z, qual.c_str());        
       // mqttPublish(mqttLrrTopic, msg);
        publishMsg(msg);
        refreshTime=millis();        
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

      for (int x = 0; x < MAX_ZONES; x++) {
        if ((!zones[x+1].bypass && zones[x+1].open ) || (zones[x+1].bypass && !vista.statusFlags.bypass)) {
            forceZoneUpdate=true;            
            zones[x+1].open=false;
            zones[x+1].bypass=false;
            zones[x+1].alarm=false; 
            //publish zone status
           // Serial.printf("ready flag, setting zone %d to false",x+1);     
        }
      }
    }
    //system armed prompt type
    if (strstr(p1, ARMED) && vista.statusFlags.systemFlag) {
      strncpy(systemPrompt.p1, p1, 17);
      strncpy(systemPrompt.p2, p2, 17);
      systemPrompt.time = millis();
      systemPrompt.state = true;
    }
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
        if (!zones[vista.statusFlags.zone].alarm ) {
          forceZoneUpdate=true;
          //publish zone alarm status
        }
        zones[vista.statusFlags.zone].alarm=true;
        zones[vista.statusFlags.zone].time = millis();
      } else {
        panicStatus.zone = vista.statusFlags.zone;
        panicStatus.time = millis();
        panicStatus.state = true;
        strncpy(panicStatus.prompt, p1, 17);
      }
    }
    //zone check status 
    if (strstr(p1, CHECK) && !vista.statusFlags.systemFlag) {
       if (!zones[vista.statusFlags.zone].trouble) {
        forceZoneUpdate=true;
        //publish zone status
       }
      zones[vista.statusFlags.zone].time = millis();
      zones[vista.statusFlags.zone].trouble=true;    


    }
    //zone fault status 
    if (strstr(p1, FAULT) && !vista.statusFlags.systemFlag) {
      if (!zones[vista.statusFlags.zone].open) {
        forceZoneUpdate=true;
        //publish zone status
      }
      zones[vista.statusFlags.zone].time = millis();
      zones[vista.statusFlags.zone].open=true;
      zones[vista.statusFlags.zone].bypass=false;
      zones[vista.statusFlags.zone].alarm=false;   
      //Serial.printf("setting all flags to false and open to true for zone %d\n",vista.statusFlags.zone);   
    }
    //zone bypass status
    if (strstr(p1, BYPAS) && !vista.statusFlags.systemFlag) {
       if (!zones[vista.statusFlags.zone].bypass) {
        forceZoneUpdate=true;
        //publish zone status
       }
      zones[vista.statusFlags.zone].time = millis();
      zones[vista.statusFlags.zone].bypass=true;  
       
    }

    //trouble lights 
    if (!vista.statusFlags.acPower) {
      currentLightState.ac = false;
    } else currentLightState.ac = true;

    if (vista.statusFlags.lowBattery && vista.statusFlags.systemFlag) {
      currentLightState.bat = true;
      lowBatteryTime = millis();
    }

    if (vista.statusFlags.fire) {
      currentLightState.fire = true;
      currentSystemState = striggered;
    } else currentLightState.fire = false;
    if (vista.statusFlags.inAlarm) {
      currentSystemState = striggered;
      currentLightState.alarm = true;
    } else currentLightState.alarm = false;

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
    if ((millis() - systemPrompt.time) > TTL) systemPrompt.state = false;
    if ((millis() - lowBatteryTime) > TTL) currentLightState.bat = false;
    if (currentLightState.ac && !currentLightState.bat)
      currentLightState.trouble = false;
    else
      currentLightState.trouble = true;

    //system status message
    
    uint8_t statusLights;
    if (currentSystemState != previousSystemState)
      switch (currentSystemState) {

      case striggered:
           //publish system msg
        //mqttPublish(mqttSystemStatusTopic, STATUS_TRIGGERED);
        break;
      case sarmedaway:
          statusLights!=2;
          //publish system msg
       // mqttPublish(mqttSystemStatusTopic, STATUS_ARMED);
        break;
      case sarmednight:
          statusLights!=2;
          //publish system msg
       // mqttPublish(mqttSystemStatusTopic, STATUS_NIGHT);
        break;
      case sarmedstay:
         statusLights!=2;  
            //publish system msg
       // mqttPublish(mqttSystemStatusTopic, STATUS_STAY);
        break;
      case sunavailable:
         statusLights&=0xfe;  
         //publish system msg
       // mqttPublish(mqttSystemStatusTopic, STATUS_NOT_READY);
        break;
      case sdisarmed:
        //publish system msg
        //mqttPublish(mqttSystemStatusTopic, STATUS_OFF);
        break;
      default:
         statusLights&=0xfe; 
         //publish system msg         
        //mqttPublish(mqttSystemStatusTopic, STATUS_NOT_READY);
        break;
      }

    //publish status on change only - keeps api traffic down

if (changedLightStates() || forceZoneUpdate ) {
      uint8_t lights=0;    
    
      if (currentLightState.ready)
        lights|=1;
      else
        lights&=~1;
      if (currentLightState.armed)
        lights|=2;
      else
        lights&=~2;   
      if (currentLightState.trouble)
        lights|=0x10;
      else
        lights&=~0x10; 
      if (currentLightState.fire)
        lights|=0x40;
      else
        lights&=~0x40;
      if (currentLightState.bypass)
        lights|=8;
      else
        lights&=~8; 
      if (vista.statusFlags.programMode)
        lights|=0x20;
      else
        lights&=~0x20;    
      publishStatus("status_lights",lights);   
      //Serial.printf("AC=%d,bat=%d,trouble=%d,lights=%02X\n",currentLightState.ac,currentLightState.bat,currentLightState.trouble,lights);       

}
    if (currentLightState.ac != previousLightState.ac)
       publishStatus("power_status",currentLightState.ac);      
    /*
    if (currentLightState.fire != previousLightState.fire)
      mqttPublish(mqttStatusTopic, "FIRE", currentLightState.fire);
    if (currentLightState.alarm != previousLightState.alarm)
      mqttPublish(mqttStatusTopic, "ALARM", currentLightState.alarm);
    if (currentLightState.trouble != previousLightState.trouble)
      mqttPublish(mqttStatusTopic, "TROUBLE", currentLightState.trouble);
    if (currentLightState.chime != previousLightState.chime)
      mqttPublish(mqttStatusTopic, "CHIME", currentLightState.chime);
    if (currentLightState.away != previousLightState.away)
      mqttPublish(mqttStatusTopic, "AWAY", currentLightState.away);
    if (currentLightState.ac != previousLightState.ac)
      mqttPublish(mqttStatusTopic, "AC", currentLightState.ac);
    if (currentLightState.stay != previousLightState.stay)
      mqttPublish(mqttStatusTopic, "STAY", currentLightState.stay);
    if (currentLightState.night != previousLightState.night)
      mqttPublish(mqttStatusTopic, "NIGHT", currentLightState.night);
    if (currentLightState.instant != previousLightState.instant)
      mqttPublish(mqttStatusTopic, "INST", currentLightState.instant);
    if (currentLightState.bat != previousLightState.bat)
      mqttPublish(mqttStatusTopic, "BATTERY", currentLightState.bat);
    if (currentLightState.bypass != previousLightState.bypass)
      mqttPublish(mqttStatusTopic, "BYPASS", currentLightState.bypass);
    if (currentLightState.ready != previousLightState.ready)
      mqttPublish(mqttStatusTopic, "READY", currentLightState.ready);
    if (currentLightState.armed != previousLightState.armed)
      mqttPublish(mqttStatusTopic, "ARMED", currentLightState.armed);
*/





    //clears restored zones after timeout
    for (int x = 0; x < MAX_ZONES ; x++) {
      if (((!zones[x+1].bypass && zones[x+1].open) || (zones[x+1].bypass && !vista.statusFlags.bypass)) && (millis() - zones[x+1].time) > TTL) {
            forceZoneUpdate=true;
            //publish zone status

            zones[x+1].open=false;
            zones[x+1].bypass=false;
            zones[x+1].alarm=false;     
                  //Serial.printf("timeout. setting all flags to false for zone %d \n",x+1);        
      }
    }

    if (forceZoneUpdate) {
      uint8_t bypassGroup[8],alarmGroup[8],programGroup[8],openGroup[8];
      uint8_t g;
      uint8_t b;          
      for (int x=0; x < MAX_ZONES; x++) {
            g = x / 8;
            b=x % 8;
            if (zones[x+1].open)
              openGroup[g]&= ~(1<<b);             
            else
              openGroup[g]|= 1<<b;            

            if (zones[x+1].bypass)
             bypassGroup[g]|= 1<<b;
            else
              bypassGroup[g]&= ~(1<<b); 
            if (zones[x+1].alarm)
              alarmGroup[g]|= 1<<b;
            else
              alarmGroup[g]&= ~(1<<b);

      }
      publishZones("open_zone",openGroup,(MAX_ZONES/8)-1);
      publishZones("alarm_zone",alarmGroup,(MAX_ZONES/8)-1);
      publishZones("bypass_zone",bypassGroup,(MAX_ZONES/8)-1);      
    }
    

    
    previousSystemState = currentSystemState;
    previousLightState = currentLightState;

    if (millis() - refreshTime > 30000 ) {
        publishMsg("");
        publishLcd (p1,p2);        
     refreshTime=millis();        
    }

    previousLrr = lrr;
    if (strstr(vista.statusFlags.prompt, HITSTAR))
      vista.write('*');

    forceZoneUpdate = false;

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
void set_keypad_address(int addr) {
  if (addr > 0 and addr < 24)
    vista.setKpAddr(addr);
}

long int toInt(const char * s) {
  char * p;
  long int li = strtol(s, & p, 10);
  return li;
}

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


bool changedLightStates() {
    if (currentLightState.fire != previousLightState.fire)
    return true;
    if (currentLightState.alarm != previousLightState.alarm)
     return true;
    if (currentLightState.trouble != previousLightState.trouble)
    return true;
    if (currentLightState.chime != previousLightState.chime)
     return true;
    if (currentLightState.away != previousLightState.away)
     return true;
    if (currentLightState.ac != previousLightState.ac)
      return true;
    if (currentLightState.stay != previousLightState.stay)
      return true;
    if (currentLightState.night != previousLightState.night)
      return true;
    if (currentLightState.instant != previousLightState.instant)
     return true;
    if (currentLightState.bat != previousLightState.bat)
      return true;
    if (currentLightState.bypass != previousLightState.bypass)
     return true;
    if (currentLightState.ready != previousLightState.ready)
      return true;
    if (currentLightState.armed != previousLightState.armed)
      return true;

      return false;
}

void onWsEvent(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len) {

  if (type == WS_EVT_CONNECT) {
    client->printf("{\"connected_id\": %u}", client->id());
       Serial.printf("ws[%s][%u] connected\n", server->url(), client->id());
       
    if (vista.keybusConnected && ws.count()) {
        publishLcd((char*)"Vista bus",(char*)"connected",client->id());
        forceZoneUpdate = true;
    }
    else if (!vista.keybusConnected && ws.count()) {
        publishLcd((char*)"Vista bus",(char*)"disconnected");      
    }
    
    //client->ping();
    ws_ping_pong.restart();
  }

  else if (type == WS_EVT_DISCONNECT) {
    Serial.printf("ws[%s][%u] disconnect\n", server->url(), client->id());
    if (ws.count() <= 0) {
      ws_ping_pong.stop();
    }
  }

  else if (type == WS_EVT_ERROR) {
    Serial.printf("ws[%s][%u] error(%u): %s\n", server->url(), client->id(), *((uint16_t*)arg), (char*)data);
  }

  else if (type == WS_EVT_PONG) {
    Serial.printf("ws[%s][%u] pong[%u]: %s\n", server->url(), client->id(), len, (len) ? (char*)data : "");
  }

  else if (type == WS_EVT_DATA) {
    AwsFrameInfo * info = (AwsFrameInfo*)arg;
    String msg = "";
    if (info->final && info->index == 0 && info->len == len) {
      //the whole message is in a single frame and we got all of it's data
      Serial.printf("ws[%s][%u] %s-message[%llu]: ", server->url(), client->id(), (info->opcode == WS_TEXT) ? "text" : "binary", info->len);
      if (info->opcode == WS_TEXT) {
        for (size_t i = 0; i < info->len; i++) {
          msg += (char) data[i];
        }
      }
      Serial.printf("%s\n",msg.c_str());
      if (info->opcode == WS_TEXT) {
        msg=decrypt(msg);
        StaticJsonDocument<200> doc;
        auto err = deserializeJson(doc, msg);
        if (!err) {
          JsonObject root = doc.as<JsonObject>();
          if (root.containsKey("btn_single_click")) {
            char *tmp = (char *)root["btn_single_click"].as<const char*>();
            char * const sep_at = strchr(tmp, '_');
            if (sep_at != NULL)            {
              *sep_at = '\0';

             char * v=sep_at+1;
            if (vista.keybusConnected)
                
                if (strcmp(v,"s") ==0) { vista.write(accessCode);vista.write("3");}
                else if (strcmp(v,"w")==0) { vista.write(accessCode);vista.write("2");}
                else if (strcmp(v,"c") ==0) { vista.write (accessCode);vista.write("9");}
                else if (strcmp(v,"x") ==0 && !currentLightState.armed) { vista.write(accessCode);vista.write("1");}
                else vista.write(v); 
            Serial.printf("got key %s\n",v);
            }
          }
        }
      }
    }

    else {
      //message is comprised of multiple frames or the frame is split into multiple packets
      if (info->index == 0) {
        if (info->num == 0) {
          Serial.printf("ws[%s][%u] %s-message start\n", server->url(), client->id(), (info->message_opcode == WS_TEXT) ? "text" : "binary");
        }
        Serial.printf("ws[%s][%u] frame[%u] start[%llu]\n", server->url(), client->id(), info->num, info->len);
      }

      Serial.printf("ws[%s][%u] frame[%u] %s[%llu - %llu]: ", server->url(), client->id(), info->num, (info->message_opcode == WS_TEXT) ? "text" : "binary", info->index, info->index + len);

      if (info->opcode == WS_TEXT) {
        for (size_t i = 0; i < info->len; i++) {
          msg += (char) data[i];
        }
      }
      else {
        char buff[3];
        for (size_t i = 0; i < info->len; i++) {
          sprintf(buff, "%02x ", (uint8_t) data[i]);
          msg += buff ;
        }
      }

      Serial.printf("%s\n", msg.c_str());

      if ((info->index + len) == info->len) {
        Serial.printf("ws[%s][%u] frame[%u] end[%llu]\n", server->url(), client->id(), info->num, info->len);
        if (info->final) {
          Serial.printf("ws[%s][%u] %s-message end\n", server->url(), client->id(), (info->message_opcode == WS_TEXT) ? "text" : "binary");
          if (info->message_opcode == WS_TEXT) client->text("I got your text message");
          else client->binary("I got your binary message");
        }
      }
    }
  }
}


std::string encrypt(std::string msg)  {
    char b64data[200];
    byte cipher[200];
    char ivdata[40];    
    gen_iv( ivaes );   
    base64_encode( ivdata, (char *)ivaes, 16);
    
    aes.do_aes_encrypt((byte *)msg.c_str(), msg.length() , cipher, (byte*) aeskey, 128,  ivaes);
    base64_encode(b64data, (char *)cipher, aes.get_size() );
    char outmsg[200];
    sprintf(outmsg,"{\"iv\":\"%s\",\"data\":\"%s\"}",ivdata,b64data);
    return outmsg;
}


String decrypt(String wsmsg) {
      StaticJsonDocument<300> doc;
      auto err = deserializeJson(doc, wsmsg);
      if (!err) {
          std::string eiv,emsg;
          JsonObject root = doc.as<JsonObject>();
          if (root.containsKey("iv")) {
           eiv=root["iv"].as<std::string>();
          }
          if (root.containsKey("data")) {
            emsg = root["data"].as<std::string>();
          }          
    char data_decoded[200];
    char iv_decoded[40];
    char out[200];
    //Serial.printf("iv=%s,data=%s\n",eiv.c_str(),emsg.c_str());
    int encrypted_length = base64_decode( data_decoded, (char *)emsg.c_str(), emsg.length());
    base64_decode( iv_decoded, (char *)eiv.c_str(), eiv.length());
    aes.do_aes_decrypt((byte *)data_decoded, encrypted_length, (byte*) out,(byte*) aeskey, 128, (byte *)iv_decoded);
    int len=aes.get_size() - out[aes.get_size()-1]; //remove padding
   // Serial.printf("out is %s,%d\n",out,len);
        out[len]='\0';
     return out;
      } else return "";
}

#ifdef ESP32
uint8_t getrnd() {
    uint8_t really_random = (uint8_t) random(0,0xff);
    return really_random;
}
#else
uint8_t getrnd() {
    uint8_t really_random = *(volatile uint8_t *)0x3FF20E44;
    return really_random;
}
#endif

// Generate a random initialization vector
void gen_iv(byte  *iv) {
    for (int i = 0 ; i < N_BLOCK ; i++ ) {
        iv[i]= (byte) getrnd();
    }
}
