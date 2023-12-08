//
//  sequencer.h
//  LaunchpadSeq
//
//  Created by Guillaume Gekiere on 18/10/2023.
//

#ifndef sequencer_h
#define sequencer_h

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include "sequence.h"

#define N_SEQUENCES                 16
//TODO: remove from here
#define N_TRIGGERS                  8
#define NO_NEXT_SEQUENCE			-1
#define DEFAULT_CLOCK_DIVIDER		3

typedef enum SequencerState {
	kSequencerState_Stopped,
	kSequencerState_Playing,
	kSequencerState_Paused
}SequencerState;

typedef enum Direction {
	kDirection_Forward = 1,
	kDirection_Backward = -1
}Direction;

/*
*       Step: A representation of a value at a given time
*       Pattern: Group of pre-defined number of steps
*/

typedef struct step_sequencer_t {
	step_sequence_t				sequences[N_SEQUENCES];
	volatile uint8_t            clock_cpt;
	volatile uint8_t            clock_divider;
	SequencerState              current_state;
	Direction		            current_direction;
	uint8_t						current_sequence_index;
	int8_t						next_sequence_index;
	
	uint8_t						triggers[N_TRIGGERS];
	bool                        muted_triggers[N_TRIGGERS];

	void 						(*step_updated_cb)(void * seq, uint8_t sequence_index, uint8_t patternIndex, uint8_t stepIndex);
	void 						(*pattern_updated_cb)(void * seq, uint8_t sequence_index, uint8_t patternIndex);
	void 						(*muted_triggers_updated_cb)(void * seq, uint8_t pattern_index);
	void 						(*direction_updated_cb)(void * seq);
	void 						(*state_updated_cb)(void * seq);
	void 						(*sequence_index_updated_cb)(void * sequencer, uint8_t sequence_index);
	void						(*trigger_updated_cb)(void *seq, uint8_t triggerIndex);
	void 						(*triggers_updated_cb)(void * seq);
	void 						(*next_seq_index_updated_cb)(void * seq);
} step_sequencer_t;

void 				sequencer_init(step_sequencer_t * s);
void 				sequencer_clearAll(step_sequencer_t * s);
void 				sequencer_clock(step_sequencer_t * s);
void 				sequencer_stop(step_sequencer_t * s);
void 				sequencer_play(step_sequencer_t * s);
void 				sequencer_pause(step_sequencer_t * s);
uint8_t				sequencer_getCurrentSequenceIndex(step_sequencer_t * s);
step_sequence_t *	sequencer_getCurrentSequence(step_sequencer_t *s);
int 				sequencer_setSequenceIndex(step_sequencer_t * s, uint8_t sequenceIndex);
int					sequencer_setMutedPattern(step_sequencer_t * s, uint8_t patternIndex, bool value);
int 				sequencer_setNextSequenceIndex(step_sequencer_t * s, int8_t sequenceIndex);
int 				sequencer_load(step_sequencer_t * s);
void 				sequencer_resetCurrentStepIndexes(step_sequencer_t * s, uint8_t sequence_index);

#endif /* sequencer_h */
