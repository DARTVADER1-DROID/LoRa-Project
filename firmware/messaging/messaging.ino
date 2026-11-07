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
#define ACK_QUEUE_SIZE              4
#define ACK_JITTER_MIN_MS           40UL
#define ACK_JITTER_MAX_MS          420UL
#define EEPROM_MAGIC          0x4C52

enum PacketType : uint8_t { PKT_HELLO = 1, PKT_TEXT = 2, PKT_ACK = 3 };

struct __attribute__((packed)) RadioPacket {
  uint16_t magic;
  uint16_t networkId;
  uint8_t type;
  uint16_t senderId;
  uint16_t messageId;
  uint16_t ackForSenderId;
  uint16_t senderBootId;
  uint16_t ackForBootId;
  char userName[16];
  char text[MAX_MESSAGE_CHARS + 1];
};

struct PendingAck { uint16_t messageId; uint16_t targetId; uint16_t targetBootId; unsigned long dueAt; bool used; };
struct Peer {
  uint16_t id;
  uint16_t bootId;
  char name[16];
  unsigned long lastSeen;
  bool used;
};

RadioPacket txPacket;
RadioPacket rxPacket;
Peer peers[MAX_PEERS];
uint16_t deviceId = 0;
uint16_t nextMessageId = 1;
uint16_t bootId = 0;
PendingAck ackQueue[ACK_QUEUE_SIZE];
uint8_t ackQueueNext = 0;
struct PendingTx { RadioPacket packet; uint16_t expected[MAX_PEERS]; uint16_t expectedBootIds[MAX_PEERS]; uint16_t acknowledged[MAX_PEERS]; uint8_t expectedCount; uint8_t ackCount; uint8_t attempts; unsigned long deadline; unsigned long nextTxAt; bool active; } pendingTx;
unsigned long lastHelloAt = 0;
struct SeenMessage { uint16_t senderId; uint16_t senderBootId; uint16_t messageId; bool used; };
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
  p.senderBootId = bootId;
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

void updatePeer(uint16_t id, uint16_t peerBootId, const char *name) {
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
  peers[slot].bootId = peerBootId;
  copyText(peers[slot].name, sizeof(peers[slot].name), name);
  peers[slot].lastSeen = millis();
  peers[slot].used = true;
}

bool wasMessageSeen(uint16_t senderId, uint16_t senderBootId, uint16_t messageId) {
  for (uint8_t i=0; i<SEEN_CACHE_SIZE; ++i)
    if (seenMessages[i].used && seenMessages[i].senderId==senderId &&
        seenMessages[i].senderBootId==senderBootId && seenMessages[i].messageId==messageId) return true;
  return false;
}

void rememberMessage(uint16_t senderId, uint16_t senderBootId, uint16_t messageId) {
  SeenMessage &entry=seenMessages[seenNext];
  entry.senderId=senderId; entry.senderBootId=senderBootId; entry.messageId=messageId; entry.used=true;
  seenNext=(uint8_t)((seenNext+1)%SEEN_CACHE_SIZE);
}

void scheduleAck(const RadioPacket &original) {
  for (uint8_t n=0; n<ACK_QUEUE_SIZE; ++n) {
    uint8_t slot=(uint8_t)((ackQueueNext+n)%ACK_QUEUE_SIZE);
    if (!ackQueue[slot].used) {
      ackQueue[slot].messageId=original.messageId;
      ackQueue[slot].targetId=original.senderId;
      ackQueue[slot].targetBootId=original.senderBootId;
      ackQueue[slot].dueAt=millis()+(unsigned long)random(ACK_JITTER_MIN_MS, ACK_JITTER_MAX_MS+1);
      ackQueue[slot].used=true;
      ackQueueNext=(uint8_t)((slot+1)%ACK_QUEUE_SIZE);
      return;
    }
  }
  Serial.println(F("[ACK BUSY] ACK queue full; sender retry can recover"));
}

