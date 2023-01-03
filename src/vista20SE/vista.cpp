#include "vista.h"

#include "Arduino.h"

Vista * pointerToVistaClass;

void ICACHE_RAM_ATTR rxISRHandler() { // define global handler
  pointerToVistaClass -> rxHandleISR(); // calls class member handler
}

#ifdef MONITORTX
void ICACHE_RAM_ATTR txISRHandler() { // define global handler
  pointerToVistaClass -> txHandleISR(); // calls class member handler
}
#endif

//Vista::Vista(Stream *stream):outStream(stream)
Vista::Vista(Stream * stream) {
  outStream = stream;

  #ifdef MONITORTX
  szExt = 20;
  extbuf = (char * ) malloc(szExt);
  extcmd = (char * ) malloc(szExt);
  #endif

  szOutbuf = 15;
  szCbuf = 50;
  inbufIdx = 0;
  outbufIdx = 0;
  rxState = sNormal;
  pointerToVistaClass = this;
  cbuf = (char * ) malloc(szCbuf);
  outbuf = new keyType[szOutbuf];
  szFaultQueue = 5;
  faultQueue = (uint8_t * ) malloc(szFaultQueue);
  lrrSupervisor = false;

}

Vista::~Vista() {
  free(vistaSerial);
  detachInterrupt(rxPin);
  #ifdef MONITORTX
  if (validMonitorPin) {
    free(vistaSerialMonitor);
    detachInterrupt(monitorPin);
  }
  #endif
  pointerToVistaClass = NULL;
}
expanderType Vista::getNextFault() {
  uint8_t currentFaultIdx;
  expanderType currentFault;
  if (inFaultIdx == outFaultIdx) return currentFault;
  currentFaultIdx = faultQueue[outFaultIdx];
  outFaultIdx = (outFaultIdx + 1) % szFaultQueue;
  return zoneExpanders[currentFaultIdx];
}

expanderType ICACHE_RAM_ATTR Vista::peekNextFault() {
  expanderType currentFault;
  if (inFaultIdx == outFaultIdx) return currentFault;
  return zoneExpanders[faultQueue[outFaultIdx]];
}

void Vista::setNextFault(uint8_t idx) {
  faultQueue[inFaultIdx] = idx;
  inFaultIdx = (inFaultIdx + 1) % szFaultQueue;
}

void Vista::readChar(char buf[], int * idx) {
  char c;
  int idxval = * idx;
  int t = 0;
  unsigned long timeout = millis();

  while (!vistaSerial -> available() && millis() - timeout < 100);

  if (vistaSerial -> available()) {
    c = vistaSerial -> read();
    buf[idxval++] = c;
    * idx = idxval;
  }

}

uint8_t Vista::readChars(int ct, char buf[], int * idx, int limit) {
  char c;
  int x = 0;
  int idxval = * idx;
  if (ct > limit) return 1;
  unsigned long timeout = millis();
  while (x < ct && millis() - timeout < 100) {
    if (vistaSerial -> available()) {
      timeout = millis();
      c = vistaSerial -> read();
      buf[idxval++] = c;
      x++;
    }
  }
  * idx = idxval;
  return 0;
}

void Vista::onStatus(char cbuf[], int * idx) {

  //byte 2 is length of message
  //byte 3 is length of headers
  //last byte of headers is counter
  //remaining bytes are body

  //F2 messages with 18 bytes or less don't seem to have
  // any important information
  if (18 >= (uint8_t) cbuf[1]) {
    return;
  }

  //19th spot seems to be a decimal value
  //01 is disarmed
  //02 is armed
  //03 is disarmed chime
  //short armed = 2 == (uint8_t) cbuf[19];
  statusFlags.armed = (0x02 & cbuf[19]) && !(cbuf[19] & 0x01);
  //20th spot is away / stay
  // this bit is really confusing
  // it clearly switches to 2 when you set away mode
  // but it is also 0x02 when an alarm is canceled,
  // but not cleared - even if you are in stay mode.
  //11th  byte in status body

  statusFlags.away = ((0x02 & cbuf[20]) > 0);

  statusFlags.zoneBypass = ((0x02 & cbuf[21]) > 0);
  //21st spot is for bypass
  //12th byte in status_body
  //short bypass = 0x02 & status_body[12];

  //22nd spot is for alarm types
  //1 is no alarm
  //2 is ignore faults (like exit delay)
  //4 is a alarm
  //6 is a fault that does not cause an alarm
  //8 is for panic alarm.

  statusFlags.noAlarm = ((cbuf[22] & 0x01) > 0);
  statusFlags.exitDelay = ((cbuf[22] & 0x02) > 0);
  statusFlags.fault = ((cbuf[22] & 0x04) > 0);
  statusFlags.panicAlarm = ((cbuf[22] & 0x08) > 0);

  if (statusFlags.armed && statusFlags.fault && !statusFlags.exitDelay) {
    statusFlags.alarm = 1;
  } else if (!statusFlags.armed && statusFlags.fault && !statusFlags.exitDelay) {
    statusFlags.alarm = 0;
  }

}

