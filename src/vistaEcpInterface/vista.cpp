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
                   
                if (statusFlags.systemFlag) {
                    statusFlags.armedStay = ((cbuf[x] & BIT_MASK_BYTE2_ARMED_HOME) > 0);
                    statusFlags.lowBattery = ((cbuf[x] & BIT_MASK_BYTE2_LOW_BAT) > 0);
                    statusFlags.ready = ((cbuf[x] & BIT_MASK_BYTE2_READY) > 0);
                    statusFlags.acLoss = ((cbuf[x] & BIT_MASK_BYTE2_AC_LOSS) > 0);
                  
                } else {
                    statusFlags.check = ((cbuf[x] & BIT_MASK_BYTE2_CHECK_FLAG) > 0);
                    statusFlags.fireZone = ((cbuf[x] & BIT_MASK_BYTE2_ALARM_ZONE) > 0);
                }

                break;
            case 8:
                statusFlags.inAlarm = ((cbuf[x] & BIT_MASK_BYTE3_IN_ALARM) > 0);
               if (statusFlags.systemFlag) {
                    statusFlags.instant = ((cbuf[x] & BIT_MASK_BYTE3_INSTANT) > 0);
                    statusFlags.acPower = ((cbuf[x] & BIT_MASK_BYTE3_AC_POWER) > 0);
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
                statusFlags.chime = ((cbuf[x] & BIT_MASK_BYTE3_CHIME_MODE) > 0);
                statusFlags.bypass = ((cbuf[x] & BIT_MASK_BYTE3_BYPASS) > 0);
                break;
                  
           case 9:
              statusFlags.programMode = (cbuf[x] == 0x01);
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
        expectByte=(char)((cbuf[1] + 0x40) & 0xFF);
	}

	//we don't need a checksum for 1 byte messages (no length bit)
	//if we don't even have a message length byte, then we are just
	// ACKing a cycle header byte.
	if (lcbuflen >= 2) {
		int chksum = 0;
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
    for (int i=0;i<MAX_ZONE_EXPANDERS;i++) {
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
    expFault=z << 5 | (fault?8:0); //0 = ok, 0x08=open, 0x10 = shorted  - convert to bitfield for F1 response
    if (z>0) z--; else z=7; //now convert to 0 - 7 for F7 poll response
    expFaultBits=  (fault?expFaultBits | (0x80 >> z):expFaultBits & ((0x80 >> z)^0xFF)); //setup bit fields for return response with fault values for each zone
    zoneExpanders[idx].expFault=expFault;
    zoneExpanders[idx].expFaultBits=expFaultBits;
    setNextFault(zoneExpanders[idx]); //push to the pending queue
}

//98 2E 02 20 F7 EC
//98 2E 04 20 F7 EA
// byte 3 is the binary position encoded expander addresss 02=107, 04=108,08=109, etc
//byte 4 is a seq byte, changes from 20 to 25 every request sequence
//byte 5 F7 for poll, F1 for a msg request, 80 for a resend request
// The 98 serial data has some inconsistencies with some bit durations. Might be an issue with my panel.  Be interested to know how it looks on another panel.  The checksum does not work as a two's complement so something is off. Either way, the bytes we need work fine for our purposes.

void Vista::onExp(char cbuf[]) {

     char type=cbuf[4];
     char seq=cbuf[3];
     char lcbuf[12];
  
    int idx;
    for (idx=0;idx<MAX_ZONE_EXPANDERS;idx++) {
        expansionAddr=zoneExpanders[idx].expansionAddr;
        if (cbuf[2]== (0x01 << (expansionAddr-6))) break; //for us
    }
    if (idx==2) return; //no match return
    expFaultBits=zoneExpanders[idx].expFaultBits;
    
    sending=true;
	int  lcbuflen = 0;
    expSeq=(seq == 0x20?0x34:0x31);
    
   // we use zone to either | or & bits depending if in fault or reset
    //0xf1 - response to request, 0xf7 - poll, 0x80 - retry
     if (type ==  0xF1 )
	 {
        expanderType currentFault = peekNextFault();  //check next item. Don't pop it yet
        if (currentFault.expansionAddr) {
            if ( expansionAddr != currentFault.expansionAddr) return; //no match to the pending fault data so not for us. Sanity check.
           getNextFault(); //pop it from the queue since we are processing it now
        } else
            currentFault=zoneExpanders[idx]; //no pending fault, use current fault data instead

        lcbuf[0] = (char) currentFault.expansionAddr;
        lcbuf[1]= (char) expSeq;
        lcbuf[2]= (char) 0;
        lcbuf[3] = (char) currentFault.expFault;  // we send out the current zone state 
  		lcbuflen = (char) 4;
       
    } else if (type ==   0xF7) {  // periodic  zone state poll (every 30 seconds)
		lcbuf[0] = (char) 0xF0;
        lcbuf[1]= (char) expSeq;
        lcbuf[2]= (char) 0; //shorts - we don't use this.  We set fault as open
        lcbuf[3] = (char) expFaultBits; //opens  - we send out the list of zone states
  		lcbuflen = (char) 4; 
	} else return; //we don't acknowledge if we don't know
    
    delayMicroseconds(200); 
    int chksum = 0;
	for (int x=0; x<lcbuflen; x++) {
		chksum += lcbuf[x];
        vistaSerial->write(lcbuf[x]);
	}
	chksum -= 1;
	chksum = chksum ^ 0xFF;
    vistaSerial->write((char) chksum);
    sending=false;
}


void Vista::outQueueInit() {
    outbufIdx = 0;
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
  outbuf[outbufIdx] = byt; // Save the data in a character array
  outbufIdx++; //Increment position in array
}

/**
 * Send 0-9, # and * characters
 * 1,2,3,4,5,6,7,8,9,0,#,*,
 * 31,32,33,34,35,36,37,38,39,30,23,2A,
 * 0011-0001,0011-0010,0011-0011,0011-0100,0011-0101,0011-0110,0011-0111,0011-1000,0011-1001,0011-0000,0010-0011,0010-1010,
 *
 */
void Vista::writeChars(){

  if (outbufIdx == 0) {return ;}

  uint8_t header = ((++writeSeq  << 6) & 0xc0) | (kpAddr & 0x3F);
  
  char x;
   //if retries are getting out of control with no successfull callback
  //just clear the buffer
  if (retries > 5) {
    retries = 0;
    outQueueInit();
    return;
  }
   sending=true;
   delayMicroseconds(200);

  //header is the bit mask YYXX-XXXX
  //  where YY is an incrementing sequence number
  //  and XX-XXXX is the keypad address (decimal 16-31)
  vistaSerial->write(header);
  vistaSerial->write(outbufIdx +1);
  
   //adjust characters to hex values.
  //ASCII numbers get translated to hex numbers
  //# and * get converted to 0xA and 0xB
  // send any other chars straight, although this will probably 
  // result in errors
  //0xc = A (*/1), 0xd=B (*/#) , 0xe=C (3/#)
  int checksum = 0;
  for(int x =0; x < outbufIdx; x++) {
    //translate digits between 0-9 to hex/decimal
    if (outbuf[x] >= 0x30 && outbuf[x] <= 0x39) {
      outbuf[x] -= 0x30;
    } else 
    //translate * to 0x0b
    if (outbuf[x] == 0x23) {
      outbuf[x] = 0x0B;
    } else 
    //translate # to 0x0a
    if (outbuf[x] == 0x2A) {
      outbuf[x] = 0x0A;
    } else
    //translate A to 0x1C (function key A)
    //translate B to 0x1D (function key B)
    //translate C to 0x1E (function key C)
    //translate D to 0x1F (function key D)
    if (outbuf[x] >= 0x41 && outbuf[x] <= 0x44) {
      outbuf[x] = outbuf[x] - 0x25;
    }
    checksum += outbuf[x];
    vistaSerial->write(outbuf[x]);
  }
   int chksum = 0x100 - (header + checksum + (uint8_t)outbufIdx+1);
   vistaSerial->write((char) chksum);  
   expectByte=header;
   setOnResponseCompleteCallback(std::bind(&Vista::keyAckComplete,this,(char) header)); 
   retries++;
   sending=false;
}

void Vista::keyAckComplete(char data) {

	outQueueInit();
	retries = 0;
    
    //here we can upate the sent buffer to what was already sent instead of just clearing it
    // this way, we won't lose keys entered while the process was sending

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
             } else if (outbufIdx > 0 ) {
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
     
      if (extcmd[0]==0x98 ) {
        for (uint8_t i=0;i<7;i++) {
            if ( (extcmd[2] >> i) & 0x01) {
                extcmd[1]=i+6; //get device id
                break;
            }
        }
       if (extcmd[4]==0xf1) {  // expander channel change request
        uint8_t channel=(extbuf[3] >> 5);
        if (!channel) channel=8;
        channel=((extcmd[1]-7) *8) + 8 + channel; //calculate zone
        extcmd[2]=channel; //zone
        extcmd[3]=(extbuf[3] >> 3 & 3)?1:0; //fault
        extcmd[4]=0;
        newExtCmd=true;
        extidx=0;
        return;
       } 
      } else if (extcmd[0] != 0 && extcmd[0] != 0xf6) {
          extcmd[1]=0; //no device
      }
      for (uint8_t i=0;i<extidx;i++) extcmd[2+i]=extbuf[i]; //populate  buffer 0=cmd, 1=device, rest is tx data
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
    if (expectByte != 0 && x != expectByte) {
       if (x) {
		onResponseError(x);
		clearExpect();
       }
	}
	if (expectByte != 0 && x == expectByte) {
		onResponseComplete(x);
		clearExpect();
		return 0;
	}

    memset(cbuf, 0, szCbuf); //clear buffer mem

	if (x == 0xF7) {
        newCmd=true;
        gidx=0;
		cbuf[ gidx++ ] = x;
 		readChars( F7_MESSAGE_LENGTH -1, cbuf, &gidx, 45);
        rxState=sAckf7; 
		onDisplay(cbuf, &gidx);
		return 1;
	}

    //Long Range Radio (LRR)
	if (x == 0xF9) {
        newCmd=true;        
		gidx = 0;
		cbuf[ gidx++ ] = x;
		//read cycle
		readChar(cbuf, &gidx);
		//read len
		readChar(cbuf, &gidx);
		readChars(cbuf[2] , cbuf, &gidx, 30);
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
    
    //expander request command
    if (x == 0x98) {
        newCmd=true;        
        gidx=0;
		cbuf[ gidx++ ] = x;
		readChars( N98_MESSAGE_LENGTH - 1, cbuf, &gidx, 6);
        onExp(cbuf);
#ifdef MONITORTX
        memset(extcmd, 0,szExt); //store the previous panel sent data in extcmd buffer for later use
        memcpy(extcmd,cbuf,5);  
#endif        
        return 1;
	}   
    
	//secondary statuses
	if (x == 0xF2) {
        newCmd=true;        
        gidx = 0;
		cbuf[ gidx++ ] = x;
		readChar(cbuf, &gidx);
		uint8_t len = cbuf[ gidx-1 ];
		readChars(len, cbuf, &gidx, 30);
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
    //unknow
     
	if (x == 0x9E) {
        newCmd=true;        
        gidx = 0;
		cbuf[ gidx++ ] = x;
		readChar(cbuf, &gidx);
		int len = cbuf[ gidx-1 ];
        len++;
		readChars(len, cbuf, &gidx, 30);
		return 1;
	}
    
	//for debugging if needed
    if (expectByte == 0 ) {
        #ifdef DEBUG
        gidx = 0;
		cbuf[ gidx++ ] = x;
        printPacket(cbuf,gidx); //debugging
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
  hw_wdt_enable(); //debugging only
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
