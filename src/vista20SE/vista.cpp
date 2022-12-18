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

    szOutbuf = 20;
    szCbuf = 50;
    inbufIdx = 0;
    outbufIdx = 0;
    rxState = sSyncInit;
    pointerToVistaClass = this;
    cbuf = (char * ) malloc(szCbuf);
    outbuf = (char * ) malloc(szOutbuf);
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
    int t=0;
    unsigned long timeout=millis();

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
    unsigned long timeout=millis();
    while (x < ct && millis() - timeout < 100) {
        if (vistaSerial -> available()) {
            timeout=millis();
            c = vistaSerial -> read();
            buf[idxval++] = c;
            x++;
        }
    }
    * idx = idxval;
    return 0;
}

void Vista::clearExpect() {
    expectByte = 0;
    expectCallbackComplete = NULL;
    expectCallbackError = NULL;

}

bool Vista::onResponseError(char data) {
    if (expectCallbackError == NULL) {
        return false;
    }
    expectCallbackError(data);
    return true;
}
/**
 * return false if no complete callback was specified
 * true otherwise
 */
bool Vista::onResponseComplete(char data) {

    if (expectCallbackComplete == NULL) {
        return false;
    }
    expectCallbackComplete(data);

    return true;
}

void Vista::keyAckComplete(char data) {

   // retries = 0;

}

void Vista::processStatus(char cbuf[], int * idx) {


    for (int x = 0; x <= 10; x++) {
        switch (x) {

        case 0:
            statusFlags.zone = (uint8_t) toDec(cbuf[x]);
            break;
        case 1:
            statusFlags.beeps = cbuf[x] & BIT_MASK_BYTE1_BEEP;
            statusFlags.night = ((cbuf[x] & BIT_MASK_BYTE1_NIGHT) > 0);

            break;
        case 2:
            statusFlags.fire = ((cbuf[x] & BIT_MASK_BYTE2_FIRE) > 0);
            statusFlags.systemFlag = ((cbuf[x] & BIT_MASK_BYTE2_SYSTEM_FLAG) > 0);
            statusFlags.ready = ((cbuf[x] & BIT_MASK_BYTE2_READY) > 0);
            if (statusFlags.systemFlag) {
                statusFlags.armedStay = ((cbuf[x] & BIT_MASK_BYTE2_ARMED_HOME) > 0);
                statusFlags.lowBattery = ((cbuf[x] & BIT_MASK_BYTE2_LOW_BAT) > 0);
                statusFlags.acLoss = ((cbuf[x] & BIT_MASK_BYTE2_AC_LOSS) > 0);
            } else {
                statusFlags.check = ((cbuf[x] & BIT_MASK_BYTE2_CHECK_FLAG) > 0);
                statusFlags.fireZone = ((cbuf[x] & BIT_MASK_BYTE2_ALARM_ZONE) > 0);
            }

            break;
        case 3:
            statusFlags.inAlarm = ((cbuf[x] & BIT_MASK_BYTE3_IN_ALARM) > 0);
            statusFlags.acPower = ((cbuf[x] & BIT_MASK_BYTE3_AC_POWER) > 0);
            statusFlags.chime = ((cbuf[x] & BIT_MASK_BYTE3_CHIME_MODE) > 0);
            statusFlags.bypass = ((cbuf[x] & BIT_MASK_BYTE3_BYPASS) > 0);
            statusFlags.programMode = (cbuf[x] & BIT_MASK_BYTE3_PROGRAM);
            if (statusFlags.systemFlag) {
                statusFlags.instant = ((cbuf[x] & BIT_MASK_BYTE3_INSTANT) > 0);
                statusFlags.armedAway = ((cbuf[x] & BIT_MASK_BYTE3_ARMED_AWAY) > 0);
            } else {
                statusFlags.alarm = ((cbuf[x] & BIT_MASK_BYTE3_SYSTEM_ALARM) > 0);
            }

            break;

        }
    }

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
    int lcbuflen = 0;

    //0x52 means respond with only cycle message
    //0x48 means same thing
    //, i think 0x42 and and 0x58 are the same
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
    } else { //0x53

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
    if (lrrSupervisor) {
        
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


        delayMicroseconds(500);
        for (int x = 0; x < lcbuflen; x++) {
            vistaSerial -> write(lcbuf[x]);
        }
    }
    sending = false;
}

