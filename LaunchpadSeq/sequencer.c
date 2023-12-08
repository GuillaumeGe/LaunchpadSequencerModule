//
//  sequencer.c
//  LaunchpadSeq
//
//  Created by Guillaume Gekiere on 18/10/2023.
//

#include "sequencer.h"
#include <stdio.h>
#include <string.h>
#include "utils.h"

int sequencer_setTriggerValue(step_sequencer_t* s, size_t index, uint8_t value) {
	if (index > N_TRIGGERS) {
		return -1;
	}
	
	if (s->triggers[index] != value) {
		s->triggers[index] = value;
		if (s->trigger_updated_cb != NULL) {
			s->trigger_updated_cb(s, index);
		}
		
		return 1;
	}
	
	return 0;
}

void _sequencer_sequence_step_update_callback(void * sequence, uint8_t patternIndex, uint8_t stepIndex) {
	step_sequence_t * sq = (step_sequence_t *)sequence;
	
	if (sq != NULL && sq->sequencer_ref != NULL) {
		if (sq->sequencer_ref->step_updated_cb != NULL) {
			size_t sequenceIndex = 0;
			for (size_t i = 0; i < N_SEQUENCES; i++) {
				if (sq == &sq->sequencer_ref->sequences[i]) {
					sequenceIndex = i;
					break;
				}
			}
			sq->sequencer_ref->step_updated_cb(sq->sequencer_ref, sequenceIndex, patternIndex, stepIndex);
		}
	}
}

void _sequencer_sequence_pattern_update_callback(void * sequence, uint8_t patternIndex) {
	step_sequence_t * sq = (step_sequence_t *)sequence;
	
	if (sq != NULL && sq->sequencer_ref != NULL) {
		if (sq->sequencer_ref->pattern_updated_cb != NULL) {
			size_t sequenceIndex = 0;
			for (size_t i = 0; i < N_SEQUENCES; i++) {
				if (sq == &sq->sequencer_ref->sequences[i]) {
					sequenceIndex = i;
					break;
				}
			}
			sq->sequencer_ref->pattern_updated_cb(sq->sequencer_ref, sequenceIndex, patternIndex);
		}
	}
}

//-----------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------

void sequencer_init(step_sequencer_t * s) {
	s->current_sequence_index = 0;
	s->clock_cpt = 0;
	s->clock_divider = 1;
	s->current_state = kSequencerState_Stopped;
	s->current_direction = kDirection_Forward;

	sequencer_setNextSequenceIndex(s, NO_NEXT_SEQUENCE);
	
	memset((void *) s->triggers, 0x00, sizeof(s->triggers));
	memset((void *) s->muted_triggers, false, sizeof(s->muted_triggers));
	
	for (size_t i = 0; i < N_SEQUENCES; i++) {
		seq_init(&s->sequences[i]);
		//TODO: load preset for seq 0
		s->sequences[i].sequencer_ref = s;
		s->sequences[i].step_updated_cb = _sequencer_sequence_step_update_callback;
		s->sequences[i].pattern_updated_cb = _sequencer_sequence_pattern_update_callback;		
	}
}

void sequencer_incrCurrentStepIndexes(step_sequencer_t * s, int value) {
	
}

int sequencer_setSequenceIndex(step_sequencer_t * s, uint8_t sequenceIndex) {
	if (sequenceIndex > N_SEQUENCES) {
		return -1;
	}
	
	if (s->current_sequence_index == sequenceIndex) {
		return 0;
	}
	
	s->current_sequence_index = sequenceIndex;
	
	if (s->sequence_index_updated_cb != NULL) {
		s->sequence_index_updated_cb(s, sequenceIndex);
	}
	
	return 1;
}

void sequencer_clock(step_sequencer_t * s) {
	//TODO: we will probably miss the first step
	s->clock_cpt++;
	
	// Block further instructions
	if (s->clock_cpt % (DEFAULT_CLOCK_DIVIDER * s->clock_divider) != 0) {
		return;
	}
	
	if (s->current_state != kSequencerState_Playing) {
		return;
	}
	
	uint8_t stepCpt = s->clock_cpt / (DEFAULT_CLOCK_DIVIDER);
	step_sequence_t * sq = sequencer_getCurrentSequence(s);
	int dir = s->current_direction == kDirection_Forward ? 1 : -1;

	printf("seq: %d; step: %d\n", s->current_sequence_index ,stepCpt);

	//auto play next seq
	if (s->next_sequence_index != NO_NEXT_SEQUENCE && s->next_sequence_index < N_SEQUENCES) {
		bool goToNextSequence = false;
				
		if (dir == 1 && stepCpt >= sq->length) {
			goToNextSequence = true;
		} else if (dir == -1 && s->clock_cpt % sq->length == 0) {
			goToNextSequence = true;
		}
		
		if (goToNextSequence) {
			printf("Swap to %d\n", s->next_sequence_index);

			if (sequencer_setSequenceIndex(s, s->next_sequence_index) > 0) {
				sequencer_resetCurrentStepIndexes(s, (uint8_t)s->current_sequence_index);
				sequencer_setNextSequenceIndex(s, NO_NEXT_SEQUENCE);
				//do not go step + 1
				//or go last step if backward
				dir = dir > 0 ? 0 : -1;
				s->clock_cpt = 0;
			}
		}
	}
	
	sq = sequencer_getCurrentSequence(s);
	
	// update sequence's patterns indexes
	// incr/retain/decr cur sequence index
	// value can be positive or negative
	seq_incrCurrentStepIndexes(sq, dir);
		
	for (size_t i = 0; i < N_TRIGGERS; i++) {
		const bool muted = s->muted_triggers[i];
		const size_t resolvedNextIndex = utils_circularLoopGetIndex(sq->current_step_indexes[i], 0, sq->last_step_indexes[i]);
		const uint8_t value = sq->patterns[i].steps[resolvedNextIndex];

		if (muted) {
			sequencer_setTriggerValue(s, i, 0x00);
		} else {
			// check if we need to "noteOff" this step by comparing to next index value
			// reset otherwise (spike)
			// TODO: might find a better way with millis for reset
			if (!(sequencer_getCurrentSequence(s)->link_steps[i] && value > 0)) {
				sequencer_setTriggerValue(s, i, 0x00);
			}
			
			//const uint8_t value = sq->patterns[i].steps[sq->current_step_indexes[i]];
			sequencer_setTriggerValue(s, i, value);
		}
	}
	
	//TODO: reset to 0 after DEFAULT_CLOCK_DIVIDER * MAX_STTEPS because 3 is odd number uint8_t will not be enough
	if (stepCpt > sq->length - 1) {
		s->clock_cpt = 0;
	}
	
	//TODO: check if really needed
	if (s->triggers_updated_cb != NULL) {
		s->triggers_updated_cb(s);
	}
}

