/*
 * LoRa Project - Firmware 2: Campus Mobility Assistance
 * One sketch, compile-time role selection: STATION or CART.
 * Target: Arduino Uno + SX1278-compatible radio.
 *
 * Prototype only. See docs/HARDWARE_AND_SAFETY.md and docs/PROTOCOL.md.
 * Required libraries: LoRa (Sandeep Mistry), LiquidCrystal_I2C (cart role).
 */

// ======================== USER CONFIGURATION ========================
#define DEVICE_ROLE_STATION 1
#define DEVICE_ROLE_CART    2
#ifndef DEVICE_ROLE
#define DEVICE_ROLE         DEVICE_ROLE_STATION  // Override with -DDEVICE_ROLE=2 for cart build
#endif

#define DEVICE_ID           1                    // Must be unique per physical device
#define NETWORK_ID          2001
#define LOCATION_NAME       "LIBRARY"            // Station only; max 15 chars
#define CART_NAME           "CART_01"            // Cart only; max 15 chars

#define LORA_FREQUENCY_HZ   433E6                // BENCH DEFAULT ONLY; match module and regulations
#define LORA_NSS_PIN        10
#define LORA_RESET_PIN       9
#define LORA_DIO0_PIN        2

// Station controls / indicators
#define STATION_BUTTON_PIN  3
#define STATION_LED_R_PIN   4
#define STATION_LED_G_PIN   5
#define STATION_LED_B_PIN   6
#define STATION_BUZZER_PIN  7

// Cart controls / indicators
#define CART_ACCEPT_PIN     3
#define CART_CANCEL_PIN     4
#define CART_COMPLETE_PIN   5
#define CART_LED_PIN        6
#define CART_BUZZER_PIN     7

// I2C LCD pins are board-defined (A4 SDA, A5 SCL on Uno).
#define LCD_I2C_ADDRESS    0x27

// Timings are starting values; tune after measured campus RF tests.
#define REQUEST_TIMEOUT_MS          600000UL
#define REQUEST_RETRY_INTERVAL_MS     5000UL
#define CART_HEARTBEAT_INTERVAL_MS    5000UL
#define CART_OFFLINE_TIMEOUT_MS      30000UL
#define CLAIM_WINDOW_MS                900UL
#define BUTTON_HOLD_CANCEL_MS         3000UL
#define BUTTON_DEBOUNCE_MS               40UL
#define MAX_PENDING_REQUESTS             10
#define CLAIM_RETRY_INTERVAL_MS        2500UL
#define ASSIGN_RETRY_INTERVAL_MS       1500UL
#define CANCEL_RETRY_INTERVAL_MS       1500UL
#define CANCEL_MAX_RETRIES                4
#define TOMBSTONE_CAPACITY                12
#define MAX_CLAIMANTS                       8
#define TERMINAL_RETRY_INTERVAL_MS       1500UL
#define TERMINAL_MAX_RETRIES                4
// ====================================================================

#include <SPI.h>
#include <LoRa.h>
#include <EEPROM.h>
#include <string.h>
#include <stddef.h>
#if DEVICE_ROLE == DEVICE_ROLE_CART
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
LiquidCrystal_I2C lcd(LCD_I2C_ADDRESS, 16, 2);
#endif

enum PacketType : uint8_t {
  PKT_REQUEST = 1, PKT_REQUEST_ACK = 2, PKT_CLAIM = 3,
  PKT_ASSIGN = 4, PKT_STUDENT_CANCEL = 5, PKT_CART_CANCEL = 6,
  PKT_COMPLETE = 7, PKT_HEARTBEAT = 8, PKT_EXPIRE = 9,
  PKT_ASSIGN_ACK = 10, PKT_QUEUE_FULL = 11, PKT_CANCEL_ACK = 12,
  PKT_CART_CANCEL_ACK = 13, PKT_COMPLETE_ACK = 14
};

struct __attribute__((packed)) CampusPacket {
  uint16_t magic;
  uint16_t networkId;
  uint8_t type;
  uint16_t senderId;
  uint16_t stationId;
  uint16_t cartId;
  uint16_t requestId;
  uint8_t generation;
  char location[16];
};

struct Tombstone { uint16_t stationId; uint16_t requestId; uint8_t generation; bool used; };

struct PendingRequest {
  uint16_t stationId;
  uint16_t requestId;
  uint8_t generation;
  char location[16];
  bool used;
};

