/*
 * LoRa Project - Firmware 1: Group Messaging
 * Target: Arduino Uno + SX1278-compatible module + Sandeep Mistry LoRa library
 *
 * Bench prototype only. Configure RF frequency to match the exact module and
 * local regulations. See docs/HARDWARE_AND_SAFETY.md.
 */

#include <SPI.h>
#include <LoRa.h>
#include <EEPROM.h>
#include <stddef.h>
#include <string.h>

#define USER_NAME                 "DEVICE_USER"
#define NETWORK_ID               1001
#define LORA_FREQUENCY_HZ        433E6  // BENCH DEFAULT ONLY: must match the RF module
#define LORA_NSS_PIN             10
#define LORA_RESET_PIN            9
#define LORA_DIO0_PIN             2

#define MAX_MESSAGE_CHARS       128
#define MAX_RETRIES               3
#define ACK_TIMEOUT_MS         1800UL
#define RETRY_BACKOFF_MS        350UL
#define HELLO_INTERVAL_MS     20000UL
#define PEER_TIMEOUT_MS       90000UL
#define MAX_PEERS                  8
#define EEPROM_MAGIC          0x4C52

enum PacketType : uint8_t { PKT_HELLO = 1, PKT_TEXT = 2, PKT_ACK = 3 };

struct __attribute__((packed)) RadioPacket {
  uint16_t magic;
  uint16_t networkId;
  uint8_t type;
  uint16_t senderId;
  uint16_t messageId;
  uint16_t ackForSenderId;
  char userName[16];
  char text[MAX_MESSAGE_CHARS + 1];
};

struct Peer {
  uint16_t id;
  char name[16];
  unsigned long lastSeen;
  bool used;
};

RadioPacket txPacket;
RadioPacket rxPacket;
Peer peers[MAX_PEERS];
uint16_t deviceId = 0;
uint16_t nextMessageId = 1;
unsigned long lastHelloAt = 0;
struct SeenMessage { uint16_t senderId; uint16_t messageId; bool used; };
#define SEEN_CACHE_SIZE 12
SeenMessage seenMessages[SEEN_CACHE_SIZE];
uint8_t seenNext = 0;

static void copyText(char *dst, size_t capacity, const char *src) {
  if (capacity == 0) return;
  strncpy(dst, src, capacity - 1);
  dst[capacity - 1] = '\0';
}

uint16_t makeCandidateId() {
  randomSeed(analogRead(A0) ^ micros() ^ (millis() << 3));
  uint16_t value = (uint16_t)random(1, 65535);
  return value == 0 ? 1 : value;
}

void loadOrCreateDeviceId() {
  uint16_t storedMagic = 0;
  uint16_t storedId = 0;
  EEPROM.get(0, storedMagic);
  EEPROM.get(2, storedId);
  if (storedMagic == EEPROM_MAGIC && storedId != 0 && storedId != 0xFFFF) {
    deviceId = storedId;
    return;
  }
  deviceId = makeCandidateId();
  EEPROM.put(0, EEPROM_MAGIC);
  EEPROM.put(2, deviceId);
}

void initPacket(RadioPacket &p, PacketType type) {
  memset(&p, 0, sizeof(p));
  p.magic = 0x4C52;
  p.networkId = NETWORK_ID;
  p.type = (uint8_t)type;
  p.senderId = deviceId;
  copyText(p.userName, sizeof(p.userName), USER_NAME);
}

void transmitPacket(const RadioPacket &p) {
  LoRa.beginPacket();
  LoRa.write((const uint8_t *)&p, sizeof(p));
  LoRa.endPacket();
}

int findPeer(uint16_t id) {
  for (uint8_t i = 0; i < MAX_PEERS; ++i)
    if (peers[i].used && peers[i].id == id) return i;
  return -1;
}