void Vista::onDisplay(char cbuf[], int * idx) {
  // Byte 0 = F7
  // Bytes 1,2,3,4 are the keypad address masks 
  // 5th byte represents zone
  // 6th beeps/night mode
  // 7th various system statuses 
  // 8th various system statuses 
  // 9th byte Programming mode = 0x01
  // 10th byte prompt position in the display message of the expected input

  
  statusFlags.keypad[0] = cbuf[1]; //0 to 7

  statusFlags.keypad[1] = cbuf[2]; //8 to 15

  statusFlags.keypad[2] = cbuf[3]; //16 - 23

  statusFlags.keypad[3] = cbuf[4]; //24 - 30.  High bit is unkown flag.

  statusFlags.zone = (int) toDec(cbuf[5]);

  statusFlags.beeps = cbuf[6] & BIT_MASK_BYTE1_BEEP;
  statusFlags.night = ((cbuf[6] & BIT_MASK_BYTE1_NIGHT) > 0);

  statusFlags.fire = ((cbuf[7] & BIT_MASK_BYTE2_FIRE) > 0);
  statusFlags.systemFlag = ((cbuf[7] & BIT_MASK_BYTE2_SYSTEM_FLAG) > 0);
  statusFlags.ready = ((cbuf[7] & BIT_MASK_BYTE2_READY) > 0);

  if (statusFlags.systemFlag) {
    statusFlags.armedStay = ((cbuf[7] & BIT_MASK_BYTE2_ARMED_HOME) > 0);
    statusFlags.lowBattery = ((cbuf[7] & BIT_MASK_BYTE2_LOW_BAT) > 0);
    statusFlags.acLoss = ((cbuf[7] & BIT_MASK_BYTE2_AC_LOSS) > 0);
  } else {
    statusFlags.check = ((cbuf[7] & BIT_MASK_BYTE2_CHECK_FLAG) > 0);
    statusFlags.fireZone = ((cbuf[7] & BIT_MASK_BYTE2_ALARM_ZONE) > 0);
  }

  statusFlags.inAlarm = ((cbuf[8] & BIT_MASK_BYTE3_IN_ALARM) > 0);
  statusFlags.acPower = ((cbuf[8] & BIT_MASK_BYTE3_AC_POWER) > 0);
  statusFlags.chime = ((cbuf[8] & BIT_MASK_BYTE3_CHIME_MODE) > 0);
  statusFlags.bypass = ((cbuf[8] & BIT_MASK_BYTE3_BYPASS) > 0);
  statusFlags.programMode = (cbuf[8] & BIT_MASK_BYTE3_PROGRAM);
  if (statusFlags.systemFlag) {
    statusFlags.instant = ((cbuf[8] & BIT_MASK_BYTE3_INSTANT) > 0);
    statusFlags.armedAway = ((cbuf[8] & BIT_MASK_BYTE3_ARMED_AWAY) > 0);
  } else {
    statusFlags.alarm = ((cbuf[8] & BIT_MASK_BYTE3_ZONE_ALARM) > 0);
  }
  /*
  if (!statusFlags.inAlarm && (statusFlags.fireZone || statusFlags.alarm) )
           statusFlags.cancel=true;
   else
      statusFlags.cancel=false;
  */

  statusFlags.promptPos = cbuf[10];

  int y = 0;
  statusFlags.backlight = ((cbuf[12] & 0x80) > 0);
  cbuf[12] = (cbuf[12] & 0x7F);
  for (int x = 12; x < * idx - 1; x++) {
    if ((uint8_t) cbuf[x] > 31 ) {
      statusFlags.prompt[y++] = cbuf[x];
    }
  }
  statusFlags.prompt[y] = '\0'; //add string terminator

}

int Vista::toDec(int n) {
  char b[4];
  char * p;
  itoa(n, b, 16);
  long int li = strtol(b, & p, 10);

  return (int) li;
}

/**
 * Typical packet
 * positions 8 and 9 hold the report code
 * in Ademco Contact ID format
 * the lower 4 bits of 8 and both bites of 9
 * ["F9","43","0B","58","80","FF","FF","18","14","06","01","00","20","90"]}
 *
 * It seems that, for trouble indicators (0x300 range) the qualifier is flipped
 * where 1 means "new" and 3 means "restored"
 * 0x48 is a startup sequence, the byte after 0x48 will be 00 01 02 03
 //1=new event or opening, 3=new restore or closing,6=previous still present 
 */
void Vista::onLrr(char cbuf[], int * idx) {

  int len = cbuf[2];

  if (len == 0) return;
  sending = true;
  char type = cbuf[3];
  char lcbuf[12];
  delayMicroseconds(500);
  int lcbuflen = 0;

  //0x52 means respond with only cycle message
  //0x48 means same thing
  //, i think 0x52 and and 0x48 are the same
  if (type == (char) 0x52 || type == (char) 0x48

  ) {
    lcbuf[0] = (char) cbuf[1];
    lcbuflen++;
  } else if (type == (char) 0x58) {
    //just respond, but 0x58s have lots of info
    int c = (((0x0f & cbuf[8]) << 8) | cbuf[9]);
    c = toDec(c); //convert to decimal representation for correct code display
    statusFlags.lrr.qual = (uint8_t)(0xf0 & cbuf[8]) >> 4;
    statusFlags.lrr.code = c;
    statusFlags.lrr.zone = toDec(((uint8_t) cbuf[12] >> 4) | ((uint8_t) cbuf[11] << 4));
    statusFlags.lrr.user = statusFlags.lrr.zone;
    statusFlags.lrr.partition = (uint8_t) cbuf[10];

    lcbuf[0] = (char)(cbuf[1]);
    lcbuflen++;
  } else if (type == (char) 0x53){ 

    lcbuf[0] = (char)((cbuf[1] + 0x40) & 0xFF);
    lcbuf[1] = (char) 0x04;
    lcbuf[2] = (char) 0x00;
    lcbuf[3] = (char) 0x00;
    //0x08 is sent if we're in test mode
    //0x0a after a test
    //0x04 if you have network problems?
    //0x06 if you have network problems?
    lcbuf[4] = (char) 0x00;
    lcbuflen = 5;
    expectByte = lcbuf[0];
  }

  //we don't need a checksum for 1 byte messages (no length bit)
  //if we don't even have a message length byte, then we are just
  // ACKing a cycle header byte.
  if (lcbuflen >= 2) {
    uint32_t chksum = 0;
    for (int x = 0; x < lcbuflen; x++) {
      chksum += lcbuf[x];
    }
    chksum -= 1;
    chksum = chksum ^ 0xFF;
    lcbuf[lcbuflen] = (char) chksum;
    lcbuflen++;
  }

  if (lrrSupervisor) {
    vistaSerial -> setBaud(4800);
    for (int x = 0; x < lcbuflen; x++) {
      vistaSerial -> write(lcbuf[x]);
    }
  }
  sending = false;
}

