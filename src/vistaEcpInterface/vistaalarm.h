#ifndef _vistaalarm_h
#define _vistaalarm_h

#if !defined(ARDUINO_MQTT)
#include "esphome.h"
using namespace esphome;
#if defined(USE_MQTT)
#define ESPHOME_MQTT
#endif
#endif

#include "vista.h"
#include <string>
#include "paneltext.h"

 //for documentation see project at https://github.com/Dilbert66/esphome-vistaecp

#define KP_ADDR 17 //only used as a default if not set in the yaml
#define MAX_ZONES 48
#define MAX_PARTITIONS 3  
#define DEFAULTPARTITION 1

//default pins to use for serial comms to the panel
//The pinouts below are only examples. You can choose any other gpio pin that is available and not needed for boot.
//These have proven to work fine.
#ifdef ESP32
//esp32 use gpio pins 4,13,16-39 
#define RX_PIN 22
#define TX_PIN 21
#define MONITOR_PIN 18 // pin used to monitor the green TX line (3.3 level dropped from 12 volts
#else
#define RX_PIN 5
#define TX_PIN 4
#define MONITOR_PIN 14 // pin used to monitor the green TX line (3.3 level dropped from 12 volts
#endif

Stream * OutputStream = & Serial;
Vista vista(OutputStream);

void disconnectVista() {
  vista.stop();

}
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
  sready,
  sarmed
};

#if !defined(ARDUINO_MQTT)
void publishBinaryState(std::string * str,bool open) {
  std::vector<binary_sensor::BinarySensor *> bs = App.get_binary_sensors();
  for (auto *obj : bs ) {
    std::string name=obj->get_name();
    if (name.find(*str) != std::string::npos){
      obj->publish_state(open) ;
      return;
    }
  }
}
    
void publishTextState(std::string * str,std::string * text) {
 std::vector<text_sensor::TextSensor *> ts = App.get_text_sensors();
 for (auto *obj : ts ) {
   std::string name=obj->get_name();
   if (name.find(*str) != std::string::npos ){
    obj->publish_state(*text) ;
    return;
   }
 }
}
#endif

#if defined(ESPHOME_MQTT) 
class vistaECPHome: public CustomMQTTDevice, public RealTimeClock {
#elif defined(ARDUINO_MQTT)
class vistaECPHome { 
#else
class vistaECPHome: public CustomAPIDevice, public RealTimeClock {
#endif
    public: vistaECPHome(char kpaddr = KP_ADDR, int receivePin = RX_PIN, int transmitPin = TX_PIN, int monitorTxPin = MONITOR_PIN,int maxzones=MAX_ZONES,int maxpartitions=MAX_PARTITIONS): 
    keypadAddr1(kpaddr),
    rxPin(receivePin),
    txPin(transmitPin),
    monitorPin(monitorTxPin),
    maxZones(maxzones),
    maxPartitions(maxpartitions)
    {
         partitionKeypads = new char[maxPartitions+1];
         zones=new zoneType[maxZones+1];
         partitions = new uint8_t[maxPartitions];
         partitionStates = new partitionStateType[maxPartitions];
         if (maxZones > 99) {
             relaySup = new supervisoryType[32];
             addressSup = new supervisoryType[31];
         }
    }
    

    std:: function < void(int, std::string) > zoneStatusChangeCallback;
    std:: function < void(int, bool) > zoneStatusChangeBinaryCallback;    
    std:: function < void(std::string, uint8_t) > systemStatusChangeCallback;
    std:: function < void(sysState, bool, uint8_t) > statusChangeCallback;
    std:: function < void(std::string , uint8_t) > systemMsgChangeCallback;
    std:: function < void(std::string) > lrrMsgChangeCallback;
    std:: function < void(std::string ) > rfMsgChangeCallback;
    std:: function < void(std::string , uint8_t) > line1DisplayCallback;
    std:: function < void(std::string , uint8_t) > line2DisplayCallback;
    std:: function < void(std::string , uint8_t) > beepsCallback;
    std:: function < void(std::string ) > zoneExtendedStatusCallback;
    std:: function < void(uint8_t, int, bool) > relayStatusChangeCallback;

    void onZoneStatusChange(std:: function < void(int zone,
     std::string msg) > callback) {
      zoneStatusChangeCallback = callback;
    }
    void onZoneStatusChangeBinarySensor(std:: function < void(int zone,
      bool open) > callback) {
      zoneStatusChangeBinaryCallback = callback;
    }    
    void onSystemStatusChange(std:: function < void(std::string status, uint8_t partition) > callback) {
      systemStatusChangeCallback = callback;
    }
    void onStatusChange(std:: function < void(sysState led, bool isOpen, uint8_t partition) > callback) {
      statusChangeCallback = callback;
    }
    void onSystemMsgChange(std:: function < void(std::string msg, uint8_t partition) > callback) {
      systemMsgChangeCallback = callback;
    }
    void onLrrMsgChange(std:: function < void(std::string msg) > callback) {
      lrrMsgChangeCallback = callback;
    }
    void onLine1DisplayChange(std:: function < void(std::string msg, uint8_t partition) > callback) {
      line1DisplayCallback = callback;
    }
    void onLine2DisplayChange(std:: function < void(std::string msg, uint8_t partition) > callback) {
      line2DisplayCallback = callback;
    }
    void onBeepsChange(std:: function < void(std::string beeps, uint8_t partition) > callback) {
      beepsCallback = callback;
    }
    void onZoneExtendedStatusChange(std:: function < void(std::string zoneExtendedStatus) > callback) {
      zoneExtendedStatusCallback = callback;
    }
    void onRelayStatusChange(std:: function < void(uint8_t addr, int channel, bool state) > callback) {
      relayStatusChangeCallback = callback;
    }
    void onRfMsgChange(std:: function < void(std::string msg) > callback) {
      rfMsgChangeCallback = callback;
    }
    
