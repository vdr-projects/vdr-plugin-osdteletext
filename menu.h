/*************************************************************** -*- c++ -*-
 *       Copyright (c) 2003,2004 by Marcel Wiesweg                         *
 *       Copyright (c) 2021      by Peter Bieringer (extenions)            *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef OSDTELETEXT_MENU__H
#define OSDTELETEXT_MENU__H

#include <time.h>

#include <vdr/osd.h>
#include <vdr/osdbase.h>
#include <vdr/status.h>

#include "txtrecv.h"
#include "setup.h"

// status of current channel
enum eChannelInfo {
   ChannelIsLive,
   ChannelIsTuned,
   ChannelIsCached,
   ChannelWasTuned,
   ChannelWasTunedNewChannelIsLive,
   ChannelIsLiveButHasNoTeletext,
   ChannelIsTunedButHasNoTeletext
};

class TeletextBrowser : public cOsdObject {
public:
   TeletextBrowser(cTxtStatus *txtSt,Storage *s);
   ~TeletextBrowser();
   void Show(void);
   static void ChannelSwitched(int ChannelNumber, const eChannelInfo info);
   static void ChannelPage100Stored(int ChannelNumber);
   virtual eOSState ProcessKey(eKeys Key);
protected:
   enum Direction { DirectionForward, DirectionBackward };
   void SetNumber(int i);
   void ShowPage(bool suppressMessage = false);
   void UpdateClock();
   void UpdateHotkey();
   bool DecodePage(bool suppressMessage = false);
   void ChangePageRelative(Direction direction);
   void ChangeSubPageRelative(Direction direction);
   bool CheckPage();
   void ShowAskForChannel();
   bool CheckFirstSubPage(int startWith);
   void SetPreviousPage(int oldPage, int oldSubPage, int newPage);
   bool CheckIsValidChannel(int number);
   int  PageCheckSum();
   void ShowPageNumber();
   void ExecuteAction(eTeletextAction e);
   bool ExecuteActionConfig(eTeletextActionConfig e, int delta);
   int nextValidPageNumber(int start, Direction direction);
   bool TriggerChannelSwitch(const int channelNumber);
   char fileName[PATH_MAX];
   char page[40][24];
   int cursorPos;
   eTeletextAction TranslateKey(eKeys Key);
   bool pageFound;
   bool selectingChannel;
   static eChannelInfo ChannelInfo;
   int hotkeyLevel;
   int delayClearMessage;
   bool needClearMessage;
   int selectingChannelNumber;
   int checkSum;
   cTxtStatus *txtStatus;
   bool paused;
   int previousPage;
   int previousSubPage;
   int pageBeforeNumberInput;
   time_t lastActivity;
   int inactivityTimeout;
   static int currentPage;
   static int currentSubPage;
   static tChannelID channel; // TODO: rename to channelId
   static cChannel channelClass;
   static int currentChannelNumber;
   static int liveChannelNumber;
   static bool switchChannelInProgress;
   static TeletextBrowser* self;
   Storage *storage;
private:
   void ChangeBackground();
};


#endif

// vim: ts=3 sw=3 et
