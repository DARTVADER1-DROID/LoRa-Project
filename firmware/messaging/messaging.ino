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

#define MAX_MESSAGE_CHARS             128
#define MAX_RETRIES                     3
#define ACK_TIMEOUT_MS               1800UL
#define RETRY_BACKOFF_BASE_MS         150UL
#define RETRY_BACKOFF_MAX_MS         1200UL
#define MAX_TRANSACTION_LIFETIME_MS 10000UL
#define ACK_SLOT_MS                   160UL
#define ACK_SLOT_JITTER_MS             35UL
#define HELLO_INTERVAL_MS           20000UL
#define PEER_TIMEOUT_MS             90000UL
#define MAX_PEERS                        8
#define ACK_QUEUE_SIZE                   4
#define SEEN_CACHE_SIZE                 12
#define EEPROM_MAGIC                0x4C52
#define PROTOCOL_VERSION                 1

enum PacketType : uint8_t { PKT_HELLO = 1, PKT_TEXT = 2, PKT_ACK = 3 };

struct __attribute__((packed)) RadioPacket {
  uint16_t magic;
  uint8_t version;
  uint16_t networkId;
  uint8_t type;
  uint16_t senderId;
  uint32_t senderBootId;
  uint32_t messageId;
  uint16_t ackForSenderId;
  uint32_t ackForBootId;
  uint32_t ackForMessageId;
  uint8_t payloadLength;
  char userName[16];
  char text[MAX_MESSAGE_CHARS + 1];
};

struct PendingAck { uint32_t messageId; uint16_t targetId; uint32_t targetBootId; unsigned long dueAt; bool used; };
struct Peer {
  uint16_t id;
  uint32_t bootId;
  char name[16];
  unsigned long lastSeen;
  bool used;
};

RadioPacket rxPacket; // Single reusable receive buffer; keep AVR SRAM use bounded.
Peer peers[MAX_PEERS];
uint16_t deviceId = 0;
uint32_t nextMessageId = 1;
uint32_t bootId = 0;
PendingAck ackQueue[ACK_QUEUE_SIZE];
uint8_t ackQueueNext = 0;
uint16_t ackQueueOverflowCount = 0;
uint16_t ignoredAckCount = 0;
uint16_t peerTableFullCount = 0;
unsigned long lastPeerFullDiagnosticAt = 0;
struct PendingTx {
  RadioPacket packet;
  uint16_t expected[MAX_PEERS];
  uint32_t expectedBootIds[MAX_PEERS];
  uint16_t acknowledged[MAX_PEERS];
  uint8_t expectedCount;
  uint8_t ackCount;
  uint8_t attempts;
  unsigned long startedAt;
  unsigned long deadline;
  unsigned long nextTxAt;
  bool bestEffortTxSucceeded;
  bool active;
} pendingTx;
unsigned long lastHelloAt = 0;
struct SeenMessage { uint16_t senderId; uint32_t senderBootId; uint32_t messageId; bool used; };
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
  } else {
    deviceId = makeCandidateId();
    EEPROM.put(0, EEPROM_MAGIC);
    EEPROM.put(2, deviceId);
  }

  // A wear-aware persistent boot epoch prevents message-ID reuse after reboot.
  EEPROM.get(4, bootId);
  if (bootId == 0 || bootId == 0xFFFFFFFFUL) bootId = 0;
  bootId++;
  if (bootId == 0) bootId = 1;
  EEPROM.put(4, bootId);
}

void initPacket(RadioPacket &p, PacketType type) {
  memset(&p, 0, sizeof(p));
  p.magic = 0x4C52;
  p.version = PROTOCOL_VERSION;
  p.networkId = NETWORK_ID;
  p.type = (uint8_t)type;
  p.senderId = deviceId;
  p.senderBootId = bootId;
  copyText(p.userName, sizeof(p.userName), USER_NAME);
}

bool transmitPacket(const RadioPacket &p) {
  if (!LoRa.beginPacket()) return false;
  size_t written = LoRa.write((const uint8_t *)&p, sizeof(p));
  if (written != sizeof(p)) { LoRa.endPacket(); return false; }
  return LoRa.endPacket() == 1;
}

