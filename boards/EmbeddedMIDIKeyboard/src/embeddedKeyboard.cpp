// ==============================================================
// ESP32-S3 BLE MIDI Touch Controller with MPR121 + TCA9548A
// Description:
//   - Scans 15 MPR121 sensors via 2 TCA9548A I²C multiplexers
//   - Sends BLE MIDI NoteOn/NoteOff and Control Change #74 (expression)
//   - Each logical column uses 13 electrodes: 12 from current MPR121,
//     plus 1 from the *next* sensor’s electrode 0.
//   - Last 2 columns (out of 15) may miss the final electrode
//   - Retry logic and watchdog timer for robust operation
// ==============================================================

#include <Arduino.h>             // Base Arduino support
#include <Wire.h>                // I²C support
#include <BLEMidi.h>             // BLE MIDI library
#include <Adafruit_MPR121.h>     // Capacitive touch controller
#include "esp_task_wdt.h"        // Watchdog for system stability

#define SDA_PIN 20
#define SCL_PIN 21
TwoWire I2CBus = TwoWire(1);     // Use I2C Bus 1 for TCA/MPR communication

#define TCA1_ADDR 0x70
#define TCA2_ADDR 0x71
#define NUM_SENSORS 15
#define MIDI_CC 74
#define MAX_RETRIES 5

Adafruit_MPR121 mpr[NUM_SENSORS];
bool sensorPresent[NUM_SENSORS] = {false};
uint16_t lastTouched[NUM_SENSORS] = {0};
bool connected = false;

// MIDI note numbers (C1=36 to D#2=50) — one per sensor/column
const uint8_t midiNotes[NUM_SENSORS] = {
  36, 37, 38, 39, 40, 41, 42,
  43, 44, 45, 46, 47, 48, 49, 50
};

// ========== I²C Multiplexer Channel Select ==========
void tcaSelect(uint8_t tcaAddr, uint8_t channel) {
  if (channel > 7) return;
  I2CBus.beginTransmission(tcaAddr);
  I2CBus.write(1 << channel);
  I2CBus.endTransmission();
}

// ========== Retry TCA availability ==========
bool checkTCA(uint8_t addr, uint8_t retries = MAX_RETRIES, uint32_t delayMs = 500) {
  for (uint8_t i = 0; i < retries; i++) {
    I2CBus.beginTransmission(addr);
    if (I2CBus.endTransmission() == 0) return true;
    delay(delayMs);
  }
  return false;
}

// ========== MIDI Functions ==========
void sendNoteOn(uint8_t channel, uint8_t note, uint8_t velocity) {
  BLEMidiServer.noteOn(channel, note, velocity);
  Serial.printf("NoteOn: ch=%d note=%d vel=%d\n", channel, note, velocity);
}
void sendNoteOff(uint8_t channel, uint8_t note, uint8_t velocity) {
  BLEMidiServer.noteOff(channel, note, velocity);
  Serial.printf("NoteOff: ch=%d note=%d vel=%d\n", channel, note, velocity);
}
void sendCC(uint8_t channel, uint8_t cc, uint8_t value) {
  BLEMidiServer.controlChange(channel, cc, value);
  Serial.printf("CC: ch=%d cc=%d val=%d\n", channel, cc, value);
}

// ========== System Setup ==========
void setup() {
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  I2CBus.begin(SDA_PIN, SCL_PIN, 400000);         // 400kHz I2C
  esp_task_wdt_init(8, true);                     // 8s watchdog
  esp_task_wdt_add(NULL);

  Serial.println("Checking TCA multiplexers...");

  if (!checkTCA(TCA1_ADDR)) {
    Serial.println("❌ TCA 0x70 not responding. Restarting...");
    delay(1000); ESP.restart();
  }
  Serial.println("✅ TCA 0x70 OK");

  if (!checkTCA(TCA2_ADDR)) {
    Serial.println("❌ TCA 0x71 not responding. Restarting...");
    delay(1000); ESP.restart();
  }
  Serial.println("✅ TCA 0x71 OK");

  Serial.println("Initializing MPR121 sensors...");
  int found = 0;

  for (uint8_t i = 0; i < NUM_SENSORS; i++) {
    uint8_t addr = (i < 8) ? TCA1_ADDR : TCA2_ADDR;
    uint8_t chan = (i < 8) ? i : (i - 8);
    tcaSelect(addr, chan);
    delay(5);

    Serial.printf("→ Scanning MPR%d on TCA 0x%02X Ch%d... ", i, addr, chan);
    if (mpr[i].begin(0x5A, &I2CBus)) {
      Serial.println("✅ FOUND");
      sensorPresent[i] = true;
      found++;
    } else {
      Serial.println("❌ Not found");
    }
  }

  if (found == 0) {
    Serial.println("❌ No MPR121s found. Restarting...");
    delay(1000); ESP.restart();
  }

  Serial.printf("✅ %d MPR121s initialized.\n", found);
  BLEMidiServer.begin("MIDIble");
  Serial.println("Waiting for BLE connection...");
}

// ========== Main Loop: BLE connection, sensor read, MIDI transmit ==========
void loop() {
  esp_task_wdt_reset();  // Feed watchdog

  if (BLEMidiServer.isConnected()) {
    if (!connected) {
      connected = true;
      Serial.println("BLE MIDI Connected!");
      digitalWrite(LED_BUILTIN, HIGH);
    }

    static unsigned long lastScan = 0;
    if (millis() - lastScan >= 10) {
      lastScan = millis();

      for (uint8_t col = 0; col < NUM_SENSORS; col++) {
        if (!sensorPresent[col]) continue;

        uint8_t addr = (col < 8) ? TCA1_ADDR : TCA2_ADDR;
        uint8_t chan = (col < 8) ? col : (col - 8);
        tcaSelect(addr, chan);
        delayMicroseconds(300);

        uint16_t touched = mpr[col].touched();
        if (touched == 0xFFFF) {
          Serial.printf("⚠️ Read error on sensor %d\n", col);
          continue;
        }

        for (uint8_t e = 0; e < 13; e++) {
          bool nowTouch = false;

          if (e < 12) {
            nowTouch = touched & (1 << e);
          } else if (col + 1 < NUM_SENSORS && sensorPresent[col + 1]) {
            uint8_t nextAddr = (col + 1 < 8) ? TCA1_ADDR : TCA2_ADDR;
            uint8_t nextChan = (col + 1 < 8) ? (col + 1) : (col + 1 - 8);
            tcaSelect(nextAddr, nextChan);
            delayMicroseconds(300);
            uint16_t nextTouch = mpr[col + 1].touched();
            nowTouch = nextTouch & (1 << 0);  // Electrode 0 of next sensor
            tcaSelect(addr, chan);            // Restore current
          }

          bool prevTouch = lastTouched[col] & (1 << e);

          if (nowTouch && !prevTouch) {
            uint8_t note = midiNotes[col];
            uint8_t ccVal = map(e, 0, 12, 0, 127);
            sendNoteOn(0, note, 127);
            sendCC(0, MIDI_CC, ccVal);
            Serial.printf("Touch Detected: Col %d Electrode %d → Note %d, CC %d\n", col, e, note, ccVal);
          }

          if (!nowTouch && prevTouch) {
            sendNoteOff(0, midiNotes[col], 0);
          }
        }

        lastTouched[col] = touched;  // Update last state
      }
    }

  } else {
    if (connected) {
      connected = false;
      Serial.println("BLE MIDI Disconnected.");
      digitalWrite(LED_BUILTIN, LOW);
    }
  }
}
