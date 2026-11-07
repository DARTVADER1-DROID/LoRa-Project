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
#define DEVICE_ROLE         DEVICE_ROLE_STATION  // Change to DEVICE_ROLE_CART on cart

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
#define STATION_LED_PIN     4
#define STATION_BUZZER_PIN  5

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
#define REQUEST_ACK_TIMEOUT_MS        3000UL
#define CART_HEARTBEAT_INTERVAL_MS    5000UL
#define CART_OFFLINE_TIMEOUT_MS      30000UL
#define CLAIM_WINDOW_MS                900UL
#define BUTTON_HOLD_CANCEL_MS         3000UL
#define MAX_PENDING_REQUESTS             10
#define MAX_REQUEST_RETRIES               3
// ====================================================================

#include <SPI.h>
#include <LoRa.h>
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
  PKT_COMPLETE = 7, PKT_HEARTBEAT = 8, PKT_EXPIRE = 9
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
unsigned long lastRadioSendAt = 0;

#if DEVICE_ROLE == DEVICE_ROLE_STATION
bool requestActive = false;
bool requestAssigned = false;
bool claimWindowOpen = false;
uint16_t activeRequestId = 0;
uint16_t assignedCartId = 0;
uint16_t candidateCartId = 0;
uint8_t assignmentGeneration = 1;
unsigned long requestStartedAt = 0;
unsigned long lastRequestTxAt = 0;
unsigned long claimWindowStartedAt = 0;
unsigned long assignedCartLastSeenAt = 0;
unsigned long buttonPressedAt = 0;
bool buttonWasDown = false;
uint8_t retryCount = 0;
#else
PendingRequest queueItems[MAX_PENDING_REQUESTS];
uint8_t queueCount = 0;
bool assignmentActive = false;
PendingRequest activeRequest;
bool acceptPending = false;
unsigned long lastHeartbeatAt = 0;
unsigned long cancelPressedAt = 0;
bool cancelWasDown = false;
bool cancelHandled = false;
bool acceptWasDown = false;
bool completeWasDown = false;
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
  lastRadioSendAt = millis();
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
  digitalWrite(STATION_LED_PIN, assigned ? HIGH : (active ? (millis() / 250) % 2 : LOW));
}

void startRequest() {
  if (requestActive) return;
  requestActive = true;
  requestAssigned = false;
  claimWindowOpen = false;
  activeRequestId = nextRequestId++;
  assignedCartId = 0;
  candidateCartId = 0;
  assignmentGeneration = 1;
  retryCount = 0;
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
  requestActive = false;
  requestAssigned = false;
  claimWindowOpen = false;
  assignedCartId = 0;
  Serial.println(F("[CANCEL] Request closed locally"));
  tone(STATION_BUZZER_PIN, 900, 250);
}

void handleStationPacket(const CampusPacket &p) {
  if (p.stationId != DEVICE_ID || p.requestId != activeRequestId) return;
  if (p.generation != assignmentGeneration) return;

  if (p.type == PKT_REQUEST_ACK) {
    Serial.print(F("[RECEIVED] Cart ")); Serial.println(p.senderId);
  } else if (p.type == PKT_CLAIM && requestActive && !requestAssigned) {
    if (!claimWindowOpen) {
      claimWindowOpen = true;
      claimWindowStartedAt = millis();
      candidateCartId = p.senderId;
      Serial.print(F("[CLAIM] Candidate cart ")); Serial.println(candidateCartId);
    }
  } else if (p.type == PKT_HEARTBEAT && requestAssigned && p.senderId == assignedCartId) {
    assignedCartLastSeenAt = millis();
  } else if (p.type == PKT_CART_CANCEL && requestAssigned && p.cartId == assignedCartId) {
    requestAssigned = false;
    assignedCartId = 0;
    assignmentGeneration++;
    claimWindowOpen = false;
    lastRequestTxAt = 0;
    Serial.println(F("[CART CANCEL] Request returned to pending queue"));
    tone(STATION_BUZZER_PIN, 700, 400);
  } else if (p.type == PKT_COMPLETE && requestAssigned && p.cartId == assignedCartId) {
    requestActive = false;
    requestAssigned = false;
    assignedCartId = 0;
    Serial.println(F("[COMPLETE] Journey completed"));
    tone(STATION_BUZZER_PIN, 1500, 150);
  }
}

void serviceStation() {
  if (receivePacket(rxPacket)) handleStationPacket(rxPacket);

  bool down = digitalRead(STATION_BUTTON_PIN) == LOW;
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
      if (retryCount < MAX_REQUEST_RETRIES) retryCount++;
      broadcastRequest();
    }
  }

  if (claimWindowOpen && millis() - claimWindowStartedAt >= CLAIM_WINDOW_MS) {
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
    assignedCartLastSeenAt = millis();
    claimWindowOpen = false;
    Serial.print(F("[ASSIGNED] Cart ")); Serial.println(assignedCartId);
    tone(STATION_BUZZER_PIN, 1300, 180);
  }

  if (requestAssigned && millis() - assignedCartLastSeenAt >= CART_OFFLINE_TIMEOUT_MS) {
    requestAssigned = false;
    assignedCartId = 0;
    assignmentGeneration++;
    claimWindowOpen = false;
    lastRequestTxAt = 0;
    Serial.println(F("[OFFLINE] Cart heartbeat lost; request rebroadcast"));
    tone(STATION_BUZZER_PIN, 600, 500);
  }
  setStationStatus(requestActive, requestAssigned);
}

