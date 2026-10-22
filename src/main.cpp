// ESP32-C3 + LR1121 (or ESP8266 + SX1276) beacon — indoor lab use.
// Every 500 ms it transmits a 9-byte packet carrying its chip ID (the ESP32's
// Wi-Fi base MAC) and its TX power, so a remote receiver can distinguish beacons and
// later estimate distance. See docs/RECEIVER.md.
#include <Arduino.h>
#include <SPI.h>
#include <RadioLib.h>
#include "board.h"

static constexpr uint16_t PERIOD_MS = 500;   // 2 Hz

RadioChip radio = newRadioModule();

static uint8_t chipId[6] = {0, 0, 0, 0, 0, 0};
static bool    radioReady = false;
static uint32_t nextTx = 0;
static uint32_t txCount = 0;

static void initRadio() {
  int16_t err = radioBegin(radio);
  if (err == RADIOLIB_ERR_NONE) {
    radioApplyBoard(radio);
    int16_t pwr = radioSetTxPower(radio);
    if (pwr != RADIOLIB_ERR_NONE) {
      Serial.print(F("WARN: setOutputPower failed, code "));
      Serial.println(pwr);
    }
    radioReady = true;
  } else {
    Serial.print(F("WARN: radio.begin failed, code "));
    Serial.println(err);
  }
}

void setup() {
  Serial.begin(115200);
  delay(300);
  ledInit();

  spiBegin();
  readChipId(chipId);

  // Boot banner: chip id + key radio params.
  Serial.print(F("Beacon chipId "));
  for (int i = 0; i < 6; i++) {
    if (chipId[i] < 0x10) Serial.print('0');
    Serial.print(chipId[i], HEX);
  }
  Serial.print(F(" @ ")); Serial.print(BAND_FREQ_MHZ, 1);
  Serial.print(F(" MHz SF")); Serial.print(SF);
  Serial.print(F(" P")); Serial.print(TX_POWER_DBM);
  Serial.println(F(" dBm, 2 Hz"));

  initRadio();
  nextTx = millis();
}

void loop() {
  if (!radioReady) {
    // Keep retrying init every couple seconds instead of crashing.
    static uint32_t lastTry = 0;
    if (millis() - lastTry > 2000) { lastTry = millis(); initRadio(); }
    delay(200);
    return;
  }

  uint32_t now = millis();
  if (now < nextTx) {
    // Idle until the next 500 ms slot. The LR1121 sits in standby between packets; the MCU
    // would save more by light-sleeping, but a deep sleep re-inits the radio and breaks the
    // 500 ms cadence. Real battery savings come from gating the radio power, a later step.
    delay(PERIOD_MS - (nextTx - now));
    return;
  }
  // Anchor to the schedule so sleep+tx time doesn't drift the 2 Hz cadence.
  nextTx = now + PERIOD_MS;

  uint8_t pkt[PKT_LEN];
  pkt[0] = MAGIC[0];
  pkt[1] = MAGIC[1];
  for (int i = 0; i < 6; i++) pkt[2 + i] = chipId[i];
  pkt[8] = (uint8_t)TX_POWER_DBM;

  ledTx();
  int16_t r = radio.transmit(pkt, PKT_LEN);   // blocking; RADIOLIB_ERR_NONE on success
  ledOff();
  txCount++;

  // ~1 Hz heartbeat so a headless serial read proves liveness + radio health.
  if (txCount % 2 == 0) {
    Serial.print(F("beat "));
    Serial.print(txCount);
    if (r != RADIOLIB_ERR_NONE) {
      Serial.print(F(" txerr "));
      Serial.print(r);
    }
    Serial.println();
  }
}
