//
//  launchpad.c
//  LaunchpadSeq
//
//  Created by Guillaume Gekière on 18/10/2023.
//

#include <stdio.h>
#include <stdlib.h>
#include "sequencer.h"
#include "launchpad.h"
#include "utils.h"

void ls_init(launchpad_t * l, step_sequencer_t * seq) {
	l->page_index = 0;
	l->trigger_index = 0;
	l->shift_btn_hold = false;
	l->clear_btn_hold = false;
	l->sequencer = seq;
	l->auto_follow_sequence = true;
	l->sequence_view_mode = kLaunchpadSequenceViewMode_Paginated;
	
	if (seq != NULL) {
		l->current_sequence_index = l->sequencer->current_sequence_index;
		//l->current_sequence_index = 2;
	}
}

void ls_midi_send(launchpad_t * l, SLMIDIPacket * pkt, uint8_t channel) {
	if (l->midi_snd_cb != NULL) {
		l->midi_snd_cb(pkt, channel);
	}
}

void ls_incrPageIndex(launchpad_t * l, int8_t value) {
	const uint8_t max = (uint8_t)(MAX_STEPS / LS_MAX_STEPS_PER_ROW);
	const uint8_t newValue = utils_circularLoopGetIndex(l->page_index, value, max);
	if (newValue != l->page_index) {
		l->page_index = newValue;
	}
	
	ls_updateDisplay(l);
}

void ls_setCurrentSequenceIndex(launchpad_t * l, uint8_t sequenceIndex) {
	if (sequenceIndex == l->current_sequence_index) {
		return;
	}
	
	l->current_sequence_index = sequenceIndex;
	//reset page index (might be confusing)
	l->page_index = 0;
	ls_updateDisplay(l);
}


void ls_setExtButton(launchpad_t * l, uint16_t btnIndex, uint8_t color) {
	SLMIDIPacket pkt = {0};
	pkt.length = 3;
	pkt.data[0] = btnIndex >> 8;
	pkt.data[1] = btnIndex & 0x00ff;
	pkt.data[2] = color;
	ls_midi_send(l, &pkt, 0);
}

void ls_setGridButton(launchpad_t * l, uint8_t x, uint8_t y, uint8_t color) {
	SLMIDIPacket pkt = {0};
	pkt.length = 3;
	pkt.data[0] = kSLMIDIMessageType_NoteOn;
	pkt.data[1] = LS_PKT_TO_GRID_POS(x, y);
	pkt.data[2] = color;
	ls_midi_send(l, &pkt, 0);
}

uint16_t ls_btnMapValue(SLMIDIPacket *packet) {
	return LS_BT_CONVERT(packet->data[0], packet->data[1]);
}

bool ls_btnIsDown(SLMIDIPacket * packet) {
	return packet->data[2] > 0;
}

void ls_updateRow(launchpad_t * l, uint8_t rowIndex) {
	if (rowIndex > LS_ROWS) {
		return;
	}
	
	for(size_t x = 0; x < LS_COLS; x++) {
		ls_updateCell(l, x, rowIndex);
	}
}

void ls_clearOutCol(launchpad_t * l) {
	for (size_t i = 0; i < N_TRIGGERS; i++) {
		uint16_t baseValue = LS_BT_VOL | ((i << 4) & 0xf0);
		
		ls_setExtButton(l, baseValue, LS_COLOR_NONE);
	}
}

void ls_editOutSub(launchpad_t * l) {
	if (l->sequence_view_mode == kLaunchpadSequenceViewMode_Paginated) {
		for (size_t i = 0; i < N_TRIGGERS; i++) {
			uint16_t btn = LS_BT_VOL | ((i << 4) & 0xf0);
			uint8_t color = LS_COLOR_NONE;
			
			if (l->sequencer->triggers[i]) {
				color = LS_COLOR_AMBER;
			}
			
			ls_setExtButton(l, btn, color);
		}
	} else {
		for (size_t i = 0; i < N_TRIGGERS; i++) {
			uint16_t btn = LS_BT_VOL | ((i << 4) & 0xf0);
			uint8_t color = LS_COLOR_NONE;
			
			if (l->trigger_index == i) {
				color = LS_COLOR_AMBER;
			}
			
			ls_setExtButton(l, btn, color);
		}
	}
}

