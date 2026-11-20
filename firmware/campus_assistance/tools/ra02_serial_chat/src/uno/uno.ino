/*
 * RA-02 serial chat test - Arduino Uno R3
 * Hardware: Arduino Uno R3 + AI-Thinker RA-02 (SX1278) only.
 * Serial Monitor: 115200 baud, newline line ending.
 *
 * IMPORTANT: RA-02 is 3.3 V ONLY. Use a proper 3.3 V regulator and
 * 5 V-to-3.3 V level shifters for SCK, MOSI, NSS and RESET.
 */
#include <Arduino.h>
#include <SPI.h>
#include <LoRa.h>

static const long LORA_FREQUENCY = 433E6;
static const uint8_t LORA_SS = 10;
static const uint8_t LORA_RESET = 9;
static const uint8_t LORA_DIO0 = 2;
static const size_t MAX_MESSAGE = 200;
static char inputLine[MAX_MESSAGE + 1];
static size_t inputLength = 0;

static void sendLine() {
  if (inputLength == 0)
    return;
  LoRa.beginPacket();
  LoRa.write(reinterpret_cast<const uint8_t*>(inputLine), inputLength);
  const int result = LoRa.endPacket();
  Serial.print(result == 1 ? "TX: " : "TX FAILED: ");
  Serial.println(inputLine);
  inputLength = 0;
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {
  }
  LoRa.setPins(LORA_SS, LORA_RESET, LORA_DIO0);
  Serial.println(F("\nRA-02 serial chat - Arduino Uno R3"));
  Serial.println(F("Starting radio..."));
  if (!LoRa.begin(LORA_FREQUENCY)) {
    Serial.println(
        F("ERROR: RA-02 not detected. Check power, wiring and level shifters."));
    while (true)
      delay(1000);
  }
  LoRa.setSpreadingFactor(7);
  LoRa.setSignalBandwidth(125E3);
  LoRa.setCodingRate4(5);
  LoRa.setSyncWord(0x12);
  LoRa.enableCrc();
  LoRa.setTxPower(10);
  Serial.println(F("Radio ready at 433 MHz. Type a message and press Enter."));
}

void loop() {
  while (Serial.available()) {
    const char c = static_cast<char>(Serial.read());
    if (c == '\r' || c == '\n') {
      sendLine();
    } else if (inputLength < MAX_MESSAGE) {
      inputLine[inputLength++] = c;
    } else {
      Serial.println(F("ERROR: message too long (max 200 bytes); line discarded."));
      inputLength = 0;
    }
  }

  const int packetSize = LoRa.parsePacket();
  if (packetSize > 0) {
    Serial.print(F("RX: "));
    while (LoRa.available())
      Serial.write(LoRa.read());
    Serial.println();
  }
}
