/*

SoftwareSerial.cpp - Implementation of the Arduino software serial for ESP8266/ESP32.
Copyright (c) 2015-2016 Peter Lerup. All rights reserved.
Copyright (c) 2018-2019 Dirk O. Kaar. All rights reserved.

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

Modified for 4800 8E2 
*/

#include <Arduino.h>

#include "ECPSoftwareSerial.h"

SoftwareSerial::SoftwareSerial(
    int receivePin, int transmitPin, bool inverse_logic, int bufSize, int isrBufSize) {
    m_isrBuffer = 0;
    m_isrOverflow = false;
    m_isrLastCycle = 0;
    m_oneWire = (receivePin == transmitPin);
    m_invert = inverse_logic;
    if (isValidGPIOpin(receivePin)) {
        m_rxPin = receivePin;
        m_bufSize = bufSize;
        m_buffer = (uint8_t * ) malloc(m_bufSize);
        m_isrBufSize = isrBufSize ? isrBufSize : 10 * bufSize;
        m_isrBuffer = static_cast < std::atomic < uint32_t > * > (malloc(m_isrBufSize * sizeof(uint32_t)));
    }
    if (isValidGPIOpin(transmitPin) 
        #ifdef ESP8266 
    ||  (!m_oneWire && (transmitPin == 16))) {
        #else
    ) {
        #endif
        m_txValid = true;
        m_txPin = transmitPin;
    }
}

SoftwareSerial::~SoftwareSerial() {
    end();
    if (m_buffer) {
        free(m_buffer);
    }
    if (m_isrBuffer) {
        free(m_isrBuffer);
    }
}

bool SoftwareSerial::isValidGPIOpin(int pin) {
    #ifdef ESP8266
    return (pin >= 0 && pin <= 5) || (pin >= 12 && pin <= 15);
    #endif
    #ifdef ESP32
    return pin == 0 || pin == 2 || (pin >= 4 && pin <= 5) || (pin >= 12 && pin <= 19) ||
        (pin >= 21 && pin <= 23) || (pin >= 25 && pin <= 27) || (pin >= 32 && pin <= 36) || pin==39;
    #endif
}

void  SoftwareSerial::setBaud(int32_t baud) {
      m_bitCycles = ESP.getCpuFreqMHz() * 1000000 / baud;

      if (baud==4800) //we save 4800 bit cycles for call from ISR later
          m_4800_bitCycles=m_bitCycles;
}

void SoftwareSerial::setConfig(int32_t baud, SoftwareSerialConfig config) {
    setBaud(baud);
    m_dataBits = 5 + (config % 4);
}

void SoftwareSerial::begin(int32_t baud, SoftwareSerialConfig config) {
    m_dataBits = 5 + (config % 4);
    setBaud(baud);
    m_intTxEnabled = true;

    if (m_buffer != 0 && m_isrBuffer != 0) {
        m_rxValid = true;
        m_inPos = m_outPos = 0;
        m_isrInPos.store(0);
        m_isrOutPos.store(0);
        pinMode(m_rxPin, INPUT);
    }
    if (m_txValid && !m_oneWire) {
        pinMode(m_txPin, OUTPUT);
        digitalWrite(m_txPin, !m_invert);
    }

    if (!m_rxEnabled) {
        enableRx(true);
    }

}

void SoftwareSerial::end() {
    enableRx(false);

}

void SoftwareSerial::setTransmitEnablePin(int transmitEnablePin) {
    if (isValidGPIOpin(transmitEnablePin)) {
        m_txEnableValid = true;
        m_txEnablePin = transmitEnablePin;
        pinMode(m_txEnablePin, OUTPUT);
        digitalWrite(m_txEnablePin, LOW);
    } else {
        m_txEnableValid = false;
    }
}

void SoftwareSerial::enableIntTx(bool on) {
    m_intTxEnabled = on;
}

void SoftwareSerial::enableTx(bool on) {
    if (m_txValid && m_oneWire) {
        if (on) {
            enableRx(false);
            pinMode(m_txPin, OUTPUT);
            digitalWrite(m_txPin, !m_invert);
        } else {
            pinMode(m_rxPin, INPUT);
            enableRx(true);
        }
    }
}

