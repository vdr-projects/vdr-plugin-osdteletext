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
#define HOTKEY_LEVEL_MAX_LIMIT  9  // maximum supported by SetupStore parser: 9, required minimum: 1
#define HOTKEY_LEVEL_MAX_LIMIT_STRING "9"   // as string to be displayed in online help

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
// 1:1 relation between st_modes[] in osdteletext.c + eTeletextAction in setup.h + st_modesFooter in setup.c
enum eTeletextAction {
   Zoom,
   HalfPage,
   SwitchChannel,
   DarkScreen,
   /*SuspendReceiving,*/
   Config,
   LineMode24,
   ToggleConceal,
   TogglePause,
   HotkeyLevelPlus,
   HotkeyLevelMinus,
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
   NotActive, // has to stay always the last one
};

enum eTeletextActionValueType {
   Pct,
   Pix,
   Int,
   Str,
   None,
};


// values stored in setup.c:
extern const char *st_modesFooter[];
extern const char *config_modes[];

enum ActionKeys {
   // keep in sync: static const ActionKeyName st_actionKeyNames in osdteletext.c
   ActionKeyPlay,
   ActionKeyStop,
   ActionKeyFastFwd,
   ActionKeyFastRew,
   ActionKeyOk,

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

enum FooterFlags {
   FooterNormal,
   FooterYellowValue,
   FooterGreenYellowValue
};

//Default values are set in menu.c, setup menu, parsing and storing can be found in osdteletext.c
class TeletextSetup {
public:
   TeletextSetup();
   int mapKeyToAction[(int) ActionKeys::LastActionKey]; // see enum ActionKeys
   int mapHotkeyToAction[(int) ActionHotkeys::LastActionHotkey][HOTKEY_LEVEL_MAX_LIMIT]; // see enum ActionHotkeys and HotkeyLevelMax
   unsigned int configuredClrBackground;
   int showClock;
   int suspendReceiving;
   int autoUpdatePage;
   int OSDheightPct;
   int OSDwidthPct;
   int OSDtopPct;
   int OSDleftPct;
   int OSDframePix;
   int hotkeyLevelMax;
   int inactivityTimeout;
   int HideMainMenu;
   cString txtFontName;
   cStringList txtFontNames;
   int txtFontIndex;
   int txtG0Block;
   int txtG2Block;
   int txtVoffset;
   const char *txtBlock[11];
   int colorMode4bpp;
   int lineMode24;
};

extern TeletextSetup ttSetup;

#endif

// vim: ts=3 sw=3 et
