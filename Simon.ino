//
// This is the Adafruit Qt Pi ESP32 code
//

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>

#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define SEQCOUNT_CHAR_UUID  "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define MAX_LEN_CHAR_UUID   "beb5483f-36e1-4688-b7f5-ea07361b26a8"
#define KEY_TIMEOUT_UUID    "beb54840-36e1-4688-b7f5-ea07361b26a8"

#define RED_LED 14
#define GRN_LED 13
#define YEL_LED 12
#define BLU_LED 7
#define RED_SW 27
#define GRN_SW 25
#define YEL_SW 26
#define BLU_SW 15

#define BUZZER_OUT 32

#define WAITING_FOR_CONNECTION 0
#define WAITING_FOR_START 1
#define ACTIVE_GAME 2 

#define MAX_POSSIBLE_SEQ_LEN 40
#define INITIAL_MAX_SEQ_LEN  16
#define INITIAL_KEY_TMIEOUT 2000

const int flashPort[4] = {RED_LED, GRN_LED, YEL_LED, BLU_LED};
const int tones[4] = { 220, 261, 329, 392 };

BLEServer *pServer;
BLEService *pService;
BLECharacteristic *pCharacteristic;
bool deviceConnected = false;

int sequence[MAX_POSSIBLE_SEQ_LEN + 1];
int seqLen = 0;
int maxSeqLen = INITIAL_MAX_SEQ_LEN;
int maxKeyTimeout = INITIAL_KEY_TMIEOUT;
int playing = WAITING_FOR_CONNECTION;
bool flashState = false;

class ServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
    };

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
    }
};

class MaxSeqLenCallback: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      String value = pCharacteristic->getValue();
      if (value.length() > 0) {
        maxSeqLen = (int)value[0];
      }
    }
};

class KeyTimeoutCallback: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      String value = pCharacteristic->getValue();
      if (value.length() > 0) {
        maxKeyTimeout = (int)value[0] * 1000;
      }
    }
};

void setup() {
  Serial.begin(115200);

  BLEDevice::init("SimonGame");
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new ServerCallbacks());

  pService = pServer->createService(SERVICE_UUID);
  pCharacteristic = pService->createCharacteristic(SEQCOUNT_CHAR_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
  pCharacteristic->addDescriptor(new BLE2902());

  BLECharacteristic *pChar;
  pChar = pService->createCharacteristic(MAX_LEN_CHAR_UUID, BLECharacteristic::PROPERTY_WRITE);
  pChar->setCallbacks(new MaxSeqLenCallback());
  pChar = pService->createCharacteristic(KEY_TIMEOUT_UUID, BLECharacteristic::PROPERTY_WRITE);
  pChar->setCallbacks(new KeyTimeoutCallback());

  pService->start();
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);  // functions that help with iPhone connections issue
  pAdvertising->setMinPreferred(0x12);
  BLEDevice::startAdvertising();

  pinMode(RED_LED, OUTPUT);
  pinMode(GRN_LED, OUTPUT);
  pinMode(BLU_LED, OUTPUT);
  pinMode(YEL_LED, OUTPUT);
  pinMode(RED_SW, INPUT_PULLUP);
  pinMode(GRN_SW, INPUT_PULLUP);
  pinMode(BLU_SW, INPUT_PULLUP);
  pinMode(YEL_SW, INPUT_PULLUP);
  delay(2000);
  digitalWrite(RED_LED, HIGH);
  digitalWrite(GRN_LED, HIGH);
  digitalWrite(BLU_LED, HIGH);
  digitalWrite(YEL_LED, HIGH);
}

void extendSequence() {
  int val = millis();
  val &= 3;
  if (seqLen > 0 && sequence[seqLen-1] == val) {
    val = (val + 1) & 3;
  }
  sequence[seqLen] = val;
  seqLen += 1;
}

void light(int n, int val) {
  int port = flashPort[n];
  digitalWrite(port, val);
}

void clearLights() {
  for (int i=0; i<4; i++) {
    light(i, HIGH);
  }
}

void flash(int n) {
  delay(100);
  light(n, LOW);
  tone(BUZZER_OUT, tones[n], 500);
  delay(600);
  light(n, HIGH);
}

