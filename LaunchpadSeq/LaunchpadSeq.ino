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
#define ARDUINO 1

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
#define ENCODER_CLK_PIN      6
#define ENCODER_DT_PIN       7
#define ENCODER_SW_PIN       8

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

typedef enum MenuState {
    kMenuState_Main,
    kMenuState_PresetSelection,
    kMenuState_Settings
} MenuState;

const uint8_t outputs[N_TRIGGERS] = { 30, 31, 32, 33, 34, 35, 36, 37 };

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
volatile size_t selectionIndex = 0;   // for fun buttons and pattern/sequence selection
volatile MenuState menuState = kMenuState_Main;

#define MENU_ITEM_COUNT 4
volatile uint8_t menuItemIndex  = 0;
volatile int8_t  encoderDir     = 0;   // +1 CW, -1 CCW, consumed in loop()
volatile bool    pendingEncoderBtn = false;



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
void updateOLEDDisplay(void);
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

// ── Menu ──────────────────────────────────────────────────────────────────────

typedef void (*MenuAction)(void);
typedef struct { MenuAction action; } MenuItem;

void loadPreset(uint8_t presetIndex) {
    const ls_preset_t *p = &ls_presets[presetIndex];
    step_sequence_t   *sq = sequencer_getCurrentSequence(&sequencer);
    seq_clearAllPatterns(sq);
    for (uint8_t t = 0; t < N_TRIGGERS; t++) {
        for (uint8_t s = 0; s < DEFAULT_STEPS; s++)
            seq_setPatternStepValue(sq, t, s, p->patterns[t][s]);
        seq_setLastStepIndex(sq, t, p->last_steps[t] - 1);
    }
    ls_updateDisplay(&ls);
}

void menuAction_playPause(void) {
    if (sequencer_getState(&sequencer) == kSequencerState_Playing)
        sequencer_pause(&sequencer);
    else
        sequencer_play(&sequencer);
    updateOLEDDisplay();
}

void menuAction_stop(void) {
    sequencer_stop(&sequencer);
    updateOLEDDisplay();
}

void menuAction_toggleDirection(void) {
    sequencer.current_direction =
        (sequencer.current_direction == kDirection_Forward)
        ? kDirection_Backward
        : kDirection_Forward;
    updateOLEDDisplay();
}

void menuAction_openPresets(void) {
    selectionIndex = 0;
    menuState = kMenuState_PresetSelection;
    updateOLEDDisplay();
}

static const MenuItem menuItems[MENU_ITEM_COUNT] = {
    { menuAction_playPause      },
    { menuAction_stop           },
    { menuAction_toggleDirection },
    { menuAction_openPresets    },
};

// ── Interrupt service routines ────────────────────────────────────────────────
// Only set the flag here; the sequencer runs in loop() to avoid calling
// digitalWrite / MIDI send from interrupt context.

void clockISR(void) {
    pendingClockTick = true;
}

void resetISR(void) {
    sequencer_stop(&sequencer);
    sequencer_play(&sequencer);
}

void dirISR(void) {
    sequencer.current_direction =
        (sequencer.current_direction == kDirection_Forward)
        ? kDirection_Backward
        : kDirection_Forward;
}

void encoderISR(void) {
    static uint8_t prevCLK = HIGH;
    uint8_t clk = digitalRead(ENCODER_CLK_PIN);
    if (clk != prevCLK) {
        prevCLK = clk;
        if (clk == LOW)
            encoderDir = (digitalRead(ENCODER_DT_PIN) == HIGH) ? 1 : -1;
    }
}

void encoderBtnISR(void) {
    pendingEncoderBtn = true;
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

void drawPresetsMenu() {
    display.clearDisplay();
    display.setTextSize(FONT_SIZE);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println(F("Presets:"));
    display.println(F("--------------------"));
    for (size_t i = 0; i < LS_PRESETS_COUNT; i++) {
        if (i == selectionIndex) {
            display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);  // Draw 'inverse' text
        } else {
            display.setTextColor(SSD1306_WHITE);
        }
        display.println(ls_presets[i].name);
    }
    display.display();
}

void drawMainMenu() {
    display.clearDisplay();
  
  /*
  _printCLK(sequencer_getClockDivider(&sequencer));
  _printDIR(0);
  display.println(F("")); // Break line
  _printPRESET(1);
    */
  // e.g. top-right corner of the 128x32 display

    drawIcon(0,  8, sequencer_getState(&sequencer) == kSequencerState_Playing ? ICON_PAUSE : ICON_PLAY, ICON_WIDTH, ICON_HEIGHT, menuItemIndex == 0);
    drawIcon(32, 8, ICON_STOP, ICON_WIDTH, ICON_HEIGHT, menuItemIndex == 1);
    drawIcon(64, 8, sequencer_getDirection(&sequencer) == kDirection_Forward ? ICON_ARROW_RIGHT : ICON_ARROW_LEFT, ICON_WIDTH, ICON_HEIGHT, menuItemIndex == 2);
    drawIcon(96, 8, ICON_LETTER_P, ICON_WIDTH, ICON_HEIGHT, menuItemIndex == 3);

    display.display();
}

void updateOLEDDisplay() {
    switch (menuState) {
        case kMenuState_Main:
            drawMainMenu();
            break;
        case kMenuState_PresetSelection:
            drawPresetsMenu();
            break;
        default:
            break;
    }
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

    pinMode(ENCODER_CLK_PIN, INPUT_PULLUP);
    pinMode(ENCODER_DT_PIN,  INPUT_PULLUP);
    pinMode(ENCODER_SW_PIN,  INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(ENCODER_CLK_PIN), encoderISR,    CHANGE);
    attachInterrupt(digitalPinToInterrupt(ENCODER_SW_PIN),  encoderBtnISR, FALLING);

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
    if (display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
        display.setTextSize(FONT_SIZE);
        display.setRotation(2);
        // Show initial display buffer contents on the screen --
        // the library initializes this with an Adafruit splash screen.
        display.display();
        display.clearDisplay(); // Clear the buffer

        drawMainMenu();
    } else {
        Serial.println(F("SSD1306 initialization failed"));
    }

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
        sequencer_clock(&sequencer);
    }

    if (encoderDir != 0) {
        int8_t dir = encoderDir;
        encoderDir = 0;
        if (menuState == kMenuState_Main) {
            menuItemIndex = (menuItemIndex + MENU_ITEM_COUNT + dir) % MENU_ITEM_COUNT;
        } else if (menuState == kMenuState_PresetSelection) {
            selectionIndex = (selectionIndex + LS_PRESETS_COUNT + dir) % LS_PRESETS_COUNT;
        }
        updateOLEDDisplay();
    }

    if (pendingEncoderBtn) {
        pendingEncoderBtn = false;
        static unsigned long lastBtnTime = 0;
        unsigned long now = millis();
        if (now - lastBtnTime > 200) {
            lastBtnTime = now;
            if (menuState == kMenuState_Main) {
                menuItems[menuItemIndex].action();
            } else if (menuState == kMenuState_PresetSelection) {
                loadPreset(selectionIndex);
                menuState = kMenuState_Main;
                updateOLEDDisplay();
            }
        }
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
