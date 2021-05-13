#include "vista.h"
#include "Arduino.h"

Vista *pointerToVistaClass;

void ICACHE_RAM_ATTR rxISRHandler() { // define global handler
  pointerToVistaClass->rxHandleISR(); // calls class member handler
}

#ifdef MONITORTX
void ICACHE_RAM_ATTR txISRHandler() { // define global handler
  pointerToVistaClass->txHandleISR(); // calls class member handler
}
#endif

Vista::Vista(int receivePin, int transmitPin, char keypadAddr, Stream *stream,int monitorPin=-1)
  : rxPin(receivePin), txPin(transmitPin), kpAddr(keypadAddr), monitorTxPin(monitorPin), outStream(stream)
{
    
  vistaSerial = new SoftwareSerial(receivePin, transmitPin, true, 50);
  vistaSerial->begin(4800,SWSERIAL_8E2);
  
#ifdef MONITORTX
  vistaSerialMonitor = new SoftwareSerial(monitorPin,-1, true, 50);
  vistaSerialMonitor->begin(4800,SWSERIAL_8E2);
  szExt=20;
  extbuf = (char*) malloc(szExt);
  extcmd = (char*) malloc(szExt);
#endif  

  szOutbuf=20;
  szCbuf=50;
  inbufIdx=0;
  outbufIdx=0;
  rxState=sNormal;
  pointerToVistaClass=this;
  cbuf = (char*) malloc(szCbuf);
  outbuf = (char*) malloc(szOutbuf);
  szFaultQueue=10;
  faultQueue=(expanderType*) malloc(szFaultQueue);
  lrrSupervisor=false;

}


Vista::~Vista() {
  free(vistaSerial);
  detachInterrupt(rxPin);
  #ifdef MONITORTX
    free(vistaSerialMonitor);
    detachInterrupt(monitorTxPin);
  #endif
  pointerToVistaClass=NULL;
}
expanderType Vista::getNextFault() {
    expanderType currentFault;
	if (inFaultIdx == outFaultIdx) return currentFault;
    currentFault = faultQueue[outFaultIdx];
	outFaultIdx = (outFaultIdx + 1) % szFaultQueue;
	return currentFault;
}

expanderType ICACHE_RAM_ATTR Vista::peekNextFault() {
    expanderType currentFault;
	if (inFaultIdx == outFaultIdx) return currentFault;
	return faultQueue[outFaultIdx];
}

void Vista::setNextFault(expanderType rec) {
    
	int next = (inFaultIdx + 1) % szFaultQueue;
    faultQueue[inFaultIdx]=rec;
    inFaultIdx=next;
}

void Vista::readChar(char buf[], int *idx)
{
	char c;
	int idxval = *idx;
	while (!vistaSerial->available());
    c = vistaSerial->read();
	buf[ idxval++ ] = c;
	*idx = idxval;
}

uint8_t Vista::readChars(int ct, char buf[], int *idx, int limit)
{
	char c;
	int x=0;
	int idxval = *idx;
    if (ct > limit) return 1;
	while (x < ct ) {
       if (vistaSerial->available()) {
			c = vistaSerial->read();
            buf[ idxval++] =  c;
			x++;
		}
    }
   	*idx = idxval;
	return 0;
}

