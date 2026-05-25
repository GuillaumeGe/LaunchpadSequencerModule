//
//  teensy_main.cpp
//  LaunchpadSeq — Teensy 4.1 port
//
//  Target board : Teensy 4.1
//  Required library: USBHost_t36 (https://github.com/PaulStoffregen/USBHost_t36)
//
//  Wiring:
//    Launchpad USB → Teensy 4.1 USB Host port (5-pin header on the bottom edge)
//    Gate outputs  → pins 30-37 (active HIGH, 3.3 V logic)
//    CLOCK_IN      → pin 2  (optional: external Eurorack clock)
//    RESET         → pin 4  (optional: external reset)
//    DIR           → pin 5  (optional: external direction toggle)
//
//  Clock source: define USE_EXTERNAL_CLOCK 1 to drive the sequencer from
//  CLOCK_IN_PIN instead of the internal IntervalTimer.

#ifdef ARDUINO

#include <Arduino.h>
#include <USBHost_t36.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

extern "C" {
#include "launchpad.h"
#include "sequencer.h"
#include "sequence.h"
#include "utils.h"
#include "oled_bitmaps.h"

}

// ── Configuration ────────────────────────────────────────────────────────────

#define DEBUG                1

#define USE_EXTERNAL_CLOCK   1   // 1 = CLOCK_IN_PIN interrupt, 0 = internal timer
#if !USE_EXTERNAL_CLOCK
    #define INTERNAL_CLOCK_US    20000   // 20 ms → matches original 0.06/3 s interval
#endif
#define CLOCK_IN_PIN         2
#define CLOCK_OUT_PIN        3
#define RESET_PIN            4
#define DIR_PIN              5

#define SCREEN_WIDTH 128  // OLED display width, in pixels
#define SCREEN_HEIGHT 32  // OLED display height, in pixels
#define FONT_SIZE   2
#define OLED_MOSI   9
#define OLED_CLK   10
#define OLED_DC    11
#define OLED_CS    12
//#define OLED_RESET 13
#define OLED_RESET -1        // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C  ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32

// ── DISPLAY -─────────────────────────────────────────────────────────────────

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ── USB Host ─────────────────────────────────────────────────────────────────

USBHost     myusb;
USBHub      hub1(myusb);
MIDIDevice  midi1(myusb);

// ── Globals ───────────────────────────────────────────────────────────────────

launchpad_t      ls;
step_sequencer_t sequencer;

#if !USE_EXTERNAL_CLOCK
IntervalTimer clockTimer;
#endif

// Deferred clock tick — set in ISR, consumed in loop() to keep ISR short
// and avoid calling digitalWrite / MIDI send from interrupt context.
volatile bool pendingClockTick = false;
volatile bool usbControllerConnected = false;

const uint8_t outputs[N_TRIGGERS] = { 30, 31, 32, 33, 34, 35, 36, 37 };


// ── Forward declarations ──────────────────────────────────────────────────────

void wrap_sq_updateTriggers(void *s);
void wrap_sq_updateTrigger(void *s, uint8_t triggerIndex);
void wrap_sq_updateMutedTriggers(void *s, uint8_t triggerIndex);
void wrap_sq_updatePattern(void *s, uint8_t sequenceIndex, uint8_t pI);
void wrap_sq_updateStep(void *s, uint8_t sequenceIndex, uint8_t pI, uint8_t stepIndex);
void wrap_sq_updateState(void *s);
void wrap_sq_updateSequenceIndex(void *s, uint8_t sequenceIndex);
void wrap_sq_updateNextSequenceIndex(void *s);
void wrap_ls_midi_snd(SLMIDIPacket *pkt, uint8_t channel);
void wrap_ls_midi_rcv(SLMIDIPacket *pkt);
bool processFunButton(SLMIDIPacket *packet);
bool processColButton(SLMIDIPacket *packet);
bool processGridButton(SLMIDIPacket *packet);
void updateOutput(size_t outputIndex, uint8_t value);
step_sequence_t *getCurrentSequenceLS(void);

