/*************************************************************** -*- c++ -*-
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


//There are two places to be kept in sync with these enums:
//TeletextBrowser::TranslateKey and 
//the constants in cPluginTeletextosd::initTexts
enum eTeletextAction { Zoom, HalfPage, SwitchChannel,
                       DarkScreen, /*SuspendReceiving,*/ LastAction }; //and 100-899 => jump to page

static const char *st_modes[] =
{
      tr("Zoom"),
      tr("Half page"),
      tr("Change channel"),
      tr("Switch background"),
      //tr("Suspend receiving"),
      tr("Jump to...")
};

enum ActionKeys {
ActionKeyRed,
ActionKeyGreen,
ActionKeyYellow,
ActionKeyBlue,
ActionKeyPlay,
ActionKeyStop,
ActionKeyFastFwd,
ActionKeyFastRew,

LastActionKey
};

//Default values are set in menu.c, setup menu, parsing and storing can be found in osdteletext.c
class TeletextSetup {
public:
   TeletextSetup();
   int mapKeyToAction[10]; //4 color keys + kPlay, kPause etc.
   unsigned int configuredClrBackground;
   int showClock;
   int suspendReceiving;
   int autoUpdatePage;
   int OSDheightPct;
   int OSDwidthPct;
   int OSDtopPct;
   int OSDleftPct;
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