void Vista::setExpFault(int zone, bool fault) {
    //expander address 7 - zones: 9 - 16
    //expander address 8 - zones:  17 - 24
    //expander address 9 - zones: 25 - 323
    //expander address 10 - zones: 33 - 40
    //expander address 11 - zones: 41 - 48
    uint8_t idx = 0;
    expansionAddr = 0;
    for (uint8_t i = 0; i < MAX_MODULES; i++) {
        switch (zoneExpanders[i].expansionAddr) {
        case 1:
            if (zone > 9 && zone < 18) {
                idx = i;
                expansionAddr = zoneExpanders[i].expansionAddr;
            }
            break;            
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

    int z = zone % 9; //convert zone to range of 1 - 7,0 (last zone is 0)
    expFault = z << 5 | (fault ? 0x8 : 0); //0 = terminated(eol resistor), 0x08=open, 0x10 = closed (shorted)  - convert to bitfield for F1 response
    if (z > 0) z--;
    else z = 7; //now convert to 0 - 7 for F7 poll response
    expFaultBits = (fault ? expFaultBits | (0x80 >> z) : expFaultBits & ((0x80 >> z) ^ 0xFF)); //setup bit fields for return response with fault values for each zone
    zoneExpanders[idx].expFault = expFault;
    zoneExpanders[idx].expFaultBits = expFaultBits;
    setNextFault(idx); //push to the pending queue
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

    char type = cbuf[3];
    char seq = cbuf[2];
    char lcbuf[4];

   expansionAddr = zoneExpanders[0].expansionAddr;
   if (cbuf[1] != 0x04 || expansionAddr!=1) {
        return; //no match return
   }

    sending = true;
    expFaultBits = zoneExpanders[idx].expFaultBits; 

    int lcbuflen = 0;
    expSeq = (seq == 0x020 ? 0x34 : 0x31);

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
       // lcbuf[2] = (char) currentFault.relayState;
        lcbuf[2] = 0;     
        lcbuf[3] = (char) currentFault.expFault; // we send out the current zone state 

    } else if ( type == 0xF7  ) { // periodic  zone state poll (every 30 seconds) expander
        lcbuflen = (char) 4;
        lcbuf[0] = (char) 0xF0;
        lcbuf[1] = (char) expSeq;
        //lcbuf[2]= (char) expFaultBits ^ 0xFF; //closed zones - opposite of expfaultbits. If set in byte3 we clear here. (not used )
        lcbuf[2] = 0; // we simulate having a termination resistor so set to zero for all zones
        lcbuf[3] = (char) expFaultBits; //opens zones - we send out the list of zone states. if 0 in both fields, means terminated 

    } else if (type == 0x00 || type == 0x0D || type == 0X0A) { // relay module
        lcbuflen = (char) 4;
        lcbuf[0] = (char) expansionAddr;
        lcbuf[1] = (char) expSeq;
        lcbuf[2] = (char) 0x00;
        zoneExpanders[idx].relayState = cbuf[4] & 0x80 ? zoneExpanders[idx].relayState | (cbuf[4] & 0x7f) : zoneExpanders[idx].relayState & ((cbuf[4] & 0x7f) ^ 0xFF);
        lcbuf[3] = (char) cbuf[4];

    } else {
        sending = false;
        return; //we don't acknowledge if we don't know  //0x80 or 0x81 
    }

    delayMicroseconds(500);
    uint32_t chksum = 0;
    for (int x = 0; x < lcbuflen; x++) {
        chksum += lcbuf[x];
        vistaSerial -> write(lcbuf[x]);
    }
    chksum -= 1;
    chksum = chksum ^ 0xFF;

    vistaSerial -> write((char) chksum);
    
    sending = false;
}

void Vista::write(const char key) {

    if ((key >= 0x30 && key <= 0x39) || key == 0x23 || key == 0x2a || (key >= 0x41 && key <= 0x44))
        outQueue(key);
}

void Vista::write(const char * receivedKeys) {
    char key1 = receivedKeys[1];
    char key2 = receivedKeys[2];

    int x = 0;
    while (receivedKeys[x] != '\0') {
        if ((receivedKeys[x] >= 0x30 && receivedKeys[x] <= 0x39) || receivedKeys[x] == 0x23 || receivedKeys[x] == 0x2a || (receivedKeys[x] >= 0x41 && receivedKeys[x] <= 0x44)) {
            outQueue(receivedKeys[x]);
        }
        x++;
    }
}

void Vista::outQueue(char byt) {
    outbuf[inbufIdx] = byt;
    inbufIdx = (inbufIdx + 1) % szOutbuf;
}

char Vista::getChar() {
    if (outbufIdx == inbufIdx) return 5;
    char c = outbuf[outbufIdx];
    outbufIdx = (outbufIdx + 1) % szOutbuf;
    return c;
}

bool Vista::sendPending() {
    if (outbufIdx == inbufIdx)
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


    if (!charAvail() ) return;

    int tmpIdx = 0;


    //adjust characters to hex values.
    //ASCII numbers get translated to hex numbers
    //# and * get converted to 0xA and 0xB
    // send any other chars straight, although this will probably 
    // result in errors
    //0xc = A (*/1), 0xd=B (*/#) , 0xe=C (3/#)
    int checksum = 0;
    int sz = 0;
        tmpOutBuf[tmpIdx++] = 0; //place holder for size
        char c;
        int key;

            c = getChar();
            if (c >= 0x30 && c <= 0x39) {
                c -= 0x30;
            } else
                //translate * to 0x0a
                if (c == 0x23) {
                    c = 0x0B;
                } else
                    //translate # to 0x0b
                    if (c == 0x2A) {
                        c = 0x0A;
                    } else
                        //translate A to 0x1C (function key A)
                        //translate B to 0x1D (function key B)
                        //translate C to 0x1E (function key C)
                        //translate D to 0x1F (function key D)
                        if (c >= 0x41 && c <= 0x44) {
                            c = c - 0x25;
                        }            
             if (c < 0x0C) {
               // key=keyDefs[(uint8_t)c]; 
            
            
            
                int t=4 - (millis() - lowTime);
                if (t> 0) delay(t);

  
                  vistaSerial -> setConfig(2400, SWSERIAL_5N1);
                  vistaSerial -> write(c);
                  vistaSerial -> write(c);
                  vistaSerial -> write(c);
                  vistaSerial -> setConfig(2400, SWSERIAL_8E2);
             }
            okToSend=false;
}



void ICACHE_RAM_ATTR Vista::rxHandleISR() {
    static byte b;
   
    if (digitalRead(rxPin)) {
        highTime=millis();
        lowTime=millis() - lowTime;
        
        if ((rxState==sCmdData  || rxState==sCmdDataHigh)&& lowTime > 10) {
                //shortSync=true;
                rxState=sSyncHigh;
                syncTime=millis();                
                return;
          }
        
        if (rxState==sSyncInit) {

            if (lowTime > 9) {
                rxState=sSyncHigh;
                syncTime=millis();  
                return;                
            }  else if (lowTime > 5) {
                    rxState=sCmdHigh;
                    is2400=true;
                    return;                     
            } else {
                    rxState=sCmdDataHigh;
                    is2400=false;
            }

        }                

        
        if (rxState==sSyncLow) {

            if (lowTime > 9) {
                syncTime=millis();
                rxState=sSyncHigh;
                expanderType currentFault = peekNextFault();
                if (currentFault.expansionAddr) {
                    ackAddr = currentFault.expansionAddr; // use the expander address 01 as the requestor
                    vistaSerial -> write(addrToBitmask1(ackAddr), false,4800); //send byte 1 address mask
                }
                return;
            } else if (lowTime > 5) {
                rxState=sCmdHigh;
                shortSync=false;
                is2400=true;
                return;  
            } else {
                shortSync=false;
                rxState=sCmdDataHigh;
                is2400=false;
            }
          
        }
        if (rxState==sCmdLow) {

            if ( lowTime < 10)
                if (lowTime > 5) {
                   rxState=sCmdHigh;
                   is2400=true;
                   return;
                } else { //no cmdhigh pulse so we are getting a cmd at 4800 baud
                    rxState=sCmdDataHigh;
                    is2400=false;
                }
            else {
                    if (millis() - syncTime < 90)
                        shortSync=true;
                    syncTime=millis();
                    rxState=sSyncHigh; 
                    return;
                }
                
          
        }


    } else {
        lowTime = millis();
        highTime=millis() - highTime;
        
        if (rxState==sSyncInit) {
            if (highTime > 40 ) {
                rxState=sCmdLow;
            }
        }
      
      
        if ((rxState==sCmdData  || rxState==sCmdDataHigh) && highTime > 10) {
            if (millis() - syncTime < 175 ) {
                rxState=sCmdLow;
            } else {
                okToSend=true;
                rxState=sSyncLow;
                markPulse=1;
            }
            
        }


        if (rxState==sSyncHigh) {
            if (millis() - syncTime < 175 && !shortSync ) {
                rxState=sCmdLow; 
            } else {
                rxState=sSyncLow;
                okToSend=true;
                markPulse=1;                
            }

        }   
        if (rxState==sCmdHigh) {
            if ( highTime < 10) {
                rxState=sCmdData;
            }  else {
                rxState=sCmdLow;
            }                
            

        }
  

    }
    vistaSerial -> rxRead(vistaSerial);

    #ifndef ESP32
    //clear pending interrupts for this pin if any occur during transmission
        GPIO_REG_WRITE(GPIO_STATUS_W1TC_ADDRESS, 1 << rxPin);
    #endif
}



#ifdef MONITORTX
void ICACHE_RAM_ATTR Vista::txHandleISR() {
    if ((!sending || !filterOwnTx) && rxState!=sSyncLow && (millis() - syncTime > 10)  )
        vistaSerialMonitor -> rxRead(vistaSerialMonitor);

}
#endif

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
            extcmd[12]=0x71; //flag to identify chksum error
            newExtCmd=true;
            return 1; // for debugging return what was sent so we can see why the chcksum failed
        }

        char cmdtype = (extcmd[1] & 1) ? extcmd[4] : extcmd[3];

        if (extcmd[1] & 1) {
            for (uint8_t i = 0; i < 8; i++) {
                if ((extcmd[1] >> i) & 0x01) {
                    extcmd[1] = i + 13; //get device id
                    break;
                }
            }

        } else {
            if (extcmd[1] ==0x04) 
                extcmd[1]=1;
            else
            for (uint8_t i = 0; i < 8; i++) {
                if ((extcmd[1] >> i) & 0x01) {
                    extcmd[1] = i + 6; //get device id
                    break;
                }
            }
        }
        if (cmdtype == 0xF1) { // expander channel update
            uint8_t channel;
            if (extbuf[3]) { //if we have zone expander data
                channel = (extbuf[3] >> 5);
                if (!channel) channel = 9;
                channel =  9 + channel; //calculate zone
                extcmd[4] = (extbuf[3] >> 3 & 3) ? 1 : 0; //fault
            } else {
                channel = 0; //no zone data so set to zero
                extcmd[4] = 0;
            }
            extcmd[2] = cmdtype; //copy subcommand to byte 2
            extcmd[3] = channel;
            extcmd[5] = extbuf[2]; //relay data
            
            extcmd[6] = extbuf[0];
            extcmd[7] = extbuf[1];
            extcmd[8] = extbuf[2];
            extcmd[9] = extbuf[3];
            extcmd[10] = extbuf[4];
            extcmd[11] = extbuf[5];
            extcmd[12]= 0x44;

            newExtCmd = true;
            return 1;

        } else if (cmdtype == 0xF7) { // expander poll request
            extcmd[2] = cmdtype; //copy subcommand to byte 2
            extcmd[3] = 0;
            extcmd[4] = extbuf[3]; //zone faults
            newExtCmd = true;
            return 1;
        } else if (cmdtype == 0x00 || cmdtype == 0x0D) { //relay channel update
            extcmd[2] = cmdtype; //copy subcommand to byte 2
            uint8_t channel;
            switch (extbuf[3] & 0x7f) {
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
            
            extcmd[6] = extbuf[0];
            extcmd[7] = extbuf[1];
            extcmd[8] = extbuf[2];
            extcmd[9] = extbuf[3];
            extcmd[10] = extbuf[4];
            extcmd[11] = extbuf[5];
            extcmd[12]= 0x55;
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
            extcmd[12]=0x72; //flag to identify unknown subcommand
            newExtCmd=true;
            return 1; // for debugging return what was sent so we can see why the chcksum failed

        }
    } else if (extcmd[0] == 0xFB) {
        // Check how many bytes are in RF message (stored in upper nibble of Byte 2)
        uint8_t n_rf_bytes = extbuf[1] >> 4;

        if (n_rf_bytes == 5) { // For monitoring, we only care about 5 byte messages since that contains data about sensors
            // Verify data 
            uint16_t rf_checksum = 0;
            for (uint8_t i = 0; i < n_rf_bytes + 2; i++) {
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
                extcmd[12]=0x73; //flag to identify cheksum failed             
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
            extcmd[12]=0x74; //flag to identify unknown command
            newExtCmd = true;
            return 1;

        }
    } else if (extcmd[0] != 0 && extcmd[0] != 0xf6) {
        extcmd[1] = 0; //no device
    }
    for (uint8_t i = 0; i < szExt - 3; i++) extcmd[3 + i] = extbuf[i]; //populate  buffer 0=cmd, 1=device, rest is tx data
    newExtCmd = true;
    return 1;

}
#endif
#ifdef MONITORTX
bool Vista::getExtBytes() {
    uint8_t x;
    bool ret = 0;

    if (!validMonitorPin) return 0;
    while (vistaSerialMonitor -> available()  ) {
        x = vistaSerialMonitor -> read();
        if (extidx < szExt)
            extbuf[extidx++] = x;
        markPulse = 0; //reset pulse flag to wait for next inter msg gap
    }

    if (extidx > 0) {
        //ok, we are on the next pulse (gap) , lets decode the previous msg data
        if (decodePacket())
            ret = 1;
        extidx = 0;
        memset(extbuf, 0, szExt);
    }

    return ret;
}
#endif