// ── Sequencer → UI callbacks ─────────────────────────────────────────────────

void wrap_sq_updateMutedTriggers(void *s, uint8_t triggerIndex) {
    ls_updateRow(&ls, triggerIndex);
    ls_updateFnButtons(&ls);
    ls_updateOutColumn(&ls);
}

void wrap_sq_updatePattern(void *s, uint8_t sequenceIndex, uint8_t pI) {
    if (ls.current_sequence_index != sequenceIndex) return;
    if (ls.sequence_view_mode == kLaunchpadSequenceViewMode_Paginated)
        ls_updateRow(&ls, pI);
    else
        ls_updateGrid(&ls);
}

void wrap_sq_updateStep(void *s, uint8_t sequenceIndex, uint8_t pI, uint8_t stepIndex) {
    if (ls.sequence_view_mode == kLaunchpadSequenceViewMode_Paginated) {
        if (ls.current_sequence_index == sequenceIndex)
            ls_updateCell(&ls, stepIndex % LS_MAX_STEPS_PER_ROW, pI);
    } else if (ls.sequence_view_mode == kLaunchpadSequenceViewMode_Grid) {
        if (ls.current_sequence_index == sequenceIndex && pI == ls.trigger_index)
            ls_updateCell(&ls, stepIndex % LS_MAX_STEPS_PER_ROW, stepIndex / LS_MAX_STEPS_PER_ROW);
    }
}

void wrap_sq_updateState(void *s) {
    ls_updateDisplay(&ls);
}

void wrap_sq_updateTriggers(void *s) {
    ls_updateOutColumn(&ls);
}

void wrap_sq_updateTrigger(void *s, uint8_t triggerIndex) {
    updateOutput(triggerIndex, sequencer.triggers[triggerIndex]);
}

void wrap_sq_updateSequenceIndex(void *s, uint8_t sequenceIndex) {
    if (ls.auto_follow_sequence)
        ls.current_sequence_index = sequenceIndex;
    if (ls.current_sequence_index == sequenceIndex)
        ls_updateDisplay(&ls);
}

void wrap_sq_updateNextSequenceIndex(void *s) {
    ls_updateDisplay(&ls);
}

// ── MIDI send (Teensy USB Host → Launchpad) ───────────────────────────────────

void wrap_ls_midi_snd(SLMIDIPacket *pkt, uint8_t channel) {
    if (pkt == NULL) return;
    uint8_t status  = pkt->data[0];
    uint8_t d1      = pkt->data[1];
    uint8_t d2      = pkt->data[2];
    uint8_t msgType = status & 0xF0;
    uint8_t ch      = (status & 0x0F) + 1;   // USBHost_t36 uses 1-based channels

    switch (msgType) {
        case 0x90:
          midi1.sendNoteOn(d1, d2, ch);
          break;
        case 0x80:
          midi1.sendNoteOff(d1, d2, ch);
          break;
        case 0xB0:
          midi1.sendControlChange(d1, d2, ch);
          break;
        default:
          break;
    }
}

// ── MIDI receive dispatch ─────────────────────────────────────────────────────

void wrap_ls_midi_rcv(SLMIDIPacket *packet) {
    if (packet == NULL || packet->length < 3) {
      return;
    }      
    if (!processFunButton(packet) && !processColButton(packet)) {
        processGridButton(packet);
    }
#if DEBUG
    if (ls_btnIsDown(packet))
        Serial.printf("RECV: %02X %02X %02X\n",
                      packet->data[0], packet->data[1], packet->data[2]);
#endif
}

// ── USB MIDI callbacks (called from myusb.Task() in main loop) ───────────────

static void onNoteOn(uint8_t ch, uint8_t note, uint8_t vel) {
    SLMIDIPacket pkt = {};
    pkt.length   = 3;
    pkt.data[0]  = 0x90 | (ch - 1);
    pkt.data[1]  = note;
    pkt.data[2]  = vel;
    wrap_ls_midi_rcv(&pkt);
}