void Vista::clearExpect() {
	expectByte               = 0;
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

void Vista::onStatus(char cbuf[], int *idx) {

	//byte 2 is length of message
	//byte 3 is length of headers
	//last byte of headers is counter
	//remaining bytes are body

  	//F2 messages with 18 bytes or less don't seem to have
	// any important information
	if ( 18 >= (uint8_t) cbuf[1]) {
		return;
	}

	//19th spot seems to be a decimal value
	//01 is disarmed
	//02 is armed
	//03 is disarmed chime
	//short armed = 2 == (uint8_t) cbuf[19];
   statusFlags.armed=(0x02 & cbuf[19]) && !(cbuf[19] & 0x01);
	//20th spot is away / stay
	// this bit is really confusing
	// it clearly switches to 2 when you set away mode
	// but it is also 0x02 when an alarm is canceled,
	// but not cleared - even if you are in stay mode.
	//11th  byte in status body

  statusFlags.away=((0x02 & cbuf[20])>0);
    //21st spot is for bypass
  statusFlags.zoneBypass=((0x02 & cbuf[21]) > 0);
 	//21st spot is for bypass
	//12th byte in status_body
	//short bypass = 0x02 & status_body[12];

	//22nd spot is for alarm types
	//1 is no alarm
	//2 is ignore faults (like exit delay)
	//4 is a alarm
	//6 is a fault that does not cause an alarm
	//8 is for panic alarm.

	
  statusFlags.noAlarm=((cbuf[22] & 0x01) > 0);
  statusFlags.exitDelay=((cbuf[22] & 0x02)>0);
  statusFlags.fault=((cbuf[22] & 0x04)>0);
  statusFlags.panicAlarm=((cbuf[22] & 0x08)>0);
 
	if ( statusFlags.armed && statusFlags.fault && !statusFlags.exitDelay ) {
    statusFlags.alarm=1;
	} else if ( !statusFlags.armed && statusFlags.fault && !statusFlags.exitDelay) {
      statusFlags.alarm=0;
   	} 
   
}



void Vista::onDisplay(char cbuf[], int *idx) {
    // first 4 bytes are addresses of intended keypads to display this message
    // from left to right MSB to LSB
    // 5th byte represents  ??? (not the zone)
    // 6th binary encoded data including beeps
    // 7th binary encoded data including status armed mode
    // 8th binary encoded data including ac power and chime
    // 9th byte Programming mode = 0x01
    // 10th byte promt position in the display message of the expected input

 
    for (int x = 0; x <= 10 ; x++) {
        switch ( x ) {
            case 1: 
              // device addresses 0-7
              statusFlags.keypad[0]=cbuf[x];
              break;
            case 2:
                //device addresses 8 - 15
                 statusFlags.keypad[1]=cbuf[x];
                break;
            case 3:
                statusFlags.keypad[2]=cbuf[x];
              //keypads 16-23
              break;
            case 4:
             statusFlags.keypad[3]=cbuf[x];
            // keypads 24-31
              break;
             case 5:
              
                statusFlags.zone = (uint8_t) toDec(cbuf[x]);
               break;
            case 6:
                statusFlags.beeps = cbuf[x] & BIT_MASK_BYTE1_BEEP ;                
                statusFlags.night = ((cbuf[x] & BIT_MASK_BYTE1_NIGHT) > 0);

                break;
            case 7:
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
            case 8:
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
               /*
               if (!statusFlags.inAlarm && (statusFlags.fireZone || statusFlags.alarm) )
                        statusFlags.cancel=true;
                else
                   statusFlags.cancel=false;
               */
             
                break;
                  
           case 9:
             // statusFlags.programMode = (cbuf[x] == 0x01);
               break;
         
            case 10:
                statusFlags.promptPos=cbuf[x];
                break;
            }
	}
    int y=0;
    statusFlags.backlight=((cbuf[12] & 0x80) > 0);
     cbuf[12]=(cbuf[12] & 0x7F);
  	for (int x = 12; x < *idx-1; x++) {
		  if ((uint8_t)cbuf[x]  > 31 && (uint8_t)cbuf[x] < 127) {
                statusFlags.prompt[y++] = cbuf[x];
		  }
	  }
    statusFlags.prompt[y] = '\0';	//add string terminator

}

int Vista::toDec(int n){
   char b[4];
   char * p ;
   itoa(n,b,16);
   long int li=strtol(b, &p, 10) ;
   
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
void Vista::onLrr(char cbuf[], int *idx) {

	int len = cbuf[2];

	if (len == 0 ) return;
    sending=true;
	char type = cbuf[3];
    char lcbuf[12];
    delayMicroseconds(200);
	int  lcbuflen = 0;

    //0x52 means respond with only cycle message
	//0x48 means same thing
	//, i think 0x42 and and 0x58 are the same
	if (type == (char) 0x52 || type == (char) 0x48

	) {
		lcbuf[0] = (char) cbuf[1];
		lcbuflen++;
	} else if (type == (char) 0x58) {
		//just respond, but 0x58s have lots of info
        int c    = (((0x0f & cbuf[8]) << 8) | cbuf[9]);
        c=toDec(c); //convert to decimal representation for correct code display
		statusFlags.lrr.qual    = (uint8_t) (0xf0 & cbuf[8]) >> 4;
        statusFlags.lrr.code= c;
		statusFlags.lrr.zone   = (uint8_t) cbuf[12] >> 4;
		statusFlags.lrr.user   = (uint8_t) cbuf[12] >> 4;
        statusFlags.lrr.partition   = (uint8_t) cbuf[10];

		lcbuf[0] = (char)(cbuf[1]);
		lcbuflen++;
	} else {

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
        expectByte=lcbuf[0];
	}

	//we don't need a checksum for 1 byte messages (no length bit)
	//if we don't even have a message length byte, then we are just
	// ACKing a cycle header byte.
	if (lcbuflen >= 2) {
		uint32_t chksum = 0;
		for (int x=0; x<lcbuflen; x++) {
			chksum += lcbuf[x];
		}
		chksum -= 1;
		chksum = chksum ^ 0xFF;
		lcbuf[lcbuflen] = (char) chksum;
		lcbuflen++;
	}

	if (lrrSupervisor) {
        for (int x=0; x<lcbuflen; x++) {
            vistaSerial->write(lcbuf[x]);
        }
	}
    sending=false;
}

void Vista::setExpFault(int zone,bool fault) {
    //expander address 7 - zones: 9 - 16
    //expander address 8 - zones:  17 - 24
    //expander address 9 - zones: 25 - 32
    //expander address 10 - zones: 33 - 40
    //expander address 11 - zones: 41 - 48
    int idx=0;
    expansionAddr=0;
    for (int i=0;i<MAX_MODULES;i++) {
        switch (zoneExpanders[i].expansionAddr) {
            case 7: if (zone > 8  && zone < 17) {idx=i; expansionAddr=zoneExpanders[i].expansionAddr;}
                break;
            case 8: if (zone > 16 && zone < 25) {idx=i; expansionAddr=zoneExpanders[i].expansionAddr;}
                break;       
            case 9: if (zone > 24 &&  zone < 33) {idx=i; expansionAddr=zoneExpanders[i].expansionAddr;}
                break;        
    
            case 10: if (zone > 32 && zone < 41) {idx=i; expansionAddr=zoneExpanders[i].expansionAddr;}
                break;
                
            case 11: if (zone > 40 && zone < 49) {idx=i; expansionAddr=zoneExpanders[i].expansionAddr;}
                break;
            default: break;
        } 
        if (expansionAddr) break;
    }
    if (!expansionAddr) return;
    expFaultBits=zoneExpanders[idx].expFaultBits;
    
    int z = zone % 8;  //convert zone to range of 1 - 7,0 (last zone is 0)
    expFault=z << 5 | (fault?0x8:0); //0 = terminated(eol resistor), 0x08=open, 0x10 = closed (shorted)  - convert to bitfield for F1 response
    if (z>0) z--; else z=7; //now convert to 0 - 7 for F7 poll response
    expFaultBits=  (fault?expFaultBits | (0x80 >> z):expFaultBits & ((0x80 >> z)^0xFF)); //setup bit fields for return response with fault values for each zone
    zoneExpanders[idx].expFault=expFault;
    zoneExpanders[idx].expFaultBits=expFaultBits;
    setNextFault(zoneExpanders[idx]); //push to the pending queue
}

//98 2E 02 20 F7 EC
//98 2E 04 20 F7 EA
//98 2E 01 02 20 F1 F1 - relay address 14 and 15 have an extra byte . byte 1 is flag and shifts other bytes right
// byte 3 is the binary position encoded expander addresss 02=107, 04=108,08=109, etc
//byte 4 is a seq byte, changes from 20 to 25 every request sequence
//byte 5 F7 for poll, F1 for a msg request, 80 for a resend request
// The 98 serial data has some inconsistencies with some bit durations. Might be an issue with my panel.  Be interested to know how it looks on another panel.  The checksum does not work as a two's complement so something is off. Either way, the bytes we need work fine for our purposes.

void Vista::onExp(char cbuf[]) {

     char type=cbuf[4];
     char seq=cbuf[3];
     char lcbuf[4];
   sending=true;
   delayMicroseconds(200);
    int idx;
    

    if (cbuf[2] & 1) {
         seq=cbuf[4];
         type=cbuf[5];
         for (idx=0;idx<MAX_MODULES;idx++) {
            expansionAddr=zoneExpanders[idx].expansionAddr;
            if (cbuf[3]== (0x01 << (expansionAddr-13))) break; //for us - relay addresses 14-15
        }  
        
    } else {
        for (idx=0;idx<MAX_MODULES;idx++) {
            expansionAddr=zoneExpanders[idx].expansionAddr;
            if (cbuf[2]== (0x01 << (expansionAddr-6))) break; //for us - address range 7 -13
        }
    }
    if (idx==MAX_MODULES)
    {
        sending=false;
        return; //no match return
    }
    expFaultBits=zoneExpanders[idx].expFaultBits;
    
   
	int  lcbuflen = 0;
    expSeq=(seq == 0x20?0x34:0x31);
    
   // we use zone to either | or & bits depending if in fault or reset
    //0xF1 - response to request, 0xf7 - poll, 0x80 - retry ,0x00 relay control
     if (type ==  0xF1   )
	 {
        expanderType currentFault = peekNextFault();  //check next item. Don't pop it yet
        if (currentFault.expansionAddr) {
            if ( expansionAddr != currentFault.expansionAddr) {
                sending=false;
                return; //no match to the pending fault data so not for us. Sanity check.
            }
           getNextFault(); //pop it from the queue since we are processing it now
        } else
            currentFault=zoneExpanders[idx]; //no pending fault, use current fault data instead
        lcbuflen = (char) 4;
        lcbuf[0] = (char) currentFault.expansionAddr;
        lcbuf[1]= (char) expSeq;
        lcbuf[2]= (char) currentFault.relayState;
        lcbuf[3] = (char) currentFault.expFault;  // we send out the current zone state 
  		
       
    } else if (type ==   0xF7) {  // periodic  zone state poll (every 30 seconds) expander
        lcbuflen = (char) 4; 
		lcbuf[0] = (char) 0xF0;
        lcbuf[1]= (char) expSeq;
        //lcbuf[2]= (char) expFaultBits ^ 0xFF; //closed zones - opposite of expfaultbits. If set in byte3 we clear here. (not used )
        lcbuf[2]= 0; // we simulate having a termination resistor so set to zero for all zones
        lcbuf[3] = (char) expFaultBits; //opens zones - we send out the list of zone states. if 0 in both fields, means terminated 
  		
	} else if (type == 0x00) { // relay module
    	lcbuflen = (char) 4;
        lcbuf[0] = (char) expansionAddr;
        lcbuf[1]= (char) expSeq;
        lcbuf[2]= (char) 0x00;
      if (cbuf[2] & 1) { //address 14/15
        zoneExpanders[idx].relayState =  cbuf[6]&0x80?zoneExpanders[idx].relayState | (cbuf[6]&0x7f):zoneExpanders[idx].relayState & ((cbuf[6]&0x7f)^0xFF);
        lcbuf[3] = (char) cbuf[6] ; 
      } else {
        zoneExpanders[idx].relayState =  cbuf[5]&0x80?zoneExpanders[idx].relayState | (cbuf[5]&0x7f):zoneExpanders[idx].relayState & ((cbuf[5]&0x7f)^0xFF);
        lcbuf[3] = (char) cbuf[5] ; 
      }
    } else {
        sending=false;
        return; //we don't acknowledge if we don't know  //0x80 or 0x81 or 0x00
    }
    
    
    uint32_t chksum = 0;
	for (int x=0; x<lcbuflen; x++) {
		chksum += lcbuf[x];
        vistaSerial->write(lcbuf[x]);
	}
	chksum -= 1;
	chksum = chksum ^ 0xFF;
    vistaSerial->write((char) chksum);
    sending=false;
}

void Vista::write(const char key) {

   if ((key >= 0x30 && key <=0x39) || key == 0x23 || key == 0x2a || (key >= 0x41 && key <=0x44))
       outQueue(key);
}

void Vista::write(const char *receivedKeys) {
  char key1=receivedKeys[1];
  char key2=receivedKeys[2];
  
  int x=0;
  while (receivedKeys[x] != '\0') {
    if ((receivedKeys[x] >= 0x30 && receivedKeys[x] <=0x39) || receivedKeys[x] == 0x23 || receivedKeys[x] == 0x2a || (receivedKeys[x] >= 0x41 && receivedKeys[x] <=0x44)){
       outQueue(receivedKeys[x]);
      }
  x++;
  }
}

void Vista::outQueue(char byt ) {
   outbuf[inbufIdx]=byt;
   inbufIdx=(inbufIdx + 1) % szOutbuf;
}

char Vista::getChar() {
   if (outbufIdx==inbufIdx) return 5;
   char c=outbuf[outbufIdx];
   outbufIdx = (outbufIdx + 1) % szOutbuf;
   return c;
}

bool Vista::sendPending() {
     if (outbufIdx == inbufIdx && retries == 0  )
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
 
 

void Vista::writeChars(){

 // if (outbufIdx == 0) {return ;}
    if (!charAvail() && retries==0) return;

  uint8_t header = ((++writeSeq  << 6) & 0xc0) | (kpAddr & 0x3F);
  
   //if retries are getting out of control with no successfull callback
  //just clear the buffer
  if (retries > 5) {
    retries = 0;
    outbufIdx=inbufIdx;
    return;
  } 
   sending=true;
   delayMicroseconds(200);

   int tmpIdx=0;
  //header is the bit mask YYXX-XXXX
  //  where YY is an incrementing sequence number
  //  and XX-XXXX is the keypad address (decimal 16-31)
  //vistaSerial->write(header);
  //vistaSerial->write(outbufIdx +1); 
    tmpOutBuf[tmpIdx++]=header;

  
   //adjust characters to hex values.
  //ASCII numbers get translated to hex numbers
  //# and * get converted to 0xA and 0xB
  // send any other chars straight, although this will probably 
  // result in errors
  //0xc = A (*/1), 0xd=B (*/#) , 0xe=C (3/#)
  int checksum = 0;
  int sz=0;
  //for(int x =0; x < outbufIdx; x++) {
  if (retries==0) {
   tmpOutBuf[tmpIdx++]=0; //place holder for size
   char c;
   while (charAvail()) {
    c=getChar();
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
    //translate A to 0x1C (function key A)
    //translate B to 0x1D (function key B)
    //translate C to 0x1E (function key C)
    //translate D to 0x1F (function key D)
    if (c >= 0x41 && c <= 0x44) {
      c = c - 0x25;
    }
   // checksum += c;
    //vistaSerial->write(outbuf[x]);
     tmpOutBuf[tmpIdx++]=c;
  }
   //int chksum = 0x100 - (header + checksum + (uint8_t)sz+1);
   //vistaSerial->write((char) chksum); 
    tmpOutBuf[1]=sz+1;
  }
     vistaSerial->write(tmpOutBuf[0]); 
     vistaSerial->write(tmpOutBuf[1]); 
     for (int x=2;x<tmpOutBuf[1]+1;x++) {
         checksum += (char) tmpOutBuf[x];
        vistaSerial->write(tmpOutBuf[x]); 
         
     }
    uint32_t chksum = 0x100 - (tmpOutBuf[0] + tmpOutBuf[1] + checksum );   
    vistaSerial->write((char) chksum); 
   expectByte=tmpOutBuf[0];
   setOnResponseCompleteCallback(std::bind(&Vista::keyAckComplete,this,(char) header)); 
   retries++;
   sending=false;

}

void Vista::keyAckComplete(char data) {

	retries = 0;
    

}


void ICACHE_RAM_ATTR Vista::rxHandleISR() {

  if (digitalRead(rxPin)) {
//addressing format for request to send
//Panel pulse 1.  Addresses 1-7, 
//Panel pulse 2. Addresses 8-15
//Panel pulse 3. Addresses 16-23
    switch (rxState) {
        case sPulse:
           b=addrToBitmask2(ackAddr); // send byte 2 encoded addresses 
          if (b) vistaSerial->write(b,false); //write byte - no parity bit
          rxState=sSendkpaddr; //set flag to send 3rd address byte
          break;
         case sSendkpaddr:
           b=addrToBitmask3(ackAddr); //send byte 3 encoded addresses 
           if (b) vistaSerial->write( b,false);  //only send if needed - no parity bit
           rxState=sPolling;  //we set to polling to ignore pulses until the next 4ms gap
           break;
        case sAckf7:
           ackAddr=kpAddr; //F7 acks are for keypads
           vistaSerial->write( addrToBitmask1(ackAddr),false); // send byte 1 encoded addresses
           rxState=sPulse;
          break;
          case sPolling:
          case sNormal: 
           if (lowTime && (millis() - lowTime) > 9) {
             markPulse=2;
             currentFault=peekNextFault();
             if (currentFault.expansionAddr) {
                 ackAddr=currentFault.expansionAddr; // use the expander address 07/08/09/10/11 as the requestor
                 vistaSerial->write(addrToBitmask1(ackAddr),false); //send byte 1 address mask
                 rxState=sPulse; //set flag to send address byte2 if needed
             } else if (outbufIdx != inbufIdx || retries > 0) {
                 ackAddr=kpAddr;  //use the keypad address as the requestor
                 vistaSerial->write( addrToBitmask1(ackAddr),false);
                 rxState=sPulse; //set flag to send our request to send pulses
             } else {
                rxState=sPolling; // set flag to skip capturing pulses in the receive buffer during polling phase
             } 
           } else  if (lowTime && (millis() - lowTime) > 3) {
                 markPulse=1;
                 rxState=sNormal; // ok we have the inter message gap. Lets start capturing receive bytes 
           }
         
           break;
        default:
            break; 
      }  
      lowTime=0;

  } else {
     lowTime=millis();
     
  }
  if (rxState==sNormal)   
      vistaSerial->rxRead(vistaSerial);
#ifndef ESP32  
  else //clear pending interrupts for this pin if any occur during transmission
      GPIO_REG_WRITE(GPIO_STATUS_W1TC_ADDRESS, 1 << rxPin);
#endif      
}


bool Vista::validChksum(char cbuf[],int start, int len) {
	uint16_t chksum = 0;
    for (uint8_t x=start; x<=len-1; x++) {
		chksum += cbuf[x];
	}
    if (chksum % 256 == 0) 
        return true;
    else
        return false;
    
}


#ifdef MONITORTX
void ICACHE_RAM_ATTR Vista::txHandleISR() {
   if ((!sending || !filterOwnTx) && rxState==sNormal)
        vistaSerialMonitor->rxRead(vistaSerialMonitor);
#ifndef ESP32    
    else    
        GPIO_REG_WRITE(GPIO_STATUS_W1TC_ADDRESS, 1 << rxPin);
#endif    
}
#endif

#ifdef MONITORTX
void Vista::decodePacket() {
     //here we re-use extcmd buffer for formating data for easier decoding
     //format 0x98 deviceid subcommand channel on/off 
      if (extcmd[0]==0x98 ) {
      if (!validChksum(extbuf,0,5)) return; //make sure we have good cmd98 response or ignore it

        char cmdtype=extcmd[2]&1?extcmd[5]:extcmd[4];
        if (extcmd[2] & 1) {
            for (uint8_t i=0;i<8;i++) {
                if ( (extcmd[3] >> i) & 0x01) {
                    extcmd[1]=i+13; //get device id
                    break;
                }
            }
            //shift packets down
           extcmd[2]=extcmd[3];
           extcmd[3]=extcmd[4];
           extcmd[4]=extcmd[5];
        } else {
            for (uint8_t i=0;i<8;i++) {
                if ( (extcmd[2] >> i) & 0x01) {
                    extcmd[1]=i+6; //get device id
                    break;
                }
            }
        }
       if (cmdtype==0xF1) {  // expander channel update
        uint8_t channel;
        if (extbuf[3]) { //if we have zone expander data
            channel =(extbuf[3] >> 5);
            if (!channel) channel=8;
            channel=((extcmd[1]-7) *8) + 8 + channel; //calculate zone
            extcmd[4]=(extbuf[3] >> 3 & 3)?1:0; //fault
        } else {
             channel=0; //no zone data so set to zero
             extcmd[4]=0;
        }
        extcmd[2]=cmdtype;//copy subcommand to byte 2
        extcmd[3]=channel; 
        extcmd[5]=extbuf[2];  //relay data

        newExtCmd=true;
        extidx=0;
        return;
      
        
       } else if (cmdtype==0xf7) {  // expander poll request
        extcmd[2]=cmdtype;//copy subcommand to byte 2
        extcmd[3] = 0;
        extcmd[4]=extbuf[3]; //zone faults
        newExtCmd=true;
        extidx=0;
        return;
       } else if (cmdtype==0x00) { //relay channel update
           extcmd[2]=cmdtype;//copy subcommand to byte 2
           uint8_t channel;
           switch(extbuf[3]& 0x07f) { 
               case 1: channel=1;break;
               case 2: channel=2;break;
               case 4: channel=3;break;
               case 8: channel=4;break;
               default: channel=0;
           }
        extcmd[3]=channel; 
        extcmd[4]=extbuf[3]&0x80?1:0;
        newExtCmd=true;
        extidx=0;
        return;
       }
     } else if (extcmd[0] == 0x9E) {
    // Check how many bytes are in RF message (stored in upper nibble of Byte 2)
    uint8_t n_rf_bytes = extbuf[1] >> 4;

    if (n_rf_bytes == 5) { // For monitoring, we only care about 5 byte messages since that contains data about sensors
        // Verify data 
        uint16_t rf_checksum = 0;
        for (uint8_t i = 1; i <= n_rf_bytes + 1; i++) {
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
            extidx = 0;
            return;

            // How to rebuild serial into single integer
            // uint32_t device_serial = (extbuf[2] & 0xF) << 16;  // Only the lower nibble is part of the device serial
            // device_serial += extbuf[3] << 8;
            // device_serial += extbuf[4];
        } 
       }
    #ifdef DEBUG
        else {
            outStream->println("RF Checksum failed.");
        }
    #endif
      }  




      else if (extcmd[0] != 0 && extcmd[0] != 0xf6) {
          extcmd[1]=0; //no device
      }
      for (uint8_t i=0;i<=extidx;i++) extcmd[3+i]=extbuf[i]; //populate  buffer 0=cmd, 1=device, rest is tx data
      newExtCmd=true;
      extidx=0;

}
#endif

bool Vista::handle()
{
  uint8_t x;
  
#ifdef MONITORTX
    while (vistaSerialMonitor->available()) {
        x=vistaSerialMonitor->read();
        if (extidx < szExt)
            extbuf[extidx++]=x;
        
    }
    if (  extidx > 0 ) {
            if (!gotcmd) {
                gotcmd=true; //set our flag so that we don't reprocess again
                markPulse=0;
            }
            if (markPulse > 0 ) { //ok, we are on the next pulse, lets decode the previous data
             decodePacket();
             gotcmd=false;
             return 1;
           }
   }
#endif

  if (vistaSerial->available()) {
    x = vistaSerial->read();
    
   //we need to skips initial zero's here since the RX line going back high after a command, can create a bogus character
    if (!x) return 0;
    
    if (expectByte != 0) {
       if ( x != expectByte) {
            onResponseError(x);
            clearExpect();
        } else {
            onResponseComplete(x);
            clearExpect();
            return 0;
        }
    }

    memset(cbuf, 0, szCbuf); //clear buffer mem
    
    //expander request command
    if (x == 0x98) {
        gidx=0;
		cbuf[ gidx++ ] = x;
		readChar(cbuf, &gidx);
        readChar(cbuf, &gidx);
        int len=N98_MESSAGE_LENGTH;
        if (!(cbuf[2] & 1)) {// byte 2 = 01 if extended addressing for relay boards 14,15 so packet longer by 1 byte 
           len=len-1;
        } 
        readChars( len - 2, cbuf, &gidx, 8);
       // if (!validChksum(cbuf,0,len) return 0;
        newCmd=true; 
        onExp(cbuf);
#ifdef MONITORTX
        memset(extcmd, 0,szExt); //store the previous panel sent data in extcmd buffer for later use
        memcpy(extcmd,cbuf,5);  
#endif        
        return 1;
	}   
    
	if (x == 0xF7) {

        gidx=0;
		cbuf[ gidx++ ] = x;
 		readChars( F7_MESSAGE_LENGTH -1, cbuf, &gidx, 45);
        if (!validChksum(cbuf,0,F7_MESSAGE_LENGTH)) return 0;
        newCmd=true;
        rxState=sAckf7; 
		onDisplay(cbuf, &gidx);
		return 1;
	}

    //Long Range Radio (LRR)
	if (x == 0xF9) {
        
		gidx = 0;
		cbuf[ gidx++ ] = x;
		//read cycle
		readChar(cbuf, &gidx);
		//read len
		readChar(cbuf, &gidx);
		readChars(cbuf[2] , cbuf, &gidx, 30);
        //if (!validChksum(cbuf,0,cbuf[2]+2)) return 0;
        newCmd=true;       
		onLrr(cbuf, &gidx);
#ifdef MONITORTX
        memset(extcmd, 0,szExt); //store the previous panel sent data in extcmd buffer for later use
        memcpy(extcmd,cbuf,5);  
#endif           
		return 1;
	} 
	//key ack
	if (x == 0xF6) {
        newCmd=true;        
		gidx = 0;
		cbuf[ gidx++ ] = x;
		readChar(cbuf, &gidx);
        if (cbuf[1] == kpAddr) {
            writeChars();
        } 
#ifdef MONITORTX
        memset(extcmd, 0,szExt); //store the previous panel sent data in extcmd buffer for later use
        memcpy(extcmd,cbuf,5);  
#endif       
		return 1;
	}
    

	//secondary statuses
	if (x == 0xF2) {
      
        gidx = 0;
		cbuf[ gidx++ ] = x;
		readChar(cbuf, &gidx);
		readChars(cbuf[1], cbuf, &gidx, 30);
       // if (!validChksum(cbuf,cbuf[1])) return 0;
        newCmd=true;                
		onStatus(cbuf, &gidx);
		return 1;
	}
    
    //unknown
    if (x == 0xF8) {
        newCmd=true;
        gidx=0;
		cbuf[ gidx++ ] = x;
		readChars( F8_MESSAGE_LENGTH -1, cbuf, &gidx, 9);
		return 1;
	}  
    
    //unknown
    if (x == 0x87) {
        newCmd=true;        
        gidx=0;
		cbuf[ gidx++ ] = x;
		readChars(8, cbuf, &gidx, 9);
		return 1;
        
	}    
    //RF supervision
	if (x == 0x9E) {
        newCmd=true;        
        gidx = 0;
		cbuf[ gidx++ ] = x;
        //if (!validChksum(cbuf,5) return 0;
		readChars(4, cbuf, &gidx, 8);
#ifdef MONITORTX
        memset(extcmd, 0,szExt); //store the previous panel sent data in extcmd buffer for later use
        memcpy(extcmd,cbuf,5);  
#endif    
		return 1;
	}
    
	//for debugging if needed
    if (expectByte == 0 ) {
        #ifdef DEBUG
        newCmd=true;
        gidx = 0;
		cbuf[ gidx++ ] = x;
       readChars(4, cbuf, &gidx, 4);
       	return 1;
        #endif
        return 0;
	}

  }

 return 0;
}

//i've included these for debugging code only to stop processing during a hw lockup. do not use in production
void Vista::hw_wdt_disable(){
  *((volatile uint32_t*) 0x60000900) &= ~(1); // Hardware WDT OFF
}

void Vista::hw_wdt_enable(){
  *((volatile uint32_t*) 0x60000900) |= 1; // Hardware WDT ON
}

void Vista::stop() {
  //hw_wdt_enable(); //debugging only
  detachInterrupt(rxPin);
#ifdef MONITORTX  
  detachInterrupt(monitorTxPin);
#endif  
  keybusConnected=false;
}

void Vista::begin()   {
  //hw_wdt_disable(); //debugging only
  //ESP.wdtDisable(); //debugging
  expectByte=0;
  retries=0;
  //panel data rx interrupt - yellow line
  if (vistaSerial->isValidGPIOpin(rxPin))
    attachInterrupt(digitalPinToInterrupt(rxPin), rxISRHandler, CHANGE);
 
#ifdef MONITORTX
 //interrupt for capturing keypad/module data on green transmit line
 if (vistaSerialMonitor->isValidGPIOpin(monitorTxPin))
    attachInterrupt(digitalPinToInterrupt(monitorTxPin), txISRHandler, CHANGE);
#endif
  keybusConnected=true;

}