void Vista::setExpFault(int zone, bool fault) {
  //expander address 7 - zones: 9 - 16
  //expander address 8 - zones:  17 - 24
  //expander address 9 - zones: 25 - 32
  //expander address 10 - zones: 33 - 40
  //expander address 11 - zones: 41 - 48
  uint8_t idx = 0;
  expansionAddr = 0;
  for (uint8_t i = 0; i < MAX_MODULES; i++) {
    switch (zoneExpanders[i].expansionAddr) {
    case 7:
      if (zone > 8 && zone < 17) {
        idx = i;
        expansionAddr = zoneExpanders[i].expansionAddr;
      }
      break;
    case 8:
      if (zone > 16 && zone < 25) {
        idx = i;
        expansionAddr = zoneExpanders[i].expansionAddr;
      }
      break;
    case 9:
      if (zone > 24 && zone < 33) {
        idx = i;
        expansionAddr = zoneExpanders[i].expansionAddr;
      }
      break;

    case 10:
      if (zone > 32 && zone < 41) {
        idx = i;
        expansionAddr = zoneExpanders[i].expansionAddr;
      }
      break;

    case 11:
      if (zone > 40 && zone < 49) {
        idx = i;
        expansionAddr = zoneExpanders[i].expansionAddr;
      }
      break;
    default:
      break;
    }
    if (expansionAddr) break;
  }
  if (!expansionAddr) return;
  expFaultBits = zoneExpanders[idx].expFaultBits;

  int z = zone % 8; //convert zone to range of 1 - 7,0 (last zone is 0)
  expFault = z << 5 | (fault ? 0x8 : 0); //0 = terminated(eol resistor), 0x08=open, 0x10 = closed (shorted)  - convert to bitfield for F1 response
  if (z > 0) z--;
  else z = 7; //now convert to 0 - 7 for F7 poll response
  expFaultBits = (fault ? expFaultBits | (0x80 >> z) : expFaultBits & ((0x80 >> z) ^ 0xFF)); //setup bit fields for return response with fault values for each zone
  expanderType lastFault = peekNextFault();
  if (lastFault.expansionAddr != expansionAddr || lastFault.expFault != expFault || lastFault.expFaultBits != expFaultBits) {
    zoneExpanders[idx].expFault = expFault;
    zoneExpanders[idx].expFaultBits = expFaultBits;
    setNextFault(idx); //push to the pending queue
  }
}

//98 2E 02 20 F7 EC
//98 2E 04 20 F7 EA
//98 2E 01 04 25 F1 EA - relay address 14 and 15  have an extra byte . byte 2 is flag and shifts other bytes right
//98 2E 40 35 00 01 xx - relay cmd 00 has the extra cmd data byte
// byte 3 is the binary position encoded expander addresss 02=107, 04=108,08=109, etc
//byte 4 is a seq byte, changes from 20 to 25 every request sequence
//byte 5 F7 for poll, F1 for a msg request, 80 for a resend request
// The 98 serial data has some inconsistencies with some bit durations on byte 1. Might be an issue with my panel.  Be interested to know how it looks on another panel.  The checksum does not work as a two's complement so something is off. if instead of 0x2e we use 0x63, it works.  Either way, the bytes we need work fine for our purposes.

