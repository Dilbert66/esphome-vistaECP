#include "vista.h"

void Vista::decodeBeeps(){
    uint8_t beeps=statusFlags.beeps;
    switch (beeps) {
      case 0:  outStream->println(F("\tno tone"));
        break;
       case 1:  outStream->println(F("\tchime once"));
        break;       
       case 2:  outStream->println(F("\tchime twice"));
        break;
       case 3:  outStream->println(F("\tchime three times"));
        break; 
       case 4:  outStream->println(F("\tfast repeating tone (used when arm/disarm delay timeout is nearing completion)"));
        break;
        case 5: 
        case 6:  outStream->println(F("\tslow repeating tone (used for arm/disarm delay timeout)"));
        break;
        case 7:  outStream->println(F("\tcontinuous loud tone (alarm triggered)"));
        break;           
    
    }
  
}
void Vista::decodeKeypads(){
    
      
    uint8_t kbyte=statusFlags.keypad[0];
    if (kbyte & 0x01) {
        outStream->println(F(" kp0: active"));
        }
    if (kbyte & 0x02) {
        outStream->println(F(" kp1: active"));
        }
  if (kbyte & 0x04) {
        outStream->println(F(" kp2: active"));
        }
    if (kbyte & 0x08) {
        outStream->println(F(" kp3: active"));
        }
  if (kbyte & 0x10) {
        outStream->println(F(" kp4: active"));
        }
  if (kbyte & 0x20) {
        outStream->println(F(" kp5: active"));
        }
  if (kbyte & 0x40) {
        outStream->println(F(" kp6: active"));
        }
  if (kbyte & 0x80) {
        outStream->println(F(" kp7: active"));
  }  
     kbyte=statusFlags.keypad[1];
    if (kbyte & 0x01) {
        outStream->println(F(" kp8: active"));
        }
    if (kbyte & 0x02) {
        outStream->println(F(" kp9: active"));
        }
  if (kbyte & 0x04) {
        outStream->println(F(" kp10: active"));
        }
    if (kbyte & 0x08) {
        outStream->println(F(" kp11: active"));
        }
  if (kbyte & 0x10) {
        outStream->println(F(" kp12: active"));
        }
  if (kbyte & 0x20) {
        outStream->println(F(" kp13: active"));
        }
  if (kbyte & 0x40) {
        outStream->println(F(" kp14: active"));
        }
  if (kbyte & 0x80) {
        outStream->println(F(" kp15: active"));
  }     
    kbyte=statusFlags.keypad[2];
    if (kbyte & 0x01) {
        outStream->println(F(" kp16: active"));
        }
    if (kbyte & 0x02) {
        outStream->println(F(" kp17: active"));
        }
  if (kbyte & 0x04) {
        outStream->println(F(" kp18: active"));
        }
    if (kbyte & 0x08) {
        outStream->println(F(" kp19: active"));
        }
  if (kbyte & 0x10) {
        outStream->println(F(" kp20: active"));
        }
  if (kbyte & 0x20) {
        outStream->println(F(" kp21: active"));
        }
  if (kbyte & 0x40) {
        outStream->println(F(" kp22: active"));
        }
  if (kbyte & 0x80) {
        outStream->println(F(" kp23: active"));
  }
        
    kbyte=statusFlags.keypad[3];
    if (kbyte & 0x01) {
        outStream->println(F(" kp24: active"));
        }
    if (kbyte & 0x02) {
        outStream->println(F(" kp25: active"));
        }
  if (kbyte & 0x04) {
        outStream->println(F(" kp26: active"));
        }
    if (kbyte & 0x08) {
        outStream->println(F(" kp27: active"));
        }
  if (kbyte & 0x10) {
        outStream->println(F(" kp28: active"));
        }
  if (kbyte & 0x20) {
        outStream->println(F(" kp29: active"));
        }
  if (kbyte & 0x40) {
        outStream->println(F(" kp30: active"));
        }
  if (kbyte & 0x80) {
        outStream->println(F(" kp31: active"));
        }
  
}



void Vista::printTrouble() {
        outStream->print("LRR qual:");
         outStream->println(statusFlags.lrr.qual);
        outStream->print("LRR code:");
         outStream->println(statusFlags.lrr.code);
        outStream->print("LRR user/zone:");
         outStream->println(statusFlags.lrr.zone);
        outStream->print("LRR partition:");
         outStream->println(statusFlags.lrr.partition);
    
}

void Vista::printPacket(char cbuf[], int len) {

  outStream->print(F("\nPacket: "));
  for (int x=0;x<len;x++) {
    if (x>0) outStream->print(",");
    outStream->print( cbuf[x], HEX);
  }
  outStream->println();
  
}

 void Vista::printStatus() {

  outStream->println(F("\n*** F7 status ***"));
  outStream->print(F("Beeps: ")); outStream->println(statusFlags.beeps,BIN);decodeBeeps();
  outStream->print(F("Armed Stay: ")); outStream->println(statusFlags.armedStay,BIN);
  outStream->print(F("Armed Away: ")); outStream->println(statusFlags.armedAway,BIN); 
  outStream->print(F("Chime: ")); outStream->println(statusFlags.chime,BIN);  
  outStream->print(F("AC Power: ")); outStream->println(statusFlags.acPower,BIN);
  outStream->print(F("Ready: ")); outStream->println(statusFlags.ready,BIN);
  outStream->print(F("Entry Delay: ")); outStream->println(statusFlags.entryDelay,BIN); 
  outStream->print(F("Program Mode: ")); outStream->println(statusFlags.programMode,BIN);
  outStream->print(F("Zone Bypass: ")); outStream->println(statusFlags.zoneBypass,BIN);
  outStream->print(F("Alarm: ")); outStream->println(statusFlags.alarm,BIN);
  outStream->print(F("Low Battery: ")); outStream->println(statusFlags.lowBattery,BIN);
  outStream->print(F("System Trouble: ")); outStream->println(statusFlags.systemTrouble,BIN);
  outStream->print(F("Fire: ")); outStream->println(statusFlags.fire,BIN);
  outStream->print(F("Backlight: ")); outStream->println(statusFlags.backlight,BIN);
  outStream->print(F("Keypads: ")); outStream->print(statusFlags.keypad[0],HEX);outStream->print(F(":"));outStream->print(statusFlags.keypad[1],HEX);outStream->print(F(":"));outStream->print(statusFlags.keypad[2],HEX);outStream->print(F(":"));outStream->println(statusFlags.keypad[3],HEX);decodeKeypads();
 
  outStream->println(F("\n*** F2 status ***"));
  outStream->print(F("Armed: ")); outStream->println(statusFlags.armed,BIN);
  outStream->print(F("Away: ")); outStream->println(statusFlags.away,BIN);
  outStream->print(F("Bypass: ")); outStream->println(statusFlags.bypass,BIN);
  outStream->print(F("No Alarm: ")); outStream->println(statusFlags.noAlarm,BIN);
  outStream->print(F("Exit Delay: ")); outStream->println(statusFlags.exitDelay,BIN);

  outStream->print(F("Fault: ")); outStream->println(statusFlags.fault,BIN);
  outStream->print(F("Panic Alarm: ")); outStream->println(statusFlags.panicAlarm,BIN);
  outStream->print(F("Zone: ")); outStream->println(statusFlags.zone,DEC);
  outStream->print(F("Prompt: ")); outStream->println(statusFlags.prompt);
   outStream->print(F("Beeps: ")); outStream->println(statusFlags.beeps,BIN);decodeBeeps();
  outStream->print(F("Prompt Position: ")); outStream->println(statusFlags.promptPos,HEX);

    
 }