void ls_muteOutSub(launchpad_t * l) {
	if (l->sequence_view_mode == kLaunchpadSequenceViewMode_Paginated) {
		//	step_sequence_t * cs = &l->sequencer->sequences[l->current_sequence_index];
		for (size_t i = 0; i < N_TRIGGERS; i++) {
			uint16_t baseValue = LS_BT_VOL | ((i << 4) & 0xf0);
			uint8_t color = 0x1C;
			if (!l->sequencer->muted_triggers[i]) {
				color = LS_COLOR_GREEN;
			}
			ls_setExtButton(l, baseValue, color);
		}
	} else {
		for (size_t i = 0; i < N_TRIGGERS; i++) {
			uint16_t btn = LS_BT_VOL | ((i << 4) & 0xf0);
			uint8_t color = LS_COLOR_NONE;
			
			if (l->trigger_index == i) {
				color = LS_COLOR_GREEN;
			}
			
			ls_setExtButton(l, btn, color);
		}
	}
}

//TODO: redo logic
void ls_updateOutColumn(launchpad_t * l) {
	switch(l->current_view_mode) {
		case kLaunchpadViewMode_Pattern:
			ls_editOutSub(l);
			break;
		case kLaunchpadViewMode_Mute:
			ls_muteOutSub(l);
			break;
		case kLaunchpadViewMode_Sequence:
			ls_editOutSub(l);
			break;
		case kLaunchpadViewMode_Settings:
			ls_clearOutCol(l);
			break;
		default:
			break;
	}
}

//TODO
void ls_updateFnButtons(launchpad_t * l) {
	switch (l->current_view_mode) {
		case kLaunchpadViewMode_Pattern:
			ls_setExtButton(l, LS_BT_MODE, LS_COLOR_AMBER);
			break;
		case kLaunchpadViewMode_Mute:
			ls_setExtButton(l, LS_BT_MODE, LS_COLOR_GREEN);
			break;
		case kLaunchpadViewMode_Sequence:
			ls_setExtButton(l, LS_BT_MODE, LS_COLOR_RED);
			break;
		case kLaunchpadViewMode_Settings:
			ls_setExtButton(l, LS_BT_MODE, LS_COLOR_NONE);
			break;
		default:
			break;
	}
	if (l->page_index == 0) {
		//LS_BT_RIGHT_ARROW --> color green
		//LS_BT_LEFT_ARROW --> color none
	} else {
		//LS_BT_RIGHT_ARROW --> color green
		//LS_BT_LEFT_ARROW --> color green
	}
}

SLMIDIPacket * _createPacket_updateCellPaginated(launchpad_t * l, uint8_t x, uint8_t y) {
	SLMIDIPacket * result = NULL;
	
	uint8_t color = LS_COLOR_NONE;
	const step_sequencer_t * sequencer = l->sequencer;
	const step_sequence_t * cs = &sequencer->sequences[l->current_sequence_index];
	const uint8_t patternIndex = y;
	const step_pattern_t * pattern = &cs->patterns[patternIndex];
	const bool isPlayingSequenceDisplayed = l->current_sequence_index == sequencer->current_sequence_index && sequencer->current_state == kSequencerState_Playing;
	const bool channelMuted = sequencer->muted_triggers[patternIndex];
	const uint8_t bckColor = l->current_view_mode == kLaunchpadViewMode_Pattern ? 0x0D : LS_COLOR_NONE;
	const uint8_t stepIndex = x + (l->page_index * LS_MAX_STEPS_PER_ROW);
	const uint8_t stepValue = pattern->steps[stepIndex];
	
	//let's check if we are in range first
	if (stepIndex >= (l->page_index * LS_MAX_STEPS_PER_ROW) && stepIndex < LS_MAX_STEPS_PER_ROW + (l->page_index * LS_MAX_STEPS_PER_ROW)) {
		//are we in range of last step ?
		if (stepIndex < cs->last_step_indexes[patternIndex]) {
			if (isPlayingSequenceDisplayed && stepIndex == cs->current_step_indexes[patternIndex]) {
				// seq line index --> yellow
				color = 0x1D;
				
				if (IS_HIGH(stepValue)) {
					color =  !channelMuted ? LS_COLOR_AMBER : LS_COLOR_LOW_RED;
				}
			} else {
				if (IS_HIGH(stepValue)) {
					//light up high green for used slots
					color = !channelMuted ? LS_COLOR_GREEN : LS_COLOR_LOW_GREEN;
				} else {
					//light up low red for unused slots
					color = bckColor;
				}
			}
		}
		
		result = (SLMIDIPacket *)malloc(sizeof(SLMIDIPacket));
		result->length = 3;
		result->data[0] = kSLMIDIMessageType_NoteOn;
		result->data[1] = LS_PKT_TO_GRID_POS(x % LS_MAX_STEPS_PER_ROW, y);
		result->data[2] = color;
	}
	
	return result;
}

