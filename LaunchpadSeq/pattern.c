//
//  pattern.c
//  LaunchpadSeq
//
//  Created by Guillaume Gekière on 30/11/2023.
//

#include "pattern.h"
#include <string.h>

void pattern_init(step_pattern_t * p) {
	pattern_clear(p);
}

void pattern_clear(step_pattern_t *p) {
	for (size_t i = 0; i < MAX_STEPS; i++) {
		p->steps[i] = 0x00;
		//pattern_setStep(p, i, 0x00);
	}
	
	if (p->pattern_updated_cb != NULL) {
		p->pattern_updated_cb(p);
	}
}

int pattern_setStep(step_pattern_t *p, size_t index, uint8_t value) {
	if (index > MAX_STEPS) {
		return -1;
	}
	
	p->steps[index] = value;
	
	if (p->step_updated_cb != NULL) {
		p->step_updated_cb(p, index);
	}
	
	return 1;
}

int pattern_setSteps(step_pattern_t *p, uint8_t * values, size_t length) {
	if (values == NULL || length > MAX_STEPS) {
		return -1;
	}
	
	memcpy(&p->steps, values, length);
	
	if (p->pattern_updated_cb != NULL) {
		p->pattern_updated_cb(p);
	}
	
	return 1;
}