void SoftwareSerial::enableRx(bool on) {
    if (m_rxValid) {
        if (on) {
            m_rxCurBit = m_dataBits + 3;

        }
        m_rxEnabled = on;
    }
}

int SoftwareSerial::read() {
    if (!m_rxValid) {
        return -1;
    }
    if (m_inPos == m_outPos) {
        if (m_inPos == m_outPos) {
            return -1;
        }
    }
    uint8_t ch = m_buffer[m_outPos];
    m_outPos = (m_outPos + 1) % m_bufSize;
    return ch;
}

int SoftwareSerial::read(bool processRxbits=false) {
    if (!m_rxValid) {
        return -1;
    }
    if (m_inPos == m_outPos) {
      if (processRxbits)  rxBits();
        if (m_inPos == m_outPos) {
            return -1;
        }
    }
    uint8_t ch = m_buffer[m_outPos];
    m_outPos = (m_outPos + 1) % m_bufSize;
    return ch;
}

int SoftwareSerial::available() {
    if (!m_rxValid) {
        return 0;
    }
    rxBits();
    int avail = m_inPos - m_outPos;
    if (avail < 0) {
        avail += m_bufSize;
    }
    if (!avail) {
        optimistic_yield(2 * (m_dataBits + 4) * m_bitCycles / ESP.getCpuFreqMHz());
        rxBits();
        avail = m_inPos - m_outPos;
        if (avail < 0) {
            avail += m_bufSize;
        }
    }
    return avail;
}

#define WAIT {     while (ESP.getCycleCount() - start < wait);    wait += m_bitCycles; }

size_t IRAM_ATTR SoftwareSerial::write(uint8_t b, bool parity,int32_t baud ) {
    int32_t origCycles=m_bitCycles;
    bool origParity = m_parity;
    

    if (baud == 4800 && m_4800_bitCycles > 0)
        m_bitCycles=m_4800_bitCycles;
    else
        m_bitCycles = ESP.getCpuFreqMHz() * 1000000 / baud;  //we use a precalculated value for 4800 baud rate when called from an ISR since getcpufreqmhz is not an isr friendly function. Only need 4800 for the isr call.
                                                                              
    
    m_parity = parity;
    size_t r = write(b);
    m_parity = origParity;
    m_bitCycles=origCycles;
    return r;
}

size_t IRAM_ATTR SoftwareSerial::write(uint8_t b, bool parity) {
    bool origParity = m_parity;
    m_parity = parity;
    size_t r = write(b);
    m_parity = origParity;
    return r;
}


size_t IRAM_ATTR SoftwareSerial::write(uint8_t b) {
    uint8_t parity = 0;
    if (!m_txValid) return 0;
    bool s = m_invert;

    if (m_invert) b = ~b;
    if (m_txEnableValid) digitalWrite(m_txEnablePin, HIGH);
    unsigned long wait = m_bitCycles;
    unsigned long start = ESP.getCycleCount();
    // Start bit;
    if (m_invert)
        digitalWrite(m_txPin, HIGH);
    else
        digitalWrite(m_txPin, LOW);
    WAIT;
    for (int i = 0; i < m_dataBits; i++) {
        if (b & 1) {
            digitalWrite(m_txPin, HIGH);
            parity = parity ^ 0x01;
        } else {
            digitalWrite(m_txPin, LOW);
            parity = parity ^ 0x00;
        }
        WAIT;
        b >>= 1;
    }
    // parity bit
    if (m_parity) {
        if (parity == 0) {
            if (m_invert && m_dataBits !=5) {
                digitalWrite(m_txPin, HIGH);
            } else {
                digitalWrite(m_txPin, LOW);
            }
        } else {
            if (m_invert && m_dataBits !=5) {
                digitalWrite(m_txPin, LOW);
            } else {
                digitalWrite(m_txPin, HIGH);
            }
        }
        WAIT;
    }

    // restore pin to natural state
    if (m_invert) {
        digitalWrite(m_txPin, LOW);
    } else {
        digitalWrite(m_txPin, HIGH);
    }
    WAIT; //1st stop bit
    if (m_dataBits != 5) // 1 stop bit for keypad send
        WAIT;
    if (m_txEnableValid) digitalWrite(m_txEnablePin, LOW);

    return 1;
}