bool Vista::handle() {
    uint8_t x;

    #ifdef MONITORTX
    if (getExtBytes()) return 1;
    #endif
    if (rxState==sSyncLow && (millis() - lowTime < 5 ) && okToSend && charAvail()) {
        writeChars();
    }   

    if (is2400)
        vistaSerial->setBaud(2400);
    else
        vistaSerial->setBaud(4800);
    

    
    if (vistaSerial -> available()) {

        x = vistaSerial->read();

        //we need to skips initial zero's here since the RX line going back high after a command, can create a bogus character

       if (!x && rxState != sCmdData)
                    return 0;


        memset(cbuf, 0, szCbuf); //clear buffer mem
       bool ret=0;
        if (expectByte != 0) {
            if (x != expectByte) {
                onResponseError(x);
                clearExpect();
            } else {
                cbuf[0]=0x77; //for testing only
                cbuf[1]=x;
                onResponseComplete(x);
                clearExpect();
                
                newCmd=true;                
                return 1;
            }
        }

        //expander request command
        if (x == 0xFA) {

            vistaSerial->setBaud(4800);
            gidx = 0;
            cbuf[gidx++] = x;

            readChar(cbuf, & gidx); //dev id 04
            readChar(cbuf, & gidx); // seq 20
            readChar(cbuf, & gidx); //type F7
            readChar(cbuf, & gidx); //chksum
            if (cbuf[3] == 0x00 || cbuf[3] == 0x0D || cbuf[3] == 0x0A) { // 00 cmds use an extra byte
                readChar(cbuf, & gidx); //cmd
            }            
            if (!validChksum(cbuf, 0, gidx))
                cbuf[12]=0x77;
            else
                onExp(cbuf);
            newCmd = true;            
            #ifdef MONITORTX
            memset(extcmd, 0, szExt); //store the previous panel sent data in extcmd buffer for later use
            memcpy(extcmd, cbuf, 6);
            vistaSerial->setBaud(2400 ); 
            #endif
         
            return 1;
        }


        if (x == 0xFE && !statusFlags.programMode) {
            vistaSerial->setBaud(2400);
            promptIdx=0;
            newCmd = false;
            gidx = 0;
            cbuf[gidx++] = x;
            readChars(4, cbuf, & gidx, 5);
            statusFlags.backlight = ((cbuf[1] & 0x80) > 0);
            cbuf[1] = (cbuf[1] & 0x7F);
            for (int x=1;x<5;x++) {
                tempPrompt[promptIdx++]=cbuf[x];
            }
            return 0;
        }
        
        if (x == 0xFF ) {
            vistaSerial->setBaud(2400);
            gidx = 0;
            cbuf[gidx++] = x;
            readChars(4, cbuf, & gidx, 5);
            for (int x=1;x<5;x++) {
                tempPrompt[promptIdx++]=cbuf[x];
            }
            if (promptIdx ==32) {
                int y=0;
                for (int x = 0;x < 32; x++) {
                    if ((uint8_t) tempPrompt[x] > 31 && (uint8_t)tempPrompt[x] < 127) {
                        statusFlags.prompt[y++] = tempPrompt[x];
                    } else   statusFlags.prompt[y++] = ' ';
                }
                statusFlags.prompt[y]='\0';
                promptIdx=0;
                cbuf[0]=0xFE;
                newCmd=true;
   
               ret = 1;
            }
            return ret;
        }
        
        if (x == 0xFE && statusFlags.programMode ) {
            vistaSerial->setBaud(4800);
            newCmd = true;
            gidx = 0;
            cbuf[gidx++] = x;
            readChars(32, cbuf, & gidx, 32);
            int y=0;
            for (int x = 1;x < 33; x++) {
                if ((uint8_t)cbuf[x] && (uint8_t)cbuf[x] < 127) {
                    statusFlags.prompt[y++] = cbuf[x];
                } else y++;
            }
            statusFlags.prompt[y]='\0';
            return 1;
        }

        //RF supervision
        if (x == 0xFB) {
            vistaSerial->setBaud(4800);
            newCmd = true;
            gidx = 0;
            cbuf[gidx++] = x;
            readChars(4, cbuf, & gidx, 8);
            if (!validChksum(cbuf, 0, gidx)) 
                cbuf[12]=0x77;
            #ifdef MONITORTX
            memset(extcmd, 0, szExt); //store the previous panel sent data in extcmd buffer for later use
            memcpy(extcmd, cbuf, 5);
            #endif
            return 1;
        }

        //unknown 
         if (x ==0xF8 ) { 
            vistaSerial->setBaud(4800);
            newCmd = true;
            gidx = 0;
            cbuf[gidx++] = x;
            readChars(7, cbuf, & gidx, 8);
            return 1;
        }
        
        //Long Range Radio (LRR)
        if (x == 0xF9) {
            vistaSerial->setBaud(4800);
            gidx = 0;
            cbuf[gidx++] = x;
            //read cycle
            readChar(cbuf, & gidx);
            //read len
            readChar(cbuf, & gidx);
            readChars(cbuf[2], cbuf, & gidx, 30);
            if (!validChksum(cbuf, 0, gidx))
                cbuf[12]=0x77;
            else
                onLrr(cbuf, & gidx);
            newCmd = true;            
            #ifdef MONITORTX
            memset(extcmd, 0, szExt); //store the previous panel sent data in extcmd buffer for later use
            memcpy(extcmd, cbuf, 6);
            #endif
            return 1;
        }
        
        //unknown
        if (x == 0xF0) {
            vistaSerial -> setBaud(4800);
            newCmd = true;
            gidx = 0;
            cbuf[gidx++] = x;
            readChar(cbuf, & gidx);
            return 1;
        }  
        
         if (x >=0 && x <0xf0 ) { //assume it's a status cmd
            vistaSerial->setBaud(2400);
            newCmd = true;
            gidx = 0;
            cbuf[gidx++] = x;
            readChars(3, cbuf, & gidx, 5);
            processStatus(cbuf, & gidx);
            return 1;
        }


    }

    return 0;
}