void Vista::onExp(char cbuf[]) {

  char type = cbuf[4];
  char seq = cbuf[3];
  char lcbuf[4];
  sending = true;

  int idx;

  if (cbuf[2] & 1) {
    seq = cbuf[4];
    type = cbuf[5];
    for (idx = 0; idx < MAX_MODULES; idx++) {
      expansionAddr = zoneExpanders[idx].expansionAddr;
      if (cbuf[2] == (0x01 << (expansionAddr - 13))) break; //for us - relay addresses 14-15
    }

  } else {
    for (idx = 0; idx < MAX_MODULES; idx++) {
      expansionAddr = zoneExpanders[idx].expansionAddr;
      if (cbuf[2] == (0x01 << (expansionAddr - 6))) break; //for us - address range 7 -13
    }
  }
  if (idx == MAX_MODULES) {
    sending = false;
    return; //no match return
  }
  expFaultBits = zoneExpanders[idx].expFaultBits;

  int lcbuflen = 0;
  expSeq = (seq == 0x20 ? 0x34 : 0x31);

  // we use zone to either | or & bits depending if in fault or reset
  //0xF1 - response to request, 0xf7 - poll, 0x80 - retry ,0x00 relay control
  if (type == 0xF1) {
    expanderType currentFault = peekNextFault(); //check next item. Don't pop it yet
    if (currentFault.expansionAddr && expansionAddr == currentFault.expansionAddr) {
      getNextFault(); //pop it from the queue since we are processing it now
    } else
      currentFault = zoneExpanders[idx]; //no pending fault, use current fault data instead
    lcbuflen = (char) 4;
    lcbuf[0] = (char) currentFault.expansionAddr;
    lcbuf[1] = (char) expSeq;
    //lcbuf[2] = (char) currentFault.relayState;
    lcbuf[2] = 0;
    lcbuf[3] = (char) currentFault.expFault; // we send out the current zone state 

  } else if (type == 0xF7) { // periodic  zone state poll (every 30 seconds) expander
    lcbuflen = (char) 4;
    lcbuf[0] = (char) 0xF0;
    lcbuf[1] = (char) expSeq;
    //lcbuf[2]= (char) expFaultBits ^ 0xFF; //closed zones - opposite of expfaultbits. If set in byte3 we clear here. (not used )
    lcbuf[2] = 0; // we simulate having a termination resistor so set to zero for all zones
    lcbuf[3] = (char) expFaultBits; //opens zones - we send out the list of zone states. if 0 in both fields, means terminated 

  } else if (type == 0x00 || type == 0x0D) { // relay module
    lcbuflen = (char) 4;
    lcbuf[0] = (char) expansionAddr;
    lcbuf[1] = (char) expSeq;
    lcbuf[2] = (char) 0x00;
    if (cbuf[2] & 1) { //address 14/15
      zoneExpanders[idx].relayState = cbuf[6] & 0x80 ? zoneExpanders[idx].relayState | (cbuf[6] & 0x7f) : zoneExpanders[idx].relayState & ((cbuf[6] & 0x7f) ^ 0xFF);
      lcbuf[3] = (char) cbuf[6];
    } else {
      zoneExpanders[idx].relayState = cbuf[5] & 0x80 ? zoneExpanders[idx].relayState | (cbuf[5] & 0x7f) : zoneExpanders[idx].relayState & ((cbuf[5] & 0x7f) ^ 0xFF);
      lcbuf[3] = (char) cbuf[5];
    }
  } else {
    sending = false;
    return; //we don't acknowledge if we don't know  //0x80 or 0x81 
  }

  delayMicroseconds(500);
  uint32_t chksum = 0;
  vistaSerial -> setBaud(4800);
  for (int x = 0; x < lcbuflen; x++) {
    chksum += lcbuf[x];
    vistaSerial -> write(lcbuf[x]);
  }
  chksum -= 1;
  chksum = chksum ^ 0xFF;
  vistaSerial -> write((char) chksum);

  sending = false;
}


void Vista::write(const char key, uint8_t addr) {

  if ((key >= 0x30 && key <= 0x39) || key == 0x23 || key == 0x2a || (key >= 0x41 && key <= 0x44) || key==0x46 ||  key==0x4d ||  key==0x50 || key==0x47 ) {
    keyType kt;
    kt.key=key;
    kt.kpaddr=addr;
    outbuf[inbufIdx] = kt;
    inbufIdx = (inbufIdx + 1) % szOutbuf;
  }
}

void Vista::write(const char key) {
    write(key,kpAddr);
}

void Vista::write(const char * receivedKeys) {
  int x = 0;
  while (receivedKeys[x] != '\0') {
    write(receivedKeys[x],kpAddr);
    x++;
  }
}


void Vista::write(const char * receivedKeys, uint8_t addr) {
  int x = 0;
  while (receivedKeys[x] != '\0') {
    write(receivedKeys[x],addr);
    x++;
  }
}


keyType Vista::getChar() {
  keyType c;
  if (outbufIdx == inbufIdx) return c;
  c = outbuf[outbufIdx];
  outbufIdx = (outbufIdx + 1) % szOutbuf;
  return c;
}

uint8_t Vista::peekNextKpAddr() {
  if (outbufIdx == inbufIdx) return 0;
  keyType c = outbuf[outbufIdx];
  return c.kpaddr;
}

bool Vista::sendPending() {
  if (outbufIdx == inbufIdx && retries == 0)
    return false;
  else
    return true;
}

bool Vista::charAvail() {
  if (outbufIdx == inbufIdx)
    return false;
  else
    return true;
}

/**
 * Send 0-9, # and * characters
 * 1,2,3,4,5,6,7,8,9,0,#,*,
 * 31,32,33,34,35,36,37,38,39,30,23,2A,
 * 0011-0001,0011-0010,0011-0011,0011-0100,0011-0101,0011-0110,0011-0111,0011-1000,0011-1001,0011-0000,0010-0011,0010-1010,
 *
 */