int sequencer_setNextSequenceIndex(step_sequencer_t * s, int8_t sequenceIndex) {
	if (sequenceIndex > N_SEQUENCES) {
		return -1;
	}
	
	if (s->next_sequence_index == sequenceIndex || sequenceIndex == s->current_sequence_index) {
		return 0;
	}
	
	s->next_sequence_index = sequenceIndex;
	
	if (s->next_seq_index_updated_cb != NULL) {
		s->next_seq_index_updated_cb(s);
	}
	
	return 1;
}

void sequencer_stop(step_sequencer_t * s) {
	if (s->current_state != kSequencerState_Stopped) {
		seq_resetCurrentStepIndexes(sequencer_getCurrentSequence(s));
	
		s->current_state = kSequencerState_Stopped;
		if (s->state_updated_cb != NULL) {
			s->state_updated_cb(s);
		}
	}
}

void sequencer_play(step_sequencer_t * s) {
	if (s->current_state != kSequencerState_Playing) {
		s->current_state = kSequencerState_Playing;
		if (s->state_updated_cb != NULL) {
			s->state_updated_cb(s);
		}
	}
}

void sequencer_pause(step_sequencer_t * s) {
	if (s->current_state != kSequencerState_Paused) {
		s->current_state = kSequencerState_Paused;
		if (s->state_updated_cb != NULL) {
			s->state_updated_cb(s);
		}
	}
}

void sequencer_clearPattern(step_sequencer_t * s, uint8_t sequence_index, uint8_t patternIndex) {
	if (sequence_index >= N_SEQUENCES) {
		return;
	}
	
	seq_clearPattern(&s->sequences[sequence_index], patternIndex);
}

void sequencer_clearAllPatterns(step_sequencer_t * s, uint8_t sequence_index) {
	if (sequence_index >= N_SEQUENCES) {
		return;
	}
	
	for (size_t i = 0; i < N_SEQUENCES; i++) {
		sequencer_clearPattern(s, sequence_index, i);
	}
	//TODO: update grid ?
}

//void sequencer_setCurrentPatternIndex(step_sequencer_t * s, uint8_t sequence_index, uint8_t pattern_index) {
//	if (s->current_pattern_index != index) {
//		s->current_pattern_index = index;
//	}
//
//}

void sequencer_resetCurrentStepIndexes(step_sequencer_t * s, uint8_t sequence_index) {
	if (sequence_index < N_SEQUENCES) {
		seq_resetCurrentStepIndexes(&s->sequences[sequence_index]);
		
		//TODO: add specific cb
		if (s->state_updated_cb != NULL) {
			s->state_updated_cb(s);
		}
	}
}

void sequencer_setPatternStepValue(step_sequencer_t * s, uint8_t sequence_index, uint8_t patternIndex, uint8_t stepIndex, uint8_t value) {
	if (sequence_index < N_SEQUENCES) {
		seq_setPatternStepValue(&s->sequences[sequence_index], patternIndex, stepIndex, value);
		if (s->step_updated_cb != NULL) {
			s->step_updated_cb(s, sequence_index, patternIndex, stepIndex);
		}
	}
}

void sequencer_togglePatternStepValue(step_sequencer_t * s, uint8_t sequence_index, uint8_t patternIndex, uint8_t stepIndex) {
	if (sequence_index < N_SEQUENCES) {
		seq_togglePatternStepValue(&s->sequences[sequence_index], patternIndex, stepIndex);
	}
}

int sequencer_setMutedPattern(step_sequencer_t * s, uint8_t patternIndex, bool value) {
	if (patternIndex > N_TRIGGERS) {
		return -1;
	}
	
	s->muted_triggers[patternIndex] = value;
	
	if (s->muted_triggers_updated_cb != NULL) {
		s->muted_triggers_updated_cb(s, patternIndex);
	}
	
	return 1;
}

uint8_t sequencer_getCurrentSequenceIndex(step_sequencer_t * s) {
	return s->current_sequence_index;
}

step_sequence_t * sequencer_getCurrentSequence(step_sequencer_t *s) {
	return &s->sequences[sequencer_getCurrentSequenceIndex(s)];
}

step_sequence_t * sequencer_getNextSequence(step_sequencer_t *s) {
	if (s->next_sequence_index != NO_NEXT_SEQUENCE && s->next_sequence_index < N_SEQUENCES) {
		return &s->sequences[(uint8_t)s->next_sequence_index];
	}
	
	return NULL;
}