CampusPacket txPacket;
CampusPacket rxPacket;
uint16_t nextRequestId = 1;

bool debouncedButtonState(uint8_t pin, uint8_t slot) {
  static bool stableState[4] = {false, false, false, false};
  static bool lastReading[4] = {false, false, false, false};
  static unsigned long lastChangeAt[4] = {0, 0, 0, 0};
  bool reading = (digitalRead(pin) == LOW);
  if (reading != lastReading[slot]) {
    lastReading[slot] = reading;
    lastChangeAt[slot] = millis();
  }
  if (millis() - lastChangeAt[slot] >= BUTTON_DEBOUNCE_MS)
    stableState[slot] = lastReading[slot];
  return stableState[slot];
}

#if DEVICE_ROLE == DEVICE_ROLE_STATION
bool requestActive = false;
bool cancellationPending = false;
uint16_t cancellingRequestId = 0;
uint8_t cancellingGeneration = 0;
uint8_t cancellationAttempts = 0;
uint16_t cancellingCartId = 0;
uint8_t cancellingPacketType = PKT_STUDENT_CANCEL;
unsigned long lastCancellationTxAt = 0;
bool requestAssigned = false;
bool claimWindowOpen = false;
bool assignmentConfirmed = false;
uint16_t activeRequestId = 0;
uint16_t assignedCartId = 0;
uint16_t candidateCartId = 0;
uint16_t claimantIds[MAX_CLAIMANTS];
uint8_t claimantCount = 0;
uint16_t lastWinningCartId = 0;
uint8_t lastTerminalType = 0;
uint16_t lastTerminalCartId = 0;
uint16_t lastTerminalRequestId = 0;
uint8_t lastTerminalGeneration = 0;
uint8_t assignmentGeneration = 1;
unsigned long requestStartedAt = 0;
unsigned long lastRequestTxAt = 0;
unsigned long claimWindowStartedAt = 0;
unsigned long assignedCartLastSeenAt = 0;
unsigned long lastAssignmentTxAt = 0;
unsigned long buttonPressedAt = 0;
bool buttonWasDown = false;
#else
PendingRequest queueItems[MAX_PENDING_REQUESTS];
Tombstone tombstones[TOMBSTONE_CAPACITY];
uint8_t tombstoneNext = 0;
uint8_t queueCount = 0;
bool assignmentActive = false;
PendingRequest activeRequest;
bool claimRequested = false;
unsigned long lastClaimAt = 0;
unsigned long lastHeartbeatAt = 0;
unsigned long cancelPressedAt = 0;
bool cancelWasDown = false;
bool cancelHandled = false;
bool acceptWasDown = false;
bool completeWasDown = false;
CampusPacket terminalPacket;
bool terminalPending = false;
uint8_t terminalAttempts = 0;
unsigned long lastTerminalTxAt = 0;
#endif

void copyField(char *dst, size_t size, const char *src) {
  if (!size) return;
  strncpy(dst, src, size - 1);
  dst[size - 1] = '\0';
}

void initPacket(CampusPacket &p, PacketType type) {
  memset(&p, 0, sizeof(p));
  p.magic = 0xCA51;
  p.networkId = NETWORK_ID;
  p.type = (uint8_t)type;
  p.senderId = DEVICE_ID;
  p.stationId = (DEVICE_ROLE == DEVICE_ROLE_STATION) ? DEVICE_ID : 0;
}

void sendPacket(const CampusPacket &p) {
  LoRa.beginPacket();
  LoRa.write((const uint8_t *)&p, sizeof(p));
  LoRa.endPacket();
}

bool receivePacket(CampusPacket &p) {
  int packetSize = LoRa.parsePacket();
  if (packetSize != (int)sizeof(CampusPacket)) {
    while (LoRa.available()) LoRa.read();
    return false;
  }
  uint8_t *dst = (uint8_t *)&p;
  size_t i = 0;
  while (LoRa.available() && i < sizeof(CampusPacket)) dst[i++] = (uint8_t)LoRa.read();
  return i == sizeof(CampusPacket) && p.magic == 0xCA51 && p.networkId == NETWORK_ID;
}

#if DEVICE_ROLE == DEVICE_ROLE_STATION

void setStationStatus(bool active, bool assigned) {
  bool blink = active && ((millis() / 250) % 2);
  digitalWrite(STATION_LED_R_PIN, LOW);
  digitalWrite(STATION_LED_G_PIN, assigned && assignmentConfirmed ? HIGH : LOW);
  digitalWrite(STATION_LED_B_PIN, (!assigned && blink) ? HIGH : LOW);
}