#else

int findQueued(uint16_t stationId, uint16_t requestId) {
  for (uint8_t i = 0; i < queueCount; ++i)
    if (queueItems[i].used && queueItems[i].stationId == stationId &&
        queueItems[i].requestId == requestId) return i;
  return -1;
}

void enqueueRequest(const CampusPacket &p) {
  if (findQueued(p.stationId, p.requestId) >= 0) return;
  if (queueCount >= MAX_PENDING_REQUESTS) {
    Serial.println(F("[QUEUE FULL] Request not added"));
    tone(CART_BUZZER_PIN, 500, 700);
    return;
  }
  PendingRequest &item = queueItems[queueCount++];
  item.stationId = p.stationId;
  item.requestId = p.requestId;
  item.generation = p.generation;
  copyField(item.location, sizeof(item.location), p.location);
  item.used = true;
  CampusPacket ack;
  initPacket(ack, PKT_REQUEST_ACK);
  ack.stationId = p.stationId; ack.requestId = p.requestId;
  ack.generation = p.generation;
  delay((unsigned long)random(40, 350));
  sendPacket(ack);
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

void handleCartPacket(const CampusPacket &p) {
  if (p.type == PKT_REQUEST) {
    enqueueRequest(p);
  } else if (p.type == PKT_ASSIGN && p.cartId == DEVICE_ID) {
    if (findQueued(p.stationId, p.requestId) >= 0 && !assignmentActive) {
      activeRequest.stationId = p.stationId;
      activeRequest.requestId = p.requestId;
      activeRequest.generation = p.generation;
      copyField(activeRequest.location, sizeof(activeRequest.location), p.location);
      activeRequest.used = true;
      assignmentActive = true;
      removeQueued(p.stationId, p.requestId);
      Serial.print(F("[ASSIGNED] Proceed to ")); Serial.println(activeRequest.location);
      tone(CART_BUZZER_PIN, 1400, 250);
    }
  } else if (p.type == PKT_STUDENT_CANCEL || p.type == PKT_EXPIRE ||
             p.type == PKT_CART_CANCEL || p.type == PKT_COMPLETE) {
    removeQueued(p.stationId, p.requestId);
    if (assignmentActive && activeRequest.stationId == p.stationId &&
        activeRequest.requestId == p.requestId) {
      assignmentActive = false;
      activeRequest.used = false;
      Serial.println(F("[NOTICE] Active request closed or cancelled"));
      tone(CART_BUZZER_PIN, 600, 350);
    }
  }
}

void claimNextRequest() {
  if (assignmentActive || queueCount == 0) return;
  PendingRequest &item = queueItems[0];
  CampusPacket claim;
  initPacket(claim, PKT_CLAIM);
  claim.stationId = item.stationId;
  claim.requestId = item.requestId;
  claim.generation = item.generation;
  claim.cartId = DEVICE_ID;
  sendPacket(claim);
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
  if (receivePacket(rxPacket)) handleCartPacket(rxPacket);

  bool acceptDown = digitalRead(CART_ACCEPT_PIN) == LOW;
  if (acceptDown && !acceptWasDown) claimNextRequest();
  acceptWasDown = acceptDown;

  bool cancelDown = digitalRead(CART_CANCEL_PIN) == LOW;
  if (cancelDown && !cancelWasDown) {
    cancelPressedAt = millis();
    cancelHandled = false;
  }
  if (cancelDown && !cancelHandled && assignmentActive &&
      millis() - cancelPressedAt >= BUTTON_HOLD_CANCEL_MS) {
    CampusPacket p;
    initPacket(p, PKT_CART_CANCEL);
    p.stationId = activeRequest.stationId;
    p.requestId = activeRequest.requestId;
    p.cartId = DEVICE_ID;
    p.generation = activeRequest.generation;
    sendPacket(p);
    assignmentActive = false;
    activeRequest.used = false;
    cancelHandled = true;
    Serial.println(F("[CANCEL] Assignment cancellation broadcast"));
    tone(CART_BUZZER_PIN, 600, 500);
  }
  cancelWasDown = cancelDown;

  bool completeDown = digitalRead(CART_COMPLETE_PIN) == LOW;
  if (completeDown && !completeWasDown && assignmentActive) {
    CampusPacket p;
    initPacket(p, PKT_COMPLETE);
    p.stationId = activeRequest.stationId;
    p.requestId = activeRequest.requestId;
    p.cartId = DEVICE_ID;
    p.generation = activeRequest.generation;
    sendPacket(p);
    assignmentActive = false;
    activeRequest.used = false;
    Serial.println(F("[COMPLETE] Journey marked complete"));
    tone(CART_BUZZER_PIN, 1500, 150);
  }
  completeWasDown = completeDown;

  if (millis() - lastHeartbeatAt >= CART_HEARTBEAT_INTERVAL_MS) {
    CampusPacket heartbeat;
    initPacket(heartbeat, PKT_HEARTBEAT);
    heartbeat.cartId = DEVICE_ID;
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
  randomSeed(analogRead(A0) ^ micros());
#if DEVICE_ROLE == DEVICE_ROLE_STATION
  pinMode(STATION_BUTTON_PIN, INPUT_PULLUP);
  pinMode(STATION_LED_PIN, OUTPUT);
  pinMode(STATION_BUZZER_PIN, OUTPUT);
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