void Vista::writeChars() {

  if (!charAvail() && retries == 0) return;

  //if retries are getting out of control with no successfull callback
  //just clear the buffer
  if (retries > 5) {
    retries = 0;
    expectByte = 0;
    outbufIdx = inbufIdx;
    return;
  }
  sending = true;
  delayMicroseconds(500);

  int tmpIdx = 0;
  //header is the bit mask YYXX-XXXX
  //  where YY is an incrementing sequence number
  //  and XX-XXXX is the keypad address (decimal 16-31)
  //vistaSerial->write(header);
  //vistaSerial->write(outbufIdx +1); 
  
   //adjust characters to hex values.
  //ASCII numbers get translated to hex numbers
  //# and * get converted to 0xA and 0xB
  // send any other chars straight, although this will probably 
  // result in errors
  //0xc = A (*/1), 0xd=B (*/#) , 0xe=C (3/#)

  if (retries == 0) {

    keyType kt;
    char c;
    int sz = 0;
    tmpIdx=2;
    uint8_t lastkpaddr=0;
    while (charAvail()) {
    if (!(lastkpaddr==0 || lastkpaddr==peekNextKpAddr())) break;
      kt = getChar();
      c=kt.key;
      ackAddr=kt.kpaddr;
      lastkpaddr=kt.kpaddr;
      sz++;
      //translate digits between 0-9 to hex/decimal
      if (c >= 0x30 && c <= 0x39) {
        c -= 0x30;
      } else
        //translate * to 0x0b
        if (c == 0x23) {
          c = 0x0B;
        } else
          //translate # to 0x0a
          if (c == 0x2A) {
            c = 0x0A;
          } else 
              if ( c==0x46) {// zone 95 (F)
                c=0x0C;
          } else 
             if (c==0x4D) { //zone 99 (M)
               c=0x0D;
           } else 
               if ( c==0x50) { // zone 96 (P)
               c=0x0E;
           } else 
             if (c==0x47) {// zone 92 (G)
                    c=0x0F;
           } else
            //translate A to 0x1C (function key A)
            //translate B to 0x1D (function key B)
            //translate C to 0x1E (function key C)
            //translate D to 0x1F (function key D)
            if (c >= 0x41 && c <= 0x44) {
              c = c - 0x25;
            } 
      tmpOutBuf[tmpIdx++] = c;
    }
    tmpOutBuf[0] = ((++writeSeq << 6) & 0xc0) | (ackAddr & 0x3F);  
    tmpOutBuf[1] = sz + 1;
  }
  vistaSerial -> setBaud(4800);
  vistaSerial -> write(tmpOutBuf[0]);
  vistaSerial -> write(tmpOutBuf[1]);
  int checksum = 0;
  for (int x = 2; x < tmpOutBuf[1] + 1; x++) {
    checksum += (char) tmpOutBuf[x];
    vistaSerial -> write(tmpOutBuf[x]);

  }
  uint32_t chksum = 0x100 - (tmpOutBuf[0] + tmpOutBuf[1] + checksum);
  vistaSerial -> write((char) chksum);
  expectByte = tmpOutBuf[0];
  retries++;
  sending = false;

}


void ICACHE_RAM_ATTR Vista::rxHandleISR() {
  static byte b;    
  if (digitalRead(rxPin)) {
      if (lowTime)
          lowTime=micros() - lowTime;
      highTime=micros();
      if ( lowTime > 9000 ) {
        markPulse = 2;
        expanderType currentFault = peekNextFault();
        if (currentFault.expansionAddr && currentFault.expansionAddr < 24) {
          ackAddr = currentFault.expansionAddr; // use the expander address 07/08/09/10/11 as the requestor
           vistaSerial -> write(addrToBitmask1(ackAddr), false, 4800);
           b = addrToBitmask2(ackAddr); 
           if (b) vistaSerial -> write(b, false, 4800);
           b = addrToBitmask3(ackAddr); 
           if (b) vistaSerial -> write(b, false, 4800);
        } else if (outbufIdx != inbufIdx || retries > 0) {
          keyType c = outbuf[outbufIdx]; //get pending keypad address
          ackAddr=c.kpaddr;
          if (ackAddr && ackAddr < 24) {
           vistaSerial -> write(addrToBitmask1(ackAddr), false, 4800);
           b = addrToBitmask2(ackAddr); 
           if (b) vistaSerial -> write(b, false, 4800);
           b = addrToBitmask3(ackAddr); 
           if (b) vistaSerial -> write(b, false, 4800);           
          } 
        }
        rxState = sPolling; // set flag to skip capturing pulses in the receive buffer during polling phase
      } else if ( lowTime > 4600 && rxState == sPolling) { // 2400 baud cmd preamble
        is2400 = true;
        markPulse = 3;
        rxState = sCmdHigh;
      } else if (lowTime  > 3000 && rxState == sPolling) { // 4800 baud cmd preamble
        is2400 = false;
        markPulse = 4;
        rxState = sNormal; // ok we have the message preamble. Lets start capturing receive bytes 
      }

    lowTime = 0;
  } else {
    if (highTime && micros() - highTime > 6000 && rxState==sNormal)
      rxState=sPolling;  
    if (rxState == sCmdHigh) // end 2400 baud cmd preamble
      rxState = sNormal;
    lowTime = micros();
    highTime=0;
    
  }
  if (rxState == sNormal || highTime==0 )
    vistaSerial -> rxRead(vistaSerial);

  #ifndef ESP32
  else //clear pending interrupts for this pin if any occur during transmission
    GPIO_REG_WRITE(GPIO_STATUS_W1TC_ADDRESS, 1 << rxPin);
  #endif
}

bool Vista::validChksum(char cbuf[], int start, int len) {
  uint16_t chksum = 0;
  for (uint8_t x = start; x < len; x++) {
    chksum += cbuf[x];
  }
  if (chksum % 256 == 0)
    return true;
  else
    return false;
}

