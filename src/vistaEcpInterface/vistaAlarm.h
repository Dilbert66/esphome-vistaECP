#include "esphome.h"

#include "vista.h"
 //for documentation see project at https://github.com/Dilbert66/esphome-vistaecp

#define KP_ADDR 16
#define MAX_ZONES 48

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
namespace esphome {
class vistaECPHome: public PollingComponent, public CustomAPIDevice {
    public: vistaECPHome(char kpaddr = KP_ADDR, int receivePin = RX_PIN, int transmitPin = TX_PIN, int monitorTxPin = MONITOR_PIN): kpaddr(kpaddr),
    rxPin(receivePin),
    txPin(transmitPin),
    monitorPin(monitorTxPin) {}

    // start panel language definitions
 
    //lookups for determining zone status as strings
    
    const char * FAULT = "FAULT";    
    const char * BYPAS = "BYPAS";
    const char * ALARM = "ALARM";
    const char * FIRE = "FIRE";
    const char * CHECK = "CHECK";
    const char * HITSTAR = "Hit *"; 

    
    /*
    //alternative lookups as character array
    //find the matching characters in an ascii chart for the messages that your panel sends
    //for the statuses below. Only need the first 5 characters plus a zero at the end.
    const char FAULT[6] = {70,65,85,76,84,0}; //"FAULT"
    const char BYPAS[6] = {66,89,80,65,83,0}; //"BYPASS"
    const char ALARM[6] = {65,76,65,82,77,0}; //"ALARM"
    const char FIRE[6]  = {70,73,82,69,32,0}; //"FIRE "
    const char CHECK[6] = {67,72,69,67,75,0}; //"CHECK"
    const char HITSTAR[6] = {72,105,116,32,42,0}; //  "Hit *";
    */
 

 //messages to display to home assistant
    const char *
        const STATUS_ARMED = "armed_away";
    const char *
        const STATUS_STAY = "armed_home";
    const char *
        const STATUS_NIGHT = "armed_night";
    const char *
        const STATUS_OFF = "disarmed";
    const char *
        const STATUS_ONLINE = "online";
    const char *
        const STATUS_OFFLINE = "offline";
    const char *
        const STATUS_TRIGGERED = "triggered";
    const char *
        const STATUS_READY = "ready";
    //the default ha alarm panel card likes to see "unavailable" instead of not_ready when the system can't be armed
    const char *
        const STATUS_NOT_READY = "unavailable";
    const char *
        const MSG_ZONE_BYPASS = "zone_bypass_entered";
    const char *
        const MSG_ARMED_BYPASS = "armed_custom_bypass";
    const char *
        const MSG_NO_ENTRY_DELAY = "no_entry_delay";
    const char *
        const MSG_NONE = "no_messages";
    //end panel language definitions

    std:: function < void(uint8_t, const char * ) > zoneStatusChangeCallback;
    std:: function < void(const char * ) > systemStatusChangeCallback;
    std:: function < void(sysState, bool) > statusChangeCallback;
    std:: function < void(const char * ) > systemMsgChangeCallback;
    std:: function < void(const char * ) > lrrMsgChangeCallback;
    std:: function < void(const char * ) > rfMsgChangeCallback;
    std:: function < void(const char * ) > line1DisplayCallback;
    std:: function < void(const char * ) > line2DisplayCallback;
    std:: function < void(std::string) > beepsCallback;
    std:: function < void(std::string) > zoneExtendedStatusCallback;
    std:: function < void(uint8_t, uint8_t, bool) > relayStatusChangeCallback;