    void zoneStatusUpdate(int zone) {
      if (zoneStatusChangeCallback != NULL ) {
          std::string msg,zs1;          
          zs1=zones[zone].open?"O":"C";
          msg = zones[zone].bypass ? "B" : zones[zone].alarm ? "A" : "";
          zoneStatusChangeCallback(zone,msg.append(zs1).c_str());
      }
      
      if (zoneStatusChangeBinaryCallback != NULL ) 
        zoneStatusChangeBinaryCallback(zone,zones[zone].open);
    }

    byte debug;
    char keypadAddr1;
    int rxPin;
    int txPin;
    int monitorPin;
    const char * accessCode;
    const char * rfSerialLookup;    
    bool quickArm;
    bool displaySystemMsg = false;
    bool lrrSupervisor,
    vh;
    char expanderAddr1,
    expanderAddr2,
    expanderAddr3,
    expanderAddr4,
    expanderAddr5,
    relayAddr1,
    relayAddr2,
    relayAddr3,
    relayAddr4;
    
    int maxZones;
    int maxPartitions;
    char * partitionKeypads;
    int defaultPartition=DEFAULTPARTITION;
    bool forceRefreshGlobal,forceRefreshZones,forceRefresh;
    int TTL = 30000;

    long int x;

    sysState currentSystemState,
    previousSystemState;
    
#if defined(ESPHOME_MQTT)
const char setalarmcommandtopic[] PROGMEM = "/alarm/set"; 
#endif
    

    private:

    int zone;
    bool sent;
    char p1[18];
    char p2[18];

    uint8_t * partitions;

    char msg[50];

    struct zoneType {
      unsigned long time;
      uint8_t partition;
      uint8_t open:1;
      uint8_t bypass:1;
      uint8_t alarm:1;
      uint8_t check:1;
      uint8_t fire:1;
      uint8_t panic:1;
    };
    
    struct supervisoryType {
        bool check;
        unsigned long time;
      
    };
    struct {
        uint8_t bell:1;
        uint8_t wrx1:1;
        uint8_t wrx2:1;
        uint8_t loop:1;
        uint8_t duress:1;
        uint8_t panic1:1;
        uint8_t panic2:1;
        uint8_t panic3:1;
    } otherSup;
        

public:
    zoneType * zones;
    
private:    
    unsigned long lowBatteryTime;
    supervisoryType * relaySup;
    supervisoryType * addressSup;

    struct alarmStatusType {
      unsigned long time;
      bool state;
      int zone;
      char prompt[17];
    };

    struct lrrType {
      int code;
      uint8_t qual;
      int zone;
      uint8_t user;
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

    lightStates currentLightState,
    previousLightState;
    enum lrrtype {
      user_t,
      zone_t
    };

    struct partitionStateType {
      sysState previousSystemState;
      lightStates previousLightState;
      std::string lastp1=" ";
      std::string lastp2=" ";
      int lastbeeps;
      bool refreshStatus;
      bool refreshLights;
    } ;
    
public:
    partitionStateType * partitionStates;
    
private:
    std::string previousMsg,
    previousZoneStatusMsg;

    alarmStatusType fireStatus,
    panicStatus,
    alarmStatus;
    lrrType lrr,
    previousLrr;
    uint8_t partitionTargets;
    unsigned long sendWaitTime;
    bool firstRun;
    
