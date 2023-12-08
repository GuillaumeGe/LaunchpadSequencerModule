//
//  main.c
//  LaunchpadSeq
//
//  Created by Guillaume Gekière on 01/10/2023.
//

#include <CoreMIDI/MIDIServices.h>
#include <CoreFoundation/CFRunLoop.h>

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include "launchpad.h"
#include "sequencer.h"
#include "utils.h"
//#include "preset.h"

#define DEBUG						0

#define DEFAUTL_MIDI_IN_CHANNEL     1
#define DEFAUTL_MIDI_OUT_CHANNEL    2

#define CLOCK_IN_PIN                2
#define CLOCK_OUT_PIN               3
#define RESET_PIN                   4
#define DIR_PIN    		            5

// PINS DEFINITIONS
const uint8_t              	outputs[N_TRIGGERS] = {
	30,31,32,33,
	34,35,36,37
};

MIDIPortRef     			gOutPort = NULL;
MIDIEndpointRef 			gDest = NULL;

// MIDI
uint8_t                     midi_mapping_offset = 0;                        //ROM
uint8_t                     midi_out_channel = DEFAUTL_MIDI_OUT_CHANNEL;     //ROM
uint8_t                     midi_in_channel = DEFAUTL_MIDI_IN_CHANNEL;     //ROM

launchpad_t					ls;
step_sequencer_t			sequencer;

void wrap_sq_updateTriggers(void * s);
void wrap_sq_updateTrigger(void * s, uint8_t triggerIndex);
void wrap_sq_updateMutedTriggers(void *s, uint8_t triggerIndex);
void wrap_sq_updatePattern(void *s, uint8_t sequenceIndex, uint8_t pI);
void wrap_sq_updateStep(void * s, uint8_t sequenceIndex, uint8_t pI, uint8_t sI);
void wrap_sq_updateState(void * s);
void wrap_sq_updateSequenceIndex(void * s, uint8_t sequenceIndex);
void wrap_sq_updateNextSequenceIndex(void * s);

void wrap_ls_midi_snd(SLMIDIPacket * pkt, uint8_t channel);
void wrap_ls_midi_rcv(SLMIDIPacket * pkt);

bool processFunButton(SLMIDIPacket *packet);
bool processColButton(SLMIDIPacket *packet);
bool processGridButton(SLMIDIPacket *packet);
void clockInterruptCallback(void);
void resetInterruptCallback(void);

void endTriggerOutput(size_t outputIndex);
void updateOutput(size_t outputIndex, uint8_t value);

step_sequence_t * getCurrentSequenceSQ(void);
step_sequence_t * getCurrentSequenceLS(void);

// -----------------------------------------------------------------

void wrap_sq_updateMutedTriggers(void *s, uint8_t triggerIndex) {
	ls_updateRow(&ls, triggerIndex);
	ls_updateFnButtons(&ls);
	ls_updateOutColumn(&ls);
}

void wrap_sq_updatePattern(void *s, uint8_t sequenceIndex, uint8_t pI) {
	if (ls.current_sequence_index == sequenceIndex) {
		ls_updateRow(&ls, pI);
	}
}

void wrap_sq_updateStep(void * s, uint8_t sequenceIndex, uint8_t pI, uint8_t stepIndex) {
	if (ls.current_sequence_index == sequenceIndex) {
		ls_updateCell(&ls, stepIndex % LS_MAX_STEPS_PER_ROW, pI);
	}
}

void wrap_sq_updateState(void * s) {
	ls_updateDisplay(&ls);
}

void wrap_sq_updateTriggers(void * s) {
	ls_updateOutColumn(&ls);
}

void wrap_sq_updateTrigger(void * s, uint8_t triggerIndex) {
	updateOutput(triggerIndex, sequencer.triggers[triggerIndex]);
}

void wrap_sq_updateSequenceIndex(void * s, uint8_t sequenceIndex) {
	if (ls.auto_follow_sequence) {
		ls.current_sequence_index = sequenceIndex;
	}
	
	if (ls.current_sequence_index == sequenceIndex) {
		ls_updateDisplay(&ls);
	}
}

void wrap_sq_updateNextSequenceIndex(void * s) {
	ls_updateDisplay(&ls);
}

