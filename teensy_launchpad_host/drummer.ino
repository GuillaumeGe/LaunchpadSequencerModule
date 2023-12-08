#include <stdint.h>
#include <MIDI.h>
#include <FastLED.h>

/*
*       Step: A representation of a value at a given time
*       Pattern: Group of pre-defined number of steps
*       Sequence: Group of patterns
*       Song: Group of sequences
*/

typedef enum Mode {
    kMode_Edit = 0,
    kMode_Instr = 1,
}

typedef enum SequencerState {
    kSequencerState_Stopped,
    kSequencerState_Playing,
    kSequencerState_Paused
}

typedef enum Direction {
    kDirection_Forward = 1,
    kDirection_Backward = -1
}

#define EDIT_COLOR 757575
#define IS_HIGH(value)              value > 0


#define N_B_COLS                    4
#define N_B_ROWS                    4
#define N_TRIGGERS                  N_B_COLS * N_B_ROWS
#define MAX_STEPS                   64
#define MAX_STEPS_ROW               16
#define HOLD_NOTES                  0

#define DEFAUTL_MIDI_IN_CHANNEL     1
#define DEFAUTL_MIDI_OUT_CHANNEL    1

#define CLOCK_IN_PIN                2
#define CLOCK_OUT_PIN               3
#define RESET_PIN                   4

#define N_LEDS                      16              // Number of LEDs in the strip
#define LED_DATA_PIN                6               // Pin connected to the data input of the LED strip


// PINS DEFINITIONS
const uint8_t               b_rows[N_B_ROWS] = {22,24,26,28};
const uint8_t               b_cols[N_B_COLS] = {23,25,27,29};
const uint8_t               t_outputs[N_TRIGGERS] = {
    30,31,32,33,
    34,35,36,37,
    38,39,40,41,
    42,43,44,45
};
// MAPPERS
const uint8_t               b_midi_mapping[N_TRIGGERS] = {
    60,61,62,63,
    64,65,66,67,
    68,69,70,71,
    72,73,74,75
};  //used for midi notes
const uint8_t               b_value_mapping[N_TRIGGERS] = {
    255,255,255,255,
    255,255,255,255,
    255,255,255,255,
    255,255,255,255
};  //might be for velocity or other analog purposes


CRGB                        leds[N_LEDS];                                   // Declare an array to store LED colors

// STATE
uint8_t                     patterns[N_TRIGGERS][MAX_STEPS] = {0};          // N_TRIGGERS triggers with MAX_STEPS steps
bool                        muted_triggers[N_TRIGGERS] = {0};
uint8_t                     current_pattern_index = 0;
Mode                        current_mode = kMode_Instr;
uint8_t                     state_indexes[N_TRIGGERS] = {0};
uint8_t                     last_state_indexes[N_TRIGGERS] = {0};
volatile uint8_t            clock_cpt = 0;
volatile uint8_t            clock_divider = 0;                              //ROM

// MIDI
uitn8_t                     midi_mapping_offset = 0;                        //ROM
uint8_t                     midi_out_channel = DEFAUTL_MIDI_IN_CHANNEL;     //ROM
uint8_t                     midi_in_channel = DEFAUTL_MIDI_OUT_CHANNEL;     //ROM

MIDI_CREATE_INSTANCE(HardwareSerial, Serial, MIDI);

// - Sequence related
SequencerState              current_seq_state = kSequencerState_Stopped;
volatile uint8_t            current_step_indexes[N_TRIGGERS] = {0};
uint8_t                     last_step_indexes[N_TRIGGERS] = {MAX_STEPS_ROW};

// --- SETs  ---

void setCurrentPatternIndex(uint8_t index) {
    if (current_pattern_index != index) {
        current_pattern_index = index;
    }
}