void startRequest() {
  if (requestActive) return;
  requestActive = true;
  requestAssigned = false;
  assignmentConfirmed = false;
  claimWindowOpen = false;
  activeRequestId = nextRequestId++;
  EEPROM.put(0, nextRequestId);
  assignedCartId = 0;
  candidateCartId = 0;
  claimantCount = 0;
  assignmentGeneration = 1;
  requestStartedAt = millis();
  lastRequestTxAt = 0;
  Serial.print(F("[REQUEST] Created "));
  Serial.print(DEVICE_ID); Serial.print('-'); Serial.println(activeRequestId);
}

void broadcastRequest() {
  CampusPacket p;
  initPacket(p, PKT_REQUEST);
  p.stationId = DEVICE_ID;
  p.requestId = activeRequestId;
  p.generation = assignmentGeneration;
  copyField(p.location, sizeof(p.location), LOCATION_NAME);
  sendPacket(p);
  lastRequestTxAt = millis();
  Serial.println(F("[TX] Assistance request broadcast"));
}

void cancelRequest(uint8_t packetType) {
  if (!requestActive) return;
  CampusPacket p;
  initPacket(p, (PacketType)packetType);
  p.stationId = DEVICE_ID;
  p.requestId = activeRequestId;
  p.cartId = assignedCartId;
  p.generation = assignmentGeneration;
  copyField(p.location, sizeof(p.location), LOCATION_NAME);
  sendPacket(p);
  cancellationPending = true;
  cancellingRequestId = activeRequestId;
  cancellingGeneration = assignmentGeneration;
  cancellationAttempts = 1;
  cancellingCartId = assignedCartId;
  cancellingPacketType = packetType;
  lastCancellationTxAt = millis();
  requestActive = false;
  requestAssigned = false;
  claimWindowOpen = false;
  assignedCartId = 0;
  Serial.println(F("[CANCEL] Request closed locally"));
  tone(STATION_BUZZER_PIN, 900, 250);
}

void handleStationPacket(const CampusPacket &p) {
  if ((p.type==PKT_CART_CANCEL && lastTerminalType==PKT_CART_CANCEL) ||
      (p.type==PKT_COMPLETE && lastTerminalType==PKT_COMPLETE)) {
    if (p.senderId==lastTerminalCartId && p.cartId==lastTerminalCartId &&
        p.stationId==DEVICE_ID && p.requestId==lastTerminalRequestId &&
        p.generation==lastTerminalGeneration) {
      CampusPacket ack;
      initPacket(ack,(PacketType)(p.type==PKT_CART_CANCEL ? PKT_CART_CANCEL_ACK : PKT_COMPLETE_ACK));
      ack.stationId=DEVICE_ID; ack.requestId=p.requestId; ack.generation=p.generation; ack.cartId=p.cartId;
      sendPacket(ack);
      return;
    }
  }
  if (p.type == PKT_HEARTBEAT) {
    if (requestAssigned && p.senderId == assignedCartId &&
        p.stationId == DEVICE_ID && p.requestId == activeRequestId &&
        p.generation == assignmentGeneration) assignedCartLastSeenAt = millis();
    return;
  }
  if (p.stationId != DEVICE_ID || p.requestId != activeRequestId) return;
  if (p.generation != assignmentGeneration) return;

  if (p.type == PKT_REQUEST_ACK) {
    Serial.print(F("[RECEIVED] Cart ")); Serial.println(p.senderId);
  } else if (p.type == PKT_CLAIM && requestActive && !requestAssigned) {
    bool known=false;
    for(uint8_t i=0;i<claimantCount;i++) if(claimantIds[i]==p.senderId) known=true;
    if(!known && claimantCount<MAX_CLAIMANTS) claimantIds[claimantCount++]=p.senderId;
    if (!claimWindowOpen) { claimWindowOpen=true; claimWindowStartedAt=millis(); }
    Serial.print(F("[CLAIM] Cart ")); Serial.println(p.senderId);
  } else if (p.type == PKT_ASSIGN_ACK && requestAssigned && p.senderId == assignedCartId && p.cartId == assignedCartId) {
    assignmentConfirmed = true;
    assignedCartLastSeenAt = millis();
    Serial.println(F("[ASSIGN ACK] Cart confirmed assignment"));
  } else if (p.type == PKT_CANCEL_ACK && cancellationPending &&
             p.requestId == cancellingRequestId && p.generation == cancellingGeneration &&
             (cancellingCartId==0 || p.senderId==cancellingCartId)) {
    cancellationPending = false;
    Serial.print(F("[CANCEL MCU ACK] Cart ")); Serial.println(p.senderId);
  } else if (p.type == PKT_QUEUE_FULL) {
    Serial.print(F("[QUEUE FULL] Cart ")); Serial.println(p.senderId);
    tone(STATION_BUZZER_PIN, 500, 700);
  } else if (p.type == PKT_CART_CANCEL && requestAssigned && p.senderId == assignedCartId && p.cartId == assignedCartId) {
    lastTerminalType=PKT_CART_CANCEL; lastTerminalCartId=p.senderId; lastTerminalRequestId=p.requestId; lastTerminalGeneration=p.generation;
    CampusPacket ack; initPacket(ack,PKT_CART_CANCEL_ACK);
    ack.stationId=DEVICE_ID; ack.requestId=activeRequestId; ack.generation=assignmentGeneration; ack.cartId=assignedCartId; sendPacket(ack);
    requestAssigned = false;
    assignmentConfirmed = false;
    assignedCartId = 0;
    assignmentGeneration++;
    claimWindowOpen = false;
    claimantCount = 0;
    lastRequestTxAt = 0;
    Serial.println(F("[CART CANCEL] Request returned to pending queue"));
    tone(STATION_BUZZER_PIN, 700, 400);
  } else if (p.type == PKT_COMPLETE && requestAssigned && p.senderId == assignedCartId && p.cartId == assignedCartId) {
    lastTerminalType=PKT_COMPLETE; lastTerminalCartId=p.senderId; lastTerminalRequestId=p.requestId; lastTerminalGeneration=p.generation;
    CampusPacket ack; initPacket(ack,PKT_COMPLETE_ACK);
    ack.stationId=DEVICE_ID; ack.requestId=activeRequestId; ack.generation=assignmentGeneration; ack.cartId=assignedCartId; sendPacket(ack);
    requestActive = false;
    requestAssigned = false;
    assignmentConfirmed = false;
    assignedCartId = 0;
    Serial.println(F("[COMPLETE] Journey completed"));
    tone(STATION_BUZZER_PIN, 1500, 150);
  }
}