static void onNoteOff(uint8_t ch, uint8_t note, uint8_t vel) {
    SLMIDIPacket pkt = {};
    pkt.length   = 3;
    pkt.data[0]  = 0x80 | (ch - 1);
    pkt.data[1]  = note;
    pkt.data[2]  = vel;
    wrap_ls_midi_rcv(&pkt);
}

static void onControlChange(uint8_t ch, uint8_t cc, uint8_t val) {
    SLMIDIPacket pkt = {};
    pkt.length   = 3;
    pkt.data[0]  = 0xB0 | (ch - 1);
    pkt.data[1]  = cc;
    pkt.data[2]  = val;
    wrap_ls_midi_rcv(&pkt);
}

// ── Button processing ─────────────────────────────────────────────────────────

bool processFunButton(SLMIDIPacket *packet) {
    bool result = true;

    if (ls_btnMapValue(packet) == LS_BT_SHIFT) {
        ls.shift_btn_hold = ls_btnIsDown(packet);
        ls_setExtButton(&ls, LS_BT_SHIFT, ls_btnIsDown(packet) ? LS_COLOR_YELLOW : LS_COLOR_NONE);
    } else if (ls_btnMapValue(packet) == LS_BT_CLEAR) {
        ls.clear_btn_hold = ls_btnIsDown(packet);
        ls_setExtButton(&ls, LS_BT_CLEAR, ls_btnIsDown(packet) ? LS_COLOR_RED : LS_COLOR_NONE);
        if (ls.shift_btn_hold && ls.clear_btn_hold)
            seq_clearAllPatterns(getCurrentSequenceLS());
    } else if (ls_btnMapValue(packet) == LS_BT_LEFT_ARROW && ls_btnIsDown(packet)) {
        ls_incrPageIndex(&ls, -1);
    } else if (ls_btnMapValue(packet) == LS_BT_RIGHT_ARROW && ls_btnIsDown(packet)) {
        ls_incrPageIndex(&ls, 1);
    } else if (ls_btnMapValue(packet) == LS_BT_MODE && ls_btnIsDown(packet)) {
        ls.current_view_mode = (LaunchpadViewMode)utils_circularLoopGetIndex(ls.current_view_mode, 1, 3);
        ls_updateDisplay(&ls);
#if DEBUG
    } else if (ls_btnMapValue(packet) == LS_BT_UP_ARROW && ls_btnIsDown(packet)) {
        sequencer.current_direction = kDirection_Forward;
    } else if (ls_btnMapValue(packet) == LS_BT_DOWN_ARROW && ls_btnIsDown(packet)) {
        sequencer.current_direction = kDirection_Backward;
    } else if (ls_btnMapValue(packet) == LS_BT_RESET && ls_btnIsDown(packet)) {
        sequencer_resetCurrentStepIndexes(&sequencer, ls.current_sequence_index);
#endif
    } else {
        result = false;
    }

    return result;
}

bool processColButton(SLMIDIPacket *packet) {
    for (size_t i = 0; i < N_TRIGGERS; i++) {
        uint16_t v = (uint16_t)(0x90 << 8) | (uint16_t)(i << 4) | 0x08;
        if (ls_btnMapValue(packet) == v && ls_btnIsDown(packet)) {
            if (ls.sequence_view_mode == kLaunchpadSequenceViewMode_Paginated) {
                if (ls.clear_btn_hold)
                    seq_clearPattern(getCurrentSequenceLS(), i);
                if (ls.current_view_mode == kLaunchpadViewMode_Mute)
                    sequencer_setMutedPattern(&sequencer, i, !sequencer.muted_triggers[i]);
            } else {
                ls.trigger_index = i;
                ls_updateDisplay(&ls);
            }
            return true;
        }
    }
    return false;
}