void incrCurrentStepIndexes(int value) {
    // value can be positive or negative
    for (size_t i = 0; i < N_TRIGGERS; i++) {
        //circular loop (0 to )
        current_step_indexes[i] = (current_step_indexes[i] + value + last_step_indexes[i]) % last_step_indexes[i];
    }
}

// --- UPDATES ---

void updateDisplay() {
    switch (current_mode) {
        case kMode_Edit:
            break;
        case kMode_Instr:
            break;
        default:
            break;
    }
}

void updateLeds() {
    for (size_t i = 0; i < N_TRIGGERS; i++) {
        uint8_t val = patterns[current_pattern_index][i];
        //TODO: Led map might be necessary (depending on the wiring) 
        led[i] = IS_HIGH(val) ? CRGB::Red : CRGB(0,0,0);
    }

    if (current_seq_state == kSequencerState_Playing) {
        //update current step led
        led[current_step_indexes[current_pattern_index] % MAX_STEPS_ROW] = CRGB::Green;
    }

    FastLED.show();
}

void updateButtonsState() {
    // reset to 0
    memset(state_indexes, 0, sizeof(state_indexes));
    // Register pressed indexes
    for (size_t i = 0; i < N_B_COLS; i++) {
        digitalWrite(b_cols[i], HIGH);
        for (size_t j = 0; j < N_B_ROWS; j++) {
            const size_t ii = i * N_B_ROWS + j;
            const uitn8_t readed = digitalRead(b_rows[j]) * b_value_mapping[ii];

            last_state_indexes[ii] = state_indexes[ii];

            if (readed != state_indexes[ii]) {
                state_indexes[ii] = readed;
            }
        }

        digitalWrite(b_cols[i], LOW);
    }
}

// --- PATTERN ---

void clearPattern(uint8_t patternIndex) {
    if (patternIndex < N_B_COLS * N_B_ROWS) {
        patterns[patternIndex] = {0};
    }
}

void clearAllPatterns() {
    for (int i = 0; i < N_TRIGGERS; i++) {
        clearPattern(i);
    }
}

// --- OUTPUTS ---

void endTriggerOutput(size_t outputIndex) {
    if (outputIndex < N_B_COLS * N_B_ROWS) {
        digitalWrite(t_outputs[outputIndex], LOW);
        MIDI.sendNoteOff(b_midi_mapping[outputIndex] + midi_mapping_offset, 0, midi_out_channel);
    }
}

void triggerOutput(size_t outputIndex, uint8_t value) {
    if (outputIndex < N_TRIGGERS) {
#if HOLD_NOTES
        // Analog out
        // Update value no matter if it changed or not
        digitalWrite(t_outputs[outputIndex], value > 0 ? HIGH : LOW);
        // MIDI
        
        // Send a MIDI event only if the state has changed since
        if (state_indexes[outputIndex] != last_state_indexes[outputIndex]) {
            // We need to see if which event to send
            if (state_indexes[outputIndex] && !last_state_indexes[outputIndex]) {
                //noteOn (0 to 1)

            } else if (!state_indexes[outputIndex] && last_state_indexes[outputIndex]) {
                //noteOff (1 to 0)
            }
        }
#else
        if (!muted_triggers[outputIndex]) {
            // TODO: what if we need dotted notes for each step ?
            digitalWrite(t_outputs[outputIndex], value > 0 ? HIGH : LOW);
            MIDI.sendNoteOn(b_midi_mapping[outputIndex] + midi_mapping_offset, b_value_mapping[outputIndex], midi_out_channel); 
        }
#endif
        
    }
}

// --- SEQ ---

void stopSequencer() {
    // Reset all step indexes to 0
    memset(current_step_indexes, 0, sizeof(current_step_indexes));
    current_seq_state = kSequencerState_Stopped;
}

void playSequencer() {
    current_seq_state = kSequencerState_Playing;
}

void pauseSequencer() {
    current_seq_state = kSequencerState_Paused;
}

// --- Interrupts ---

