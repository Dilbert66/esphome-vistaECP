/*
SoftwareSerial.h

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

#pragma once

#include <inttypes.h>

#include <Stream.h>

#include <functional>

#include <atomic>

#include "Arduino.h"

#if defined( ESP32) && not defined(IRAM_ATTR)
#define IRAM_ATTR IRAM_ATTR
#endif


// If only one tx or rx wanted then use this as parameter for the unused pin
constexpr int SW_SERIAL_UNUSED_PIN = -1;

enum SoftwareSerialConfig {
        SWSERIAL_5N1 = 0,
        SWSERIAL_6N1,
        SWSERIAL_7N1,
        SWSERIAL_8E2 //ecp is 4800 8E2
};

// This class is compatible with the corresponding AVR one,
// the constructor however has an optional rx buffer size.
// Baudrates up to 115200 can be used.

class SoftwareSerial: public Stream {
    public: SoftwareSerial(int receivePin, int transmitPin, bool inverse_logic = false, int bufSize = 64, int isrBufSize = 0);
    virtual~SoftwareSerial();

    void begin(int32_t baud = 2400) {
        begin(baud, SWSERIAL_8E2);
    }
    void begin(int32_t baud, SoftwareSerialConfig config);
    void setConfig(int32_t baud, SoftwareSerialConfig config);    
    void setBaud(int32_t baud);
    // Transmit control pin
    void setTransmitEnablePin(int transmitEnablePin);
    // Enable or disable interrupts during tx
    void enableIntTx(bool on);

    bool overflow();
    bool processSingle=false;

    int available();
    int peek();
    int read(bool processRxbits);
    int read();
    size_t write(uint8_t byte, bool parity);
    size_t write(uint8_t b, bool parity,int32_t baud );
    size_t write(uint8_t byte);

    // size_t write(const uint8_t * buffer, size_t size, bool parity);
    //size_t write(const uint8_t *buffer, size_t size) override;
    operator bool() const {
        return m_rxValid || m_txValid;
    }

    // Disable or enable interrupts on the rx pin
    void enableRx(bool on);
    // One wire control
    void enableTx(bool on);
    uint8_t checkParity(uint8_t b);

    void rxRead();
    
    int bitsAvailable();

    void end();

    bool m_parity = true;;
    bool isValidGPIOpin(int pin);
    bool debug;
    
    private: 
    uint32_t m_periodStart;
    uint32_t m_periodDuration;
    bool parityEven(uint8_t byte) {
        byte ^= byte >> 4;
        byte &= 0xf;
        return (0x6996 >> byte) & 1;
    }
    uint8_t pduBits = 11;
    void resetPeriodStart() {
        m_periodDuration = 0;
        m_periodStart = ESP.getCycleCount();
    }
    unsigned long m_bitTime;
    /* check m_rxValid that calling is safe */
    void rxBits();

    // Member variables
    bool m_oneWire;
    int m_rxPin = SW_SERIAL_UNUSED_PIN;
    int m_txPin = SW_SERIAL_UNUSED_PIN;
    int m_txEnablePin = SW_SERIAL_UNUSED_PIN;
    bool m_rxValid = false;
    bool m_rxEnabled = false;
    bool m_txValid = false;
    bool m_txEnableValid = false;
    bool m_invert;
    bool m_overflow = false;
    int8_t m_dataBits;
    int32_t m_bitCycles;
    int32_t m_4800_bitCycles;
    bool m_intTxEnabled;
    int m_inPos, m_outPos;
    int m_bufSize = 0;
    uint8_t * m_buffer = 0;
    // the ISR stores the relative bit times in the buffer. The inversion corrected level is used as sign bit (2's complement):
    // 1 = positive including 0, 0 = negative.
    std::atomic < int > m_isrInPos,
    m_isrOutPos;
    int m_isrBufSize = 0;
    std::atomic < uint32_t > * m_isrBuffer;
    std::atomic < bool > m_isrOverflow;
    std::atomic < uint32_t > m_isrLastCycle;
    int m_rxCurBit; // 0 - 7: data bits. -1: start bit. 8: stop bit.
    uint8_t m_rxCurByte = 0;

};