bool processGridButton(SLMIDIPacket *packet) {
    if (!ls_btnIsDown(packet)) return false;
    if ((packet->data[0] & 0xF0) != kSLMIDIMessageType_NoteOn) return false;

    uint8_t x = packet->data[1] % 16;
    uint8_t y = (packet->data[1] - x) / 16;

    switch (ls.current_view_mode) {
        case kLaunchpadViewMode_Pattern:
        case kLaunchpadViewMode_Mute:
            if (!ls.shift_btn_hold)
                ls_toggleStep(&ls, x, y);
            else
                ls_updateLastStepIndex(&ls, x, y);
            break;
        case kLaunchpadViewMode_Sequence:
            if (x < 4 && y < 4) {
                uint8_t newSeqIdx = x + 4 * y;
                if (!ls.shift_btn_hold)
                    ls_setCurrentSequenceIndex(&ls, newSeqIdx);
                else
                    sequencer_setNextSequenceIndex(ls.sequencer, newSeqIdx);
            }
            break;
        default:
            break;
    }
    return true;
}

// ── Helpers ───────────────────────────────────────────────────────────────────

step_sequence_t *getCurrentSequenceLS(void) {
    return ls_getCurrentSequence(&ls);
}

// ── GPIO output ───────────────────────────────────────────────────────────────

void updateOutput(size_t outputIndex, uint8_t value) {
    if (outputIndex < N_TRIGGERS)
        digitalWrite(outputs[outputIndex], value > 0 ? HIGH : LOW);
}

// ── Interrupt service routines ────────────────────────────────────────────────
// Only set the flag here; the sequencer runs in loop() to avoid calling
// digitalWrite / MIDI send from interrupt context.

void clockISR(void) {
    pendingClockTick = true;
    //sequencer_clock(&sequencer);
}

void resetISR(void) {
    // Reset is safe to handle directly: only writes volatile fields
    //sequencer.clock_cpt = 0;
    sequencer_stop(&sequencer);
    sequencer_play(&sequencer);
}

void dirISR(void) {
    sequencer.current_direction =
        (sequencer.current_direction == kDirection_Forward)
        ? kDirection_Backward
        : kDirection_Forward;
}

// ── DISPLAY  ────────────────────────────────────────────────

void _printCLK(uint8_t value) {
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);              // Start at top-left corner
  display.setTextSize(FONT_SIZE);               // Draw 2X-scale text
  display.print(F("CLK_DIV:"));
    //display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);  // Draw 'inverse' text
  display.print(value);
  //display.setTextColor(SSD1306_WHITE);
  //display.print(F(" "));

}

void _printDIR(uint8_t size) {
  display.setTextColor(SSD1306_WHITE);
  //display.setCursor(7, 0);              // Start at top-left corner
  display.setTextSize(FONT_SIZE);               // Draw 2X-scale text
  display.print(F("DIR:"));
  
}

void _printPRESET(uint8_t size) {
  display.setTextColor(SSD1306_WHITE);
  //display.setCursor(7, 0);              // Start at top-left corner
  display.setTextSize(FONT_SIZE);               // Draw 2X-scale text
  display.print(F("PRESET: "));
  display.print(F("Custom"));
}

void drawIcon(int16_t x, int16_t y, const uint8_t bitmap[], int16_t w, int16_t h, bool selected) {
    const uint8_t offset = (ICON_PADDING/2);
    if (selected) {
        display.drawRect(offset + x - (ICON_PADDING/2), y - (ICON_PADDING/2), w + ICON_PADDING, h + ICON_PADDING, SSD1306_WHITE);
    }
    display.drawBitmap(offset + x, y, bitmap,  ICON_WIDTH, ICON_HEIGHT, SSD1306_WHITE);
}

void drawMainScreen() {
  display.clearDisplay();
  display.setCursor((ICON_PADDING/2), 0);
  
  /*
  _printCLK(sequencer_getClockDivider(&sequencer));
  _printDIR(0);
  display.println(F("")); // Break line
  _printPRESET(1);
    */
  // e.g. top-right corner of the 128x32 display
    drawIcon(0, 8, ICON_PLAY, ICON_WIDTH, ICON_HEIGHT, true);
    drawIcon(32, 8, ICON_PAUSE, ICON_WIDTH, ICON_HEIGHT, true);
    drawIcon(64, 8, ICON_STOP, ICON_WIDTH, ICON_HEIGHT, true);


  display.display();
}

