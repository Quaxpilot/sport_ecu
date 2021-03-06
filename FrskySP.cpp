/**
 * Arduino library for Frsky Smart Port protocol.
 */

#include "Arduino.h"
#include "FrskySP.h"

#ifdef USE_TEENSY

#define sportSerial          Serial1

#define sportSerial_UART_C1  UART0_C1
#define sportSerial_UART_C3  UART0_C3
#define sportSerial_UART_S2  UART0_S2

#define UART_C3_TXDIR        0x20

#define MAX_SENSORS                8
#define SPORT_FRAME_BEGIN       0x7E
#define SPORT_BYTE_STUFF_MARKER 0x7D
#define SPORT_BYTE_STUFF_MASK   0x20

/**
 * Smart Port protocol uses 8 bytes packets.
 * 
 * Packet format (byte): tiivvvvc
 * - t: type (1 byte)
 * - i: sensor ID (2 bytes)
 * - v: value (4 bytes - int32)
 * - c: crc
 * 
 * The uint64 presentation is much easier to use for data shifting.
 */
union FrskySP_class::packet {
    //! byte[8] presentation
    uint8_t byte[8];
    //! uint64 presentation
    uint64_t uint64;
};

struct FrskySP_SensorData {
  uint16_t id;
  uint32_t val;
};

static FrskySP_SensorData _sensorTable[MAX_SENSORS];
static uint8_t _sensorTableIdx = 0;
static uint8_t _sensorId = 0;
static uint8_t _sensorValues = 0;

/**
 * Calculate the CRC of a packet
 * https://github.com/opentx/opentx/blob/next/radio/src/telemetry/frsky_sport.cpp
 */
uint8_t FrskySP_class::CRC (uint8_t *packet) {
    short crc = 0;
    for (int i=0; i<8; i++) {
        crc += packet[i]; //0-1FF
        crc += crc >> 8;  //0-100
        crc &= 0x00ff;
        crc += crc >> 8;  //0-0FF
        crc &= 0x00ff;
    }
    return ~crc;
}

/**
 * Sensors logical IDs and value formats are documented in FrskySP.h.
 * 
 * Packet format:
 * content   | length | remark
 * --------- | ------ | ------
 * type      | 8 bit  | always 0x10
 * sensor ID | 16 bit | sensor's logical ID
 * data      | 32 bit | preformated data
 * crc       | 8 bit  | calculated by CRC()
 */
void FrskySP_class::sendData (uint8_t type, uint16_t id, int32_t val)
{
  union packet packet;

  packet.uint64  = (uint64_t) type | (uint64_t) id << 8 | (int64_t) val << 24;
  packet.byte[7] = CRC (packet.byte);

  sportSerial_UART_C3 |= UART_C3_TXDIR;

  for (int i=0; i<8; i++) {

    if((packet.byte[i] == SPORT_FRAME_BEGIN) ||
       (packet.byte[i] == SPORT_BYTE_STUFF_MARKER)) {
      sportSerial.write(SPORT_BYTE_STUFF_MARKER);
      sportSerial.write(packet.byte[i] ^ SPORT_BYTE_STUFF_MASK);
    }
    else {
      sportSerial.write (packet.byte[i]);
    }
  }

  sportSerial.flush();
  sportSerial_UART_C3 &= ~(UART_C3_TXDIR);
}

#else // Arduinos

// use the software serial half duplex lib
#include "FrskySport.h"

#endif

/**
 * Init hardware serial port
 */
void FrskySP_class::begin (uint8_t sensorId, uint8_t sensorValues)
{
#ifdef USE_TEENSY
  _sensorId = sensorId;
  _sensorValues = sensorValues;

  sportSerial.begin(57600);

  sportSerial_UART_C3 = 0x10; // TX inv
  sportSerial_UART_S2 = 0x10; // RX inv
  sportSerial_UART_C1 = UART_C1_LOOPS | UART_C1_RSRC;
#else
  initSportUart();
  setSportSensorId(sensorId);
  setSportSensorValues(sensorValues);
#endif
}

/**
 * Polls the serial buffer for new sensor requests
 */
void FrskySP_class::poll ()
{
#ifdef USE_TEENSY
  while(sportSerial.available()) {
    if(sportSerial.read() == SPORT_FRAME_BEGIN) {
      
      while (!sportSerial.available());
      if((sportSerial.read() == _sensorId) &&
         _sensorValues) {


        FrskySP.sendData(0x10,
                         _sensorTable[_sensorTableIdx].id,
                         _sensorTable[_sensorTableIdx].val);

        _sensorTableIdx = (_sensorTableIdx + 1) % _sensorValues;
      }
    }
  }
#endif
}

/**
 * Updates sensor data to be sent
 */
void FrskySP_class::setSensorData(uint8_t idx, uint16_t id, uint32_t val)
{
#ifdef USE_TEENSY
  _sensorTable[idx].id  = id;
  _sensorTable[idx].val = val;
#else
  setSportNewData(idx,id,val);
#endif
}