    void onZoneStatusChange(std:: function < void(uint8_t zone,
        const char * msg) > callback) {
        zoneStatusChangeCallback = callback;
    }
    void onSystemStatusChange(std:: function < void(const char * status) > callback) {
        systemStatusChangeCallback = callback;
    }
    void onStatusChange(std:: function < void(sysState led, bool isOpen) > callback) {
        statusChangeCallback = callback;
    }
    void onSystemMsgChange(std:: function < void(const char * msg) > callback) {
        systemMsgChangeCallback = callback;
    }
    void onLrrMsgChange(std:: function < void(const char * msg) > callback) {
        lrrMsgChangeCallback = callback;
    }
    void onLine1DisplayChange(std:: function < void(const char * msg) > callback) {
        line1DisplayCallback = callback;
    }
    void onLine2DisplayChange(std:: function < void(const char * msg) > callback) {
        line2DisplayCallback = callback;
    }
    void onBeepsChange(std:: function < void(std::string beeps) > callback) {
        beepsCallback = callback;
    }
    void onZoneExtendedStatusChange(std:: function < void(std::string zoneExtendedStatus) > callback) {
        zoneExtendedStatusCallback = callback;
    }    
    void onRelayStatusChange(std:: function < void(uint8_t addr, uint8_t zone, bool state) > callback) {
        relayStatusChangeCallback = callback;
    }
    void onRfMsgChange(std:: function < void(const char * msg) > callback) {
        rfMsgChangeCallback = callback;
    }

    byte debug;
    char kpaddr;
    int rxPin;
    int txPin;
    int monitorPin;
    const char * accessCode;
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
    char relayMonitorLow,relayMonitorHigh;
    
    int TTL = 30000;

    long int x;
    enum zoneState {
        zopen,
        zclosed,
        zbypass,
        zalarm,
        zfire,
        ztrouble
    };

    sysState currentSystemState,
    previousSystemState;

    private: uint8_t zone;
    bool sent;
    char p1[18];
    char p2[18];

    std::string lastp1;
    std::string lastp2;
    int lastbeeps;
    char msg[50];

    //add zone ttl array.  zone, last seen (millis)
    struct {
        unsigned long time;
        zoneState state;
    }
    zones[MAX_ZONES + 1];

    unsigned long lowBatteryTime,refreshTime;

    struct alarmStatusType {
        unsigned long time;
        bool state;
        uint8_t zone;
        char prompt[17];
    };
/*
    struct {
        unsigned long time;
        bool state;
        uint8_t zone;
        char p1[17];
        char p2[17];
    }
    systemPrompt;
*/
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

    lightStates currentLightState,
    previousLightState;
    enum lrrtype {
        user_t,
        zone_t
    };

    std::string previousMsg,previousZoneStatusMsg;

    alarmStatusType fireStatus,panicStatus,alarmStatus;
    lrrType lrr,
    previousLrr;
    unsigned long asteriskTime,
    sendWaitTime;
    bool firstRun;

    void setExpStates() {
        int zs = id(zoneStates);
        zs = zs >> 8; //skip first 8 zones
        for (int z = 9; z <= MAX_ZONES; z++) {
            if (zs & 1)
                vista.setExpFault(z, true);
            zs = zs >> 1;
        }
    }