void updatePeer(uint16_t id, const char *name) {
  int slot = findPeer(id);
  if (slot < 0) {
    for (uint8_t i = 0; i < MAX_PEERS; ++i) {
      if (!peers[i].used || millis() - peers[i].lastSeen > PEER_TIMEOUT_MS) {
        slot = i;
        break;
      }
    }
  }
  if (slot < 0) return;
  peers[slot].id = id;
  copyText(peers[slot].name, sizeof(peers[slot].name), name);
  peers[slot].lastSeen = millis();
  peers[slot].used = true;
}

bool wasMessageSeen(uint16_t senderId, uint16_t messageId) {
  for (uint8_t i=0; i<SEEN_CACHE_SIZE; ++i)
    if (seenMessages[i].used && seenMessages[i].senderId==senderId &&
        seenMessages[i].messageId==messageId) return true;
  return false;
}

void rememberMessage(uint16_t senderId, uint16_t messageId) {
  SeenMessage &entry=seenMessages[seenNext];
  entry.senderId=senderId; entry.messageId=messageId; entry.used=true;
  seenNext=(uint8_t)((seenNext+1)%SEEN_CACHE_SIZE);
}

void sendAck(uint16_t messageId, uint16_t originalSenderId) {
  RadioPacket ack;
  initPacket(ack, PKT_ACK);
  ack.messageId = messageId;
  ack.ackForSenderId = originalSenderId;
  // Randomized response delay reduces simultaneous ACK collisions.
  delay((unsigned long)random(40, 420));
  transmitPacket(ack);
}

bool readPacket(RadioPacket &p) {
  int packetSize = LoRa.parsePacket();
  if (packetSize != (int)sizeof(RadioPacket)) {
    while (LoRa.available()) LoRa.read();
    return false;
  }
  uint8_t *dst = (uint8_t *)&p;
  size_t i = 0;
  while (LoRa.available() && i < sizeof(RadioPacket)) dst[i++] = (uint8_t)LoRa.read();
  return i == sizeof(RadioPacket) && p.magic == 0x4C52 && p.networkId == NETWORK_ID;
}

void serviceIncoming(bool acknowledgeText) {
  if (!readPacket(rxPacket)) return;
  if (rxPacket.senderId == deviceId) return;

  if (rxPacket.type == PKT_HELLO) {
    updatePeer(rxPacket.senderId, rxPacket.userName);
    return;
  }

  if (rxPacket.type == PKT_TEXT) {
    updatePeer(rxPacket.senderId, rxPacket.userName);
    bool duplicate = wasMessageSeen(rxPacket.senderId, rxPacket.messageId);
    if (!duplicate) {
      rememberMessage(rxPacket.senderId, rxPacket.messageId);
      Serial.print("[MESSAGE] ");
      Serial.print(rxPacket.userName);
      Serial.print(" (");
      Serial.print(rxPacket.senderId);
      Serial.print("): ");
      Serial.println(rxPacket.text);
    }
    if (acknowledgeText) sendAck(rxPacket.messageId, rxPacket.senderId);
    return;
  }

  if (rxPacket.type == PKT_ACK) {
    // ACK packets are consumed by the sender's collection loop.
  }
}

void sendHello() {
  RadioPacket hello;
  initPacket(hello, PKT_HELLO);
  transmitPacket(hello);
  lastHelloAt = millis();
}

void collectAcks(uint16_t messageId, uint16_t *ackIds, uint8_t &ackCount) {
  unsigned long started = millis();
  while (millis() - started < ACK_TIMEOUT_MS) {
    int packetSize = LoRa.parsePacket();
    if (packetSize == (int)sizeof(RadioPacket)) {
      uint8_t *dst = (uint8_t *)&rxPacket;
      size_t i = 0;
      while (LoRa.available() && i < sizeof(RadioPacket)) dst[i++] = (uint8_t)LoRa.read();
      if (i == sizeof(RadioPacket) && rxPacket.magic == 0x4C52 &&
          rxPacket.networkId == NETWORK_ID && rxPacket.type == PKT_ACK &&
          rxPacket.messageId == messageId && rxPacket.ackForSenderId == deviceId &&
          rxPacket.senderId != deviceId) {
        bool known = false;
        for (uint8_t j = 0; j < ackCount; ++j) if (ackIds[j] == rxPacket.senderId) known = true;
        if (!known && ackCount < MAX_PEERS) {
          ackIds[ackCount++] = rxPacket.senderId;
          updatePeer(rxPacket.senderId, rxPacket.userName);
          Serial.print("[ACK] Device ");
          Serial.println(rxPacket.senderId);
        }
      }
    } else {
      while (LoRa.available()) LoRa.read();
    }
  }
}