void flashFailure() {
  for (int i=0; i<4; i++) {
    tone(BUZZER_OUT, 180, 150);
    delay(100);
    digitalWrite(RED_LED, LOW);
    digitalWrite(GRN_LED, LOW);
    digitalWrite(BLU_LED, LOW);
    digitalWrite(YEL_LED, LOW);
    delay(100);
    digitalWrite(RED_LED, HIGH);
    digitalWrite(GRN_LED, HIGH);
    digitalWrite(BLU_LED, HIGH);
    digitalWrite(YEL_LED, HIGH);
  }
}

void flashComplete() {
  for (int i=0; i<4; i++) {
    tone(BUZZER_OUT, 392, 350);
    delay(200);
    digitalWrite(RED_LED, LOW);
    digitalWrite(GRN_LED, LOW);
    digitalWrite(BLU_LED, LOW);
    digitalWrite(YEL_LED, LOW);
    delay(200);
    digitalWrite(RED_LED, HIGH);
    digitalWrite(GRN_LED, HIGH);
    digitalWrite(BLU_LED, HIGH);
    digitalWrite(YEL_LED, HIGH);
  }
}

bool anyKeyPressed() {
  int val;
  while (true) {
    if (digitalRead(RED_SW) == LOW) {
      return true;
    }
    if (digitalRead(GRN_SW) == LOW) {
      return true;
    }
    if (digitalRead(YEL_SW) == LOW) {
      return true;
    }
    if (digitalRead(BLU_SW) == LOW) {
      return true;
    }
    break;
  }
  return false;
}

int readInput() {
  int val = -1;
  long now = millis();
  while (millis() - now < maxKeyTimeout) {
    if (digitalRead(RED_SW) == LOW) {
      continue;
    }
    if (digitalRead(GRN_SW) == LOW) {
      continue;
    }
    if (digitalRead(YEL_SW) == LOW) {
      continue;
    }
    if (digitalRead(BLU_SW) == LOW) {
      continue;
    }
    break;
  }
  while (millis() - now < maxKeyTimeout) {
    if (digitalRead(RED_SW) == LOW) {
      val = 0;
      break;
    }
    if (digitalRead(GRN_SW) == LOW) {
      val = 1;
      break;
    }
    if (digitalRead(YEL_SW) == LOW) {
      val = 2;
      break;
    }
    if (digitalRead(BLU_SW) == LOW) {
      val = 3;
      break;
    }
  }
  if (val != -1) {
    flash(val);
  }
  return val;
}

void delayForRandomTime() {
  int d = millis() % 8;
  d += 6;
  d *= 500;
  delay(d);
}

bool readAndCheckInput() {
  for (int i=0; i<seqLen; i++) {
    int val = readInput();
    if (val != sequence[i]) {
      return false;
    }
  }
  return true;
}

void playSequence() {
  for (int i=0; i<seqLen; i++) {
    flash(sequence[i]);
  }
}

int deviceDisconnected() {
  if (deviceConnected == false) {
    playing = WAITING_FOR_CONNECTION;
    delay(500);
    pServer->startAdvertising();
  }
  return deviceConnected == false;
}

void loop() {
  switch (playing) {
    case WAITING_FOR_CONNECTION:
      delay(400);
      digitalWrite(RED_LED, flashState);
      digitalWrite(GRN_LED, flashState);
      digitalWrite(BLU_LED, flashState);
      digitalWrite(YEL_LED, flashState);
      flashState = !flashState;
      if (deviceConnected == true) {
        playing = WAITING_FOR_START;
        clearLights();
      }
      break;
    case WAITING_FOR_START:
      if (deviceDisconnected()) {
        break;
      }
      delay(200);
      digitalWrite(RED_LED, flashState);
      digitalWrite(GRN_LED, !flashState);
      digitalWrite(BLU_LED, !flashState);
      digitalWrite(YEL_LED, flashState);
      flashState = !flashState;
      if (anyKeyPressed()) {
        seqLen = 0;
        playing = ACTIVE_GAME;
        clearLights();
      }
      break;
    case ACTIVE_GAME:
      if (deviceDisconnected()) {
        break;
      }
      if (seqLen < maxSeqLen) {
        delayForRandomTime();
        extendSequence();
        playSequence();

        String msg;
        if (readAndCheckInput()) {
          msg = "Success:";
        } else {
          msg = "Failure:";
          flashFailure();
          seqLen = 0;
          playing = WAITING_FOR_START;
        }
        msg = msg + seqLen;
        pCharacteristic->setValue(msg);
        pCharacteristic->notify();
      } else {
        pCharacteristic->setValue("Complete");
        pCharacteristic->notify();
        flashComplete();
        playing = WAITING_FOR_START;
      }    
      break;
    }
  }