void serviceStation() {
  if (receivePacket(rxPacket)) handleStationPacket(rxPacket);
  if (cancellationPending && millis() - lastCancellationTxAt >= CANCEL_RETRY_INTERVAL_MS) {
    if (cancellationAttempts < CANCEL_MAX_RETRIES) {
      CampusPacket cancel;
      initPacket(cancel, (PacketType)cancellingPacketType);
      cancel.stationId = DEVICE_ID; cancel.requestId = cancellingRequestId;
      cancel.cartId = cancellingCartId;
      cancel.generation = cancellingGeneration;
      copyField(cancel.location, sizeof(cancel.location), LOCATION_NAME);
      sendPacket(cancel);
      cancellationAttempts++;
      lastCancellationTxAt = millis();
      Serial.println(F("[CANCEL RETRY] Broadcast repeated"));
    } else {
      cancellationPending = false;
      Serial.println(F("[CANCEL UNCONFIRMED] No cart MCU acknowledgement; local request remains cancelled"));
    }
  }

  bool down = debouncedButtonState(STATION_BUTTON_PIN, 0);
  if (down && !buttonWasDown) buttonPressedAt = millis();
  if (!down && buttonWasDown) {
    unsigned long held = millis() - buttonPressedAt;
    if (requestActive && held >= BUTTON_HOLD_CANCEL_MS) cancelRequest(PKT_STUDENT_CANCEL);
    else if (!requestActive && held < BUTTON_HOLD_CANCEL_MS) startRequest();
  }
  buttonWasDown = down;

  if (requestActive && !requestAssigned) {
    if (millis() - requestStartedAt >= REQUEST_TIMEOUT_MS) {
      CampusPacket p;
      initPacket(p, PKT_EXPIRE);
      p.stationId = DEVICE_ID; p.requestId = activeRequestId;
      p.generation = assignmentGeneration;
      sendPacket(p);
      requestActive = false;
      Serial.println(F("[EXPIRED] No cart accepted before timeout"));
      tone(STATION_BUZZER_PIN, 500, 800);
    } else if (millis() - lastRequestTxAt >= REQUEST_RETRY_INTERVAL_MS) {
      broadcastRequest();
    }
  }

  if (claimWindowOpen && millis() - claimWindowStartedAt >= CLAIM_WINDOW_MS) {
    if (claimantCount==0) { claimWindowOpen=false; }
    else {
    uint16_t selected=0;
    for(uint8_t i=0;i<claimantCount;i++) if(claimantIds[i]>lastWinningCartId && (!selected || claimantIds[i]<selected)) selected=claimantIds[i];
    if(!selected) for(uint8_t i=0;i<claimantCount;i++) if(!selected || claimantIds[i]<selected) selected=claimantIds[i];
    candidateCartId=selected; lastWinningCartId=selected;
    claimantCount=0;
    CampusPacket p;
    initPacket(p, PKT_ASSIGN);
    p.stationId = DEVICE_ID;
    p.requestId = activeRequestId;
    p.cartId = candidateCartId;
    p.generation = assignmentGeneration;
    copyField(p.location, sizeof(p.location), LOCATION_NAME);
    sendPacket(p);
    assignedCartId = candidateCartId;
    requestAssigned = true;
    assignmentConfirmed = false;
    assignedCartLastSeenAt = millis();
    lastAssignmentTxAt = millis();
    claimWindowOpen = false;
    Serial.print(F("[ASSIGNING] Awaiting MCU confirmation from cart ")); Serial.println(assignedCartId);
    tone(STATION_BUZZER_PIN, 1300, 180);
    }
  }

  if (requestAssigned && !assignmentConfirmed && millis() - lastAssignmentTxAt >= ASSIGN_RETRY_INTERVAL_MS) {
    CampusPacket p;
    initPacket(p, PKT_ASSIGN);
    p.stationId = DEVICE_ID; p.requestId = activeRequestId;
    p.cartId = assignedCartId; p.generation = assignmentGeneration;
    copyField(p.location, sizeof(p.location), LOCATION_NAME);
    sendPacket(p);
    lastAssignmentTxAt = millis();
  }

  if (requestAssigned && millis() - assignedCartLastSeenAt >= CART_OFFLINE_TIMEOUT_MS) {
    requestAssigned = false;
    assignmentConfirmed = false;
    assignedCartId = 0;
    assignmentGeneration++;
    claimWindowOpen = false;
    claimantCount = 0;
    lastRequestTxAt = 0;
    Serial.println(F("[OFFLINE] Cart heartbeat lost; request rebroadcast"));
    tone(STATION_BUZZER_PIN, 600, 500);
  }
  setStationStatus(requestActive, requestAssigned);
}