int findPeer(uint16_t id) {
  for (uint8_t i = 0; i < MAX_PEERS; ++i)
    if (peers[i].used && peers[i].id == id) return i;
  return -1;
}

bool updatePeer(uint16_t id, uint32_t peerBootId, const char *name) {
  int slot = findPeer(id);
  if (slot < 0) {
    for (uint8_t i = 0; i < MAX_PEERS; ++i) {
      if (!peers[i].used || millis() - peers[i].lastSeen > PEER_TIMEOUT_MS) {
        slot = i;
        break;
      }
    }
  }
  if (slot < 0) {
    peerTableFullCount++;
    if (lastPeerFullDiagnosticAt == 0 || millis() - lastPeerFullDiagnosticAt >= 10000UL) {
      Serial.print(F("[PEER TABLE FULL] New peer not recorded; overflow count "));
      Serial.println(peerTableFullCount);
      lastPeerFullDiagnosticAt = millis();
    }
    return false;
  }
  if (peers[slot].used && peers[slot].bootId != peerBootId) {
    Serial.print(F("[PEER SESSION] Device "));
    Serial.print(id);
    Serial.println(F(" rebooted or changed session."));
  }
  peers[slot].id = id;
  peers[slot].bootId = peerBootId;
  copyText(peers[slot].name, sizeof(peers[slot].name), name);
  peers[slot].lastSeen = millis();
  peers[slot].used = true;
  return true;
}

bool wasMessageSeen(uint16_t senderId, uint32_t senderBootId, uint32_t messageId) {
  for (uint8_t i=0; i<SEEN_CACHE_SIZE; ++i)
    if (seenMessages[i].used && seenMessages[i].senderId==senderId &&
        seenMessages[i].senderBootId==senderBootId && seenMessages[i].messageId==messageId) return true;
  return false;
}

void rememberMessage(uint16_t senderId, uint32_t senderBootId, uint32_t messageId) {
  SeenMessage &entry=seenMessages[seenNext];
  entry.senderId=senderId; entry.senderBootId=senderBootId; entry.messageId=messageId; entry.used=true;
  seenNext=(uint8_t)((seenNext+1)%SEEN_CACHE_SIZE);
}

void scheduleAck(const RadioPacket &original) {
  // Coalesce duplicate DATA while an ACK for the same transaction is queued.
  for (uint8_t i = 0; i < ACK_QUEUE_SIZE; ++i) {
    if (ackQueue[i].used && ackQueue[i].messageId == original.messageId &&
        ackQueue[i].targetId == original.senderId &&
        ackQueue[i].targetBootId == original.senderBootId) return;
  }
  for (uint8_t n = 0; n < ACK_QUEUE_SIZE; ++n) {
    uint8_t slot = (uint8_t)((ackQueueNext + n) % ACK_QUEUE_SIZE);
    if (!ackQueue[slot].used) {
      ackQueue[slot].messageId = original.messageId;
      ackQueue[slot].targetId = original.senderId;
      ackQueue[slot].targetBootId = original.senderBootId;
      uint32_t hash = original.messageId ^ ((uint32_t)original.senderId << 11) ^
                      ((uint32_t)deviceId * 2654435761UL);
      unsigned long ackSlot = (unsigned long)(hash % MAX_PEERS);
      ackQueue[slot].dueAt = millis() + ackSlot * ACK_SLOT_MS +
          (unsigned long)random(0, ACK_SLOT_JITTER_MS + 1UL);
      ackQueue[slot].used = true;
      ackQueueNext = (uint8_t)((slot + 1) % ACK_QUEUE_SIZE);
      return;
    }
  }
  ackQueueOverflowCount++;
  Serial.print(F("[ACK QUEUE FULL] DATA retry required; overflow count "));
  Serial.println(ackQueueOverflowCount);
}

