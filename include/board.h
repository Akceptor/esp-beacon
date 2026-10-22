// Radio config, pin map and wire format shared by the beacon (src/main.cpp) and the
// receiver (src/receiver.cpp). Keep in sync with platformio.ini + docs/RECEIVER.md.
#pragma once
#include <Arduino.h>
#include <SPI.h>
#include <RadioLib.h>
#if defined(ESP32)
#include <esp_mac.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#endif

static constexpr float    BAND_FREQ_MHZ = 868.0f;
static constexpr float    BW_KHZ        = 125.0f;
static constexpr uint8_t  SF            = 12;       // spreading factor
static constexpr uint8_t  CR            = 5;        // CR denominator: 5=4/5, 6=4/6, 7=4/7, 8=4/8
static constexpr uint8_t  SYNC_WORD     = 0x34;     // private, not LoRaWAN's 0x12
static constexpr int8_t   TX_POWER_DBM  = 13;
static constexpr uint16_t PREAMBLE_LEN  = 8;
#if defined(ESP32)

static constexpr float    TCXO_VOLTAGE  = 0.0f;     // plain crystal, no TCXO on this board
static constexpr uint8_t  LED_PIN       = 8;        // single NeoPixel, not a plain LED

// ESP32-C3 pin map, from the ExpressLRS UNIFIED_ESP32C3_LR1121_RX layout.
static constexpr int PIN_SCK  = 6;
static constexpr int PIN_MISO = 5;
static constexpr int PIN_MOSI = 4;
static constexpr int PIN_CS   = 7;
static constexpr int PIN_RST  = 2;
static constexpr int PIN_BUSY = 3;
static constexpr int PIN_DIO9 = 1;   // IRQ

// RF switch mapping matches ExpressLRS's default for this board: RX=DIO7, TX/TX_HP=DIO8.
static const uint32_t RF_SWITCH_PINS[] = {
  RADIOLIB_LR11X0_DIO5, RADIOLIB_LR11X0_DIO6,
  RADIOLIB_LR11X0_DIO7, RADIOLIB_LR11X0_DIO8,
  RADIOLIB_NC
};
static const Module::RfSwitchMode_t RF_SWITCH_TABLE[] = {
  { LR11x0::MODE_STBY,  { LOW,  LOW,  LOW,  LOW  } },
  { LR11x0::MODE_RX,    { LOW,  LOW,  HIGH, LOW  } },
  { LR11x0::MODE_TX,    { LOW,  LOW,  LOW,  HIGH } },
  { LR11x0::MODE_TX_HP, { LOW,  LOW,  LOW,  HIGH } },
  { LR11x0::MODE_TX_HF, { LOW,  HIGH, LOW,  LOW  } },
  { LR11x0::MODE_GNSS,  { LOW,  LOW,  LOW,  LOW  } },
  { LR11x0::MODE_WIFI,  { HIGH, LOW,  LOW,  LOW  } },
  END_OF_MODE_TABLE,
};

using RadioChip = LR1121;

inline Module* newRadioModule() { return new Module(PIN_CS, PIN_DIO9, PIN_RST, PIN_BUSY); }
inline void spiBegin() { SPI.begin(PIN_SCK, PIN_MISO, PIN_MOSI, PIN_CS); }
inline void readChipId(uint8_t id[6]) { esp_read_mac(id, ESP_MAC_WIFI_STA); }

inline int16_t radioBegin(RadioChip& r) {
  return r.begin(BAND_FREQ_MHZ, BW_KHZ, SF, CR, SYNC_WORD, TX_POWER_DBM, PREAMBLE_LEN, TCXO_VOLTAGE);
}
inline void radioApplyBoard(RadioChip& r) { r.setRfSwitchTable(RF_SWITCH_PINS, RF_SWITCH_TABLE); }
// This board's sub-GHz path is wired for the HP PA; begin() picks the LP PA at 13 dBm,
// which comes out ~8 dB weaker, so force HP.
inline int16_t radioSetTxPower(RadioChip& r) { return r.setOutputPower(TX_POWER_DBM, true); }

inline void ledInit()  { neopixelWrite(LED_PIN, 0, 0, 0); }
inline void ledOff()   { neopixelWrite(LED_PIN, 0, 0, 0); }
inline void ledTx()    { neopixelWrite(LED_PIN, 0, 16, 0); }
inline void ledRx()    { neopixelWrite(LED_PIN, 0, 0, 16); }

#elif defined(ESP8266)

// Generic ESP8266 LoRa module, SX1276 (868 MHz variant).
static constexpr int PIN_SCK  = 14;
static constexpr int PIN_MISO = 12;
static constexpr int PIN_MOSI = 13;
static constexpr int PIN_CS   = 15;   // NSS; boot-strap pin: must be low at reset, so nothing may pull it high
static constexpr int PIN_RST  = 2;    // boot-strap pin: must be high at reset, so nothing may pull it low
static constexpr int PIN_DIO0 = 4;    // IRQ
static constexpr int PIN_DIO1 = 5;
static constexpr int PIN_BTN  = 0;    // unused; boot-strap pin: low at reset enters the bootloader
static constexpr uint8_t LED_PIN = 16;  // plain LED, active LOW

using RadioChip = SX1276;

inline Module* newRadioModule() { return new Module(PIN_CS, PIN_DIO0, PIN_RST, PIN_DIO1); }
inline void spiBegin() { SPI.begin(); }
inline void readChipId(uint8_t id[6]) { WiFi.macAddress(id); }

inline int16_t radioBegin(RadioChip& r) {
  return r.begin(BAND_FREQ_MHZ, BW_KHZ, SF, CR, SYNC_WORD, TX_POWER_DBM, PREAMBLE_LEN);
}
inline void radioApplyBoard(RadioChip&) {}
// Antenna is on PA_BOOST. The vendor config's power bytes (RegPaConfig bit7 clear) suggest
// RFO, but forcing RFO measured ~70 dB weaker, so don't "fix" this to forceRfo=true.
inline int16_t radioSetTxPower(RadioChip& r) { return r.setOutputPower(TX_POWER_DBM, false); }

inline void ledInit()  { pinMode(LED_PIN, OUTPUT); digitalWrite(LED_PIN, HIGH); }
inline void ledOff()   { digitalWrite(LED_PIN, HIGH); }
inline void ledTx()    { digitalWrite(LED_PIN, LOW); }
inline void ledRx()    { digitalWrite(LED_PIN, LOW); }

#else
#error "Unsupported target: need ESP32 or ESP8266"
#endif

// 9-byte wire packet: magic(2) + chip id(6) + tx power(1). Radio CRC covers integrity.
static constexpr uint8_t MAGIC[2] = {0xB2, 0x7D};
static constexpr uint8_t PKT_LEN  = 9;
