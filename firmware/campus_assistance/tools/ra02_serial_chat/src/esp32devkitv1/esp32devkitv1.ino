/*
 * RA-02 serial chat test - ESP32 DevKit V1
 * Hardware: ESP32 + AI-Thinker RA-02 (SX1278) only.
 * Serial Monitor: 115200 baud, newline line ending.
 *
 * The GPIO mapping is explicitly assigned below; it does not depend on
 * the board's default SPI pin mapping.
 */
#include <Arduino.h>
#include <SPI.h>
#include <LoRa.h>

static constexpr long LORA_FREQUENCY = 433E6;
static constexpr int LORA_SCK = 18;
static constexpr int LORA_MISO = 19;
static constexpr int LORA_MOSI = 23;
static constexpr int LORA_SS = 17;
static constexpr int LORA_RESET = 14;
static constexpr int LORA_DIO0 = 26;
static constexpr size_t MAX_MESSAGE = 200;

static char inputLine[MAX_MESSAGE + 1];
static size_t inputLength = 0;
static bool discardUntilNewline = false;

static void sendLine() {
  if (discardUntilNewline) {
    discardUntilNewline = false;
    inputLength = 0;
    return;
  }
  if (inputLength == 0)
    return;

  LoRa.beginPacket();
  LoRa.write(reinterpret_cast<const uint8_t*>(inputLine), inputLength);
  const int result = LoRa.endPacket();

  Serial.print(result == 1 ? "TX: " : "TX FAILED: ");
  Serial.write(reinterpret_cast<const uint8_t*>(inputLine), inputLength);
  Serial.println();
  inputLength = 0;
}

void setup() {
  Serial.begin(115200);
  delay(300);

  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_SS);
  LoRa.setPins(LORA_SS, LORA_RESET, LORA_DIO0);

  Serial.println();
  Serial.println("RA-02 serial chat - ESP32 DevKit V1");
  Serial.println("Starting radio...");

  if (!LoRa.begin(LORA_FREQUENCY)) {
    Serial.println(
        "ERROR: RA-02 not detected. Check regulated 3.3 V power and SPI wiring.");
    while (true)
      delay(1000);
  }

  LoRa.setSpreadingFactor(7);
  LoRa.setSignalBandwidth(125E3);
  LoRa.setCodingRate4(5);
  LoRa.setSyncWord(0x12);
  LoRa.enableCrc();
  LoRa.setTxPower(10);

  Serial.println("Radio ready at 433 MHz. Type a message and press Enter.");
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
        Serial.println("ERROR: message exceeds 200 bytes; discarding this line.");
        inputLength = 0;
        discardUntilNewline = true;
      }
    }
  }

  const int packetSize = LoRa.parsePacket();
  if (packetSize > 0) {
    Serial.print("RX: ");
    while (LoRa.available() > 0)
      Serial.write(LoRa.read());
    Serial.println();
  }
}