void serviceAckQueue() {
  for (uint8_t i=0; i<ACK_QUEUE_SIZE; ++i) {
    PendingAck &item=ackQueue[i];
    if (!item.used || (long)(millis()-item.dueAt)<0) continue;
    RadioPacket ack;
    initPacket(ack, PKT_ACK);
    ack.messageId=item.messageId;
    ack.ackForSenderId=item.targetId;
    ack.ackForBootId=item.targetBootId;
    transmitPacket(ack);
    item.used=false;
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
  p.userName[sizeof(p.userName)-1]='\\0';
  p.text[MAX_MESSAGE_CHARS]='\\0';
  return p.magic == 0x4C52 && p.networkId == NETWORK_ID;
}

void serviceIncoming(bool acknowledgeText) {
  if (!readPacket(rxPacket)) return;
  if (rxPacket.senderId == deviceId) return;

  if (rxPacket.type == PKT_HELLO) {
    updatePeer(rxPacket.senderId, rxPacket.senderBootId, rxPacket.userName);
    return;
  }

  if (rxPacket.type == PKT_TEXT) {
    updatePeer(rxPacket.senderId, rxPacket.userName);
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

  if (rxPacket.type == PKT_ACK && pendingTx.active &&
      rxPacket.messageId == pendingTx.packet.messageId &&
      rxPacket.ackForSenderId == deviceId && rxPacket.ackForBootId == bootId &&
      rxPacket.senderId != deviceId) {
    bool expected=false, known=false;
    for (uint8_t i=0;i<pendingTx.expectedCount;i++) if (pendingTx.expected[i]==rxPacket.senderId && pendingTx.expectedBootIds[i]==rxPacket.senderBootId) expected=true;
    for (uint8_t i=0;i<pendingTx.ackCount;i++) if (pendingTx.acknowledged[i]==rxPacket.senderId) known=true;
    if (expected && !known && pendingTx.ackCount<MAX_PEERS) {
      pendingTx.acknowledged[pendingTx.ackCount++]=rxPacket.senderId;
      updatePeer(rxPacket.senderId,rxPacket.senderBootId,rxPacket.userName);
      Serial.print(F("[ACK] Device ")); Serial.println(rxPacket.senderId);
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
  if (!text || text[0]=='\\0') { Serial.println(F("[ERROR] Empty messages are not sent.")); return; }
  size_t length=strlen(text);
  if (length>MAX_MESSAGE_CHARS) { Serial.println(F("[ERROR] Message exceeds 128 bytes.")); return; }
  if (pendingTx.active) { Serial.println(F("[BUSY] Previous message still awaiting bounded delivery result.")); return; }
  memset(&pendingTx,0,sizeof(pendingTx));
  initPacket(pendingTx.packet,PKT_TEXT);
  pendingTx.packet.messageId=nextMessageId++;
  copyText(pendingTx.packet.text,sizeof(pendingTx.packet.text),text);
  for(uint8_t i=0;i<MAX_PEERS;i++) {
    if(peers[i].used && millis()-peers[i].lastSeen<=PEER_TIMEOUT_MS)
      pendingTx.expected[pendingTx.expectedCount]=peers[i].id;
      pendingTx.expectedBootIds[pendingTx.expectedCount]=peers[i].bootId;
      pendingTx.expectedCount++;
  }
  pendingTx.active=true;
  pendingTx.nextTxAt=millis();
  Serial.print(F("[TX QUEUED] Message ID ")); Serial.println(pendingTx.packet.messageId);
  if (!pendingTx.expectedCount) Serial.println(F("[STATUS] No known peers; best-effort broadcast only."));
}

void servicePendingTx() {
  if (!pendingTx.active) return;
  if (pendingTx.ackCount>=pendingTx.expectedCount && pendingTx.expectedCount>0) {
    Serial.print(F("[STATUS] CONFIRMED_TO_")); Serial.println(pendingTx.ackCount);
    pendingTx.active=false; return;
  }
  if ((long)(millis()-pendingTx.nextTxAt)<0) return;
  if (pendingTx.expectedCount==0 && pendingTx.attempts>0) {
    Serial.println(F("[STATUS] NO_KNOWN_RECIPIENTS; one best-effort broadcast sent"));
    pendingTx.active=false; return;
  }
  if (pendingTx.attempts>MAX_RETRIES) {
    Serial.print(F("[STATUS] EXPIRED; confirmed ")); Serial.print(pendingTx.ackCount);
    Serial.print(F(" of ")); Serial.println(pendingTx.expectedCount);
    pendingTx.active=false; return;
  }
  transmitPacket(pendingTx.packet);
  pendingTx.attempts++;
  pendingTx.deadline=millis()+ACK_TIMEOUT_MS;
  pendingTx.nextTxAt=pendingTx.deadline+(unsigned long)random(100,RETRY_BACKOFF_MS+1);
  Serial.print(F("[TX] Attempt ")); Serial.println(pendingTx.attempts);
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
  bootId=(uint16_t)random(1,65535); if(!bootId) bootId=1;
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
  serviceAckQueue();
  servicePendingTx();

  static char input[MAX_MESSAGE_CHARS + 2];
  static uint8_t inputLength = 0;
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\r') continue;
    if (c == '\n') {
      input[inputLength] = '\0';
      startText(input);
      inputLength = 0;
    } else if (inputLength < sizeof(input) - 1) {
      input[inputLength++] = c;
    } else {
      inputLength = 0;
      Serial.println(F("[ERROR] Input too long; line discarded."));
    }
  }
}
