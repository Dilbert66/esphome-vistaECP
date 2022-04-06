#ifndef __VISTA_H
#define __VISTA_H

#include "Arduino.h"

#include "ECPSoftwareSerial.h"

#ifdef ESP32
#define ICACHE_RAM_ATTR IRAM_ATTR
#endif

//#define DEBUG

#define MONITORTX

// Used to read bits on F7 message
#define BIT_MASK_BYTE1_BEEP 0x07
#define BIT_MASK_BYTE1_NIGHT 0x10

#define BIT_MASK_BYTE2_ARMED_HOME 0x80
#define BIT_MASK_BYTE2_LOW_BAT 0x40
#define BIT_MASK_BYTE2_ALARM_ZONE 0x20
#define BIT_MASK_BYTE2_READY 0x10
#define BIT_MASK_BYTE2_AC_LOSS 0x08
#define BIT_MASK_BYTE2_SYSTEM_FLAG 0x04
#define BIT_MASK_BYTE2_CHECK_FLAG 0x02
#define BIT_MASK_BYTE2_FIRE 0x01

#define BIT_MASK_BYTE3_INSTANT 0x80
#define BIT_MASK_BYTE3_PROGRAM 0x40
#define BIT_MASK_BYTE3_CHIME_MODE 0x20
#define BIT_MASK_BYTE3_BYPASS 0x10
#define BIT_MASK_BYTE3_AC_POWER 0x08
#define BIT_MASK_BYTE3_ARMED_AWAY 0x04
#define BIT_MASK_BYTE3_ZONE_ALARM 0x02
#define BIT_MASK_BYTE3_IN_ALARM 0x01

#define F7_MESSAGE_LENGTH 45
#define F8_MESSAGE_LENGTH 9
#define N98_MESSAGE_LENGTH 6

#define MAX_MODULES 9

//enum ecpState { sPulse, sNormal, sAckf7,sSendkpaddr,sPolling };
#define sPulse 1
#define sNormal 2
#define sAckf7 3
#define sSendkpaddr 4
#define sPolling 5
#define sCmdHigh 6

struct statusFlagType {
    char beeps: 3;
    bool armedStay;
    bool armedAway;
    bool night;
    bool instant;
    bool chime;
    bool acPower;
    bool acLoss;
    bool ready;
    bool entryDelay;
    bool programMode;
    bool zoneBypass;
    bool zoneAlarm;
    bool alarm;
    bool check;
    bool systemFlag;
    bool lowBattery;
    bool systemTrouble;
    bool fire;
    bool fireZone;
    bool backlight;
    bool armed;
    bool away;
    bool bypass;
    bool inAlarm;
    bool noAlarm;
    bool exitDelay;
    bool cancel;
    bool fault;
    bool panicAlarm;
    char keypad[4];
    uint8_t zone;
    char prompt[36];
    char promptPos;
    uint8_t attempts = 10;
    struct {
        int code;
        uint8_t qual;
        uint8_t zone;
        uint8_t user;
        uint8_t partition;
    }
    lrr;

};

struct expanderType {
    char expansionAddr;
    char expFault;
    char expFaultBits;
    char relayState;
};

struct keyType {
    char key;
    uint8_t kpaddr;
};

class Vista {

    public:
        Vista(Stream * OutStream);
    ~Vista();
    void begin(int receivePin, int transmitPin, char keypadAddr, int monitorTxPin);
    void stop();
    bool handle();
    void outQueue(char byt,uint8_t addr);
    void printStatus();
    void printTrouble();
    void decodeBeeps();
    void decodeKeypads();
    void printPacket(char * , int);
    void write(const char * );
    void write(const char);
    void write(const char *,uint8_t addr );
    void write(const char,uint8_t addr);    
    statusFlagType statusFlags;
    SoftwareSerial * vistaSerial, * vistaSerialMonitor;
    void setKpAddr(char keypadAddr) {
        kpAddr = keypadAddr;
    }
    bool dataReceived;
    void ICACHE_RAM_ATTR rxHandleISR(), txHandleISR();
    bool areEqual(char * , char * , uint8_t);
    bool keybusConnected;
    int toDec(int);
    void resetStatus();
    void initSerialHandlers(int, int, int);
    char * cbuf, * extbuf, * extcmd;

    bool lrrSupervisor;
    char expansionAddr;
    void setExpFault(int, bool);
    bool newExtCmd, newCmd, newRelCmd;
    bool filterOwnTx;
    expanderType zoneExpanders[MAX_MODULES];
    char b; //used in isr
    bool charAvail();
    bool sendPending();

    private:
    keyType * outbuf;
    volatile uint8_t outbufIdx, inbufIdx;
    char tmpOutBuf[20];
    int rxPin, txPin;
    volatile char kpAddr;
    char monitorPin;
    volatile char ackAddr;
    Stream * outStream;
    volatile char rxState;
    volatile unsigned long lowTime;
    uint8_t * faultQueue;
    void setNextFault(uint8_t);
    expanderType getNextFault();
    expanderType peekNextFault();
    expanderType currentFault;
    uint8_t szOutbuf, szCbuf, szExt, szFaultQueue;
    uint8_t idx, outFaultIdx, inFaultIdx;
    int gidx;
    volatile int extidx;
    uint8_t write_Seq;
    void onStatus(char * , int * );
    void onDisplay(char * , int * );
    void writeChars();
    volatile uint8_t markPulse;
    uint8_t readChars(int, char * , int * , int);
    bool validChksum(char * , int, int);
    void readChar(char * , int * );
    void onLrr(char * , int * );
    void onExp(char * );
    keyType getChar();
    uint8_t peekNextKpAddr();
    uint8_t writeSeq, expSeq;
    char expZone;
    char haveExpMessage;
    char expFault, expBitAddr;
    char expFaultBits;
    bool decodePacket();
    bool getExtBytes();
    volatile bool is2400;
    bool validMonitorPin;

    char ICACHE_RAM_ATTR addrToBitmask1(char addr) {
        if (addr > 7) return 0xFF;
        else return 0xFF ^ (0x01 << (addr));
    }
    char ICACHE_RAM_ATTR addrToBitmask2(char addr) {
        if (addr < 8) return 0;
        else if (addr > 16) return 0xFF;
        else return 0xFF ^ (0x01 << (addr - 8));
    }
    char ICACHE_RAM_ATTR addrToBitmask3(char addr) {
        if (addr < 16) return 0;
        else return 0xFF ^ (0x01 << (addr - 16));
    }

    void hw_wdt_disable();
    void hw_wdt_enable();


    char expectByte;
    volatile uint8_t retries;
    volatile bool sending;
};

#endif