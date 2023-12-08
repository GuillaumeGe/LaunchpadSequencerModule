//
//  sequence.c
//  LaunchpadSeq
//
//  Created by Guillaume Gekière on 30/11/2023.
//

#include "sequence.h"
#include "sequencer.h"
#include "utils.h"

void _seq_pattern_step_update_callback(void * pattern, uint8_t stepIndex) {
	step_pattern_t * p = (step_pattern_t *)pattern;
	
	if (p != NULL && p->sequence_ref != NULL) {
		if (p->sequence_ref->step_updated_cb != NULL) {
			size_t patternIndex = 0;
			for (size_t i = 0; i < N_TRIGGERS; i++) {
				if (p == &p->sequence_ref->patterns[i]) {
					patternIndex = i;
					break;
				}
			}
			p->sequence_ref->step_updated_cb(p->sequence_ref, patternIndex, stepIndex);
		}
	}
}

void _seq_pattern_update_callback(void * pattern) {
	step_pattern_t * p = (step_pattern_t *)pattern;
	
	if (p != NULL && p->sequence_ref != NULL) {
		if (p->sequence_ref->pattern_updated_cb != NULL) {
			size_t patternIndex = 0;
			for (size_t i = 0; i < N_TRIGGERS; i++) {
				if (p == &p->sequence_ref->patterns[i]) {
					patternIndex = i;
					break;
				}
			}
			p->sequence_ref->pattern_updated_cb(p->sequence_ref, patternIndex);
		}
	}
}

//-----------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------

void seq_init(step_sequence_t * s) {
	s->current_pattern_index = 0;
	s->empty_cpt = 0;
	
	for (size_t i = 0; i < N_TRIGGERS; i++) {
		pattern_init(&s->patterns[i]);
		s->patterns[i].sequence_ref = s;
		s->patterns[i].step_updated_cb = _seq_pattern_step_update_callback;
		s->patterns[i].pattern_updated_cb = _seq_pattern_update_callback;
		
		seq_setLastStepIndex(s, i, DEFAULT_STEPS);
		seq_linkPatternSteps(s, i, false);
	}
	
	seq_resetCurrentStepIndexes(s);
}

void seq_stop(step_sequence_t * s) {
	// Reset all steps indexes to 0
	seq_resetCurrentStepIndexes(s);
}

void seq_play(step_sequence_t * s) {
	
}

void seq_pause(step_sequence_t * s) {
	
}

void seq_clearPattern(step_sequence_t * s, uint8_t patternIndex) {
	if (patternIndex < N_TRIGGERS) {
		pattern_clear(&s->patterns[patternIndex]);
	}
}

void seq_clearAllPatterns(step_sequence_t * s) {
	s->empty_cpt = 0;
	for (int i = 0; i < N_TRIGGERS; i++) {
		seq_clearPattern(s, i);
	}
}

void seq_setCurrentPatternIndex(step_sequence_t * s, uint8_t index) {
	if (s->current_pattern_index != index) {
		s->current_pattern_index = index;
	}
}

void seq_resetCurrentStepIndexes(step_sequence_t * s) {
	for (size_t i = 0; i < N_TRIGGERS; i++) {
		s->current_step_indexes[i] = 0;
	}
}

int seq_setLastStepIndex(step_sequence_t *s, uint8_t patternIndex, uint8_t index) {
	if (patternIndex >= N_TRIGGERS || index > MAX_STEPS) {
		return -1;
	}
	
	if (s->last_step_indexes[patternIndex] != index) {
		s->last_step_indexes[patternIndex] = index;
		
		if (s->last_step_indexes[patternIndex] > s->length) {
			s->length = s->last_step_indexes[patternIndex];
		} else {
			// find longest
			s->length = 1;
			for (size_t i = 0; i < N_TRIGGERS; i++) {
				if (s->last_step_indexes[patternIndex] > s->length) {
					s->length = s->last_step_indexes[patternIndex];
				}
			}
		}
		
		if (s->pattern_updated_cb != NULL) {
			s->pattern_updated_cb(s, patternIndex);
		}
	}
	
	return 1;
}

int seq_setPatternStepValue(step_sequence_t * s, uint8_t patternIndex, uint8_t stepIndex, uint8_t value) {
	if (patternIndex < N_TRIGGERS) {
		if (s->patterns[patternIndex].steps[stepIndex] != value) {
			// incr/decr cpt to know if the sequence is empty
			if (value > 0) {
				s->empty_cpt++;
			} else {
				s->empty_cpt--;
			}
			pattern_setStep(&s->patterns[patternIndex], stepIndex, value);
			return 1;
		}
		return 0;
	}
	
	return -1;
}

void seq_togglePatternStepValue(step_sequence_t * s, uint8_t patternIndex, uint8_t stepIndex) {
	if (patternIndex < N_TRIGGERS && stepIndex < MAX_STEPS) {
		uint8_t curVal = s->patterns[patternIndex].steps[stepIndex];
		if (curVal) {
			seq_setPatternStepValue(s, patternIndex, stepIndex, 0);
		} else {
			seq_setPatternStepValue(s, patternIndex, stepIndex, 255);
		}
	}
}

void seq_incrCurrentStepIndexes(step_sequence_t * s, int value) {
	uint8_t prev[N_TRIGGERS] = {0};
	
	for (size_t i = 0; i < N_TRIGGERS; i++) {
		prev[i] = s->current_step_indexes[i];
		s->current_step_indexes[i] = utils_circularLoopGetIndex(s->current_step_indexes[i], value, s->last_step_indexes[i]);
		//s->current_step_indexes[i] = (s->current_step_indexes[i] + value + s->last_step_indexes[i]) % s->last_step_indexes[i]; //circular loop (0 to n)
		// Update only previous col and current col to avoid a complete grid update (reduces midi traffic)
		if (s->step_updated_cb != NULL) {
			s->step_updated_cb(s, i, s->current_step_indexes[i]);
			s->step_updated_cb(s, i, prev[i]);
		}
	}
}

int seq_linkPatternSteps(step_sequence_t * s, uint8_t patternIndex, bool value) {
	if (patternIndex > N_TRIGGERS) {
		return -1;
	}
	s->link_steps[patternIndex] = value;
	
	if (s->pattern_updated_cb != NULL) {
		s->pattern_updated_cb(s, patternIndex);
	}
	
	return 0;
}

bool seq_isEmpty(step_sequence_t * s) {
	return s->empty_cpt > 0;
}