void clockInterruptCallback() {
    /*
    * When there is no clock in trigger no sequenced output will be triggered !
    */

    clock_cpt++;

    if (clock_divider > 0) {
        // Block further instructions
        if (clock_cpt % clock_divider != 0) {
            return;
        }
    }

    const Direction dir = digitalRead(DIR_PIN) ? kDirection_Backward : kDirection_Forward;
    if (clock_cpt > MAX_STEPS) {
        //TODO: determine highest last step or use defined current max steps (ARTURIA way)
    }

    incrCurrentStepIndexes(1);

    // reset all outputs to 0
    //TODO: might find a better way with millis
    for (size_t i = 0; i < N_TRIGGERS; i++) {
        endTriggerOutput(i);
    }

    // trigger every ON outputs selected by the current step
    for (size_t i = 0; i < N_TRIGGERS; i++) {
        triggerOutput(i, patterns[i][current_step_indexes[i]]);
    }
}

void resetInterruptCallback() {
    clock_cpt = 0;
    stopSequencer();
    playSequencer();
}

// --- MAIN ---

void setup() {

    // Setup interrupts
    attachInterrupt(digitalPinToInterrupt(CLOCK_IN_PIN), clockInterruptCallback, RISING);
    attachInterrupt(digitalPinToInterrupt(RESET_PIN), resetInterruptCallback, RISING);

    // Setup out triggers
    for (size_t i = 0; i < N_TRIGGERS; i++) {
        pinMode(t_outputs[i], OUTPUT);
    }

    // Setup MIDI
    MIDI.begin(MIDI_CHANNEL_OMNI);
    //MIDI.setHandleNoteOn(NoteOn, handleNoteOn);
    //MIDI.setHandleNoteOff(NoteOff, handleNoteOff);
    //MIDI.setHandleClock(Clock, handleClock);

    // Set the MIDI channel filter to process messages from all channels
    //MIDI.setHandleControlChange(ControlChange, handleControlChange);
    for (int i = 0; i < 16; i++) {
        MIDI.turnOnRx(i);
    }

    // Setup Buttons
    for (size_t i = 0; i < N_B_COLS; i++) {
        pinMode(b_cols[i], OUTPUT);
    }
    for (size_t i = 0; i < N_B_ROWS; i++) {
        pinMode(b_rows[i], INPUT);
    }

    // Setup Led strip
    FastLED.addLeds<WS2812, LED_DATA_PIN, GRB>(leds, N_LEDS);
    FastLED.setBrightness(128); 

    fill_solid(leds, NUM_LEDS, CRGB::Red);
    FastLED.show();

    delay(1000);
}

void ledAnimationLoop(bool interrupt) {
    while (!interrupt) {
        leds[i] = CRGB(random(256), random(256), random(256));
        FastLED.show();
        delay(300);
    }
}

void loop() {
    
    // Detect pressed buttons
    updateButtonsState();
    
    updateLeds(); // Update LEDs
    updateDisplay(); // Update Display

    //--- UX LOGIC ---

    switch (current_mode) {
        case kMode_Edit:
            for (size_t i = 0; i < N_TRIGGERS; i++) {
                // assign the pressed buttons to the current pattern
                if (state_indexes[i] > 0) {
                    patterns[current_pattern_index][i] = state_indexes[i];
                }
            }
            
            break;
        case kMode_Instr:
            for (size_t i = 0; i < N_TRIGGERS; i++) {
                // turn on matching led
                if (state_indexes[i] > 0) {
                    led[i] = CRGB::Blue;
                }
            }
            
            FastLED.show();
            break;
        default:
            break;
    }

    //--- OUTPUT PART ---

    switch (current_mode) {
        case kMode_Instr:
            // ASSIGN triggers by buttons press
            for (size_t i = 0; i < N_TRIGGERS; i++) {
                triggerOutput(i, state_indexes[i]);
            }
            
            break;
        default:
            break;
    }
}

