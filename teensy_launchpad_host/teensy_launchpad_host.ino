#include <USBHost_t36.h>

//http://little-scale.blogspot.com/2018/12/teensy-36-as-standalone-mediator.html

#define N_TRIGGERS      8
#define CLOCK_IN_PIN    3

const int interval_time = 90;

const byte Gr = 0;

//red
const byte R1 = 1;
const byte R2 = 2;
const byte R3 = 3;
//green
const byte G1 = 16;
const byte G2 = 32;
const byte G3 = 48;
//yellow
const byte Y1 = 17;
const byte Y2 = 33;
const byte Y3 = 50;

const byte O1 = 18;
const byte O2 = 19;
const byte O3 = 34;
const byte O4 = 35;
const byte O5 = 51;

const byte YG = 49;

const uint8_t               t_outputs[N_TRIGGERS] = {
    30,31,32,33,
    34,35,36,37
};

byte pattern[128];

uint8_t patterns[N_TRIGGERS][64];

USBHost myusb;
USBHub hub1(myusb);
USBHub hub2(myusb);
MIDIDevice midi1(myusb);

elapsedMillis clock_count;
byte step_count;
byte round_count;

void setup() {

  for(int i = 0; i < N_TRIGGERS; i ++) {
    pinMode(t_outputs[i], OUTPUT); 
    digitalWrite(t_outputs[i], LOW);
  }

  delay(1500);
  myusb.begin();
  midi1.setHandleNoteOn(myNoteOn);
  midi1.setHandleNoteOff(myNoteOff);
  midi1.setHandleControlChange(myControlChange);
}



void loop() {
  myusb.Task();
  midi1.read();

  if (clock_count >= interval_time) {
    doStep();
    clock_count = 0;
  }
}

void myNoteOn(byte channel, byte note, byte velocity) {
  if (note % 16 < 8) {
    pattern[note] = (pattern[note] + 1) % 4;

    if (pattern[note] == 1) {
      updateLPGrid(note % 16, note / 16, R1);
    }

    else {
      updateLPGrid(note % 16, note / 16, Gr);
    }

  }

}

void myNoteOff(byte channel, byte note, byte velocity) {
  // midi1.sendNoteOff(note, velocity, channel);
}

void myControlChange(byte channel, byte control, byte value) {
  midi1.sendControlChange(control, value, channel);
}

void doStep() {
  for (int i = 0; i < 8; i ++) {
    if (pattern[(i * 16) + step_count] == 1) {
      updateLPGrid(step_count, i, R1);
    } else if (pattern[(i * 16) + step_count] == 2 && round_count == 0) {
      updateLPGrid(step_count, i, R1);
    } else if (pattern[(i * 16) + step_count] == 3 && round_count == 1) {
      updateLPGrid(step_count, i, R1);
    } else {
      updateLPGrid(step_count, i, Gr);
    }
  }

  step_count ++;
  if (step_count > 7) {
    step_count = 0;
    round_count = 1 - round_count;
  }

  for (int i = 0; i < 8; i ++) {
    if (pattern[(i * 16) + step_count] == 1) {
      updateLPGrid(step_count, i, O4);
      midi1.sendNoteOn((i * 16) + 15, 127, 1);
      digitalWrite(7 - i , HIGH); 
    }  else if (pattern[(i * 16) + step_count] == 2 && round_count == 0) {
      updateLPGrid(step_count, i, O4);
      midi1.sendNoteOn((i * 16) + 15, 127, 1);
      digitalWrite(7 - i , HIGH);
    } else if (pattern[(i * 16) + step_count] == 3 && round_count == 1) {
      updateLPGrid(step_count, i, O4);
      midi1.sendNoteOn((i * 16) + 15, 127, 1);
      digitalWrite(7 - i , HIGH);
    } else {
      updateLPGrid(step_count, i, G3);
      midi1.sendNoteOn((i * 16) + 15, 0, 1);
      digitalWrite(7 - i , LOW);
    }
  }

  delay(3); 

  for (int i = 0; i < 8; i ++) {
    digitalWrite(7 - i , LOW);
  }
}

void updateLPGrid(byte x_value, byte y_value, byte color) {
  midi1.sendNoteOn(x_value + (y_value * 16), color, 1);
}