void serviceAckQueue() {
  for (uint8_t i=0; i<ACK_QUEUE_SIZE; ++i) {
    PendingAck &item=ackQueue[i];
    if (!item.used || (long)(millis()-item.dueAt)<0) continue;
    RadioPacket ack;
    initPacket(ack, PKT_ACK);
    ack.messageId = 0;
    ack.ackForSenderId = item.targetId;
    ack.ackForBootId = item.targetBootId;
    ack.ackForMessageId = item.messageId;
    if (transmitPacket(ack)) {
      item.used=false;
    } else {
      item.dueAt=millis()+ACK_SLOT_JITTER_MS+1UL;
      Serial.println(F("[ACK TX ERROR] Retaining ACK for retry"));
    }
    return;
  }
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
  if (i != sizeof(RadioPacket)) return false;

  if (p.magic != 0x4C52 || p.version != PROTOCOL_VERSION ||
      p.networkId != NETWORK_ID || p.senderId == 0 || p.senderBootId == 0) return false;
  if (p.type != PKT_HELLO && p.type != PKT_TEXT && p.type != PKT_ACK) return false;
  if (!memchr(p.userName, '\0', sizeof(p.userName))) return false;

  if (p.type == PKT_TEXT) {
    if (p.messageId == 0 || p.payloadLength == 0 || p.payloadLength > MAX_MESSAGE_CHARS) return false;
    if (p.text[p.payloadLength] != '\0') return false;
    if (memchr(p.text, '\0', p.payloadLength) != NULL) return false;
  } else if (p.payloadLength != 0) {
    return false;
  }
  if (p.type == PKT_ACK &&
      (p.ackForSenderId == 0 || p.ackForBootId == 0 || p.ackForMessageId == 0)) return false;

  // Terminate the unused tail only after validating the transmitted terminator.
  p.userName[sizeof(p.userName) - 1] = '\0';
  p.text[MAX_MESSAGE_CHARS] = '\0';
  return true;
}

void serviceIncoming(bool acknowledgeText) {
  if (!readPacket(rxPacket)) return;
  if (rxPacket.senderId == deviceId) {
    if (rxPacket.senderBootId != bootId)
      Serial.println(F("[IDENTITY COLLISION] Another device is using this device ID."));
    return;
  }

  if (rxPacket.type == PKT_HELLO) {
    updatePeer(rxPacket.senderId, rxPacket.senderBootId, rxPacket.userName);
    return;
  }

  if (rxPacket.type == PKT_TEXT) {
    updatePeer(rxPacket.senderId, rxPacket.senderBootId, rxPacket.userName);
    bool duplicate = wasMessageSeen(rxPacket.senderId, rxPacket.senderBootId, rxPacket.messageId);
    if (!duplicate) {
      rememberMessage(rxPacket.senderId, rxPacket.senderBootId, rxPacket.messageId);
      Serial.print("[MESSAGE] ");
      Serial.print(rxPacket.userName);
      Serial.print(" (");
      Serial.print(rxPacket.senderId);
      Serial.print("): ");
      Serial.println(rxPacket.text);
    }
    if (acknowledgeText) scheduleAck(rxPacket);
    return;
  }

  if (rxPacket.type == PKT_ACK) {
    bool transactionMatch = pendingTx.active &&
        rxPacket.ackForMessageId == pendingTx.packet.messageId &&
        rxPacket.ackForSenderId == deviceId &&
        rxPacket.ackForBootId == bootId;
    bool expected = false;
    bool alreadyCounted = false;
    if (transactionMatch) {
      for (uint8_t i = 0; i < pendingTx.expectedCount; ++i) {
        if (pendingTx.expected[i] == rxPacket.senderId &&
            pendingTx.expectedBootIds[i] == rxPacket.senderBootId) expected = true;
      }
      for (uint8_t i = 0; i < pendingTx.ackCount; ++i) {
        if (pendingTx.acknowledged[i] == rxPacket.senderId) alreadyCounted = true;
      }
    }
    if (transactionMatch && expected && !alreadyCounted &&
        pendingTx.ackCount < MAX_PEERS) {
      pendingTx.acknowledged[pendingTx.ackCount++] = rxPacket.senderId;
      updatePeer(rxPacket.senderId, rxPacket.senderBootId, rxPacket.userName);
      Serial.print(F("[ACK] Device ")); Serial.println(rxPacket.senderId);
    } else {
      ignoredAckCount++;
      if ((ignoredAckCount & 0x07) == 1) {
        Serial.print(F("[ACK IGNORED] Stale, duplicate or unexpected ACK; count "));
        Serial.println(ignoredAckCount);
      }
    }
  }
}