#else

bool generationIsNewer(uint8_t candidate, uint8_t reference) {
  return (int8_t)(candidate-reference) > 0; // modular 8-bit sequence comparison
}

bool isTombstoned(uint16_t stationId, uint16_t requestId, uint8_t generation) {
  for (uint8_t i=0; i<TOMBSTONE_CAPACITY; ++i) {
    if (tombstones[i].used && tombstones[i].stationId==stationId &&
        tombstones[i].requestId==requestId) {
      return !generationIsNewer(generation,tombstones[i].generation);
    }
  }
  return false;
}

void rememberTombstone(uint16_t stationId, uint16_t requestId, uint8_t generation) {
  for (uint8_t i=0; i<TOMBSTONE_CAPACITY; ++i) {
    if (tombstones[i].used && tombstones[i].stationId==stationId &&
        tombstones[i].requestId==requestId) {
      if (generationIsNewer(generation,tombstones[i].generation))
        tombstones[i].generation=generation;
      return;
    }
  }
  Tombstone &t=tombstones[tombstoneNext];
  t.stationId=stationId; t.requestId=requestId; t.generation=generation; t.used=true;
  tombstoneNext=(uint8_t)((tombstoneNext+1)%TOMBSTONE_CAPACITY);
}

void sendCancelAck(const CampusPacket &p) {
  CampusPacket ack; initPacket(ack, PKT_CANCEL_ACK);
  ack.stationId=p.stationId; ack.requestId=p.requestId; ack.generation=p.generation;
  ack.cartId=DEVICE_ID;
  sendPacket(ack);
}

int findQueued(uint16_t stationId, uint16_t requestId) {
  for (uint8_t i = 0; i < queueCount; ++i)
    if (queueItems[i].used && queueItems[i].stationId == stationId &&
        queueItems[i].requestId == requestId) return i;
  return -1;
}

