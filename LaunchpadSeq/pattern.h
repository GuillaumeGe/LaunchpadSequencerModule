//
//  pattern.h
//  LaunchpadSeq
//
//  Created by Guillaume Gekière on 30/11/2023.
//

#ifndef pattern_h
#define pattern_h

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

#define MAX_STEPS                   64

typedef struct step_sequence_t step_sequence_t;

typedef struct step_pattern_t {
	uint8_t                     steps[MAX_STEPS];
	step_sequence_t	*			sequence_ref;
	
	void 						(*pattern_updated_cb)(void * p);
	void 						(*step_updated_cb)(void * p, uint8_t stepIndex);
} step_pattern_t;

void 			pattern_init(step_pattern_t * p);
void 			pattern_clear(step_pattern_t *p);
int	 			pattern_setStep(step_pattern_t *p, size_t index, uint8_t value);
int	 			pattern_setSteps(step_pattern_t *p, uint8_t * values, size_t length);


#endif /* pattern_h */