#ifdef MONITORTX
void ICACHE_RAM_ATTR Vista::txHandleISR() {
  if ((!sending || !filterOwnTx) && rxState == sNormal)
    vistaSerialMonitor -> rxRead(vistaSerialMonitor);
}
#endif

#ifdef MONITORTX
bool Vista::decodePacket() {

  //format 0xFA deviceid subcommand channel on/off 
  if (extcmd[0] == 0xFA) {

    if (!validChksum(extbuf, 0, 5)) {
      extcmd[0] = extbuf[0];
      extcmd[1] = extbuf[1];
      extcmd[2] = extbuf[2];
      extcmd[3] = extbuf[3];
      extcmd[4] = extbuf[4];
      extcmd[5] = extbuf[5];
      extcmd[6] = extbuf[6];
      extcmd[12] = 0x71; //flag to identify chksum error
      newExtCmd = true;
      return 1; // for debugging return what was sent so we can see why the chcksum failed
    }

    char cmdtype = (extcmd[2] & 1) ? extcmd[5] : extcmd[4];

    if (extcmd[2] & 1) {
      for (uint8_t i = 0; i < 8; i++) {
        if ((extcmd[3] >> i) & 0x01) {
          extcmd[1] = i + 13; //get device id
          break;
        }
      }
      //shift bytes down
      extcmd[2] = extcmd[3];
      extcmd[3] = extcmd[4];
      extcmd[4] = extcmd[5];
    } else {
      for (uint8_t i = 0; i < 8; i++) {
        if ((extcmd[2] >> i) & 0x01) {
          extcmd[1] = i + 6; //get device id
          break;
        }
      }
    }
    if (cmdtype == 0xF1) { // expander channel update
      uint8_t channel;
      if (extbuf[3]) { //if we have zone expander data
        channel = (extbuf[3] >> 5);
        if (!channel) channel = 8;
        channel = ((extcmd[1] - 7) * 8) + 8 + channel; //calculate zone
        extcmd[4] = (extbuf[3] >> 3 & 3) ? 1 : 0; //fault
      } else {
        channel = 0; //no zone data so set to zero
        extcmd[4] = 0;
      }
      extcmd[2] = cmdtype; //copy subcommand to byte 2
      extcmd[3] = channel;
      extcmd[5] = extbuf[2]; //relay data
      extcmd[6] = 0;
      newExtCmd = true;
      return 1;

    } else if (cmdtype == 0xf7) { // expander poll request
      extcmd[2] = cmdtype; //copy subcommand to byte 2
      extcmd[3] = 0;
      extcmd[4] = extbuf[3]; //zone faults
      extcmd[5] = 0;
      extcmd[6] = 0;
      newExtCmd = true;
      return 1;
    } else if (cmdtype == 0x00 || cmdtype == 0x0D) { //relay channel update
      extcmd[2] = cmdtype; //copy subcommand to byte 2
      uint8_t channel;
      switch (extbuf[3] & 0x07f) {
      case 1:
        channel = 1;
        break;
      case 2:
        channel = 2;
        break;
      case 4:
        channel = 3;
        break;
      case 8:
        channel = 4;
        break;
      default:
        channel = 0;
      }
      extcmd[3] = channel;
      extcmd[4] = extbuf[3] & 0x80 ? 1 : 0;
      extcmd[5] = 0;
      extcmd[6] = 0;
      newExtCmd = true;
      return 1;
    } else { //unknown subcommand for FA
      extcmd[0] = extbuf[0];
      extcmd[1] = extbuf[1];
      extcmd[2] = extbuf[2];
      extcmd[3] = extbuf[3];
      extcmd[4] = extbuf[4];
      extcmd[5] = extbuf[5];
      extcmd[6] = extbuf[6];
      extcmd[12] = 0x72; //flag to identify unknown subcommand
      newExtCmd = true;
      return 1; // for debugging return what was sent so we can see why the chcksum failed

    }
  } else if (extcmd[0] == 0xFB) {
    // Check how many bytes are in RF message (stored in upper nibble of Byte 2)
    uint8_t n_rf_bytes = extbuf[1] >> 4;

    if (n_rf_bytes == 5) { // For monitoring, we only care about 5 byte messages since that contains data about sensors
      // Verify data 
      uint16_t rf_checksum = 0;
      for (uint8_t i = 0; i < n_rf_bytes + 2 ; i++) {
        rf_checksum += extbuf[i];
      }
      if (rf_checksum % 256 == 0) {
        // If checksum is correct, fill extcmd with data
        // Set second byte of extcmd to number of data bytes
        extcmd[1] = 4;
        // The 3rd, 4th, and 5th bytes in extbuf have the sending device serial number
        // The 3rd byte has the MSB of the number and the 5th has the LSB
        // Fill these into extcmd
        extcmd[2] = extbuf[2] & 0xF;
        extcmd[3] = extbuf[3];
        extcmd[4] = extbuf[4];
        // 6th byte in extbuf contains the sensor data
        // bit 1 - ?
        // bit 2 - Battery (0=Normal, 1=LowBat)
        // bit 3 - Heartbeat (Sent every 60-90min) (1 if sending heartbeat)
        // bit 4 - ?
        // bit 5 - Loop 3 (0=Closed, 1=Open)
        // bit 6 - Loop 2 (0=Closed, 1=Open)
        // bit 7 - Loop 4 (0=Closed, 1=Open)
        // bit 8 - Loop 1 (0=Closed, 1=Open)
        extcmd[5] = extbuf[5];
        extcmd[6] = 0;
        newExtCmd = true;
        return 1;

        // How to rebuild serial into single integer
        // uint32_t device_serial = (extbuf[2] & 0xF) << 16;  // Only the lower nibble is part of the device serial
        // device_serial += extbuf[3] << 8;
        // device_serial += extbuf[4];
      }
      //  #ifdef DEBUG
      else {
        // also print if chksum fails
        extcmd[0] = extbuf[0];
        extcmd[1] = extbuf[1];
        extcmd[2] = extbuf[2];
        extcmd[3] = extbuf[3];
        extcmd[4] = extbuf[4];
        extcmd[5] = extbuf[5];
        extcmd[6] = extbuf[6];
        extcmd[12] = 0x73; //flag to identify cheksum failed             
        newExtCmd = true;
        return 1;
        // outStream->println("RF Checksum failed.");
      }
      //  #endif

    } else {
      // FB packet but with different length then 5
      // we send out the packet as received for debugging
      extcmd[0] = extbuf[0];
      extcmd[1] = extbuf[1];
      extcmd[2] = extbuf[2];
      extcmd[3] = extbuf[3];
      extcmd[4] = extbuf[4];
      extcmd[5] = extbuf[5];
      extcmd[6] = extbuf[6];
      extcmd[12] = 0x74; //flag to identify unknown command
      newExtCmd = true;
      return 1;

    }
  } else if (extcmd[0] != 0 && extcmd[0] != 0xf6) {
    extcmd[1] = 0; //no device
  }
  extidx = extidx < szExt - 2 ? extidx : extidx - 2;
  for (uint8_t i = 0; i < extidx; i++) extcmd[2 + i] = extbuf[i]; //populate  buffer 0=cmd, 1=device, rest is tx data
  newExtCmd = true;
  return 1;

}
#endif
#ifdef MONITORTX
bool Vista::getExtBytes() {
  uint8_t x;
  bool ret = 0;

  if (!validMonitorPin) return 0;

  while (vistaSerialMonitor -> available()) {
    x = vistaSerialMonitor -> read();

    if (extidx < szExt)
      extbuf[extidx++] = x;
    markPulse = 0; //reset pulse flag to wait for next inter msg gap
  }

  if (extidx > 0 && markPulse > 1) {
    //ok, we are on the next pulse (gap) , lets decode the previous msg data
    if (decodePacket())
      ret = 1;
    extidx = 0;
    memset(extbuf, 0, szExt); //clear buffer mem    

  }

  return ret;
}
#endif

