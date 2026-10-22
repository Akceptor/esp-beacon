// ESP32-C3 + LR1121 (or ESP8266 + SX1276) beacon receiver — flash to a second board to check the beacon is
// audible. Prints one CSV line per valid packet: chipId,rssi_dBm,snr_dB,txPower_dBm.
// See docs/RECEIVER.md.
#include <Arduino.h>
#include <SPI.h>
#include <RadioLib.h>
#include "board.h"

RadioChip radio = newRadioModule();

static volatile bool rxFlag = false;
static bool     radioReady = false;
static uint32_t okCount = 0;
static uint32_t badCount = 0;
static uint32_t lastStatus = 0;

static void IRAM_ATTR onPacket() {
  rxFlag = true;
}

static bool initRadio() {
  int16_t err = radioBegin(radio);
  if (err != RADIOLIB_ERR_NONE) {
    Serial.print(F("WARN: radio.begin failed, code "));
    Serial.println(err);
    return false;
  }
  radioApplyBoard(radio);
  radio.setPacketReceivedAction(onPacket);
  err = radio.startReceive();
  if (err != RADIOLIB_ERR_NONE) {
    Serial.print(F("WARN: startReceive failed, code "));
    Serial.println(err);
    return false;
  }
  return true;
}

static void printHex(const uint8_t* p, size_t n) {
  for (size_t i = 0; i < n; i++) {
    if (p[i] < 0x10) Serial.print('0');
    Serial.print(p[i], HEX);
  }
}

void setup() {
  Serial.begin(115200);
  delay(300);
  ledInit();
  spiBegin();

  Serial.print(F("Receiver @ ")); Serial.print(BAND_FREQ_MHZ, 1);
  Serial.print(F(" MHz SF")); Serial.println(SF);
  Serial.println(F("chipId,rssi_dBm,snr_dB,txPower_dBm"));

  radioReady = initRadio();
}

void loop() {
  if (!radioReady) {
    delay(2000);
    radioReady = initRadio();
    return;
  }

  if (rxFlag) {
    rxFlag = false;
    uint8_t buf[32];
    size_t len = radio.getPacketLength();
    if (len > sizeof(buf)) len = sizeof(buf);
    int16_t err = radio.readData(buf, len);
    float rssi = radio.getRSSI();
    float snr = radio.getSNR();

    if (err == RADIOLIB_ERR_NONE && len == PKT_LEN && buf[0] == MAGIC[0] && buf[1] == MAGIC[1]) {
      okCount++;
      ledRx();
      printHex(buf + 2, 6);
      Serial.print(',');  Serial.print(rssi, 1);
      Serial.print(',');  Serial.print(snr, 1);
      Serial.print(',');  Serial.println((int8_t)buf[8]);
      ledOff();
    } else {
      badCount++;
      Serial.print(F("# rejected: err ")); Serial.print(err);
      Serial.print(F(" len ")); Serial.println(len);
    }
    radio.startReceive();
  }

  uint32_t now = millis();
  if (now - lastStatus >= 10000) {
    lastStatus = now;
    Serial.print(F("# listening, ok "));  Serial.print(okCount);
    Serial.print(F(" rejected "));        Serial.println(badCount);
  }
}