void IRAM_ATTR SoftwareSerial::flush() {
    m_inPos = m_outPos = 0;
    m_isrInPos.store(0);
    m_isrOutPos.store(0);
}

void IRAM_ATTR SoftwareSerial::flush(SoftwareSerial * self) {
    self -> m_inPos = self -> m_outPos = 0;
    self -> m_isrInPos.store(0);
    self -> m_isrOutPos.store(0);
}

bool SoftwareSerial::overflow() {
    bool res = m_overflow;
    m_overflow = false;
    return res;
}

int SoftwareSerial::peek() {
    if (!m_rxValid || (rxBits(), m_inPos == m_outPos)) {
        return -1;
    }
    return m_buffer[m_outPos];
}

void SoftwareSerial::rxBits() {
    int avail = m_isrInPos.load() - m_isrOutPos.load();
    if (avail < 0) {
        avail += m_isrBufSize;
    }
    if (m_isrOverflow.load()) {
        m_overflow = true;
        m_isrOverflow.store(false);
    }

    // stop bit can go undetected if leading data bits are at same level
    // and there was also no next start bit yet, so one byte may be pending.
    // low-cost check first
    
    if (avail == 0 && m_rxCurBit < m_dataBits+3   && m_isrInPos.load() == m_isrOutPos.load() && m_rxCurBit >= 0) {
        uint32_t expectedCycle = m_isrLastCycle.load() + (m_dataBits + 3 - m_rxCurBit) * m_bitCycles;
        if (static_cast < int32_t > (ESP.getCycleCount() - expectedCycle) > m_bitCycles) {
            // Store inverted stop bit edge and cycle in the buffer unless we have an overflow
            // cycle's LSB is repurposed for the level bit
            int next = (m_isrInPos.load() + 1) % m_isrBufSize;
            if (next != m_isrOutPos.load()) {
                m_isrBuffer[m_isrInPos.load()].store((expectedCycle | 1) ^ !m_invert);
                m_isrInPos.store(next);
                ++avail;
            } else {
                m_isrOverflow.store(true);
            }
        }
    }

    while (avail--) {
        // error introduced by edge value in LSB is negligible
        uint32_t isrCycle = m_isrBuffer[m_isrOutPos.load()].load();
        // extract inverted edge value
        bool level = (isrCycle & 1) == m_invert;
        m_isrOutPos.store((m_isrOutPos.load() + 1) % m_isrBufSize);

        int32_t cycles =  (isrCycle - m_isrLastCycle.load()) -  (m_bitCycles/2);
        if (cycles < 0) cycles=-cycles;
        m_isrLastCycle.store(isrCycle);

        /*
        if (cycles < 0 && debug) {
           Serial.printf("isrCycle=%u,lastcycle=%u,cycles=%d,cycles=%u,bitcycles=%d\n",isrCycle,m_isrLastCycle.load(),cycles,cycles,m_bitCycles/2);
        }
        */
       // uint32_t bits = cycles / m_bitCycles;
       // if (cycles % m_bitCycles > (m_bitCycles >> 1)) ++bits;
       uint8_t parity;
       uint8_t stop1;
       uint8_t stop2;
       
        do {
            // data bits
            uint32_t bits=0;
            uint32_t hiddenBits=0;
            bool lastBit=false;
            if (m_rxCurBit >= -1 && m_rxCurBit < (m_dataBits )) {
                parity=0;
                stop1=1;
                stop2=1;
                //Serial.printf("curbit=%d,cycles=%04x,level=%d\n",m_rxCurBit,cycles,level);
               
                if (cycles >= m_bitCycles) {
                    // preceding masked bits
                    bits = cycles / m_bitCycles;
                    if (bits >= m_dataBits - m_rxCurBit) {
                        hiddenBits = (m_dataBits - 1) - m_rxCurBit;
                    } else {
                        hiddenBits=bits;
                    }
                    bits-=hiddenBits;
                    lastBit = m_rxCurByte & 0x80;
                    m_rxCurByte >>= hiddenBits;
                    // masked bits have same level as last unmasked bit
                    if (lastBit ) {
                        m_rxCurByte |= 0xff << (8 - hiddenBits);
                    }
                    m_rxCurBit += hiddenBits;
                    cycles -= hiddenBits * m_bitCycles;
                   if ( bits) {
                     //  if (bits > 2)  bits=2;
                        if (lastBit) {
                            parity=1;
                        }
                       // Serial.printf("got bits. bits=%d,Lastbit=%d,curbit=%d\n",bits,lastBit,m_rxCurBit);
                        cycles-=m_bitCycles;
                        ++m_rxCurBit;
                        --bits;

                        
                  }                     
                }
                if ( m_rxCurBit == m_dataBits-1) {
                    ++m_rxCurBit;
                    cycles-=m_bitCycles;
                    parity=level;
                   //Serial.printf("Set parity from level %d,bits=%d\n",level,bits);
                }                 
 //Serial.printf("curbyte=%02X,cycle=%08X,bitcycles=%08X,bits=%d,curbit=%d,hiddenbits=%d,parity=%d,lastBit=%d\n",m_rxCurByte,cycles,m_bitCycles,bits,m_rxCurBit,hiddenBits,parity,lastBit);

                if (m_rxCurBit < (m_dataBits - 1) ) {
                    
                    ++m_rxCurBit;
                    cycles -= m_bitCycles;
                    m_rxCurByte >>= 1;
                    if (level) {
                        m_rxCurByte |= 0x80;
                    }
                
                } 
               if (!bits) 
                   continue;


            }
            //1st stop bit
               if (m_rxCurBit==m_dataBits) {
                uint8_t bits = cycles / m_bitCycles;                    
                ++m_rxCurBit;

                    if (bits < 2) {
                        stop1=level;
                        cycles -= m_bitCycles;
                        continue;
                    }
          //  Serial.printf("   stop1: stop1=%d,parity=%d,bits=%d,level=%d\n",stop1,parity,bits,level);                     
                }        
            //2nd stop bit and save byte
        if (m_rxCurBit == m_dataBits+1 ) {
                uint8_t bits = cycles / m_bitCycles; 
                ++m_rxCurBit;
                stop2=level;
                cycles -= m_bitCycles;                
                if (bits) {
                    stop2=!level;
                } 

          //   Serial.printf("   stop2: stop1=%d,parity=%d,bits=%d,level=%d\n",stop1,parity,bits,level);    
                // Store the received value in the buffer unless we have an overflow
                    int next = (m_inPos + 1) % m_bufSize;
                    char byt= m_rxCurByte >> (8 - m_dataBits);  
  
           // Serial.printf("*** byte=%02X,stop1=%d,stop2=%d,parity=%d,bits=%d,level=%d,self=%d\n\n",byt,stop1,stop2,parity,bits,level,this);                
                if (stop2 || byt ) {
                    if (next != m_outPos) {
                        m_buffer[m_inPos] = byt;
                        m_inPos = next;

                    } else {
                        m_overflow = true;
                    }
                }
                // reset to 0 is important for masked bit logic
                m_rxCurByte = 0;
   
                continue;
            }
            if (m_rxCurBit > m_dataBits + 1) {
                // start bit level is low
                if (!level) {
                    m_rxCurBit = -1;

                }
                //if flag set, we only process 1 byte at a time
                if (processSingle) {
                    avail=0;
                }                
            }
            break;
        } while (cycles >= 0);

 
    }
}

void IRAM_ATTR SoftwareSerial::rxRead(SoftwareSerial * self) {
    uint32_t curCycle = ESP.getCycleCount();
    bool level = digitalRead(self -> m_rxPin);

    // Store inverted edge value & cycle in the buffer unless we have an overflow
    // cycle's LSB is repurposed for the level bit
    int next = (self -> m_isrInPos.load() + 1) % self -> m_isrBufSize;
    if (next != self -> m_isrOutPos.load()) {
        self -> m_isrBuffer[self -> m_isrInPos.load()].store((curCycle | 1) ^ level);
        self -> m_isrInPos.store(next);
    } else {
        self -> m_isrOverflow.store(true);
    }
}

bool SoftwareSerial::bitsAvailable() {
    int avail = m_isrInPos.load() - m_isrOutPos.load();
    if (avail < 0) {
        avail += m_isrBufSize;
    }
    if (avail)
        return true;
    else
        return false;

}

