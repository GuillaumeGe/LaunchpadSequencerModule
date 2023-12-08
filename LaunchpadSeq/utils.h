//
//  utils.h
//  LaunchpadSeq
//
//  Created by Guillaume Gekière on 30/11/2023.
//

#ifndef utils_h
#define utils_h

//size_t utils_circularLoopGetIndex(size_t currentIndex, int incr, size_t endIndex);

static inline size_t utils_circularLoopGetIndex(size_t currentIndex, int incr, size_t endIndex) {
	return (currentIndex + incr + endIndex) % endIndex;
}

#endif /* utils_h */