    void setup() override {

        //use a pollingcomponent and change the default polling interval from 16ms to 8ms to enable
        // the system to not miss a response window on commands.  
        set_update_interval(8); //set looptime to 8ms 
        register_service( & vistaECPHome::alarm_keypress, "alarm_keypress", {
            "keys"
        });
        register_service( & vistaECPHome::set_keypad_address, "set_keypad_address", {
            "addr"
        });
        register_service( & vistaECPHome::alarm_disarm, "alarm_disarm", {
            "code"
        });
        register_service( & vistaECPHome::alarm_arm_home, "alarm_arm_home");
        register_service( & vistaECPHome::alarm_arm_night, "alarm_arm_night");
        register_service( & vistaECPHome::alarm_arm_away, "alarm_arm_away");
        register_service( & vistaECPHome::alarm_trigger_panic, "alarm_trigger_panic", {
            "code"
        });
        register_service( & vistaECPHome::alarm_trigger_fire, "alarm_trigger_fire", {
            "code"
        });
        register_service( & vistaECPHome::set_zone_fault, "set_zone_fault", {
            "zone",
            "fault"
        });
        systemStatusChangeCallback(STATUS_ONLINE);
        statusChangeCallback(sac, true);
        vista.begin(rxPin, txPin, kpaddr, monitorPin);

        //retrieve zone status from saved persistent global storage to keep state accross reboots
        int zs = id(zoneStates);
        int zb = id(zoneBypass);
        int za = id(zoneAlarms);
        for (int x = 1; x < MAX_ZONES + 1; x++) { //retrieve from persistant storage
            std::string s = "C";
            zoneState z = zclosed;
            if (zs & 1) {
                zones[x].state = zopen;
                s = "O";
                z = zopen;
            }

            if (zb & 1) {
                zones[x].state = zbypass;
                s = "B";
                z = zbypass;
            }

            if (za & 1) {
                zones[x].state = zalarm;
                s = "A";
                z = zalarm;
            }

            zoneStatusChangeCallback(x, s.c_str());
            zones[x].time = millis();
            zones[x].state = z;
            zs >>= 1;
            zb >>= 1;
            za >>= 1;
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
    }

    void alarm_disarm(std::string code) {

        set_alarm_state("D", code);

    }

    void alarm_arm_home() {

        set_alarm_state("S");

    }

    void alarm_arm_night() {

        set_alarm_state("N");

    }

    void alarm_arm_away() {

        set_alarm_state("A");

    }

    void alarm_trigger_fire(std::string code) {

        set_alarm_state("F", code);

    }

    void alarm_trigger_panic(std::string code) {

        set_alarm_state("P", code);

    }

    void set_zone_fault(int zone, bool fault) {

        vista.setExpFault(zone, fault);

    }

    void set_keypad_address(int addr) {
        if (addr > 0 and addr < 24)
            vista.setKpAddr(addr);
    }

    void alarm_keypress(std::string keystring) {
        const char * keys = strcpy(new char[keystring.length() + 1], keystring.c_str());
        if (debug > 0) ESP_LOGD("Debug", "Writing keys: %s", keystring.c_str());
        vista.write(keys);
    }

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

    void printPacket(const char * label, char cbuf[], int len) {

        std::string s;
        char s1[4];
        for (int c = 0; c < len; c++) {
            sprintf(s1, "%02X ", cbuf[c]);
            s.append(s1);
        }
        ESP_LOGI(label, "%s", s.c_str());

    }
    
    std::string getF7Lookup(char cbuf[]) {

        std::string s="{";
        char s1[5];
        for (int c = 12; c < 17; c++) {
            sprintf(s1, "%d,", cbuf[c]);
            s.append(s1);
        }
        s.append("0}");
        return s;

    }  
    void translatePrompt(char * cbuf) {
        
        for (x=0;x<32;x++) {
          if (cbuf[x] > 127)
            switch (cbuf[x]) {
               // case 0x88: cbuf[x]='U';break;
              //  case 0x8b: cbuf[x]='S';break;
                default: cbuf[x]='?';
             }
            
            
        }
    }


    void set_alarm_state(std::string state, std::string code = "") {

        if (code.length() != 4 || !isInt(code, 10)) code = accessCode; // ensure we get a numeric 4 digit code

        // Arm stay
        if (state.compare("S") == 0 && !vista.statusFlags.armedStay && !vista.statusFlags.armedAway) {

            if (quickArm)
                vista.write("#3");
            else if (code.length() == 4) {
                vista.write(code.c_str());
                vista.write("3");
            }
        }
        // Arm away
        else if (state.compare("A") == 0 && !vista.statusFlags.armedStay && !vista.statusFlags.armedAway) {

            if (quickArm)
                vista.write("#2");
            else if (code.length() == 4) {
                vista.write(code.c_str());
                vista.write("2");
            }
        }
        // Arm night  
        else if (state.compare("N") == 0 && !vista.statusFlags.armedStay && !vista.statusFlags.armedAway) {

            if (quickArm)
                vista.write("#33");
            else if (code.length() == 4) {
                vista.write(code.c_str());
                vista.write("33");
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
        else if (state.compare("D") == 0 && (vista.statusFlags.armedStay || vista.statusFlags.armedAway)) {

            if (code.length() == 4) { // ensure we get 4 digit code
                vista.write(code.c_str());
                vista.write('1');
                vista.write(code.c_str());
                vista.write('1');
            }
        }
    }

    //This stores the current zone states in persistant storage in case of reboots
    void setGlobalState(uint8_t zone, zoneState state) {

        id(zoneStates) = id(zoneStates) & (int)((0x01 << (zone - 1)) ^ 0xFFFFFFFF); //clear global storage value 
        id(zoneAlarms) = id(zoneAlarms) & (int)((0x01 << (zone - 1)) ^ 0xFFFFFFFF);
        id(zoneBypass) = id(zoneBypass) & (int)((0x01 << (zone - 1)) ^ 0xFFFFFFFF);
        id(zoneChecks) = id(zoneChecks) & (int)((0x01 << (zone - 1)) ^ 0xFFFFFFFF);

        switch (state) {
        case zopen:
            id(zoneStates) = id(zoneStates) | (0x01 << (zone - 1)); //set global storage value bit for zone
            break;
        case zbypass:
            id(zoneBypass) = id(zoneBypass) | (0x01 << (zone - 1));
            id(zoneStates) = id(zoneStates) | (0x01 << (zone - 1));
            break;
        case zalarm:
            id(zoneAlarms) = id(zoneAlarms) | (0x01 << (zone - 1));
            id(zoneStates) = id(zoneStates) | (0x01 << (zone - 1));
            break;
        case ztrouble:
            id(zoneChecks) = id(zoneChecks) | (0x01 << (zone - 1));
            break;
        case zclosed: //closed means no flags set
            break;
        default:
            break;

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

    void update() override {

        /* if (!firstRun && vista.keybusConnected && millis() - asteriskTime > 30000  && !vista.statusFlags.armedAway && !vista.statusFlags.armedStay && !vista.statusFlags.programMode) {
                asteriskTime=millis();
             
        }*/

        //if data to be sent, we ensure we process it quickly to avoid delays with the F6 cmd
        sendWaitTime = millis();
        vh = vista.handle();
        while (!firstRun && vista.keybusConnected && vista.sendPending() && !vh) {
            if (millis() - sendWaitTime > 5) break;
            vh = vista.handle();
        }

        if (vista.keybusConnected && vh) {

            if (firstRun) setExpStates(); //restore expander states from persistent storage        

            if (debug > 0 && vista.cbuf[0] && vista.newCmd) {
                printPacket("CMD", vista.cbuf, 13);

            }
            /*
            uint32_t ck=0;
            if (vista.cbuf[0] == 0xf7) {
                   printPacket("F7",vista.cbuf,45);
                for (x=0;x<44;x++) {
                    ck+=vista.cbuf[x];
                }
                
               ESP_LOGD("info","F7 cksum=%04X,%02X,%02X",ck,vista.cbuf[44],(ck+vista.cbuf[44])%256);
            }
            */

            //process ext messages for zones
            if (vista.newExtCmd) {
                if (debug > 0)
                    printPacket("EXT", vista.extcmd, 13);
                vista.newExtCmd = false;
                //format: [0xFA] [deviceid] [subcommand] [channel/zone] [on/off] [relaydata]

                if (vista.extcmd[0] == 0xFA) {
                    uint8_t z = vista.extcmd[3];
                    zoneState zs;
                    if (vista.extcmd[2] == 0xf1 && z > 0 && z <= MAX_ZONES) { // we have a zone status (zone expander address range)
                        zs = vista.extcmd[4] ? zopen : zclosed;
                        std::string zone_state1 = zs==zopen?"O":"C";
                        std::string zone_state2=zones[z].state==zbypass?"B":zones[z].state==zalarm?"A":"";
                        if (zones[z].state != zbypass && zones[z].state != zalarm) {
                            zones[z].time = millis();
                            zones[z].state = zs;
                        }
                        zoneStatusChangeCallback(z,(zone_state2.append(zone_state1)).c_str());
              
                    } else if (vista.extcmd[2] == 0x00) { //relay update z = 1 to 4
                        if (z > 0) {
                            relayStatusChangeCallback(vista.extcmd[1], z, vista.extcmd[4] ? true : false);
                            if (vista.extcmd[1] == relayMonitorLow ) {
                                std::string zone_state1 = vista.extcmd[4]?"O":"C";
                                std::string zone_state2=zones[z].state==zbypass?"B":zones[z].state==zalarm?"A":"";
                                zoneStatusChangeCallback(z,(zone_state2.append(zone_state1)).c_str());
                            } else  if (vista.extcmd[1] == relayMonitorHigh ) {
                                std::string zone_state1 = vista.extcmd[4]?"O":"C";
                                std::string zone_state2=zones[z+4].state==zbypass?"B":zones[z].state==zalarm?"A":"";
                                zoneStatusChangeCallback(z+4,(zone_state2.append(zone_state1)).c_str());

                            } 
                            if (debug > 0)
                                ESP_LOGD("debug", "Got relay address %d channel %d = %d", vista.extcmd[1], z, vista.extcmd[4]);
                        }
                    } else if (vista.extcmd[2] == 0x0d) { //relay update z = 1 to 4 - 1sec on / 1 sec off
                        if (z > 0) {
                            // relayStatusChangeCallback(vista.extcmd[1],z,vista.extcmd[4]?true:false);
                            if (debug > 0)
                                ESP_LOGD("debug", "Got relay address %d channel %d = %d. Cmd 0D. Pulsing 1sec on/ 1sec off", vista.extcmd[1], z, vista.extcmd[4]);
                        }
                    } else if (vista.extcmd[2] == 0xf7) { //30 second zone expander module status update
                        uint8_t faults = vista.extcmd[4];

                        for (int x = 8; x > 0; x--) {
                            z = getZoneFromChannel(vista.extcmd[1], x); //device id=extcmd[1]
                            if (!z) continue;
                            zs = faults & 1 ? zopen : zclosed; //check first bit . lower bit = channel 8. High bit= channel 1
                            faults = faults >> 1; //get next zone status bit from field
                            //only update status for zones that are not alarmed or bypassed
                            if (zones[z].state != zbypass && zones[z].state != zalarm) {
                                if (zones[z].state != zs) {
                                    if (zs == zopen)
                                        zoneStatusChangeCallback(z, "O");
                                    else
                                        zoneStatusChangeCallback(z, "C");
                                }
                                zones[z].time = millis();
                                zones[z].state = zs;
                                setGlobalState(z, zs);
                            }


                        }

                    }
                } else if (vista.extcmd[0] == 0xFB && vista.extcmd[1] == 4) {
                    char rf_serial_char[14];
                    //FB 04 06 18 98 B0 00 00 00 00 00 00 
                    uint32_t device_serial = (vista.extcmd[2] << 16) + (vista.extcmd[3] << 8) + vista.extcmd[4];
                    sprintf(rf_serial_char, "%03d%04d,%02X", device_serial / 10000, device_serial % 10000, vista.extcmd[5]);
                    if (debug > 0) ESP_LOGD("info", "RFX: %s", rf_serial_char);
                    rfMsgChangeCallback(rf_serial_char);

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

            if (vista.cbuf[0] == 0xf7 && vista.newCmd) {
                int kpaddrbit=0x01 << (kpaddr - 16);
                if (!(vista.cbuf[3] & kpaddrbit) && vista.statusFlags.systemFlag  ) return; // not addressed to this keypad
                translatePrompt(vista.statusFlags.prompt);                
                memcpy(p1, vista.statusFlags.prompt, 16);
                memcpy(p2, & vista.statusFlags.prompt[16], 16);
                p1[16] = '\0';
                p2[16] = '\0';
                if (lastp1 != p1)
                    line1DisplayCallback(p1);
                if (lastp2 != p2)
                    line2DisplayCallback(p2);
                std::string s="";
                if (!vista.statusFlags.systemFlag)                
                    s=getF7Lookup(vista.cbuf);
                ESP_LOGI("INFO", "Prompt: %s %s", p1,s.c_str());
                ESP_LOGI("INFO", "Prompt: %s", p2);
                ESP_LOGI("INFO", "Beeps: %d\n", vista.statusFlags.beeps);
                lastp1 = p1;
                lastp2 = p2;
                if (lastbeeps != vista.statusFlags.beeps)
                    beepsCallback(to_string(vista.statusFlags.beeps));
                lastbeeps = vista.statusFlags.beeps;
            }



            //publishes lrr status messages
            if ((vista.cbuf[0] == 0xf9 && vista.cbuf[3] == 0x58 && vista.newCmd) || firstRun ) { //we show all lrr messages with type 58
                int c, q, z;
                if (firstRun) { //retrieve from persistant storage
                    c = id(lrrCode) >> 16;
                    q = id(lrrCode) & 0x0F;
                    z = (id(lrrCode) >> 8) & 0xFF;
                } else {
                    c = vista.statusFlags.lrr.code;
                    q = vista.statusFlags.lrr.qual;
                    z = vista.statusFlags.lrr.zone;
                }

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
                    id(lrrCode) = (c << 16) | (z << 8) | q; //store in persistant global storage
                    refreshTime=millis();                      
                }

            }
            
            vista.newCmd = false;

            // we also return if it's not an f7, f9 or f2
            if (!(vista.cbuf[0] == 0xf7 || vista.cbuf[0] == 0xf9 || vista.cbuf[0] == 0xf2)) return;
            
            currentSystemState = sunavailable;
            currentLightState.stay = false;
            currentLightState.away = false;
            currentLightState.night = false;
            currentLightState.ready = false;
            currentLightState.alarm = false;
            currentLightState.armed = false;
            currentLightState.ac = false;

            //armed status lights
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
                for (int x = 1; x < MAX_ZONES + 1; x++) {
                    if ((zones[x].state != zbypass && zones[x].state != zclosed) || (zones[x].state == zbypass && !vista.statusFlags.bypass)) {
                        zoneStatusChangeCallback(x, "C");
                        zones[x].state = zclosed;
                        setGlobalState(x, zclosed); //save to persistent storage

                    }
                }
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
                //strncpy(fireStatus.prompt, p1, 17);
            }
            //zone alarm status 
            if (strstr(p1, ALARM) && !vista.statusFlags.systemFlag) {
                if (vista.statusFlags.zone <= MAX_ZONES) {
                    if (zones[vista.statusFlags.zone].state != zalarm)
                        zoneStatusChangeCallback(vista.statusFlags.zone, "A");
                    zones[vista.statusFlags.zone].time = millis();
                    zones[vista.statusFlags.zone].state = zalarm;
                    setGlobalState(vista.statusFlags.zone, zalarm);
                    alarmStatus.zone = vista.statusFlags.zone;
                    alarmStatus.time = millis();
                    alarmStatus.state = true;                    
                } else {
                    panicStatus.zone = vista.statusFlags.zone;
                    panicStatus.time = millis();
                    panicStatus.state = true;
                    //strncpy(panicStatus.prompt, p1, 17);
                }
            }
            //zone check status 
            if (strstr(p1, CHECK) && !vista.statusFlags.systemFlag) {
                if (zones[vista.statusFlags.zone].state != ztrouble)
                    zoneStatusChangeCallback(vista.statusFlags.zone, "T");
                zones[vista.statusFlags.zone].time = millis();
                zones[vista.statusFlags.zone].state = ztrouble;
                setGlobalState(vista.statusFlags.zone, ztrouble);
            }
            //zone fault status 

            if (strstr(p1, FAULT) && !vista.statusFlags.systemFlag) {
                if (zones[vista.statusFlags.zone].state != zopen)
                    zoneStatusChangeCallback(vista.statusFlags.zone, "O");
                zones[vista.statusFlags.zone].time = millis();
                zones[vista.statusFlags.zone].state = zopen;
                setGlobalState(vista.statusFlags.zone, zopen);
            }
            //zone bypass status
            if (strstr(p1, BYPAS) && !vista.statusFlags.systemFlag) {
                if (zones[vista.statusFlags.zone].state != zbypass)
                    zoneStatusChangeCallback(vista.statusFlags.zone, "B");
                setGlobalState(vista.statusFlags.zone, zbypass);
                zones[vista.statusFlags.zone].time = millis();
                zones[vista.statusFlags.zone].state = zbypass;
            }

            //trouble lights 
            /*
            if ( vista.statusFlags.acLoss ) {
                 currentLightState.trouble=true;
            } else  currentLightState.trouble=false;
            */
            if (!vista.statusFlags.acPower) {
                currentLightState.ac = false;
            } else currentLightState.ac = true;

            if (vista.statusFlags.lowBattery && vista.statusFlags.systemFlag) {
                currentLightState.bat = true;
                lowBatteryTime = millis();
            }
            // ESP_LOGD("info","ac=%d,batt status = %d,systemflag=%d,lightbat status=%d,trouble=%d", currentLightState.ac,vista.statusFlags.lowBattery,vista.statusFlags.systemFlag,currentLightState.bat,currentLightState.trouble);

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

            //if ( vista.statusFlags.cancel ) {
            //   currentLightState.canceled=true;
            //	}    else  currentLightState.canceled=false;        

            //clear alarm statuses  when timer expires
            if ((millis() - fireStatus.time) > TTL) fireStatus.state = false;
            if ((millis() - alarmStatus.time) > TTL) alarmStatus.state = false;            
            if ((millis() - panicStatus.time) > TTL) panicStatus.state = false;
           // if ((millis() - systemPrompt.time) > TTL) systemPrompt.state = false;
            if ((millis() - lowBatteryTime) > TTL) currentLightState.bat = false;
            
            if (currentLightState.ac && !currentLightState.bat)
                currentLightState.trouble = false;
            else
                currentLightState.trouble = true;
            
            currentLightState.alarm=alarmStatus.state;

            //system status message
            if (currentSystemState != previousSystemState)
                switch (currentSystemState) {
                case striggered:
                    systemStatusChangeCallback(STATUS_TRIGGERED);
                    break;
                case sarmedaway:
                    systemStatusChangeCallback(STATUS_ARMED);
                    break;
                case sarmednight:
                    systemStatusChangeCallback(STATUS_NIGHT);
                    break;
                case sarmedstay:
                    systemStatusChangeCallback(STATUS_STAY);
                    break;
                case sunavailable:
                    systemStatusChangeCallback(STATUS_NOT_READY);
                    break;
                case sdisarmed:
                    systemStatusChangeCallback(STATUS_OFF);
                    break;
                default:
                    systemStatusChangeCallback(STATUS_NOT_READY);
                }

            //publish status on change only - keeps api traffic down
            if (currentLightState.fire != previousLightState.fire)
                statusChangeCallback(sfire, currentLightState.fire);
            if (currentLightState.alarm != previousLightState.alarm)
                statusChangeCallback(salarm, currentLightState.alarm);
            if (currentLightState.trouble != previousLightState.trouble)
                statusChangeCallback(strouble, currentLightState.trouble);
            if (currentLightState.chime != previousLightState.chime)
                statusChangeCallback(schime, currentLightState.chime);
            if (currentLightState.away != previousLightState.away)
                statusChangeCallback(sarmedaway, currentLightState.away);
            if (currentLightState.ac != previousLightState.ac)
                statusChangeCallback(sac, currentLightState.ac);
            if (currentLightState.stay != previousLightState.stay)
                statusChangeCallback(sarmedstay, currentLightState.stay);
            if (currentLightState.night != previousLightState.night)
                statusChangeCallback(sarmednight, currentLightState.night);
            if (currentLightState.instant != previousLightState.instant)
                statusChangeCallback(sinstant, currentLightState.instant);
            if (currentLightState.bat != previousLightState.bat)
                statusChangeCallback(sbat, currentLightState.bat);
            if (currentLightState.bypass != previousLightState.bypass)
                statusChangeCallback(sbypass, currentLightState.bypass);
            if (currentLightState.ready != previousLightState.ready)
                statusChangeCallback(sready, currentLightState.ready);
            if (currentLightState.armed != previousLightState.armed)
                statusChangeCallback(sarmed, currentLightState.armed);
            //  if (currentLightState.canceled != previousLightState.canceled) 
            //   statusChangeCallback(scanceled,currentLightState.canceled);


            std::string zoneStatusMsg = "";
            char s1[7];
            //clears restored zones after timeout
            for (int x = 1; x < MAX_ZONES + 1; x++) {
                if (((zones[x].state != zbypass && zones[x].state != zclosed) || (zones[x].state == zbypass && !vista.statusFlags.bypass)) && (millis() - zones[x].time) > TTL) {
                    zoneStatusChangeCallback(x, "C");
                    zones[x].state = zclosed;
                    setGlobalState(x, zclosed);
                }
                
                if (zones[x].state==zalarm) {
                    sprintf(s1, "AL:%d", x);
                    if (zoneStatusMsg != "") zoneStatusMsg.append(",");
                    zoneStatusMsg.append(s1);
                }
                if (zones[x].state==zbypass) {
                    sprintf(s1, "BY:%d", x );
                    if (zoneStatusMsg != "") zoneStatusMsg.append(",");
                    zoneStatusMsg.append(s1);
                }
                
            }
            if (zoneStatusMsg != previousZoneStatusMsg && zoneExtendedStatusCallback != NULL)
               zoneExtendedStatusCallback(zoneStatusMsg); 
            previousZoneStatusMsg=zoneStatusMsg;

            /*
		    std::string s;
            
            if (!vista.statusFlags.acPower) {
                s=s+"AC LOSS ";
            } if (vista.statusFlags.lowBattery) {
                s=s+"LOW BATTERY ";
            } if (fireStatus.state )
                  s=s+fireStatus.prompt;
            if (panicStatus.state) 
                 s=s+panicStatus.prompt;
            if (systemPrompt.state)
                 s=s+systemPrompt.p1+" "+systemPrompt.p2;
             
            if (s != previousMsg) {
                sprintf(msg,"%s", s.c_str());
                systemMsgChangeCallback(msg);
            }
            
            
            if (systemPrompt.state)
                 s=s+systemPrompt.p1+" "+systemPrompt.p2;
             
            if (s != previousMsg) {
                sprintf(msg,"%s", s.c_str());
                systemMsgChangeCallback(msg);
            }
            previousMsg=s;
            */
            /*
            std::string s;
            if (vista.statusFlags.check || vista.statusFlags.systemFlag) {
                 s=s+vista.statusFlags.prompt;
            }

             if (s != previousMsg && displaySystemMsg) {
                systemMsgChangeCallback(vista.statusFlags.prompt);
            }
             
            previousMsg=s;
            */

            previousSystemState = currentSystemState;
            previousLightState = currentLightState;
            previousLrr = lrr;
            
            if (millis() - refreshTime > 30000 ) {
                lrrMsgChangeCallback("");
            refreshTime=millis();        
            }            

            if (strstr(vista.statusFlags.prompt, HITSTAR))
                vista.write('*');

            firstRun = false;
        }

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
};
}
