//
//  midi.h
//  LaunchpadSeq
//
//  Created by Guillaume Gekière on 18/10/2023.
//

#ifndef midi_h
#define midi_h

typedef struct SLMIDIPacket {
	uint64_t				timestamp;
	uint16_t				length;
	uint8_t					data[32];
} SLMIDIPacket;

typedef enum SLMIDIMessageType {
	kSLMIDIMessageType_NoteOff						    = 0x80,
	kSLMIDIMessageType_NoteOn						    = 0x90,
	kSLMIDIMessageType_AftertouchPoly					= 0xA0,
	kSLMIDIMessageType_ControlChange				    = 0xB0,
	kSLMIDIMessageType_ProgramChange				    = 0xC0,
	kSLMIDIMessageType_AftertouchChannel		    	= 0xD0,
	kSLMIDIMessageType_PitchWheel					    = 0xE0,
	kSLMIDIMessageType_System						    = 0xF0,
	kSLMIDIMessageType_SystemClockTick				    = 0xFA,
	kSLMIDIMessageType_SystemClockStart				    = 0xFB,
	kSLMIDIMessageType_SystemClockContinue		    	= 0xFC,
	kSLMIDIMessageType_SystemClockStop				    = 0xFD,
	kSLMIDIMessageType_SystemClockActiveSensing			= 0xFE,
	kSLMIDIMessageType_SystemClockReset			    	= 0xFF
} SLMIDIMessageType;

#endif /* midi_h */