    struct serialType {
       int zone;
       int mask;
    };


serialType getRfSerialLookup(char * serialCode) { 

  serialType rf;
  rf.zone=0;
  if (rfSerialLookup && *rfSerialLookup) {
    std::string serial=serialCode;      
    std::string token,token1, token2, token3;      
    std::string s = rfSerialLookup;

    size_t pos, pos1,pos2;
    s.append(",");
    while ((pos = s.find(',')) != std::string::npos) {
      token = s.substr(0, pos); 
      pos1 = token.find(':');
      pos2=token.find(':',pos1+1); 
      token1 = token.substr(0, pos1); //serial
      if (pos2 != std::string::npos)  {     
        token2 = token.substr(pos1 + 1,pos2-pos1-1); //zone
        token3 = token.substr(pos2+1);    //mask
      } else {
        token2 = token.substr(pos1 + 1);
        token3 = "";
          
      }
      if (token1 == serial) {
        rf.zone=toInt(token2,10);
        if (token3!="")
            rf.mask=toInt(token3,16);
        else
            rf.mask=0x80;
        break;
      }
      s.erase(0, pos + 1); /* erase() function store the current positon and move to next token. */
    }
  }
  return rf;
}   


#if defined(ESPHOME_MQTT) 
    void on_json_message(const std::string &topic, JsonObject payload) {
    int p=0;

      if (topic.find(String(FPSTR(setalarmcommandtopic)).c_str())!=std::string::npos) { 
        if (payload.containsKey("partition"))
          p=payload["partition"];
      
        if (payload.containsKey("state") )  {
            const char *c="";             
            if (payload.containsKey("code"))
                c=payload["code"];
            std::string code=c;
            std::string s=payload["state"];  
            set_alarm_state(s,code,p); 
        } else if (payload.containsKey("keys")) {
            std::string s=payload["keys"]; 
            alarm_keypress_partition(s,p);
        } else if (payload.containsKey("fault") && payload.containsKey("zone")) {
            bool b=false;
            std::string s1= (const char*) payload["fault"];
            if (s1=="ON" || s1=="on" || s1=="1")
                b=true;
            std::string s=payload["zone"];
            p=toInt(s,10);
           // ESP_LOGE("info","set zone fault %s,%s,%d,%d",s2.c_str(),c,b,p);            
            set_zone_fault(p,b);

        }
        
      } 
      
  }
#endif
 public:
#if defined(ARDUINO_MQTT)
void begin() {
#else
void setup() override {
#endif 
      //use a pollingcomponent and change the default polling interval from 16ms to 8ms to enable
      // the system to not miss a response window on commands.  
#if !defined(ARDUINO_MQTT)     
      set_update_interval(8); //set looptime to 8ms 
#endif     

#if defined(ESPHOME_MQTT)

   topic_prefix = mqtt_mqttclientcomponent->get_topic_prefix();
   topic="homeassistant/alarm_control_panel/"+ topic_prefix + "/config"; 
   subscribe_json(topic_prefix + String(FPSTR(setalarmcommandtopic)).c_str(),&vistaECPHome::on_json_message);   
   
#elif !defined(ARDUINO_MQTT)
      register_service( & vistaECPHome::alarm_keypress, "alarm_keypress", {
        "keys"
      });
      register_service( & vistaECPHome::alarm_keypress_partition, "alarm_keypress_partition", {
        "keys","partition"
      });      
      register_service( & vistaECPHome::alarm_disarm, "alarm_disarm", {
        "code","partition"
      });
      register_service( & vistaECPHome::alarm_arm_home, "alarm_arm_home",{"partition"});
      register_service( & vistaECPHome::alarm_arm_night, "alarm_arm_night",{"partition"});
      register_service( & vistaECPHome::alarm_arm_away, "alarm_arm_away",{"partition"});
      register_service( & vistaECPHome::alarm_trigger_panic, "alarm_trigger_panic", {
        "code","partition"
      });
      register_service( & vistaECPHome::alarm_trigger_fire, "alarm_trigger_fire", {
        "code","partition"
      });
      register_service( & vistaECPHome::set_zone_fault, "set_zone_fault", {
        "zone",
        "fault"
      });
      
#endif      
      systemStatusChangeCallback(STATUS_ONLINE, 1);
      statusChangeCallback(sac, true, 1);
      vista.begin(rxPin, txPin, keypadAddr1, monitorPin);


      for (uint8_t x = 1; x <= maxZones; x++) {
        zones[x].open = false;
        zones[x].alarm=false;
        zones[x].bypass=false;
        zones[x].fire=false; 
        zones[x].panic=false; 
        zoneStatusUpdate(x);
        zones[x].time = millis();

      }
      
      firstRun = true;

      vista.lrrSupervisor = lrrSupervisor; //if we don't have a monitoring lrr supervisor we emulate one if set to true
      //set addresses of expander emulators

      vista.zoneExpanders[0].expansionAddr = expanderAddr1;
      vista.zoneExpanders[1].expansionAddr = expanderAddr2;
      vista.zoneExpanders[2].expansionAddr = expanderAddr3;
      vista.zoneExpanders[3].expansionAddr = expanderAddr4;
      vista.zoneExpanders[4].expansionAddr = expanderAddr5;
      vista.zoneExpanders[5].expansionAddr = relayAddr1;
      vista.zoneExpanders[6].expansionAddr = relayAddr2;
      vista.zoneExpanders[7].expansionAddr = relayAddr3;
      vista.zoneExpanders[8].expansionAddr = relayAddr4;
      
      setDefaultKpAddr(defaultPartition);
      
      for (uint8_t p=0;p < maxPartitions;p++) { 
        systemStatusChangeCallback(STATUS_NOT_READY,p+1);
        beepsCallback("0",p+1);
      }    
      lrrMsgChangeCallback("ESP Restart");
      rfMsgChangeCallback(""); 
    }

    void alarm_disarm(std::string code,int partition) {

      set_alarm_state("D", code,partition);

    }

    void alarm_arm_home(int partition) {

      set_alarm_state("S","",partition);

    }

    void alarm_arm_night(int partition) {

      set_alarm_state("N","",partition);

    }

    void alarm_arm_away(int partition) {

      set_alarm_state("A","",partition);

    }

    void alarm_trigger_fire(std::string code,int partition) {

      set_alarm_state("F", code,partition);

    }

    void alarm_trigger_panic(std::string code,int partition) {

      set_alarm_state("P", code,partition);

    }

    void set_zone_fault(int zone, bool fault) {

      vista.setExpFault(zone, fault);

    }

    void set_keypad_address(int addr) {
     // if (addr > 0 and addr < 24) 
      ///  vista.setKpAddr(addr); //disabled for now 
    }
    
    void alarm_keypress(std::string keystring) {
         
        alarm_keypress_partition(keystring,defaultPartition);
    }    

    void alarm_keypress_partition(std::string keystring, int partition) {
      const char * keys = strcpy(new char[keystring.length() + 1], keystring.c_str());
      if (debug > 0)
          #if defined(ARDUINO_MQTT)
          Serial.printf("Writing keys: %s to partition %d\n", keystring.c_str(),partition);      
          #else
          ESP_LOGE("Debug", "Writing keys: %s to partition %d", keystring.c_str(),partition);
          #endif
      uint8_t addr=0;
      if (partition > maxPartitions || partition < 1) return;
      addr=partitionKeypads[partition];
      if (addr > 0 and addr < 24)      
        vista.write(keys,addr);
    }
    
    void setDefaultKpAddr(uint8_t p) {
    uint8_t a;
      if (p > maxPartitions || p < 1) return;
      a=partitionKeypads[p];
      if (a > 15 && a < 24) 
        vista.setKpAddr(a);
    } 
private:
    bool isInt(std::string s, int base) {
      if (s.empty() || std::isspace(s[0])) return false;
      char * p;
      strtol(s.c_str(), & p, base);
      return ( * p == 0);
    }

    long int toInt(std::string s, int base) {
      if (s.empty() || std::isspace(s[0])) return 0;
      char * p;
      long int li = strtol(s.c_str(), & p, base);
      return li;
    }
    
 

    bool areEqual(char * a1, char * a2, uint8_t len) {
      for (int x = 0; x < len; x++) {
        if (a1[x] != a2[x]) return false;
      }
      return true;
    }
    
    
    
     bool promptContains(char * p1, const char * msg, uint8_t & zone) {
            int x,y;
            for (x=0;x<strlen(msg);x++) {
                 if (p1[x]!=msg[x]) return false;
            } 
            if (p1[x] !=0x20 || !(x< 17 && p1[x+1] > 0x2f && p1[x+1] < 0x3a)) return false;

          #if defined(ARDUINO_MQTT)
          if (debug >1)
            Serial.printf("The prompt  %s was matched\n",msg);         
          #else   
           if (debug > 1)              
            ESP_LOGE("debug","The prompt  %s was matched",msg);   
        #endif        
            if (maxZones > 99) {
              char s[3]; 
              x++;
              for (y=0;y<3;y++) {
                if (p1[y+x] > 0x2F && p1[y+x] < 0x3A) 
                    s[y]=p1[y+x];
                if (p1[y+x]==0x20 && y>0) {
                  s[y]=0;
                  int z=toInt(s,10);
                  if ( z > vista.statusFlags.zone && z<=maxZones ) vista.statusFlags.zone=z;
                  zone=z;
                  if (debug > 2) 
          #if defined(ARDUINO_MQTT)
                      Serial.printf("The zone match is: %d\n",z);       
          #else                       
                      ESP_LOGE("test","The zone match is: %d ",z); 
          #endif

  
                }
              }
            }
            return true;

     }

  void printPacket(const char * label, char cbuf[], int len) {
    char s1[4];

    std::string s="";      
      
    #if !defined(ARDUINO_MQTT) 
    char s2[25];    
    ESPTime rtc=now();
    sprintf(s2,"%02d-%02d-%02d %02d:%02d ",rtc.year,rtc.month,rtc.day_of_month,rtc.hour,rtc.minute);
    #endif
    for (int c = 0; c < len; c++) {
      sprintf(s1, "%02X ", cbuf[c]);
      s=s.append(s1);
    }
    #if defined(ARDUINO_MQTT)
    Serial.printf("%s: %s\n",label, s.c_str());    
    #else 
    ESP_LOGE(label, "%s %s",s2, s.c_str());
    #endif

  }

   
    std::string getF7Lookup(char cbuf[]) {

        std::string s="{";
        char s1[4];
        for (int c = 12; c < 17; c++) {
            sprintf(s1, "%d,", cbuf[c]);
            s.append(s1);
        }
        s.append("0}");
        return s;

    }  
public:
    void set_alarm_state(std::string const& state, std::string code = "",int partition=DEFAULTPARTITION) {

      if (code.length() != 4 || !isInt(code, 10)) code = accessCode; // ensure we get a numeric 4 digit code
 
      uint8_t addr=0;
      if (partition > maxPartitions || partition < 1) return;
      addr=partitionKeypads[partition];
      if (addr < 1 || addr > 23) return;          

      // Arm stay
      if (state.compare("S") == 0 && !partitionStates[partition-1].previousLightState.armed) {
        if (quickArm )
          vista.write("#3",addr);
        else if (code.length() == 4) {
          vista.write(code.c_str(),addr);
          vista.write("3",addr);
        }
      }
      // Arm away
      else if ((state.compare("A") == 0 || state.compare("W") == 0) && !partitionStates[partition-1].previousLightState.armed) {

        if (quickArm)
          vista.write("#2",addr);
        else if (code.length() == 4) {
          vista.write(code.c_str(),addr);
          vista.write("2",addr);
        }
      }
      // Arm night  
      else if (state.compare("N") == 0 && !partitionStates[partition-1].previousLightState.armed) {

        if (quickArm)
          vista.write("#33",addr);
        else if (code.length() == 4) {
          vista.write(code.c_str(),addr);
          vista.write("33",addr);
        }
      }
      // Fire command
      else if (state.compare("F") == 0) {

        //todo

      }
      // Panic command
      else if (state.compare("P") == 0) {

        //todo
      }
      // Disarm
      else if (state.compare("D") == 0 && partitionStates[partition-1].previousLightState.armed) {
        if (code.length() == 4) { // ensure we get 4 digit code
          vista.write(code.c_str(),addr);
          vista.write("1",addr);
          vista.write(code.c_str(),addr);
          vista.write("1",addr);
        }
      }
    }
private:
    int getZoneFromChannel(uint8_t deviceAddress, uint8_t channel) {

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
    
    void translatePrompt(char * cbuf) {
        
        for (x=0;x<32;x++) {
          if (cbuf[x] > 127)
            switch (cbuf[x]) {
                //case 0x88: cbuf[x]='U';break;
               // case 0x8b: cbuf[x]='S';break;
                default: cbuf[x]='?';
             }
            
            
        }
    }

    void assignPartitionToZone(int zone) {
        for (int p=1;p<4;p++) {
            if (partitions[p-1]) {
                zones[zone].partition=p-1;
                break;
            }
            
        }
    }


    void getPartitionsFromMask() {
      partitionTargets=0;
      memset(partitions, 0,maxPartitions);
        for (uint8_t p=1;p <= maxPartitions;p++) {
            for (int8_t i=3;i>=0;i--) {
                int8_t shift=partitionKeypads[p]-(8*i);
                if (shift > 0 && (vista.statusFlags.keypad[i] & (0x01 << shift))) {
                    partitionTargets=partitionTargets+1;
                    partitions[p-1] = 1;
                    break;
                }
            }
      }
    }

public:
#if defined(ARDUINO_MQTT)
void loop()  {
#else   
void update() override {
#endif 
        
       static unsigned long refreshFlagsTime;
       if ((!firstRun && vista.keybusConnected && millis() - refreshFlagsTime > 60000  && !vista.statusFlags.programMode) || forceRefreshGlobal) {
              forceRefreshZones=true;
              forceRefreshGlobal=false;
              refreshFlagsTime=millis();
              for (uint8_t partition = 1; partition <= maxPartitions; partition++) {
                   partitionStates[partition-1].refreshStatus=true;
                   partitionStates[partition-1].refreshLights=true;
                   

             }
           
      }
    
      #if defined(ESPHOME_MQTT)
        if (firstRun) {
         publish(topic,"{\"name\":" +  topic_prefix + "alarm panel, \"cmd_t\":" +  topic_prefix + String(FPSTR(setalarmcommandtopic)).c_str() + "}",0,1);
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

            if (debug > 0 && vista.cbuf[0] && !vista.newExtCmd) {
             printPacket("CMD", vista.cbuf, 13);
        }
        static unsigned long refreshLrrTime,refreshRfTime;
        //process ext messages for zones
        if (vista.newExtCmd) {
          if (debug > 0)
            printPacket("EXT", vista.extcmd, 13);
          vista.newExtCmd = false;
          //format: [0xFA] [deviceid] [subcommand] [channel/zone] [on/off] [relaydata]

          if (vista.extcmd[0] == 0xFA) {
            int z = vista.extcmd[3];
            if (vista.extcmd[2] == 0xf1 && z > 0 && z <= maxZones) { // we have a zone status (zone expander address range)
                zones[z].time = millis();
                zones[z].open = vista.extcmd[4];
                zoneStatusUpdate(z);
          
            } else if (vista.extcmd[2] == 0x00) { //relay update z = 1 to 4
              if (z > 0) {
                relayStatusChangeCallback(vista.extcmd[1], z, vista.extcmd[4] ? true : false);
                if (debug > 0)
          #if defined(ARDUINO_MQTT)
                  Serial.printf("Got relay address %d channel %d = %d\n", vista.extcmd[1], z, vista.extcmd[4]);      
          #else                    
                  ESP_LOGE("debug", "Got relay address %d channel %d = %d", vista.extcmd[1], z, vista.extcmd[4]);
          #endif
              }
            } else if (vista.extcmd[2] == 0x0d) { //relay update z = 1 to 4 - 1sec on / 1 sec off
              if (z > 0) {
                // relayStatusChangeCallback(vista.extcmd[1],z,vista.extcmd[4]?true:false);
                if (debug > 0)
          #if defined(ARDUINO_MQTT)
                 Serial.printf("Got relay address %d channel %d = %d. Cmd 0D. Pulsing 1sec on/ 1sec off\n", vista.extcmd[1], z, vista.extcmd[4]);      
          #else                    
                  ESP_LOGE("debug", "Got relay address %d channel %d = %d. Cmd 0D. Pulsing 1sec on/ 1sec off", vista.extcmd[1], z, vista.extcmd[4]);
          #endif
              }
            } else if (vista.extcmd[2] == 0xf7) { //30 second zone expander module status update
              uint8_t faults = vista.extcmd[4];
              for (int x = 8; x > 0; x--) {
                z = getZoneFromChannel(vista.extcmd[1], x); //device id=extcmd[1]
                if (!z) continue;
                bool zs = faults & 1 ?true : false; //check first bit . lower bit = channel 8. High bit= channel 1
                faults = faults >> 1; //get next zone status bit from field

                  if (zones[z].open != zs) {
                      zones[z].open = zs;                      
                      zoneStatusUpdate(z);
                  }
                  zones[z].time = millis();
              }
            }
          } else if (vista.extcmd[0] == 0xFB && vista.extcmd[1] == 4) {
              
            char rf_serial_char[14];
            char rf_serial_char_out[20];            
            //FB 04 06 18 98 B0 00 00 00 00 00 00 
            uint32_t device_serial = (vista.extcmd[2] << 16) + (vista.extcmd[3] << 8) + vista.extcmd[4];
            sprintf(rf_serial_char, "%03d%04d", device_serial / 10000, device_serial % 10000);
            serialType rf=getRfSerialLookup(rf_serial_char);
            int z=rf.zone;            

            if (debug > 0) {
          #if defined(ARDUINO_MQTT)
                Serial.printf("RFX: %s,%02x\n", rf_serial_char,vista.extcmd[5]);          
          #else                
                ESP_LOGE("info", "RFX: %s,%02x", rf_serial_char,vista.extcmd[5]);
          #endif
            }   
            if (z && !(vista.extcmd[5]&0x04)) {
                zones[z].time = millis();
                zones[z].open = vista.extcmd[5]&rf.mask?true:false;
                zoneStatusUpdate(z);
              }
            sprintf(rf_serial_char_out,"%s,%02x",rf_serial_char,vista.extcmd[5]);
            rfMsgChangeCallback(rf_serial_char);
            refreshRfTime = millis();

          } 
          /* rf_serial_char
          
          	1 - ? (loop flag?)
              2 - Low battery
              3 -	Supervision required 
              4 - ?
              5 -	Loop 3 
              6 -	Loop 2 
              7 -	Loop 4 
              8 -	Loop 1 
          
          */

        }
        p1[0]='\0';
        p2[0]='\0';
        if (vista.cbuf[0] == 0xf7 && vista.newCmd) {
          getPartitionsFromMask();
          /*test code
          if (vista.statusFlags.prompt[0]==70) {
                    vista.statusFlags.prompt[2]=0x88;
                    vista.statusFlags.prompt[3]=0x8b;
          }
          */
          translatePrompt(vista.statusFlags.prompt);
          memcpy(p1, vista.statusFlags.prompt, 16);
          memcpy(p2, & vista.statusFlags.prompt[16], 16);
          p1[16] = '\0';
          p2[16] = '\0';

          for (uint8_t partition = 1; partition <= maxPartitions; partition++) {
            if (partitions[partition - 1] ) {
               forceRefresh=partitionStates[partition - 1].refreshStatus ;
          #if defined(ARDUINO_MQTT)
              Serial.printf("Display to partition: %02X\n", partition);          
          #else              
              ESP_LOGE("INFO", "Display to partition: %02X", partition);
          #endif
              if (partitionStates[partition - 1].lastp1 != p1 || forceRefresh)
                line1DisplayCallback(p1, partition);
              if (partitionStates[partition - 1].lastp2 != p2 || forceRefresh)
                line2DisplayCallback(p2, partition);
              if (partitionStates[partition - 1].lastbeeps != vista.statusFlags.beeps || forceRefresh ) {
               char s[4];  
               itoa(vista.statusFlags.beeps,s,10);
                beepsCallback(s, partition);
              }
              partitionStates[partition - 1].lastp1 = p1;
              partitionStates[partition - 1].lastp2 = p2;
              partitionStates[partition - 1].lastbeeps = vista.statusFlags.beeps;

             if (strstr(vista.statusFlags.prompt, HITSTAR))
                alarm_keypress_partition("*",partition);
            }
          }
          std::string s="";
        //  if (!vista.statusFlags.systemFlag)
          //  s=getF7Lookup(vista.cbuf);
          #if defined(ARDUINO_MQTT)
          Serial.printf("Prompt: %s %s\n", p1,s.c_str());
          Serial.printf("Prompt: %s\n", p2);
          Serial.printf("Beeps: %d\n", vista.statusFlags.beeps);          
          #else    
          ESP_LOGE("INFO", "Prompt: %s %s", p1,s.c_str());
          ESP_LOGE("INFO", "Prompt: %s", p2);
          ESP_LOGE("INFO", "Beeps: %d\n", vista.statusFlags.beeps);
          #endif
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
            lrrMsgChangeCallback(msg);
           // id(lrrCode) = (c << 16) | (z << 8) | q; //store in persistant global storage
            refreshLrrTime = millis();
          }

        }

        vista.newCmd = false;

        // we also return if it's not an f7, f9 or f2
        if (!(vista.cbuf[0] == 0xf7 || vista.cbuf[0] == 0xf9 || vista.cbuf[0] == 0xf2) || vista.cbuf[12]==0x77) return;

        currentSystemState = sunavailable;
        currentLightState.stay = false;
        currentLightState.away = false;
        currentLightState.night = false;
        currentLightState.ready = false;
        currentLightState.alarm = false;
        currentLightState.armed = false;
        currentLightState.ac = false;
       // currentLightState.bat = false;        
       // currentLightState.trouble = false;  
        currentLightState.bypass = false; 
        currentLightState.chime = false; 
        
        //armed status lights
        if (vista.statusFlags.systemFlag && (vista.statusFlags.armedAway || vista.statusFlags.armedStay)) {
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

        }
        //system armed prompt type
        /*
        if (strstr(p1, ARMED) && vista.statusFlags.systemFlag) {
            strncpy(systemPrompt.p1, p1, 17);
            strncpy(systemPrompt.p2, p2, 17);
            systemPrompt.time = millis();
            systemPrompt.state = true;
        }
        */
        //zone fire status
        uint8_t tz;
        if (promptContains(p1,FIRE,tz) && !vista.statusFlags.systemFlag) {
          fireStatus.zone = vista.statusFlags.zone;
          fireStatus.time = millis();
          fireStatus.state = true;
          zones[vista.statusFlags.zone].fire=true;          
        }
        //zone alarm status 
        if (promptContains(p1,ALARM,tz) && !vista.statusFlags.systemFlag) {

          if (vista.statusFlags.zone <= maxZones) {
            if (!zones[vista.statusFlags.zone].alarm) {
             zones[vista.statusFlags.zone].alarm=true;
             zoneStatusUpdate(vista.statusFlags.zone);
            }
            zones[vista.statusFlags.zone].time = millis();
            alarmStatus.zone = vista.statusFlags.zone;
            alarmStatus.time = millis();
            alarmStatus.state = true;
            assignPartitionToZone(vista.statusFlags.zone);             
          } else {
            panicStatus.zone = vista.statusFlags.zone;
            panicStatus.time = millis();
            panicStatus.state = true;
            //strncpy(panicStatus.prompt, p1, 17);
          }
        }
        //device check status 
        if (promptContains(p1,CHECK,tz) || promptContains(p1,TRBL,tz)) {
            
         // if (zones[vista.statusFlags.zone].state != ztrouble)
           //zoneStatusUpdate(vista.statusFlags.zone, "T");
          //zones[vista.statusFlags.zone].time = millis();
          //zones[vista.statusFlags.zone].state = ztrouble;

        } 
        //zone fault status 
        if (promptContains(p1,FAULT,tz) && !vista.statusFlags.systemFlag) {

          if (!zones[vista.statusFlags.zone].open) {
           zones[vista.statusFlags.zone].open=true;  
           zoneStatusUpdate(vista.statusFlags.zone);
          }
ESP_LOGD("test","fault found for zone %d,status=%d",vista.statusFlags.zone,zones[vista.statusFlags.zone].open);
          zones[vista.statusFlags.zone].time = millis();

 
        }
        //zone bypass status
        if (promptContains(p1,BYPAS,tz) && !vista.statusFlags.systemFlag) {
          if (!zones[vista.statusFlags.zone].bypass) {
            zones[vista.statusFlags.zone].bypass=true;              
            zoneStatusUpdate(vista.statusFlags.zone);
          }
             zones[vista.statusFlags.zone].time = millis();
            assignPartitionToZone(vista.statusFlags.zone);          
        }

        //trouble lights 
        if (!vista.statusFlags.acPower) {
          currentLightState.ac = false;
        } else currentLightState.ac = true;

        if (vista.statusFlags.lowBattery  && vista.statusFlags.systemFlag) {
          currentLightState.bat = true;
          lowBatteryTime = millis();
        } 
        // ESP_LOGE("info","ac=%d,batt status = %d,systemflag=%d,lightbat status=%d,trouble=%d", currentLightState.ac,vista.statusFlags.lowBattery,vista.statusFlags.systemFlag,currentLightState.bat,currentLightState.trouble);

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

        if (vista.statusFlags.instant ) {
          currentLightState.instant = true;
        } else currentLightState.instant = false;

        //if ( vista.statusFlags.cancel ) {
        //   currentLightState.canceled=true;
        //	}    else  currentLightState.canceled=false;        

        //clear alarm statuses  when timer expires
        if ((millis() - fireStatus.time) > TTL) {
          fireStatus.state = false;          
          if (fireStatus.zone > 0 && fireStatus.zone <=maxZones)          
            zones[fireStatus.zone].fire=false;
        }
        if ((millis() - alarmStatus.time) > TTL) {
          alarmStatus.state = false;
          if (alarmStatus.zone > 0 && alarmStatus.zone <=maxZones)
            zones[alarmStatus.zone].alarm=false;          
        }
        if ((millis() - panicStatus.time) > TTL) {
          panicStatus.state = false;
          if (panicStatus.zone > 0 && panicStatus.zone <=maxZones)
            zones[panicStatus.zone].panic=false;             
        }
        //  if ((millis() - systemPrompt.time) > TTL) systemPrompt.state = false;
        if ((millis() - lowBatteryTime) > TTL) currentLightState.bat = false;

        if  (!currentLightState.ac || currentLightState.bat )
          currentLightState.trouble = true;
        else
          currentLightState.trouble = false;
        
        currentLightState.alarm = alarmStatus.state;

        for (uint8_t partition = 1; partition <= maxPartitions; partition++) {
          if ((partitions[partition - 1] && partitionTargets==1) && vista.statusFlags.systemFlag) {
            //system status message
            forceRefresh=partitionStates[partition - 1].refreshStatus ;;
              
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
             partitionStates[partition - 1].refreshStatus=false;
          }
        }
        

        for (uint8_t partition = 1; partition <= maxPartitions; partition++) {
          if ((partitions[partition - 1] && partitionTargets==1) ) {

            //publish status on change only - keeps api traffic down
            previousLightState = partitionStates[partition - 1].previousLightState;
            forceRefresh=partitionStates[partition - 1].refreshLights ;
            
            if (currentLightState.fire != previousLightState.fire || forceRefresh)
              statusChangeCallback(sfire, currentLightState.fire, partition);
            if (currentLightState.alarm != previousLightState.alarm || forceRefresh)
              statusChangeCallback(salarm, currentLightState.alarm, partition);
            if ((currentLightState.trouble != previousLightState.trouble || forceRefresh) )
              statusChangeCallback(strouble, currentLightState.trouble, partition);
            if (currentLightState.chime != previousLightState.chime || forceRefresh) 
              statusChangeCallback(schime, currentLightState.chime, partition);
            //if (currentLightState.check != previousLightState.check || forceRefresh) 
            //  statusChangeCallback(scheck, currentLightState.check, partition);          
            if (((currentLightState.away != previousLightState.away)  && vista.statusFlags.systemFlag) || forceRefresh)
              statusChangeCallback(sarmedaway, currentLightState.away, partition);
            if (currentLightState.ac != previousLightState.ac || forceRefresh)
              statusChangeCallback(sac, currentLightState.ac, partition);
            if (((currentLightState.stay != previousLightState.stay ) && vista.statusFlags.systemFlag) || forceRefresh)
              statusChangeCallback(sarmedstay, currentLightState.stay, partition);
            if (((currentLightState.night != previousLightState.night ) && vista.statusFlags.systemFlag) || forceRefresh)
              statusChangeCallback(sarmednight, currentLightState.night, partition);
            if (((currentLightState.instant != previousLightState.instant ) && vista.statusFlags.systemFlag) || forceRefresh)
              statusChangeCallback(sinstant, currentLightState.instant, partition);
            if (currentLightState.bat != previousLightState.bat || forceRefresh  )
              statusChangeCallback(sbat, currentLightState.bat, partition);
            if (currentLightState.bypass != previousLightState.bypass || forceRefresh)
              statusChangeCallback(sbypass, currentLightState.bypass, partition);
            if (currentLightState.ready != previousLightState.ready || forceRefresh)
              statusChangeCallback(sready, currentLightState.ready, partition);
            if (((currentLightState.armed != previousLightState.armed ) && vista.statusFlags.systemFlag) || forceRefresh)
              statusChangeCallback(sarmed, currentLightState.armed, partition);
            //  if (currentLightState.canceled != previousLightState.canceled) 
            //   statusChangeCallback(scanceled,currentLightState.canceled,partition);

            partitionStates[partition - 1].previousLightState = currentLightState;
            partitionStates[partition - 1].refreshLights=false;
           
          }
        }

        std::string zoneStatusMsg = "";
        char s1[16];
        //clears restored zones after timeout
        for (int x = 1; x < maxZones + 1; x++) {
            
           if (zones[x].bypass && !partitionStates[zones[x].partition].previousLightState.bypass) {
            zones[x].bypass=false;  
           } 
           
           if (zones[x].alarm && !partitionStates[zones[x].partition].previousLightState.alarm) {
            zones[x].alarm=false;  
           }             
            
          if (!zones[x].bypass && zones[x].open && (millis() - zones[x].time) > TTL ) {
            zones[x].open=false;              
            zoneStatusUpdate(x);
          }
     
           if ( forceRefreshZones) {
             zoneStatusUpdate(x);
           }

            
          if (zones[x].open) {
            sprintf(s1, "OP:%d", x);              
            if (zoneStatusMsg != "") zoneStatusMsg.append(",");
            zoneStatusMsg.append(s1);
          } 
          if (zones[x].alarm) {
            sprintf(s1, "AL:%d", x);
            if (zoneStatusMsg != "") zoneStatusMsg.append(",");
            zoneStatusMsg.append(s1);
          } 
          if (zones[x].bypass) {
            sprintf(s1, "BY:%d", x);
            if (zoneStatusMsg != "") zoneStatusMsg.append(",");
            zoneStatusMsg.append(s1);
          }

        }
        if ((zoneStatusMsg != previousZoneStatusMsg  || forceRefreshZones) && zoneExtendedStatusCallback != NULL)
          zoneExtendedStatusCallback(zoneStatusMsg);
        previousZoneStatusMsg = zoneStatusMsg;

        previousLrr = lrr;
       
        if (millis() - refreshLrrTime > 30000) {
          lrrMsgChangeCallback("");
          refreshLrrTime = millis();
        }
        if (millis() - refreshRfTime > 30000) {
          rfMsgChangeCallback("");
          refreshRfTime = millis();
        }      
        
        firstRun = false;
        forceRefreshZones=false;
        forceRefreshGlobal=false;
      }


    }
private:
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
  };

#if !defined(ARDUINO_MQTT)
vistaECPHome * VistaECP;
#endif

#endif