void wrap_ls_midi_snd(SLMIDIPacket * pkt, uint8_t channel) {
	if (gOutPort != NULL && gDest != NULL && pkt != NULL) {
		// Initialize a MIDIPacketList
		MIDITimeStamp timestamp = 0; // 0 will mean play now.
		uint8_t buffer[128]; // storage space for MIDI Packets (max 65536)

		//MIDIPacketList packetList;
		MIDIPacketList *packetlist = (MIDIPacketList*)buffer;
		MIDIPacket *packet = MIDIPacketListInit(packetlist);
		
		// Create a MIDIPacket within the packetList
		packet = MIDIPacketListAdd(packetlist, sizeof(buffer), packet, timestamp, 3, pkt->data);
#if DEBUG
//		printf("SENDING: len: %d \t%02X %02X %02X\n",
//			   packet->length,
//			   packet->data[0],
//			   packet->data[1],
//			   packet->data[2]);
#endif
		// Check if the packet was added successfully
		if (packet != NULL) {
			// Send the packet list using MIDISend
			MIDISend(gOutPort, gDest, packetlist);
		} else {
			// Handle the case where the packet could not be added
			// (e.g., packet buffer is too small)
		}
	}
}

void wrap_ls_midi_rcv(SLMIDIPacket * packet) {
	if (packet == NULL) {
		return;
	}
	
	
	if (packet->length >= 3) {
		
		//--------
		if (!processFunButton(packet) && !processColButton(packet)) {
			processGridButton(packet);
		}
#if DEBUG
		if (ls_btnIsDown(packet)) {
			printf("-----------------\n");
			printf("SHIFT: %d\n", ls.shift_btn_hold);
			printf("PAGE IDX: %d\n", ls.page_index);
			printf("RECV: len: %d \t%02X %02X %02X\n",
				   packet->length,
				   packet->data[0],
				   packet->data[1],
				   packet->data[2]);
		}
#endif
	}
}

// -----------------------------------------------------------------

bool processFunButton(SLMIDIPacket *packet) {
	bool result = true;
	
	if (ls_btnMapValue(packet) == LS_BT_SHIFT) {
		ls.shift_btn_hold = ls_btnIsDown(packet);
		ls_setExtButton(&ls, LS_BT_SHIFT, ls_btnIsDown(packet) ? LS_COLOR_YELLOW : LS_COLOR_NONE);
	} else if (ls_btnMapValue(packet) == LS_BT_CLEAR) {
		
		ls.clear_btn_hold = ls_btnIsDown(packet);
		ls_setExtButton(&ls, LS_BT_CLEAR, ls_btnIsDown(packet) ? LS_COLOR_RED : LS_COLOR_NONE);
		
		if (ls.shift_btn_hold && ls.clear_btn_hold) {
			seq_clearAllPatterns(getCurrentSequenceLS());
		}
	} else if (ls_btnMapValue(packet) == LS_BT_UP_ARROW && ls_btnIsDown(packet)) {
#if DEBUG
		sequencer.current_direction = kDirection_Forward;
		//clockInterruptCallback();
#endif
	} else if (ls_btnMapValue(packet) == LS_BT_DOWN_ARROW && ls_btnIsDown(packet)) {
#if DEBUG
		sequencer.current_direction = kDirection_Backward;
		//clockInterruptCallback();
#endif
	} else if (ls_btnMapValue(packet) == LS_BT_LEFT_ARROW && ls_btnIsDown(packet)) {
		ls_incrPageIndex(&ls, -1);
	} else if (ls_btnMapValue(packet) == LS_BT_RIGHT_ARROW && ls_btnIsDown(packet)) {
		ls_incrPageIndex(&ls, 1);
	} else if (ls_btnMapValue(packet) == LS_BT_RESET && ls_btnIsDown(packet)) {
#if DEBUG
		sequencer_resetCurrentStepIndexes(&sequencer, ls.current_sequence_index);
#endif
	} else if (ls_btnMapValue(packet) == LS_BT_MODE && ls_btnIsDown(packet)) {
		//TODO: method
		ls.current_view_mode = (LaunchpadViewMode)utils_circularLoopGetIndex(ls.current_view_mode, 1, 3);
		//ls.current_view_mode = !ls.current_view_mode;
		ls_updateDisplay(&ls);
	} else {
		result = false;
	}
	
	return result;
}