void sendRequestAck(const CampusPacket &p) {
  CampusPacket ack;
  initPacket(ack, PKT_REQUEST_ACK);
  ack.stationId = p.stationId; ack.requestId = p.requestId;
  ack.generation = p.generation;
  sendPacket(ack);
}

void enqueueRequest(const CampusPacket &p) {
  if (isTombstoned(p.stationId,p.requestId,p.generation)) return;
  if (assignmentActive && activeRequest.stationId == p.stationId &&
      activeRequest.requestId == p.requestId) {
    if (generationIsNewer(p.generation, activeRequest.generation)) {
      assignmentActive = false;
      activeRequest.used = false;
      Serial.println(F("[REVOKED] Previous assignment generation superseded"));
    } else if (p.generation == activeRequest.generation) {
      sendRequestAck(p);
      return;
    } else return;
  }
  int existing = findQueued(p.stationId, p.requestId);
  if (existing >= 0) {
    if (generationIsNewer(p.generation, queueItems[existing].generation)) queueItems[existing].generation = p.generation;
    else if (p.generation != queueItems[existing].generation) return;
    sendRequestAck(p);
    return;
  }
  if (queueCount >= MAX_PENDING_REQUESTS) {
    Serial.println(F("[QUEUE FULL] Request not added"));
    CampusPacket full;
    initPacket(full, PKT_QUEUE_FULL);
    full.stationId = p.stationId; full.requestId = p.requestId; full.generation = p.generation;
    sendPacket(full);
    tone(CART_BUZZER_PIN, 500, 700);
    return;
  }
  PendingRequest &item = queueItems[queueCount++];
  item.stationId = p.stationId;
  item.requestId = p.requestId;
  item.generation = p.generation;
  copyField(item.location, sizeof(item.location), p.location);
  item.used = true;
  sendRequestAck(p);
  tone(CART_BUZZER_PIN, 1200, 100);
  Serial.print(F("[NEW REQUEST] ")); Serial.println(item.location);
}

void removeQueued(uint16_t stationId, uint16_t requestId) {
  int index = findQueued(stationId, requestId);
  if (index < 0) return;
  for (uint8_t i = index; i + 1 < queueCount; ++i) queueItems[i] = queueItems[i + 1];
  if (queueCount) queueCount--;
  if (queueCount < MAX_PENDING_REQUESTS) queueItems[queueCount].used = false;
}

void removeQueuedGeneration(uint16_t stationId, uint16_t requestId, uint8_t generation) {
  int index=findQueued(stationId,requestId);
  if (index<0 || queueItems[index].generation!=generation) return;
  removeQueued(stationId,requestId);
}

void handleCartPacket(const CampusPacket &p) {
  if (p.type == PKT_REQUEST && p.senderId==p.stationId) {
    enqueueRequest(p);
  } else if (p.type == PKT_ASSIGN && p.senderId==p.stationId && p.cartId == DEVICE_ID) {
    if (assignmentActive && activeRequest.stationId == p.stationId &&
        activeRequest.requestId == p.requestId && activeRequest.generation == p.generation) {
      CampusPacket ack; initPacket(ack, PKT_ASSIGN_ACK);
      ack.stationId = p.stationId; ack.requestId = p.requestId;
      ack.cartId = DEVICE_ID; ack.generation = p.generation; sendPacket(ack);
    } else if (!isTombstoned(p.stationId,p.requestId,p.generation) && findQueued(p.stationId, p.requestId) >= 0 && !assignmentActive &&
               queueItems[findQueued(p.stationId, p.requestId)].generation == p.generation) {
      activeRequest.stationId = p.stationId;
      activeRequest.requestId = p.requestId;
      activeRequest.generation = p.generation;
      copyField(activeRequest.location, sizeof(activeRequest.location), p.location);
      activeRequest.used = true;
      assignmentActive = true;
      removeQueued(p.stationId, p.requestId);
      claimRequested = false;
      CampusPacket ack; initPacket(ack, PKT_ASSIGN_ACK);
      ack.stationId = p.stationId; ack.requestId = p.requestId;
      ack.cartId = DEVICE_ID; ack.generation = p.generation; sendPacket(ack);
      Serial.print(F("[ASSIGNED] Proceed to ")); Serial.println(activeRequest.location);
      tone(CART_BUZZER_PIN, 1400, 250);
    }
  } else if ((p.type == PKT_STUDENT_CANCEL || p.type == PKT_EXPIRE) && p.senderId == p.stationId) {
    rememberTombstone(p.stationId,p.requestId,p.generation);
    sendCancelAck(p);
    removeQueuedGeneration(p.stationId, p.requestId, p.generation);
    if (assignmentActive && activeRequest.stationId == p.stationId &&
        activeRequest.requestId == p.requestId && activeRequest.generation == p.generation) {
      assignmentActive = false;
      activeRequest.used = false;
      Serial.println(F("[NOTICE] Active request closed or cancelled"));
      tone(CART_BUZZER_PIN, 600, 350);
    }
  }
}

