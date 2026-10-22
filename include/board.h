// Radio config, pin map and wire format shared by the beacon (src/main.cpp) and the
// receiver (src/receiver.cpp). Keep in sync with platformio.ini + docs/RECEIVER.md.
#pragma once
#include <Arduino.h>
#include <RadioLib.h>

static constexpr float    BAND_FREQ_MHZ = 868.0f;
static constexpr float    BW_KHZ        = 125.0f;
static constexpr uint8_t  SF            = 12;       // spreading factor
static constexpr uint8_t  CR            = 5;        // CR denominator: 5=4/5, 6=4/6, 7=4/7, 8=4/8
static constexpr uint8_t  SYNC_WORD     = 0x34;     // private, not LoRaWAN's 0x12
static constexpr int8_t   TX_POWER_DBM  = 13;
static constexpr uint16_t PREAMBLE_LEN  = 8;
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

// 9-byte wire packet: magic(2) + chip id(6) + tx power(1). Radio CRC covers integrity.
static constexpr uint8_t MAGIC[2] = {0xB2, 0x7D};
static constexpr uint8_t PKT_LEN  = 9;
