/*
 * RA-02 serial chat test - ESP32 DevKit V1 (legacy profile)
 * Common radio profile: 433.000 MHz, BW 125 kHz, SF9, CR 4/5,
 * preamble 8, sync word 0x12, CRC enabled, TX power 10 dBm.
 * Serial Monitor: 115200 baud, line ending Newline.
 */
#include <Arduino.h>
#include <SPI.h>
#include <LoRa.h>

static constexpr long LORA_FREQUENCY = 433E6;
static constexpr long LORA_BANDWIDTH = 125E3;
static constexpr int LORA_SPREADING_FACTOR = 9;
static constexpr int LORA_CODING_RATE_DENOMINATOR = 5;
static constexpr int LORA_PREAMBLE_LENGTH = 8;
static constexpr uint8_t LORA_SYNC_WORD = 0x12;
static constexpr int LORA_TX_POWER_DBM = 10;
static constexpr size_t MAX_MESSAGE = 200;

static constexpr int LORA_SCK = 18;
static constexpr int LORA_MISO = 19;
static constexpr int LORA_MOSI = 23;
static constexpr int LORA_SS = 17;
static constexpr int LORA_RESET = 14;
static constexpr int LORA_DIO0 = 26;

static char inputLine[MAX_MESSAGE + 1];
static size_t inputLength = 0;
static bool discardUntilNewline = false;

static void printRadioProfile() {
  Serial.println("Radio profile:");
  Serial.println("  Frequency: 433.000 MHz");
  Serial.println("  Bandwidth: 125 kHz");
  Serial.println("  Spreading factor: SF9");
  Serial.println("  Coding rate: 4/5");
  Serial.println("  Preamble: 8 symbols");
  Serial.println("  Sync word: 0x12; CRC: enabled");
  Serial.println("  TX power: 10 dBm");
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
    Serial.println("TX ERROR: could not start packet.");
    inputLength = 0;
    return;
  }

  const size_t written = LoRa.write(
      reinterpret_cast<const uint8_t*>(inputLine), inputLength);
  const int txResult = LoRa.endPacket();

  if (txResult == 1 && written == inputLength) {
    Serial.print("TX OK: ");
    Serial.print(inputLength);
    Serial.print(" bytes | ");
    Serial.write(reinterpret_cast<const uint8_t*>(inputLine), inputLength);
    Serial.println();
  } else {
    Serial.print("TX FAILED | bytes queued: ");
    Serial.print(written);
    Serial.print(" | endPacket result: ");
    Serial.println(txResult);
  }

  inputLength = 0;
}

static void receivePacket() {
  const int packetSize = LoRa.parsePacket();
  if (packetSize <= 0)
    return;

  Serial.println();
  Serial.println("--- LORA PACKET ---");
  Serial.print("Reported length: ");
  Serial.println(packetSize);
  Serial.print("RSSI: ");
  Serial.print(LoRa.packetRssi());
  Serial.print(" dBm | SNR: ");
  Serial.print(LoRa.packetSnr());
  Serial.println(" dB");
  Serial.print("Payload: ");

  int bytesRead = 0;
  while (LoRa.available() > 0) {
    const uint8_t value = static_cast<uint8_t>(LoRa.read());
    ++bytesRead;
    if (value >= 32 && value <= 126) {
      Serial.write(value);
    } else {
      Serial.print("<0x");
      if (value < 16)
        Serial.print('0');
      Serial.print(value, HEX);
      Serial.print('>');
    }
  }
  Serial.println();
  Serial.print("Bytes read: ");
  Serial.println(bytesRead);
  if (bytesRead != packetSize)
    Serial.println("WARNING: reported packet length differs from bytes read.");
  Serial.println("-------------------");
}

void setup() {
  Serial.begin(115200);
  delay(300);
  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_SS);
  LoRa.setPins(LORA_SS, LORA_RESET, LORA_DIO0);

  Serial.println();
  Serial.println("RA-02 serial chat - ESP32 DevKit V1 (legacy profile)");
  Serial.println("Starting radio...");

  if (!LoRa.begin(LORA_FREQUENCY)) {
    Serial.println("ERROR: RA-02 not detected. Check 3.3 V power, common GND, CS/RESET and SPI wiring.");
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

  Serial.println("RADIO READY");
  printRadioProfile();
  Serial.println("Type a message and press Enter (Serial Monitor: Newline).");
  Serial.println("TX OK confirms local transmit completion, not remote delivery.");
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
        Serial.println("ERROR: message exceeds 200 bytes; discarding until newline.");
        inputLength = 0;
        discardUntilNewline = true;
      }
    }
  }

  receivePacket();
}