void claimNextRequest() {
  if (assignmentActive || terminalPending || queueCount == 0) return;
  PendingRequest &item = queueItems[0];
  CampusPacket claim;
  initPacket(claim, PKT_CLAIM);
  claim.stationId = item.stationId;
  claim.requestId = item.requestId;
  claim.generation = item.generation;
  claim.cartId = DEVICE_ID;
  sendPacket(claim);
  claimRequested = true;
  lastClaimAt = millis();
  Serial.print(F("[CLAIM] ")); Serial.println(item.location);
  // Wait for station's explicit ASSIGN; do not mark active on claim alone.
}

void updateCartLcd() {
  lcd.clear();
  if (assignmentActive) {
    lcd.setCursor(0, 0); lcd.print(F("ASSIGNED: "));
    lcd.print(activeRequest.location);
    lcd.setCursor(0, 1); lcd.print(F("ID "));
    lcd.print(activeRequest.stationId); lcd.print('-'); lcd.print(activeRequest.requestId);
  } else if (queueCount > 0) {
    lcd.setCursor(0, 0); lcd.print(F("NEXT: "));
    lcd.print(queueItems[0].location);
    lcd.setCursor(0, 1); lcd.print(F("WAITING: "));
    lcd.print(queueCount); lcd.print(F("  ACCEPT"));
  } else {
    lcd.setCursor(0, 0); lcd.print(F("CAMPUS CART "));
    lcd.print(DEVICE_ID);
    lcd.setCursor(0, 1); lcd.print(F("NO REQUESTS"));
  }
}

void serviceCart() {
  if (receivePacket(rxPacket)) {
    if (terminalPending && rxPacket.senderId==terminalPacket.stationId && rxPacket.stationId==terminalPacket.stationId && rxPacket.requestId==terminalPacket.requestId && rxPacket.generation==terminalPacket.generation &&
        ((terminalPacket.type==PKT_CART_CANCEL && rxPacket.type==PKT_CART_CANCEL_ACK) || (terminalPacket.type==PKT_COMPLETE && rxPacket.type==PKT_COMPLETE_ACK))) {
      terminalPending=false; Serial.println(F("[TERMINAL ACK] Station processed cart operation"));
    } else handleCartPacket(rxPacket);
  }

  if (terminalPending && millis()-lastTerminalTxAt>=TERMINAL_RETRY_INTERVAL_MS) {
    if (terminalAttempts<TERMINAL_MAX_RETRIES) { sendPacket(terminalPacket); terminalAttempts++; lastTerminalTxAt=millis(); }
    else { terminalPending=false; Serial.println(F("[TERMINAL UNCONFIRMED] Local cart action retained; station status may be stale")); }
  }

  bool acceptDown = debouncedButtonState(CART_ACCEPT_PIN, 0);
  if (acceptDown && !acceptWasDown) claimNextRequest();
  acceptWasDown = acceptDown;

  bool cancelDown = debouncedButtonState(CART_CANCEL_PIN, 1);
  if (cancelDown && !cancelWasDown) {
    cancelPressedAt = millis();
    cancelHandled = false;
  }
  if (cancelDown && !cancelHandled && assignmentActive &&
      millis() - cancelPressedAt >= BUTTON_HOLD_CANCEL_MS) {
    initPacket(terminalPacket, PKT_CART_CANCEL);
    terminalPacket.stationId=activeRequest.stationId; terminalPacket.requestId=activeRequest.requestId;
    terminalPacket.cartId=DEVICE_ID; terminalPacket.generation=activeRequest.generation;
    sendPacket(terminalPacket); terminalPending=true; terminalAttempts=1; lastTerminalTxAt=millis();
    rememberTombstone(activeRequest.stationId,activeRequest.requestId,activeRequest.generation);
    assignmentActive=false; activeRequest.used=false; cancelHandled=true;
    Serial.println(F("[CANCEL PENDING] Waiting for station MCU acknowledgement"));
    tone(CART_BUZZER_PIN, 600, 500);
  }
  cancelWasDown = cancelDown;

  bool completeDown = debouncedButtonState(CART_COMPLETE_PIN, 2);
  if (completeDown && !completeWasDown && assignmentActive) {
    initPacket(terminalPacket, PKT_COMPLETE);
    terminalPacket.stationId=activeRequest.stationId; terminalPacket.requestId=activeRequest.requestId;
    terminalPacket.cartId=DEVICE_ID; terminalPacket.generation=activeRequest.generation;
    sendPacket(terminalPacket); terminalPending=true; terminalAttempts=1; lastTerminalTxAt=millis();
    rememberTombstone(activeRequest.stationId,activeRequest.requestId,activeRequest.generation);
    assignmentActive=false; activeRequest.used=false;
    Serial.println(F("[COMPLETE PENDING] Waiting for station MCU acknowledgement"));
    tone(CART_BUZZER_PIN, 1500, 150);
  }
  completeWasDown = completeDown;

  if (claimRequested && !assignmentActive && queueCount > 0 &&
      millis() - lastClaimAt >= CLAIM_RETRY_INTERVAL_MS) claimNextRequest();

  if (millis() - lastHeartbeatAt >= CART_HEARTBEAT_INTERVAL_MS) {
    CampusPacket heartbeat;
    initPacket(heartbeat, PKT_HEARTBEAT);
    heartbeat.cartId = DEVICE_ID;
    if (assignmentActive) {
      heartbeat.stationId = activeRequest.stationId;
      heartbeat.requestId = activeRequest.requestId;
      heartbeat.generation = activeRequest.generation;
    }
    sendPacket(heartbeat);
    lastHeartbeatAt = millis();
  }

  static unsigned long lastLcdAt = 0;
  if (millis() - lastLcdAt >= 500) {
    updateCartLcd();
    lastLcdAt = millis();
  }
  digitalWrite(CART_LED_PIN, assignmentActive ? HIGH : (queueCount ? (millis()/250)%2 : LOW));
}
#endif

