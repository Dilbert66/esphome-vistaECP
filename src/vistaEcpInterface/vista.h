#ifndef __VISTA_H
#define __VISTA_H
#include "Arduino.h"
#include "ECPSoftwareSerial.h"

#ifdef ESP32
#define ICACHE_RAM_ATTR IRAM_ATTR
#endif


//#define DEBUG

#define MONITORTX

#define D5 (14)
#define D6 (12)
#define D7 (13)
#define D8 (15)
#define TX (1)
#define D1 (5)
#define D2 (4)

// Used to read bits on F7 message
#define BIT_MASK_BYTE1_BEEP  0x07
#define BIT_MASK_BYTE1_NIGHT 0x10

#define  BIT_MASK_BYTE2_ARMED_HOME  0x80
#define  BIT_MASK_BYTE2_LOW_BAT     0x40
#define  BIT_MASK_BYTE2_ALARM_ZONE   0x20
#define  BIT_MASK_BYTE2_READY       0x10
#define  BIT_MASK_BYTE2_AC_LOSS    0x08 //zone ok?
#define  BIT_MASK_BYTE2_SYSTEM_FLAG   0x04 // need to test with good ac and not low battery
#define  BIT_MASK_BYTE2_CHECK_FLAG   0x02 // need to test with good ac and not low battery
#define  BIT_MASK_BYTE2_FIRE       0x01

#define  BIT_MASK_BYTE3_INSTANT     0x80
#define  BIT_MASK_BYTE3_UNKNOWN     0x40
#define  BIT_MASK_BYTE3_CHIME_MODE  0x20
#define  BIT_MASK_BYTE3_BYPASS      0x10
#define  BIT_MASK_BYTE3_AC_POWER    0x08
#define  BIT_MASK_BYTE3_ARMED_AWAY  0x04
#define  BIT_MASK_BYTE3_SYSTEM_ALARM  0x02
#define  BIT_MASK_BYTE3_IN_ALARM  0x01

#define  F7_MESSAGE_LENGTH  45
#define  F8_MESSAGE_LENGTH  9
#define  N98_MESSAGE_LENGTH  6

#define MAX_MODULES 9

enum ecpState { sPulse, sNormal, sAckf7,sSendkpaddr,sPolling };


struct statusFlagType {
      char beeps:3;
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
      uint8_t attempts=10;
  struct {
  int code;
  uint8_t qual;
  uint8_t zone;
  uint8_t user;
  uint8_t partition;
}lrr;
 
 };
 
 struct expanderType {
     char expansionAddr;
     char expFault;
     char expFaultBits;
     char relayState;
 };
 

class Vista {
  
  public:
  Vista(int receivePin, int transmitPin, char keypadAddr, Stream *OutStream,int monitorPin);
  ~Vista();
  void begin();
  void stop();
  bool handle();
  void outQueue(char byt);
  void printStatus();
  void printTrouble();
  void decodeBeeps();
  void decodeKeypads();
  void printPacket(char*,int);
  void write(const char*);
  void write(const char);
  statusFlagType statusFlags;
  SoftwareSerial *vistaSerial,*vistaSerialMonitor;
  void setKpAddr(char keypadAddr) { kpAddr=keypadAddr; }
  bool dataReceived;
  void ICACHE_RAM_ATTR rxHandleISR(),txHandleISR();
  bool areEqual(char*,char*,uint8_t);
  bool keybusConnected;
  int toDec(int);
  void resetStatus();
  char  *cbuf, *outbuf,*extbuf,*extcmd;
  bool lrrSupervisor;
  char expansionAddr;
  void setExpFault(int,bool);
  bool newExtCmd,newCmd,newRelCmd;
  bool filterOwnTx;
  expanderType zoneExpanders[MAX_MODULES];
  char b;//used in isr
  bool charAvail();
  bool sendPending();
  
  private:
  uint8_t outbufIdx,inbufIdx; //we will check this outside of the class
  char tmpOutBuf[20];
  int rxPin, txPin;
  char kpAddr,monitorTxPin;
  volatile char ackAddr;
  Stream *outStream;
  volatile ecpState rxState;
  volatile unsigned long lowTime;
  expanderType *faultQueue;
  void setNextFault(expanderType);
  expanderType getNextFault();
  expanderType peekNextFault();
  expanderType currentFault;
  uint8_t szOutbuf,szCbuf,szExt,szFaultQueue; 
  uint8_t idx,outFaultIdx,inFaultIdx;
  int gidx;
  volatile int extidx;
  uint8_t write_Seq;
  void onStatus(char*,int*);
  void onDisplay(char*,int*);
  void writeChars();
  volatile uint8_t markPulse;
  uint8_t readChars(int,char*,int*,int);
  void readChar(char*,int*);
  void onLrr(char*,int*);
  void onExp(char*);
  char getChar();
  uint8_t writeSeq,expSeq;
  char expZone;
  char haveExpMessage;
  char expFault,expBitAddr;
  char expFaultBits;
  void decodePacket();
  bool gotcmd;

  
  char ICACHE_RAM_ATTR addrToBitmask1(char addr) { if (addr > 7) return 0xFF; else return 0xFF ^ (0x01 << (addr)); }
  char ICACHE_RAM_ATTR addrToBitmask2(char addr) { if (addr < 8) return 0; else if (addr > 16) return 0xFF; else return 0xFF ^ (0x01 << (addr - 8)); }
  char ICACHE_RAM_ATTR addrToBitmask3(char addr) { if (addr < 16) return 0; else return 0xFF ^ (0x01 << (addr - 16)); }

  void clearExpect();
  
  void hw_wdt_disable();
 
  void hw_wdt_enable();
 
  std::function<void (char)> expectCallbackComplete;
  std::function<void (char)> expectCallbackError;
  
  bool onResponseError(char);
  bool onResponseComplete(char);
  void setOnResponseErrorCallback(std::function<void (char data)> callback) { expectCallbackError = callback; }
  void setOnResponseCompleteCallback(std::function<void (char data)> callback) { expectCallbackComplete = callback; }
  char expectByte;
  void keyAckComplete(char);
  volatile uint8_t retries=0;
  volatile bool sending;
};

#endif