bool processColButton(SLMIDIPacket *packet) {
	//uint16_t baseValue = LS_BT_VOL;
	for (size_t i = 0; i < N_TRIGGERS; i++) {
		/*
		 BT VALUE Example:
		 0x9008
		 0x9018
		 0x9028
		 */
		uint16_t v = 0x90 << 8 | i << 4 | 0x08;
		if (ls_btnMapValue(packet) == v && ls_btnIsDown(packet)) {
			if (ls.clear_btn_hold) {
				seq_clearPattern(getCurrentSequenceLS(), i);
			}
			
			if (ls.current_view_mode == kLaunchpadViewMode_Mute) {
				sequencer_setMutedPattern(&sequencer, i, !sequencer.muted_triggers[i]);
			}
			
			return true;
		}
	}
	
	return false;
}

bool processGridButton(SLMIDIPacket *packet) {
	SLMIDIMessageType   type = packet->data[0] & 0xF0;
	uint8_t             incommingChannel = packet->data[0] & 0x0F;
	
	if (ls_btnIsDown(packet)) {
		if (type == kSLMIDIMessageType_NoteOn) {
			uint8_t x = packet->data[1] % 16;
			uint8_t y = (packet->data[1] - x) / 16;
			
			switch(ls.current_view_mode) {
				case kLaunchpadViewMode_Pattern:
				case kLaunchpadViewMode_Mute:
					if (!ls.shift_btn_hold) {
						//toggle one step
						seq_togglePatternStepValue(getCurrentSequenceLS(), y, x + (ls.page_index * LS_MAX_STEPS_PER_ROW));
					} else {
						//determines the last step
						seq_setLastStepIndex(getCurrentSequenceLS(), y, x + (ls.page_index * LS_MAX_STEPS_PER_ROW) + 1);
					}
					break;
				case kLaunchpadViewMode_Sequence:
					if (x < 4 && y < 4) {
						//sequence select section
						uint8_t newSequenceIndex = x + 4 * y;
						if (!ls.shift_btn_hold) {
							ls_setCurrentSequenceIndex(&ls, newSequenceIndex);
						} else {
							sequencer_setNextSequenceIndex(ls.sequencer, newSequenceIndex);
						}
					}
					break;
				default:
					break;
			}
			
			return true;
		}
	}
	
	return false;
}

// ------

step_sequence_t * getCurrentSequenceSQ(void) {
	return sequencer_getCurrentSequence(&sequencer);
}

step_sequence_t * getCurrentSequenceLS(void) {
	return &sequencer.sequences[ls.current_sequence_index];
}

// --- Interrupts ---

void clockInterruptCallback(void) {
	/*
	* When there is no clock in trigger no sequenced output will be triggered !
	*/

	sequencer_clock(&sequencer);
}

void resetInterruptCallback(void) {
	//TODO: fix & check if play/stop necessary (normally no)
	sequencer.clock_cpt = 0;
	sequencer_stop(&sequencer);
	sequencer_play(&sequencer);
}

void dirInterruptCallback(void) {
	sequencer.current_direction = !sequencer.current_direction;
}

void updateOutput(size_t outputIndex, uint8_t value) {
	if (outputIndex < N_TRIGGERS) {
		//digitalWrite(t_outputs[outputIndex], value > 0 ? HIGH : LOW);
	}
}

// --- MAIN ---

void setup(void) {
	// Setup interrupts
	//attachInterrupt(digitalPinToInterrupt(CLOCK_IN_PIN), clockInterruptCallback, RISING);
	//attachInterrupt(digitalPinToInterrupt(RESET_PIN), resetInterruptCallback, RISING);
	//attachInterrupt(digitalPinToInterrupt(DIR_PIN), dirInterruptCallback, RISING);

	// Setup MIDI
	
	// Setup structs
	sequencer_init(&sequencer);
	sequencer.step_updated_cb = wrap_sq_updateStep;
	sequencer.pattern_updated_cb = wrap_sq_updatePattern;
	sequencer.state_updated_cb = wrap_sq_updateState;
	sequencer.triggers_updated_cb = wrap_sq_updateTriggers;
	sequencer.trigger_updated_cb = wrap_sq_updateTrigger;
	sequencer.muted_triggers_updated_cb = wrap_sq_updateMutedTriggers;
	sequencer.next_seq_index_updated_cb = wrap_sq_updateNextSequenceIndex;
	sequencer.sequence_index_updated_cb = wrap_sq_updateSequenceIndex;
		
	ls_init(&ls, &sequencer);
	ls.midi_snd_cb = &wrap_ls_midi_snd;
	ls.midi_rcv_cb = &wrap_ls_midi_rcv;
	
	sequencer_play(&sequencer);
	
	//ls_updateDisplay(&ls);
}