void setup() {
  Serial.begin(9600);
  randomSeed(analogRead(A0) ^ micros() ^ millis());
#if DEVICE_ROLE == DEVICE_ROLE_STATION
  pinMode(STATION_BUTTON_PIN, INPUT_PULLUP);
  pinMode(STATION_LED_R_PIN, OUTPUT);
  pinMode(STATION_LED_G_PIN, OUTPUT);
  pinMode(STATION_LED_B_PIN, OUTPUT);
  pinMode(STATION_BUZZER_PIN, OUTPUT);
  EEPROM.get(0, nextRequestId);
  if (nextRequestId == 0 || nextRequestId == 0xFFFF) nextRequestId = 1;
  Serial.println(F("Campus assistance - STATION role"));
  Serial.print(F("Station ID: ")); Serial.println(DEVICE_ID);
  Serial.print(F("Location: ")); Serial.println(LOCATION_NAME);
#elif DEVICE_ROLE == DEVICE_ROLE_CART
  pinMode(CART_ACCEPT_PIN, INPUT_PULLUP);
  pinMode(CART_CANCEL_PIN, INPUT_PULLUP);
  pinMode(CART_COMPLETE_PIN, INPUT_PULLUP);
  pinMode(CART_LED_PIN, OUTPUT);
  pinMode(CART_BUZZER_PIN, OUTPUT);
  memset(queueItems, 0, sizeof(queueItems));
  memset(tombstones, 0, sizeof(tombstones));
  Wire.begin();
  lcd.init();
  lcd.backlight();
  Serial.println(F("Campus assistance - CART role"));
  Serial.print(F("Cart ID: ")); Serial.println(DEVICE_ID);
#else
#error "DEVICE_ROLE must be DEVICE_ROLE_STATION or DEVICE_ROLE_CART"
#endif

  LoRa.setPins(LORA_NSS_PIN, LORA_RESET_PIN, LORA_DIO0_PIN);
  if (!LoRa.begin(LORA_FREQUENCY_HZ)) {
    Serial.println(F("[FATAL] LoRa init failed"));
    while (true) delay(1000);
  }
  LoRa.enableCrc();
  Serial.println(F("[RADIO] Ready"));
}

void loop() {
#if DEVICE_ROLE == DEVICE_ROLE_STATION
  serviceStation();
#else
  serviceCart();
#endif
}
