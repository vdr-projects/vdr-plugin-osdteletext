/*************************************************************** -*- c++ -*-
 *       Copyright (c) < 2021    by TODO                                   *
 *       Copyright (c) 2021      by Peter Bieringer (extenions)            *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/


#ifndef __SETUP_H
#define __SETUP_H

#include <vdr/config.h>

// max for setup
#define HOTKEY_LEVEL_MAX_LIMIT         9  // maximum supported by SetupStore parser: 9, required minimum: 1
#define HOTKEY_LEVEL_MAX_LIMIT_STRING "9" // as string to be displayed in online help

#define OSD_PRESET_MAX_LIMIT         9  // maximum supported by SetupStore parser: 9, required minimum: 1
#define OSD_PRESET_MAX_LIMIT_STRING "9" // as string to be displayed in online help

// min/max for setup
#define OSDleftPctMin      0
#define OSDleftPctMax     90
#define OSDtopPctMin       0
#define OSDtopPctMax      90
#define OSDwidthPctMin    10
#define OSDwidthPctMax   100
#define OSDheightPctMin   10
#define OSDheightPctMax  100
#define OSDframePixMin     0
#define OSDframePixMax   100
#define txtVoffsetMin    -10
#define txtVoffsetMax    +10
#define BackTransMin       0
#define BackTransMax     255

// There are two places to be kept in sync with these enums:
//  TeletextBrowser::TranslateKey and 
// 1:1 relation between st_modes[] in osdteletext.c + eTeletextAction in setup.h + st_modesHotkey in setup.c
enum eTeletextAction {
   Zoom,
   HalfPage,
   SwitchChannel,
   DarkScreen,
   Config,
   LineMode24,
   ToggleConceal,
   TogglePause,
   HotkeyLevelPlus,
   HotkeyLevelMinus,
   OsdPresetPlus,
   OsdPresetMinus,
   LastAction // has to stay always as the last one (special flag for 'jump to pages')
}; //and 100-899 => jump to page

enum eTeletextActionConfig {
   Left,
   Top,
   Width,
   Height,
   Frame,
   Font,
   Voffset,
   BackTrans,
   LastActionConfig, // has to stay always the last one
};

enum eTeletextActionValueType {
   Pct,
   Pix,
   Int,
   Str,
   None,
};


// values stored in setup.c:
extern const char *st_modesHotkey[];
extern const char *config_modes[];

enum ActionKeys {
   // keep in sync: static const ActionKeyName st_actionKeyNames in osdteletext.c
   ActionKeyFastRew,
   ActionKeyFastFwd,
   ActionKeyStop,
   ActionKeyOk,
   ActionKeyPlay,

   LastActionKey
};

enum ActionHotkeys {
   // keep in sync: static const ActionKeyName st_actionHotkeyNames in osdteletext.c
   ActionHotkeyRed,
   ActionHotkeyGreen,
   ActionHotkeyYellow,
   ActionHotkeyBlue,

   LastActionHotkey
};

enum HotkeyFlag {
   HotkeyNormal,
   HotkeyYellowValue,
   HotkeyGreenYellowValue
};

enum InfoLineFlag {
   InfoLine1,
   InfoLine2
};

enum HintLinesMode {
   HintLinesHotkeys = 0,
   HintLinesNone = 1,
   HintLinesHotkeysAndStdkeys = 2
};

//Default values are set in menu.c, setup menu, parsing and storing can be found in osdteletext.c
class TeletextSetup {
public:
   TeletextSetup();
   bool migrationFlag_2_2;
   int osdConfig[(int) eTeletextActionConfig::LastActionConfig][OSD_PRESET_MAX_LIMIT];
   int mapKeyToAction[(int) ActionKeys::LastActionKey]; // see enum ActionKeys
   int mapHotkeyToAction[(int) ActionHotkeys::LastActionHotkey][HOTKEY_LEVEL_MAX_LIMIT]; // see enum ActionHotkeys and HotkeyLevelMax
   int configuredClrBackground; // legacy TODO remove >= 2.3.0
   int showClock;
   int autoUpdatePage;
   int osdPresetMax;
   int hotkeyLevelMax;
   int HideMainMenu;
   cStringList txtFontNames;
   int txtG0Block;
   int txtG2Block;
   const char *txtBlock[11]; // see osdteletext.c
   int colorMode4bpp;
   int lineMode24;
   const char *lineMode[3]; // see osdteletext.c

   // current value of osdPreset
   int osdPreset;
};

// shortcut to OSD config value of current preset
#define TTSETUPPRESET(type) ttSetup.osdConfig[type][ttSetup.osdPreset]

// shortcut to OSD config value of current preset converted to TCOLOR value incl. negate
#define TTSETUPPRESET_TCOLOR(type) ((tColor) (((uint32_t) (255 - (ttSetup.osdConfig[type][ttSetup.osdPreset] & 0xff))) << 24))

// shortcut to OSD config value of current preset converted to font name
#define TTSETUPPRESET_FONTNAME(type) ttSetup.txtFontNames[TTSETUPPRESET(type)]

// Teletext display lines
//  ttSetup.lineMode24 == HintLinesHotkeys     : 25
//  ttSetup.lineMode24 == HintLinesNone        : 24
//  ttSetup.lineMode24 == (others)             : 27
#define TT_DISPLAY_LINES  ((ttSetup.lineMode24 == HintLinesNone) ? 24 : ((ttSetup.lineMode24 == HintLinesHotkeys) ? 25 : 27))


extern TeletextSetup ttSetup;

#endif

// vim: ts=3 sw=3 et