// ── Arduino entry points ──────────────────────────────────────────────────────

void setup() {
#if DEBUG
    Serial.begin(9600);
#endif

    // Gate output pins
    for (int i = 0; i < N_TRIGGERS; i++) {
        pinMode(outputs[i], OUTPUT);
        digitalWrite(outputs[i], LOW);
    }

    // Control input pins
    pinMode(CLOCK_IN_PIN, INPUT);
    pinMode(RESET_PIN, INPUT);
    pinMode(DIR_PIN, INPUT);

    attachInterrupt(digitalPinToInterrupt(RESET_PIN), resetISR, RISING);
    attachInterrupt(digitalPinToInterrupt(DIR_PIN),   dirISR,   CHANGE);

#if USE_EXTERNAL_CLOCK
    attachInterrupt(digitalPinToInterrupt(CLOCK_IN_PIN), clockISR, RISING);
#else
    clockTimer.begin(clockISR, INTERNAL_CLOCK_US);
#endif

    // USB Host MIDI
    myusb.begin();
    midi1.setHandleNoteOn(onNoteOn);
    midi1.setHandleNoteOff(onNoteOff);
    midi1.setHandleControlChange(onControlChange);

    // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
    if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
        Serial.println(F("SSD1306 allocation failed"));
        for (;;)
        ;  // Don't proceed, loop forever
    }

    display.setTextSize(FONT_SIZE);
    display.setRotation(2);
    // Show initial display buffer contents on the screen --
    // the library initializes this with an Adafruit splash screen.
    display.display();
    display.clearDisplay(); // Clear the buffer
    display.drawPixel(10, 10, SSD1306_WHITE); // Draw a single pixel in white
    // Show the display buffer on the screen. You MUST call display() after
    // drawing commands to make them visible on screen!
    display.display();

    drawMainScreen();

    // Sequencer init
    sequencer_init(&sequencer);
    sequencer.step_updated_cb           = wrap_sq_updateStep;
    sequencer.pattern_updated_cb        = wrap_sq_updatePattern;
    sequencer.state_updated_cb          = wrap_sq_updateState;
    sequencer.triggers_updated_cb       = wrap_sq_updateTriggers;
    sequencer.trigger_updated_cb        = wrap_sq_updateTrigger;
    sequencer.muted_triggers_updated_cb = wrap_sq_updateMutedTriggers;
    sequencer.next_seq_index_updated_cb = wrap_sq_updateNextSequenceIndex;
    sequencer.sequence_index_updated_cb = wrap_sq_updateSequenceIndex;

    // Launchpad init
    ls_init(&ls, &sequencer);
    ls.midi_snd_cb = &wrap_ls_midi_snd;
    ls.midi_rcv_cb = &wrap_ls_midi_rcv;

    sequencer_play(&sequencer);

    // Give USB host time to enumerate the Launchpad before sending LED updates
    delay(500);
    myusb.Task();
    ls_updateDisplay(&ls);

    usbControllerConnected = midi1.product() != NULL;
}

void loop() {
    myusb.Task();   // Pumps USB host: dispatches MIDI callbacks
    midi1.read();

    if (pendingClockTick) {
        pendingClockTick = false;
        //Serial.printf("%d\n",sequencer_getCurrentSequenceIndex(&sequencer));
        sequencer_clock(&sequencer);
    }
    

    // refresh launchpad (after a while or if usb has been reconnected)
    // compare state
    if ((midi1.product() != NULL) != usbControllerConnected) {
        // Cable connected and VBUS is powered
        // if read says it's connected, update ls
        if (!usbControllerConnected) {
            ls_updateDisplay(&ls);
        }

        usbControllerConnected = midi1.product() != NULL;
    }
}

#endif /* ARDUINO */