void sendHello() {
  RadioPacket hello;
  initPacket(hello, PKT_HELLO);
  transmitPacket(hello);
  lastHelloAt = millis();
}


uint8_t activePeerCount() {
  uint8_t count = 0;
  for (uint8_t i = 0; i < MAX_PEERS; ++i) {
    if (peers[i].used && millis() - peers[i].lastSeen <= PEER_TIMEOUT_MS) count++;
  }
  return count;
}

void startText(const char *text) {
  if (!text || text[0]=='\0') { Serial.println(F("[STATUS] REJECTED_LOCAL; empty messages are not sent.")); return; }
  size_t length=strlen(text);
  if (length>MAX_MESSAGE_CHARS) { Serial.println(F("[STATUS] REJECTED_LOCAL; message exceeds 128 bytes.")); return; }
  if (pendingTx.active) { Serial.println(F("[STATUS] BUSY; previous message still awaiting bounded delivery result.")); return; }
  memset(&pendingTx,0,sizeof(pendingTx));
  initPacket(pendingTx.packet,PKT_TEXT);
  if (nextMessageId == 0) {
    Serial.println(F("[ERROR] Message ID space exhausted; reboot for a new session."));
    return;
  }
  pendingTx.packet.messageId = nextMessageId++;
  pendingTx.packet.payloadLength = (uint8_t)length;
  copyText(pendingTx.packet.text, sizeof(pendingTx.packet.text), text);
  for(uint8_t i=0;i<MAX_PEERS;i++) {
    if(peers[i].used && millis()-peers[i].lastSeen<=PEER_TIMEOUT_MS) {
      if (pendingTx.expectedCount >= MAX_PEERS) break;
      pendingTx.expected[pendingTx.expectedCount]=peers[i].id;
      pendingTx.expectedBootIds[pendingTx.expectedCount]=peers[i].bootId;
      pendingTx.expectedCount++;
    }
  }
  pendingTx.active = true;
  pendingTx.startedAt = millis();
  pendingTx.nextTxAt = pendingTx.startedAt;
  Serial.print(F("[TX QUEUED] Message ID ")); Serial.println(pendingTx.packet.messageId);
  if (!pendingTx.expectedCount) {
    Serial.println(F("[STATUS] No known peers; best-effort broadcast only."));
  }
}

void finishPendingTx() {
  if (pendingTx.expectedCount == 0) {
    if (pendingTx.bestEffortTxSucceeded)
      Serial.println(F("[STATUS] NO_KNOWN_RECIPIENTS; one best-effort broadcast transmitted, no delivery confirmation possible"));
    else
      Serial.println(F("[STATUS] NO_KNOWN_RECIPIENTS; best-effort broadcast failed locally"));
  } else if (pendingTx.ackCount == pendingTx.expectedCount) {
    Serial.print(F("[STATUS] CONFIRMED_TO_")); Serial.println(pendingTx.ackCount);
  } else if (pendingTx.ackCount > 0) {
    Serial.print(F("[STATUS] PARTIALLY_CONFIRMED; confirmed "));
    Serial.print(pendingTx.ackCount); Serial.print(F(" of "));
    Serial.println(pendingTx.expectedCount);
  } else {
    Serial.print(F("[STATUS] UNCONFIRMED; confirmed 0 of "));
    Serial.println(pendingTx.expectedCount);
  }
  pendingTx.active = false;
}