//TODO: updated assignation from ls button in main.c
SLMIDIPacket * _createPacket_updateCellGrid(launchpad_t * l, uint8_t x, uint8_t y) {
	SLMIDIPacket * result = NULL;
	
	uint8_t color = LS_COLOR_NONE;
	const step_sequencer_t * sequencer = l->sequencer;
	const step_sequence_t * cs = &sequencer->sequences[l->current_sequence_index];
	const uint8_t patternIndex = l->trigger_index;
	const step_pattern_t * pattern = &cs->patterns[patternIndex];
	const bool isPlayingSequenceDisplayed = l->current_sequence_index == sequencer->current_sequence_index && sequencer->current_state == kSequencerState_Playing;
	const uint8_t stepIndex = x + (y * LS_ROWS);
	const uint8_t lastStepIndex = cs->last_step_indexes[patternIndex];
	
	if (stepIndex < LS_ROWS * LS_COLS && stepIndex < MAX_STEPS) {
		const uint8_t stepValue = pattern->steps[stepIndex];
		if (isPlayingSequenceDisplayed && stepIndex == cs->current_step_indexes[patternIndex]) {
			color = LS_COLOR_AMBER;
		} else {
			if (IS_HIGH(stepValue)) {
				color = LS_COLOR_GREEN;
			} else {
				if (stepIndex < lastStepIndex) {
					color = LS_COLOR_LOW_RED;
				}
			}
		}
		
		result = (SLMIDIPacket *)malloc(sizeof(SLMIDIPacket));
		result->length = 3;
		result->data[0] = kSLMIDIMessageType_NoteOn;
		result->data[1] = LS_PKT_TO_GRID_POS(x, y);
		result->data[2] = color;
	}
	
	
	return result;
}

//TODO: update with mode
void ls_updateCell(launchpad_t * l, uint8_t x, uint8_t y) {
	if (x > LS_COLS) {
		return;
	}
	if (y > LS_ROWS) {
		return;
	}
	
	//TODO: find a better way to handle max steps
	if (l->sequencer == NULL || MAX_STEPS != LS_ROWS * LS_COLS) {
		return;
	}
	
	const LaunchpadViewMode currentViewMode = l->current_view_mode;
	SLMIDIPacket * pkt = NULL;
	
	switch(currentViewMode) {
		case kLaunchpadViewMode_Pattern:
		case kLaunchpadViewMode_Mute:
			if (l->sequence_view_mode == kLaunchpadSequenceViewMode_Paginated) {
				pkt = _createPacket_updateCellPaginated(l, x, y);
			} else if (l->sequence_view_mode == kLaunchpadSequenceViewMode_Grid) {
				pkt = _createPacket_updateCellGrid(l, x, y);
			}
			break;
		default:
			break;
	}
	
	if (pkt != NULL) {
		ls_midi_send(l, pkt, 0);
		free(pkt);
	}
}


// --- UPDATES ---

void ls_updateDisplay(launchpad_t * l) {
	switch (l->current_view_mode) {
		case kLaunchpadViewMode_Pattern:
		case kLaunchpadViewMode_Mute:
			for (size_t y = 0; y < LS_ROWS; y++) {
				for (size_t x = 0; x < LS_COLS; x++) {
					ls_updateCell(l, x, y);
				}
			}
			break;
		case kLaunchpadViewMode_Sequence:
			for (size_t i = 0; i < N_SEQUENCES; i++) {
				int y = (int)(i / 4);
				uint8_t color = seq_isEmpty(&l->sequencer->sequences[i]) ? LS_COLOR_LOW_RED : LS_COLOR_LOW_RED;
				
				if (i == sequencer_getCurrentSequenceIndex(l->sequencer)) {
					//currently playing
					color = l->sequencer->current_state == kSequencerState_Playing ? LS_COLOR_GREEN : LS_COLOR_LOW_GREEN;
					if (l->current_sequence_index == i) {
						
					}
				} else if (l->sequencer->next_sequence_index != NO_NEXT_SEQUENCE && i == l->sequencer->next_sequence_index) {
					//blink ?
					color = LS_COLOR_LOW_GREEN;
					if (l->current_sequence_index == i) {
						color = LS_COLOR_GREEN;
					}
				} else if (l->current_sequence_index == i) {
					//currently viewing
					color = LS_COLOR_AMBER;
					//color = l->current_sequence_index == i ? LS_COLOR_AMBER : 0x1D;
				}
				ls_setGridButton(l, i % 4, y, color);
			}
			break;
		case kLaunchpadViewMode_Settings:
			
			break;
		default:
			break;
	}
	
	ls_updateFnButtons(l);
	ls_updateOutColumn(l);
}

void ls_setPatternViewMode(launchpad_t * l, LaunchpadSequenceViewMode newMode) {
	if (newMode != l->sequence_view_mode) {
		l->sequence_view_mode = newMode;
		l->trigger_index = 0;
		l->page_index = 0;
		ls_updateDisplay(l);
	}
}

