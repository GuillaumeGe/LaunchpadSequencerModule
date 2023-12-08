//
//  launchpad.h
//  LaunchpadSeq
//
//  Created by Guillaume Gekiere on 18/10/2023.
//

#ifndef launchpad_h
#define launchpad_h

#include "launchpad_defs.h"
#include "midi.h"

typedef struct step_sequencer_t step_sequencer_t;
typedef struct step_sequence_t step_sequence_t;

#define IS_HIGH(value)          value > 0

//TODO: rework mute & modes
typedef enum LaunchpadViewMode {
	kLaunchpadViewMode_Pattern = 0,
	kLaunchpadViewMode_Mute = 1,
	kLaunchpadViewMode_Sequence = 2,
	kLaunchpadViewMode_Settings = 3
} LaunchpadViewMode;

typedef enum LaunchpadSequenceViewMode {
	kLaunchpadSequenceViewMode_Paginated = 0,
	kLaunchpadSequenceViewMode_Grid = 1,
}LaunchpadSequenceViewMode;

typedef enum LaunchpadVersion {
	kLaunchpadVersion_MK1 = 1,
	kLaunchpadVersion_MK2 = 2,
	kLaunchpadVersion_MK3 = 3
} LaunchpadVersion;

typedef struct launchpad_t {
	uint8_t 					page_index;
	uint8_t						trigger_index;
	bool						shift_btn_hold;
	bool						clear_btn_hold;
	step_sequencer_t *			sequencer;
	LaunchpadViewMode           current_view_mode;
	LaunchpadSequenceViewMode	sequence_view_mode;
	uint8_t						current_sequence_index;
	bool						auto_follow_sequence;
	
	void 						(*midi_snd_cb)(SLMIDIPacket * pkt, uint8_t channel);
	void 						(*midi_rcv_cb)(SLMIDIPacket * pkt);
} launchpad_t;

void 						ls_init(launchpad_t * l, step_sequencer_t * seq);
void 						ls_updateDisplay(launchpad_t * l);
void 						ls_updateCell(launchpad_t * l, uint8_t x, uint8_t y);						//sends 1 MIDI messages
void 						ls_updateRow(launchpad_t * l, uint8_t rowIndex);							//sends 8 MIDI messages
void 						ls_updateGrid(launchpad_t * l);												//sends 64 MIDI messages
void 						ls_updateFnButtons(launchpad_t * l);										//sends 8 MIDI messages
void 						ls_updateOutColumn(launchpad_t * l);										//sends 8 MIDI messages
void 						ls_setExtButton(launchpad_t * l, uint16_t btnIndex, uint8_t color);			//sends 1 MIDI messages
void 						ls_setGridButton(launchpad_t * l, uint8_t x, uint8_t y, uint8_t color);		//sends 1 MIDI messages
void						ls_setCurrentSequenceIndex(launchpad_t * l, uint8_t sequenceIndex);
void 						ls_incrPageIndex(launchpad_t * l, int8_t value);
void 						ls_setSequenceViewMode(launchpad_t * l, LaunchpadSequenceViewMode newMode);
void 						ls_updateLastStepIndex(launchpad_t * l, uint8_t x, uint8_t y);
void 						ls_toggleStep(launchpad_t * l, uint8_t x, uint8_t y);

// Utilities
step_sequence_t * 			ls_getCurrentSequence(launchpad_t * l);
uint16_t 					ls_btnMapValue(SLMIDIPacket *packet);
bool 						ls_btnIsDown(SLMIDIPacket * packet);

#endif /* launchpad_h */