void loop() {
		
//	updateLeds(); // Update LEDs
//	updateDisplay(); // Update Display
	
}

static void midi_read_callback(const MIDIPacketList *evtList, void *refCon, void *connRefCon)
{
	if (gOutPort != NULL && gDest != NULL) {
		MIDIPacket *packet = (MIDIPacket *)evtList->packet;
		
		for (size_t j = 0; j < evtList->numPackets; ++j) {
			// go through each pkt bytes
			
			SLMIDIPacket *pkt = (SLMIDIPacket *)packet;
			
			wrap_ls_midi_rcv(pkt);
		
			//packet->data[2] = LS_COLOR_YELLOW - 0x08;
			
			packet = MIDIPacketNext(packet);
		}
	}
}

int main(int argc, const char * argv[]) {
	// create client and ports
	MIDIClientRef client = NULL;
	MIDIClientCreate(CFSTR("MIDI Echo"), NULL, NULL, &client);

	MIDIPortRef inPort = NULL;
	MIDIInputPortCreate(client, CFSTR("Input port"), midi_read_callback, NULL, &inPort);
	MIDIOutputPortCreate(client, CFSTR("Output port"), &gOutPort);

	// enumerate devices (not really related to purpose of the echo program
	// but shows how to get information about devices)
	int i, n;
	CFStringRef pname, pmanuf, pmodel;
	char name[64], manuf[64], model[64];

//	n = (int)MIDIGetNumberOfDevices();
//	for (i = 0; i < n; ++i) {
//		MIDIDeviceRef dev = MIDIGetDevice(i);
//
//		MIDIObjectGetStringProperty(dev, kMIDIPropertyName, &pname);
//		MIDIObjectGetStringProperty(dev, kMIDIPropertyManufacturer, &pmanuf);
//		MIDIObjectGetStringProperty(dev, kMIDIPropertyModel, &pmodel);
//
//		CFStringGetCString(pname, name, sizeof(name), 0);
//		CFStringGetCString(pmanuf, manuf, sizeof(manuf), 0);
//		CFStringGetCString(pmodel, model, sizeof(model), 0);
//		CFRelease(pname);
//		CFRelease(pmanuf);
//		CFRelease(pmodel);
//
//		printf("name=%s, manuf=%s, model=%s\n", name, manuf, model);
//	}

	// open connections from all sources
	n = (int)MIDIGetNumberOfSources();
	printf("%d sources\n", n);
	for (i = 0; i < n; ++i) {
		MIDIEndpointRef src = MIDIGetSource(i);
		MIDIPortConnectSource(inPort, src, NULL);
	}

	// find the first destination
	n = (int)MIDIGetNumberOfDestinations();
	if (n > 0)
		gDest = MIDIGetDestination(0);

	if (gDest != NULL) {
//		MIDIObjectGetStringProperty(gDest, kMIDIPropertyName, &pname);
//		CFStringGetCString(pname, name, sizeof(name), 0);
//		CFRelease(pname);
//		printf("Echoing to channel %d of %s\n", midi_out_channel, name);
	} else {
//		printf("No MIDI destinations present\n");
	}
	
	setup();
	
	// Create a dispatch queue (you can use the main queue or create a custom one)
	dispatch_queue_t queue = dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0);
	// Create a dispatch source timer
	dispatch_source_t timer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, queue);
	
	if (timer) {
		// Set the time interval for the timer (in nanoseconds)
		uint64_t interval = NSEC_PER_SEC * 0.125 / 3; // 1 second
		dispatch_source_set_timer(timer, dispatch_time(DISPATCH_TIME_NOW, 0), interval, 0);
		
		// Define the event handler block for the timer
		dispatch_source_set_event_handler(timer, ^{
			clockInterruptCallback();
		});
		
		// Start the timer
		dispatch_resume(timer);
	}
	

	CFRunLoopRun();
	
	return 0;
}
