/*
 * RA-02 serial chat test - Arduino Uno R3
 * Common radio profile: 433.000 MHz, BW 125 kHz, SF9, CR 4/5,
 * preamble 8, sync word 0x12, CRC enabled, TX power 10 dBm.
 * Serial Monitor: 115200 baud, line ending Newline.
 */
#include <Arduino.h>
#include <SPI.h>
#include <LoRa.h>

static const long LORA_FREQUENCY = 433E6;
static const long LORA_BANDWIDTH = 125E3;
static const int LORA_SPREADING_FACTOR = 9;
static const int LORA_CODING_RATE_DENOMINATOR = 5;
static const int LORA_PREAMBLE_LENGTH = 8;
static const uint8_t LORA_SYNC_WORD = 0x12;
static const int LORA_TX_POWER_DBM = 10;
static const size_t MAX_MESSAGE = 200;

static const uint8_t LORA_SS = 10;
static const uint8_t LORA_RESET = 9;
static const uint8_t LORA_DIO0 = 2;

static char inputLine[MAX_MESSAGE + 1];
static size_t inputLength = 0;
static bool discardUntilNewline = false;

static void printRadioProfile() {
  Serial.println(F("Radio profile:"));
  Serial.println(F("  Frequency: 433.000 MHz"));
  Serial.println(F("  Bandwidth: 125 kHz"));
  Serial.println(F("  Spreading factor: SF9"));
  Serial.println(F("  Coding rate: 4/5"));
  Serial.println(F("  Preamble: 8 symbols"));
  Serial.println(F("  Sync word: 0x12; CRC: enabled"));
  Serial.println(F("  TX power: 10 dBm"));
}

static void sendLine() {
  if (discardUntilNewline) {
    discardUntilNewline = false;
    inputLength = 0;
    return;
  }
  if (inputLength == 0)
    return;

  if (!LoRa.beginPacket()) {
    Serial.println(F("TX ERROR: could not start packet."));
    inputLength = 0;
    return;
  }

  const size_t written = LoRa.write(
      reinterpret_cast<const uint8_t*>(inputLine), inputLength);
  const int txResult = LoRa.endPacket();

  if (txResult == 1 && written == inputLength) {
    Serial.print(F("TX OK: "));
    Serial.print(inputLength);
    Serial.print(F(" bytes | "));
    Serial.write(reinterpret_cast<const uint8_t*>(inputLine), inputLength);
    Serial.println();
  } else {
    Serial.print(F("TX FAILED | bytes queued: "));
    Serial.print(written);
    Serial.print(F(" | endPacket result: "));
    Serial.println(txResult);
  }

  inputLength = 0;
}

static void receivePacket() {
  const int packetSize = LoRa.parsePacket();
  if (packetSize <= 0)
    return;

  Serial.println();
  Serial.println(F("--- LORA PACKET ---"));
  Serial.print(F("Reported length: "));
  Serial.println(packetSize);
  Serial.print(F("RSSI: "));
  Serial.print(LoRa.packetRssi());
  Serial.print(F(" dBm | SNR: "));
  Serial.print(LoRa.packetSnr());
  Serial.println(F(" dB"));
  Serial.print(F("Payload: "));

  int bytesRead = 0;
  while (LoRa.available() > 0) {
    const uint8_t value = static_cast<uint8_t>(LoRa.read());
    ++bytesRead;
    if (value >= 32 && value <= 126) {
      Serial.write(value);
    } else {
      Serial.print(F("<0x"));
      if (value < 16)
        Serial.print('0');
      Serial.print(value, HEX);
      Serial.print('>');
    }
  }
  Serial.println();
  Serial.print(F("Bytes read: "));
  Serial.println(bytesRead);
  if (bytesRead != packetSize)
    Serial.println(F("WARNING: reported packet length differs from bytes read."));
  Serial.println(F("-------------------"));
}

void setup() {
  Serial.begin(115200);
  while (!Serial) { }
  LoRa.setPins(LORA_SS, LORA_RESET, LORA_DIO0);

  Serial.println();
  Serial.println(F("RA-02 serial chat - Arduino Uno R3"));
  Serial.println(F("Starting radio..."));

  if (!LoRa.begin(LORA_FREQUENCY)) {
    Serial.println(F("ERROR: RA-02 not detected. Check 3.3 V power, common GND, CS/RESET and SPI wiring."));
    while (true)
      delay(1000);
  }

  LoRa.setSignalBandwidth(LORA_BANDWIDTH);
  LoRa.setSpreadingFactor(LORA_SPREADING_FACTOR);
  LoRa.setCodingRate4(LORA_CODING_RATE_DENOMINATOR);
  LoRa.setPreambleLength(LORA_PREAMBLE_LENGTH);
  LoRa.setSyncWord(LORA_SYNC_WORD);
  LoRa.enableCrc();
  LoRa.setTxPower(LORA_TX_POWER_DBM);

  Serial.println(F("RADIO READY"));
  printRadioProfile();
  Serial.println(F("Type a message and press Enter (Serial Monitor: Newline)."));
  Serial.println(F("TX OK confirms local transmit completion, not remote delivery."));
}

void loop() {
  while (Serial.available() > 0) {
    const char c = static_cast<char>(Serial.read());
    if (c == '\r' || c == '\n') {
      sendLine();
    } else if (!discardUntilNewline) {
      if (inputLength < MAX_MESSAGE) {
        inputLine[inputLength++] = c;
      } else {
        Serial.println(F("ERROR: message exceeds 200 bytes; discarding until newline."));
        inputLength = 0;
        discardUntilNewline = true;
      }
    }
  }

  receivePacket();
}
