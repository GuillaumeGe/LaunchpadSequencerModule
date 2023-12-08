//
//  sequence.h
//  LaunchpadSeq
//
//  Created by Guillaume Gekière on 30/11/2023.
//

#ifndef sequence_h
#define sequence_h

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include "pattern.h"

typedef struct step_sequencer_t step_sequencer_t;

#define DEFAULT_STEPS               16
#define N_TRIGGERS                  8

typedef struct step_sequence_t {
	step_pattern_t              patterns[N_TRIGGERS];          				// N_TRIGGERS triggers with MAX_STEPS steps
	uint8_t                     last_step_indexes[N_TRIGGERS];
	bool                     	link_steps[N_TRIGGERS];						// tells if we need to link adjacent steps together (as a long gate)
	
	uint8_t                     current_pattern_index;
	volatile uint8_t            current_step_indexes[N_TRIGGERS];
	
	step_sequencer_t *			sequencer_ref;
	uint8_t						length;
	uint16_t					empty_cpt;									// MAX N_TRIGGERS * MAX_STEPS (8 * 64 = 512)
	
	void 						(*step_updated_cb)(void * seq, uint8_t patternIndex, uint8_t stepIndex);
	void 						(*pattern_updated_cb)(void * seq, uint8_t patternIndex);
} step_sequence_t;

void 			seq_init(step_sequence_t * s);
void 			seq_stop(step_sequence_t * s);
void 			seq_play(step_sequence_t * s);
void 			seq_pause(step_sequence_t * s);

void 			seq_clearPattern(step_sequence_t * s, uint8_t patternIndex);
void 			seq_clearAllPatterns(step_sequence_t * s);
int 			seq_setPatternStepValue(step_sequence_t * s, uint8_t patternIndex, uint8_t stepIndex, uint8_t value);
void 			seq_togglePatternStepValue(step_sequence_t * s, uint8_t patternIndex, uint8_t stepIndex);

void 			seq_resetCurrentStepIndexes(step_sequence_t * s);
int				seq_setLastStepIndex(step_sequence_t *s, uint8_t patternIndex, uint8_t index);
int 			seq_linkPatternSteps(step_sequence_t * s, uint8_t patternIndex,  bool value);
bool 			seq_isEmpty(step_sequence_t * s);
uint8_t 		seq_length(step_sequence_t * s);
void 			seq_incrCurrentStepIndexes(step_sequence_t * s, int value);

#endif /* sequence_h */
