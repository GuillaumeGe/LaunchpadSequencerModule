//
//  launchpad_defs.h
//  LaunchpadSeq
//
//  Created by Guillaume Gekière on 01/10/2023.
//

#ifndef launchpad_defs_h
#define launchpad_defs_h

#define LS_ROWS								8
#define LS_COLS								8
#define LS_MAX_STEPS_PER_ROW				LS_COLS
#define LS_COLOR_NONE						0x00
#define LS_COLOR_RED						0x0F
#define LS_COLOR_LOW_RED					0x0D
#define LS_COLOR_GREEN						0x3C
#define LS_COLOR_LOW_GREEN					0x1C
#define LS_COLOR_AMBER						0x3F
#define LS_COLOR_YELLOW						0x3E
#define LS_COLOR_LOW_YELLOW					0x3E


//LS button mapping

#define LS_BT_UP_ARROW						0xB068
#define LS_BT_DOWN_ARROW					0xB069
#define LS_BT_LEFT_ARROW					0xB06A
#define LS_BT_RIGHT_ARROW					0xB06B
#define LS_BT_SESSION						0xB06C
#define LS_BT_USER1							0xB06D
#define LS_BT_USER2							0xB06E
#define LS_BT_MIXER							0xB06F

#define LS_BT_VOL							0x9008
#define LS_BT_PAN							0x9018
#define LS_BT_SNDA							0x9028
#define LS_BT_SNDB							0x9038
#define LS_BT_STOP							0x9048
#define LS_BT_TRKON							0x9058
#define LS_BT_SOLO							0x9068
#define LS_BT_ARM							0x9078

#define LS_BT_SHIFT							LS_BT_MIXER
#define LS_BT_CLEAR							LS_BT_USER2
#define LS_BT_MODE							LS_BT_SESSION
#define LS_BT_RESET							LS_BT_USER1

#define LS_PKT_TO_GRID_POS(x,y)				x + (y * 16)
#define LS_BT_CONVERT(b1, b2)				b1 << 8 | b2

#endif /* launchpad_defs_h */