bool Vista::handle() {
  uint8_t x;

  #ifdef MONITORTX
  if (getExtBytes()) return 1;
  #endif


  if (is2400)
    vistaSerial -> setBaud(2400);
  else
    vistaSerial -> setBaud(4800);

  if (vistaSerial -> available()) {

    x = vistaSerial -> read();

    //we need to skips initial zero's here since the RX line going back high after a command, can create a bogus character
    memset(cbuf, 0, szCbuf); //clear buffer mem  
    
    if (markPulse==1) return 0;
    markPulse=1;
    if (expectByte != 0 && x) {
      if (x != expectByte) {
        expectByte = 0;
        retries = 0; 
        expectByte = 0;        
      } else {
        retries = 0;
        expectByte = 0;
        cbuf[0] = 0x77; //for testing only
        cbuf[1] = x;
        return 1;    // 1 for logging. 0 for normal
      }
    }
    
    //expander request command
    if (x == 0xFA) {
      vistaSerial -> setBaud(4800);
      gidx = 0;
      cbuf[gidx++] = x;
      readChar(cbuf, & gidx); //01 ?
      readChar(cbuf, & gidx); //dev id or len code if &1
      readChar(cbuf, & gidx); // seq
      readChar(cbuf, & gidx); // type 
      if ((cbuf[2] & 1)) { // byte 2 = 01 if extended addressing for relay boards 14,15 so packet longer by 1 byte 
        readChar(cbuf, & gidx); // extra byte
      } else if (cbuf[4] == 0x00 || cbuf[4] == 0x0D) { // 00 cmds use an extra byte
        readChar(cbuf, & gidx); //cmd
      }
      readChar(cbuf, & gidx); //chksum
      if (!validChksum(cbuf, 0, gidx))
        cbuf[12] = 0x77;
      else
        onExp(cbuf);
      newCmd = true;
      #ifdef MONITORTX
      memset(extcmd, 0, szExt); //store the previous panel sent data in extcmd buffer for later use
      memcpy(extcmd, cbuf, 7);
      #endif
      return 1;
    }

    if (x == 0xF7) {
      vistaSerial -> setBaud(4800);
      gidx = 0;
      cbuf[gidx++] = x;
      readChars(F7_MESSAGE_LENGTH - 1, cbuf, & gidx, F7_MESSAGE_LENGTH - 1);
      if (!validChksum(cbuf, 0, gidx))
        cbuf[12] = 0x77;
      else {
        //rxState = sAckf7;
        onDisplay(cbuf, & gidx);
        newCmd = true; //new valid cmd, process it
      }
      return 1; // return 1 to log packet        
    }

    //Long Range Radio (LRR)
    if (x == 0xF9) {
      vistaSerial -> setBaud(4800);
      gidx = 0;
      cbuf[gidx++] = x;
      //read cycle
      readChar(cbuf, & gidx);
      //read len
      readChar(cbuf, & gidx);
      readChars(cbuf[2], cbuf, & gidx, 30);
      if (!validChksum(cbuf, 0, gidx))
        cbuf[12] = 0x77;
      else
        onLrr(cbuf, & gidx);
      newCmd = true;
      #ifdef MONITORTX
      memset(extcmd, 0, szExt); //store the previous panel sent data in extcmd buffer for later use
      memcpy(extcmd, cbuf, 6);
      #endif
      return 1;
    }
    //key ack
    if (x == 0xF6) {
      vistaSerial -> setBaud(4800);
      newCmd = true;
      gidx = 0;
      cbuf[gidx++] = x;
      readChar(cbuf, & gidx);
      if (cbuf[1] == ackAddr) {
        writeChars();
      }
      #ifdef MONITORTX
      memset(extcmd, 0, szExt); //store the previous panel sent data in extcmd buffer for later use
      memcpy(extcmd, cbuf, 7);

      #endif
      return 1;
    }

    //secondary statuses
    if (x == 0xF2) {
      vistaSerial -> setBaud(4800);
      gidx = 0;
      cbuf[gidx++] = x;
      readChar(cbuf, & gidx);
      readChars(cbuf[1], cbuf, & gidx, 30);
      if (!validChksum(cbuf, 0, gidx))
        cbuf[12] = 0x77;
      else
        onStatus(cbuf, & gidx);
      newCmd = true;
      return 1;
    }

    //unknown
    if (x == 0xF8) {
      vistaSerial -> setBaud(4800);
      newCmd = true;
      gidx = 0;
      cbuf[gidx++] = x;
      readChars(F8_MESSAGE_LENGTH - 1, cbuf, & gidx, F8_MESSAGE_LENGTH - 1);
      if (!validChksum(cbuf, 0, gidx))
        cbuf[12] = 0x77;
      return 1;
    }
    //unknown
    if (x == 0xF0) {
      vistaSerial -> setBaud(4800);
      newCmd = true;
      gidx = 0;
      cbuf[gidx++] = x;
      readChar(cbuf, & gidx);
      #ifdef MONITORTX
      memset(extcmd, 0, szExt); //store the previous panel sent data in extcmd buffer for later use
      memcpy(extcmd, cbuf, 2);
      #endif      
      return 1;
    }    

    //RF supervision
    if (x == 0xFB) {
      vistaSerial -> setBaud(4800);
      newCmd = true;
      gidx = 0;
      cbuf[gidx++] = x;
      readChars(4, cbuf, & gidx, 4);
      if (!validChksum(cbuf, 0, gidx))
        cbuf[12] = 0x77;
      #ifdef MONITORTX
      memset(extcmd, 0, szExt); //store the previous panel sent data in extcmd buffer for later use
      memcpy(extcmd, cbuf, 6);
      #endif
      return 1;
    }

   //capture any unknown cmd byte if exits
      cbuf[0]=x;
      cbuf[12]=0x90;//possible ack byte or new unknown cmd
      newCmd=false;
     // return 1;
  }

  return 0;
}