void servicePendingTx() {
  if (!pendingTx.active) return;

  if (pendingTx.expectedCount > 0 && pendingTx.ackCount == pendingTx.expectedCount) {
    finishPendingTx();
    return;
  }

  unsigned long now = millis();
  if (now - pendingTx.startedAt >= MAX_TRANSACTION_LIFETIME_MS) {
    Serial.println(F("[TX LIFETIME] Maximum transaction lifetime reached."));
    finishPendingTx();
    return;
  }
  if ((long)(now - pendingTx.nextTxAt) < 0) return;

  if (pendingTx.expectedCount == 0 && pendingTx.attempts > 0) {
    finishPendingTx();
    return;
  }
  if (pendingTx.attempts >= (uint8_t)(MAX_RETRIES + 1)) {
    finishPendingTx();
    return;
  }

  bool txOk = transmitPacket(pendingTx.packet);
  pendingTx.attempts++;
  if (txOk && pendingTx.expectedCount == 0) pendingTx.bestEffortTxSucceeded = true;
  if (!txOk) Serial.println(F("[TX RADIO ERROR] Attempt will be retried"));

  pendingTx.deadline = millis() + ACK_TIMEOUT_MS;
  if (pendingTx.expectedCount == 0) {
    pendingTx.nextTxAt = millis();
    return;
  }
  uint8_t exponent = pendingTx.attempts > 1 ? pendingTx.attempts - 1 : 0;
  unsigned long backoff = RETRY_BACKOFF_BASE_MS;
  while (exponent-- && backoff < RETRY_BACKOFF_MAX_MS) {
    backoff *= 2;
    if (backoff > RETRY_BACKOFF_MAX_MS) backoff = RETRY_BACKOFF_MAX_MS;
  }
  unsigned long jitter = (unsigned long)random(0, (long)(backoff / 3UL) + 1L);
  pendingTx.nextTxAt = pendingTx.deadline + backoff + jitter;

  Serial.print(F("[TX] Attempt ")); Serial.print(pendingTx.attempts);
  Serial.print(F("/")); Serial.println(MAX_RETRIES + 1);
}

void setup() {
  Serial.begin(9600);
  randomSeed(analogRead(A0) ^ micros() ^ millis());
  while (!Serial) { }
  loadOrCreateDeviceId();
  memset(peers, 0, sizeof(peers));
  memset(seenMessages, 0, sizeof(seenMessages));
  memset(ackQueue,0,sizeof(ackQueue));
  memset(&pendingTx,0,sizeof(pendingTx));
  Serial.println(F("LoRa Group Messaging - RMP v1 prototype"));
  Serial.print(F("User: ")); Serial.println(USER_NAME);
  Serial.print(F("Device ID: ")); Serial.println(deviceId);
  Serial.print(F("Persistent boot/session epoch: ")); Serial.println(bootId);
  Serial.print(F("Protocol version: ")); Serial.println(PROTOCOL_VERSION);
  Serial.println(F("Note: 16-bit device IDs require collision-free provisioning."));

  LoRa.setPins(LORA_NSS_PIN, LORA_RESET_PIN, LORA_DIO0_PIN);
  if (!LoRa.begin(LORA_FREQUENCY_HZ)) {
    Serial.println(F("[FATAL] LoRa init failed. Check power, wiring and RF module."));
    while (true) delay(1000);
  }
  LoRa.enableCrc();
  static_assert(sizeof(RadioPacket) == 172, "RMP v1 wire frame layout changed; increment protocol version");
  static_assert(sizeof(RadioPacket) <= 255, "LoRa packet exceeds PHY payload limit");
  static_assert(MAX_MESSAGE_CHARS <= 128, "RMP payload exceeds 128-byte limit");
  static_assert(MAX_RETRIES <= 10, "Retry count must remain tightly bounded");
  sendHello();
  Serial.println(F("Type a message (max 128 bytes), then press Enter."));
}

void loop() {
  if (millis() - lastHelloAt >= HELLO_INTERVAL_MS) sendHello();

  // Service a single incoming packet when not waiting for ACKs.
  serviceIncoming(true);
  serviceAckQueue();
  servicePendingTx();

  static char input[MAX_MESSAGE_CHARS + 2];
  static uint8_t inputLength = 0;
  static bool discardUntilNewline = false;
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\r') continue;
    if (c == '\n') {
      if (!discardUntilNewline) {
        input[inputLength] = '\0';
        startText(input);
      }
      inputLength = 0;
      discardUntilNewline = false;
    } else if (!discardUntilNewline && c == '\0') {
      inputLength = 0;
      discardUntilNewline = true;
      Serial.println(F("[STATUS] REJECTED_LOCAL; NUL byte in input; entire line discarded."));
    } else if (!discardUntilNewline && inputLength < MAX_MESSAGE_CHARS) {
      input[inputLength++] = c;
    } else if (!discardUntilNewline) {
      inputLength = 0;
      discardUntilNewline = true;
      Serial.println(F("[STATUS] REJECTED_LOCAL; input exceeds 128 bytes; entire line discarded."));
    }
  }
}