uint8_t activePeerCount() {
  uint8_t count = 0;
  for (uint8_t i = 0; i < MAX_PEERS; ++i) {
    if (peers[i].used && millis() - peers[i].lastSeen <= PEER_TIMEOUT_MS) count++;
  }
  return count;
}

void sendText(const char *text) {
  if (!text || text[0] == '\0') {
    Serial.println("[ERROR] Empty messages are not sent.");
    return;
  }
  if (strlen(text) > MAX_MESSAGE_CHARS) {
    Serial.println("[ERROR] Message exceeds 128 bytes.");
    return;
  }

  uint16_t messageId = nextMessageId++;
  uint16_t ackIds[MAX_PEERS];
  uint8_t ackCount = 0;

  RadioPacket message;
  initPacket(message, PKT_TEXT);
  message.messageId = messageId;
  copyText(message.text, sizeof(message.text), text);

  Serial.print("[TX] Message ID ");
  Serial.println(messageId);
  for (uint8_t attempt = 0; attempt <= MAX_RETRIES; ++attempt) {
    transmitPacket(message);
    collectAcks(messageId, ackIds, ackCount);
    uint8_t expectedPeers = activePeerCount();
    if ((expectedPeers > 0 && ackCount >= expectedPeers) ||
        (expectedPeers == 0 && ackCount > 0)) break;
    if (attempt < MAX_RETRIES) {
      Serial.print("[RETRY] Attempt ");
      Serial.println(attempt + 1);
      delay((unsigned long)random(100, RETRY_BACKOFF_MS + 1));
    }
  }

  Serial.print("[SUMMARY] Acknowledgements: ");
  Serial.println(ackCount);
  if (ackCount == 0) Serial.println("[STATUS] No delivery acknowledgement received.");
  else {
    Serial.print("[STATUS] Confirmed device IDs: ");
    for (uint8_t i = 0; i < ackCount; ++i) {
      Serial.print(ackIds[i]);
      if (i + 1 < ackCount) Serial.print(", ");
    }
    Serial.println();
  }
}

void setup() {
  Serial.begin(9600);
  while (!Serial) { }
  loadOrCreateDeviceId();
  memset(peers, 0, sizeof(peers));
  memset(seenMessages, 0, sizeof(seenMessages));
  Serial.println(F("LoRa Group Messaging - prototype"));
  Serial.print(F("User: ")); Serial.println(USER_NAME);
  Serial.print(F("Device ID: ")); Serial.println(deviceId);

  LoRa.setPins(LORA_NSS_PIN, LORA_RESET_PIN, LORA_DIO0_PIN);
  if (!LoRa.begin(LORA_FREQUENCY_HZ)) {
    Serial.println(F("[FATAL] LoRa init failed. Check power, wiring and RF module."));
    while (true) delay(1000);
  }
  LoRa.enableCrc();
  sendHello();
  Serial.println(F("Type a message (max 128 bytes), then press Enter."));
}

void loop() {
  if (millis() - lastHelloAt >= HELLO_INTERVAL_MS) sendHello();

  // Service a single incoming packet when not waiting for ACKs.
  serviceIncoming(true);

  static char input[MAX_MESSAGE_CHARS + 2];
  static uint8_t inputLength = 0;
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\r') continue;
    if (c == '\n') {
      input[inputLength] = '\0';
      sendText(input);
      inputLength = 0;
    } else if (inputLength < sizeof(input) - 1) {
      input[inputLength++] = c;
    } else {
      inputLength = 0;
      Serial.println(F("[ERROR] Input too long; line discarded."));
    }
  }
}