//i've included these for debugging code only to stop processing during a hw lockup. do not use in production
void Vista::hw_wdt_disable() {
  #ifndef ESP32
    *
    ((volatile uint32_t * ) 0x60000900) &= ~(1); // Hardware WDT OFF
  #endif
}

void Vista::hw_wdt_enable() {
  #ifndef ESP32
    *
    ((volatile uint32_t * ) 0x60000900) |= 1; // Hardware WDT ON
  #endif
}

void Vista::stop() {
  //hw_wdt_enable(); //debugging only
  detachInterrupt(rxPin);
  #ifdef MONITORTX
  if (validMonitorPin) {
    detachInterrupt(monitorPin);
  }
  #endif
  keybusConnected = false;
}

void Vista::begin(int receivePin, int transmitPin, char keypadAddr, int monitorTxPin) {
  #ifndef ESP32
  //hw_wdt_disable(); //debugging only
  //ESP.wdtDisable(); //debugging only
  #endif
  expectByte = 0;
  retries = 0;
  is2400 = false;

  kpAddr = keypadAddr;
  txPin = transmitPin;
  rxPin = receivePin;
  monitorPin = monitorTxPin;
  validMonitorPin = false;

  //panel data rx interrupt - yellow line
  if (vistaSerial -> isValidGPIOpin(rxPin)) {
    vistaSerial = new SoftwareSerial(rxPin, txPin, true, 50);
    vistaSerial -> begin(4800, SWSERIAL_8E2);
    attachInterrupt(digitalPinToInterrupt(rxPin), rxISRHandler, CHANGE);
    vistaSerial -> processSingle = true;
  }
  #ifdef MONITORTX
  if (vistaSerialMonitor -> isValidGPIOpin(monitorPin)) {
    validMonitorPin = true;
    vistaSerialMonitor = new SoftwareSerial(monitorPin, -1, true, 50);
    vistaSerialMonitor -> begin(4800, SWSERIAL_8E2);
    //interrupt for capturing keypad/module data on green transmit line
    attachInterrupt(digitalPinToInterrupt(monitorPin), txISRHandler, CHANGE);
    vistaSerialMonitor -> processSingle = true;
  }
  #endif
  keybusConnected = true;

}