//i've included these for debugging code only to stop processing during a hw lockup. do not use in production
void Vista::hw_wdt_disable() {
    #ifndef ESP32    
    *((volatile uint32_t * ) 0x60000900) &= ~(1); // Hardware WDT OFF
    #endif
}

void Vista::hw_wdt_enable() {
    #ifndef ESP32    
    *((volatile uint32_t * ) 0x60000900) |= 1; // Hardware WDT ON
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

    is2400=true;
    expectByte = 0;
    
    kpAddr = keypadAddr;
    txPin = transmitPin;
    rxPin = receivePin;
    monitorPin = monitorTxPin;
    validMonitorPin=false;
    //panel data rx interrupt - yellow line
    if (vistaSerial -> isValidGPIOpin(rxPin)) {
        vistaSerial = new SoftwareSerial(rxPin, txPin, true, 50);
        vistaSerial -> begin(2400, SWSERIAL_8E2);
        attachInterrupt(digitalPinToInterrupt(rxPin), rxISRHandler, CHANGE);
        vistaSerial->processSingle=true;
    }
    #ifdef MONITORTX
    if (vistaSerialMonitor -> isValidGPIOpin(monitorPin)) {
        validMonitorPin=true;
        vistaSerialMonitor = new SoftwareSerial(monitorPin, -1, true, 50);
        vistaSerialMonitor -> begin(4800, SWSERIAL_8E2);
        //interrupt for capturing keypad/module data on green transmit line
        attachInterrupt(digitalPinToInterrupt(monitorPin), txISRHandler, CHANGE);
        vistaSerialMonitor->processSingle=true;
    }
    #endif
    keybusConnected = true;

}