/*
 * RA-02 serial chat test - ESP32-WROOM-32 DevKit V1
 * Hardware: ESP32 DevKit V1 + AI-Thinker RA-02 (SX1278) only.
 * Serial Monitor: 115200 baud, newline line ending.
 */
#include <Arduino.h>
#include <SPI.h>
#include <LoRa.h>

static const long LORA_FREQUENCY = 433E6;
static const int LORA_SCK = 18;
static const int LORA_MISO = 19;
static const int LORA_MOSI = 23;
static const int LORA_SS = 17;
static const int LORA_RESET = 14;
static const int LORA_DIO0 = 26;
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
  delay(300);
  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_SS);
  LoRa.setPins(LORA_SS, LORA_RESET, LORA_DIO0);
  Serial.println("\nRA-02 serial chat - ESP32 DevKit V1");
  Serial.println("Starting radio...");
  if (!LoRa.begin(LORA_FREQUENCY)) {
    Serial.println("ERROR: RA-02 not detected. Check 3.3 V power and SPI wiring.");
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
  while (Serial.available()) {
    const char c = static_cast<char>(Serial.read());
    if (c == '\r' || c == '\n') {
      sendLine();
    } else if (inputLength < MAX_MESSAGE) {
      inputLine[inputLength++] = c;
    } else {
      Serial.println("ERROR: message too long (max 200 bytes); line discarded.");
      inputLength = 0;
    }
  }

  const int packetSize = LoRa.parsePacket();
  if (packetSize > 0) {
    Serial.print("RX: ");
    while (LoRa.available())
      Serial.write(LoRa.read());
    Serial.println();
  }
}
