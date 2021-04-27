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
  
#include <map>
#include <time.h>

#include <vdr/interface.h>
#include <vdr/config.h>

#include "menu.h"
#include "display.h"
#include "setup.h"
#include "txtrecv.h"
#include "logging.h"

#define GET_HUNDREDS(x) ( ( (x) - ((x)%256) ) /256 )
#define GET_TENS(x)  ( (( (x) - ((x)%16) )%256 ) /16 )
#define GET_ONES(x)   ( (x)%16 )

#define GET_HUNDREDS_DECIMAL(x) ( ( (x) - ((x)%100) ) /100 )
#define GET_TENS_DECIMAL(x)  ( (( (x) - ((x)%10) )%100 ) /10 )
#define GET_ONES_DECIMAL(x)   ( (x)%10 )

#define PSEUDO_HEX_TO_DECIMAL(x) ( (GET_HUNDREDS_DECIMAL(x))*256 + (GET_TENS_DECIMAL(x))*16 + (GET_ONES_DECIMAL(x)) )

using namespace std;
   
typedef map<int,int> IntMap;
IntMap channelPageMap;

//static variables
int TeletextBrowser::currentPage=0x100; //Believe it or not, the teletext numbers are somehow hexadecimal
int TeletextBrowser::currentSubPage=0;
tChannelID TeletextBrowser::channel;
cChannel TeletextBrowser::channelClass;
int TeletextBrowser::currentChannelNumber=0;
int TeletextBrowser::liveChannelNumber=0;
bool TeletextBrowser::switchChannelInProgress = false;
eChannelInfo TeletextBrowser::ChannelInfo;
TeletextBrowser* TeletextBrowser::self=0;
tColor clrBackground;
bool clrBackgroundInit = false;

eTeletextActionConfig configMode = NotActive;

TeletextBrowser::TeletextBrowser(cTxtStatus *txtSt,Storage *s)
  : cursorPos(0), pageFound(true), selectingChannel(false),
    delayClearMessage(0),
    needClearMessage(false), selectingChannelNumber(-1), txtStatus(txtSt),
    suspendedReceiving(false), previousPage(currentPage),
    previousSubPage(currentSubPage), pageBeforeNumberInput(currentPage),
    lastActivity(time(NULL)), inactivityTimeout(-1), storage(s)
{
   if (!clrBackgroundInit) {
      clrBackground = (tColor) ttSetup.configuredClrBackground; // default
      clrBackgroundInit = true;
   };

   self=this;

   //if (txtStatus)
    //  txtStatus->ForceReceiving(true);
}


TeletextBrowser::~TeletextBrowser() {
   Display::Delete();

   self=0;
   /*if (txtStatus) {
      if (suspendedReceiving)
         txtStatus->ForceSuspending(false);
      txtStatus->ForceReceiving(false);
   }*/
}

void TeletextBrowser::Show(void) {
    Display::SetMode(Display::mode, clrBackground);
    ShowPage();
}

bool TeletextBrowser::CheckIsValidChannel(int number) {
#if defined(APIVERSNUM) && APIVERSNUM >= 20301
    LOCK_CHANNELS_READ;
    return (Channels->GetByNumber(number) != 0);
#else
    return (Channels.GetByNumber(number) != 0);
#endif
}

void TeletextBrowser::ChannelSwitched(int ChannelNumber, const eChannelInfo info) {
   static eChannelInfo infoLast = ChannelIsLive;
   static int ChannelNumberLast = 0;
   DEBUG_OT_TXTRCVC("called with ChNu=%d ChNuLa=%d liChNu=%d info=%d infoLa=%d switchChannelInProgress=%s", ChannelNumber, ChannelNumberLast, liveChannelNumber, info, infoLast, BOOLTOTEXT(switchChannelInProgress));
#if defined(APIVERSNUM) && APIVERSNUM >= 20301
   LOCK_CHANNELS_READ;
   const cChannel *chan=Channels->GetByNumber(ChannelNumber);
#else
   cChannel *chan=Channels.GetByNumber(ChannelNumber);
#endif
   
   if (!chan)
      return;
      
   tChannelID chid=chan->GetChannelID();
   if (chid==tChannelID::InvalidID)
      return;
      
   channel=chid;
   channelClass = *chan; // remember for later to display channel name

   if (info == ChannelIsLive)
      liveChannelNumber= ChannelNumber; // remember active live channel

   ChannelInfo = info; // store info
   
   //store page number of current channel
   IntMap::iterator it;
   channelPageMap[currentChannelNumber] = currentPage;
   currentChannelNumber=ChannelNumber;

   currentPage=0x100;
   currentSubPage=0;

   //see if last page number on this channel was stored
   it=channelPageMap.find(ChannelNumber);
   if (it != channelPageMap.end()) { //found
      currentPage=(*it).second;
   }

   //on the one hand this must work in background mode, when the plugin is not active.
   //on the other hand, if active, the page should be shown.
   //so this self-Pointer.
   if (self) {
      char str[80] = "";
      char str2[80] = "";
      Display::ClearPage();
      enumTeletextColor color = ttcBlue;
      snprintf(str2, sizeof(str2), "%d: %s", channelClass.Number(), channelClass.Name());

      self->delayClearMessage = 1; // default

      if (info == ChannelHasNoTeletext) {
         snprintf(str, sizeof(str), "%s %s (%s %s)", tr("Switch to"), tr("Channel"), tr("without"), tr("Teletext"));
         color = ttcRed;
      }
      else if (info == ChannelIsLive) {
         if (infoLast == ChannelIsLive) {
            // don't display message during zapping
         } else {
            snprintf(str, sizeof(str), "%s %s %s", tr("Switch to"), tr("live"), tr("Channel"));
         };
      }
      else if (info == ChannelIsTuned) {
         if (liveChannelNumber == currentChannelNumber) {
            snprintf(str, sizeof(str), "%s %s", tr("Switch back to live"), tr("Channel"));
            ChannelInfo = ChannelIsLive;
         } else {
            snprintf(str, sizeof(str), "%s - %s", tr("Tuner available"), tr("display current pages"));
            color = ttcMagenta;
         };
      }
      else if (info == ChannelWasTunedNewChannelIsLive) {
         // received trigger that TUNED channel has no longer a receiver but new would be a LIVE channel
         // suppress a message which will be shortly overwritten anyhow by starting receiver on new channel
         ChannelInfo = ChannelIsCached; // new status
      }
      else if (info == ChannelWasTuned) {
         // received trigger that TUNED channel has no longer a receiver
         if (! switchChannelInProgress) {
            // but no channel switch action in progress -> display message
            snprintf(str, sizeof(str), "%s - %s", tr("Tuner busy"), tr("display cached pages"));
            color = ttcCyan;
         } else {
            // suppress a message which will be shortly overwritten anyhow by starting receiver on new channel
         };
         ChannelInfo = ChannelIsCached; // new status
      }
      else if (info == ChannelIsCached) {
         snprintf(str, sizeof(str), "%s %s %s", tr("Switch to"), tr("cached"), tr("Channel"));
         color = ttcCyan;
      }
      else {
         // all cases catched
      };

      self->ShowPage();

      if (strlen(str) > 0) {
         self->needClearMessage=true;
         Display::DrawMessage(str, str2, color);
      };
   }

   // store for acting related on next call
   infoLast = info;
   ChannelNumberLast = ChannelNumber;

   // clear status
   switchChannelInProgress = false;
}


bool TeletextBrowser::TriggerChannelSwitch(const int channelNumber) {
   bool result = false;

   // switch to new channel
#if defined(APIVERSNUM) && APIVERSNUM >= 20301
   LOCK_CHANNELS_READ;
   const cChannel* newChannel = Channels->GetByNumber(channelNumber);
#else
   const cChannel* newChannel = Channels.GetByNumber(channelNumber);
#endif
   if (!newChannel) return false;

   cDevice *device = cDevice::GetDeviceForTransponder(newChannel, TRANSFERPRIORITY - 1);
   if (device != NULL) {
      needClearMessage = true;
      switchChannelInProgress = true;
      if (device->SwitchChannel(newChannel, (channelNumber == liveChannelNumber) ? true : false)) {
         // Display::DrawMessage(tr("Channel Tuning Successful"), ttcGreen);
         result = true;
         DEBUG_OT_TXTRCVC("DVB %d successful tuned to channel %d (live=%s)", device->DeviceNumber(), channelNumber, BOOLTOTEXT(channelNumber == liveChannelNumber));
      } else {
         needClearMessage = true;
         delayClearMessage = 5;
         Display::DrawMessage(tr("Channel Tuning Not Successful"), ttcRed);
         DEBUG_OT_TXTRCVC("DVB %d cannot tune to channel %d", device->DeviceNumber(), channelNumber);
      };
   } else {
      needClearMessage = true;
      delayClearMessage = 2;
      Display::DrawMessage(tr("No Free Tuner Found - Use Cache Only"), ttcYellow);
      DEBUG_OT_TXTRCVC("no free tuner available to tune to channel %d (use cache)", channelNumber);
      ChannelSwitched(channelNumber, ChannelIsCached);
   };

   return (result);
};


eOSState TeletextBrowser::ProcessKey(eKeys Key) {
   cDisplay::enumZoom zoomR;
   Display::Mode modeR;
   tColor bgcR, bgcSetup = ttSetup.configuredClrBackground;
   bool changedConfig = false;

   if (Key != kNone)
      lastActivity = time(NULL);

   if (Key != kNone) DEBUG_OT_KEYS("called with Key=%d", Key);

   switch (Key) {
      case k1: SetNumber(1);break;
      case k2: SetNumber(2);break;
      case k3: SetNumber(3);break;
      case k4: SetNumber(4);break;
      case k5: SetNumber(5);break;
      case k6: SetNumber(6);break;
      case k7: SetNumber(7);break;
      case k8: SetNumber(8);break;
      case k9: SetNumber(9);break;
      case k0: 
         //same behavior for 0 as VDR does it with channels
         if ((cursorPos==0)  && (!selectingChannel)) {
            //swap variables
            int tempPage=currentPage;
            int tempSubPage=currentSubPage;
            currentPage=previousPage;
            currentSubPage=previousSubPage;
            previousPage=tempPage;
            previousSubPage=tempSubPage;
            ShowPage();
         } else
            SetNumber(0);
         break;

      case kOk: 
         if (selectingChannel) {
            selectingChannel=false;
            Display::ClearMessage();
            if (selectingChannelNumber>0) {
               if (CheckIsValidChannel(selectingChannelNumber)) {
                  if (selectingChannelNumber != liveChannelNumber) {
                     DEBUG_OT_KEYS("trigger switch to channel %d", selectingChannelNumber);
                     txtStatus->SetNonLiveChannelNumber(selectingChannelNumber); // preload next channel switch with a non-live channel (-> TUNED)
                     TriggerChannelSwitch(selectingChannelNumber);
                  } else {
                     // nothing todo
                     DEBUG_OT_KEYS("no need to trigger switch to channel %d because currently running as live channel", selectingChannelNumber);
                     ChannelSwitched(liveChannelNumber, ChannelIsLive);
                  };
               }
               else {
                  needClearMessage=true;
                  delayClearMessage = 3;
                  char str[9];
                  snprintf(str, sizeof(str), "%d", selectingChannelNumber);
                  Display::DrawMessage(trVDR("*** Invalid Channel ***"), str, ttcRed);
               }
            } else {
               if (selectingChannelNumber != liveChannelNumber) {
                  txtStatus->SetNonLiveChannelNumber(0); // clear non-live channel for next channel switch
                  DEBUG_OT_KEYS("trigger switch to channel %d", liveChannelNumber);
                  TriggerChannelSwitch(liveChannelNumber);
                  selectingChannelNumber = liveChannelNumber;
               } else {
                  // nothing todo
                  DEBUG_OT_KEYS("no need to trigger switch to channel because unchanged");
               };
            }
         } else {
            ExecuteAction(TranslateKey(Key));
         };
         break;        

      case kBack: return osEnd; 

      case kNone: //approx. every second
         DEBUG_OT_KNONE("section 'kNone' reached with GetPaused=%s needClearMessage=%s delayClearMessage=%d", BOOLTOTEXT(Display::GetPaused()), BOOLTOTEXT(needClearMessage), delayClearMessage);
         //checking if page changed
         if ((delayClearMessage == 0) && ! Display::GetPaused() && pageFound && ttSetup.autoUpdatePage && cursorPos==0 && !selectingChannel && (PageCheckSum() != checkSum) ) {
            DEBUG_OT_KNONE("section 'kNone' detected: 'page change'");
            ShowPage();
         //check if page was previously not found and is available now
         } else if ((delayClearMessage == 0) && !pageFound && CheckFirstSubPage(0)) {
            DEBUG_OT_KNONE("section 'kNone' detected: 'previous not found page is now available'");
            ShowPage();
         } else {
            DEBUG_OT_KNONE("section 'kNone' default");
            if (needClearMessage) {
               if (delayClearMessage > 0) {
                  delayClearMessage--;
               } else {
                  needClearMessage=false;
                  Display::ClearMessage();
                  if (!pageFound)
                     // (re-)display "not found" information
                     ShowPage();
               };
            } else {
               delayClearMessage = 0; // reset
            };
            if (! selectingChannel && pageFound) {
               //updating clock
               if (! Display::GetPaused())
                  UpdateClock();

               //updating footer
               UpdateFooter();

               //trigger blink
               if (! Display::GetPaused())
                  Display::SetBlink(not(Display::GetBlink()));
            };
         }
         //check for activity timeout
         if (ttSetup.inactivityTimeout && (time(NULL) - lastActivity > ttSetup.inactivityTimeout*60))
            return osEnd;
         break;

      case kUp:
         if (selectingChannel) {
             selectingChannel=false;
             Display::ClearMessage();
         }

         if (cursorPos != 0) {
            //fully revert cursor
            SetNumber(-3);
         }
         ChangePageRelative(DirectionForward);
         Display::ShowUpperHalf();
         ShowPage();
         break;

      case kDown:
         if (selectingChannel) {
             selectingChannel=false;
             Display::ClearMessage();
         }
         if (cursorPos != 0) {
            //fully reset
            SetNumber(-3);
         }
         ChangePageRelative(DirectionBackward);
         Display::ShowUpperHalf();
         ShowPage();
         break;       

      case kRight:
         if (selectingChannel) {
             selectingChannel=false;
             Display::ClearMessage();
         }
         if (cursorPos != 0) {
            //fully reset
            SetNumber(-3);
         }
         ChangeSubPageRelative(DirectionForward);
         Display::ShowUpperHalf();
         ShowPage();
         break;

      case kLeft:
         if (selectingChannel) {
             selectingChannel=false;
             Display::ClearMessage();
         }
         if (cursorPos != 0) {
            //revert cursor
            SetNumber(-1);
            break;
         }
         ChangeSubPageRelative(DirectionBackward);
         Display::ShowUpperHalf();
         ShowPage();
         break; 
         
      case kRed: 
         if (configMode != NotActive) { // catch config mode
            changedConfig = ExecuteActionConfig(configMode, -1); // decrement
            break;
         };
         // continue below

      case kGreen: 
         if (configMode != NotActive) { // catch config mode
            changedConfig = ExecuteActionConfig(configMode, +1); // increment
            break;
         };
         // continue below

      case kYellow:
         if (configMode != NotActive) { // key is inactive in config mode (displaying value)
            break;
         };
         // continue below

      case kBlue:
         if (configMode != NotActive) { // catch config mode
            ExecuteAction(Config);
            break;
         };
         // continue below

      case kPlay:
      //case kPause:   // not passed into plugin somehow
      case kStop:
      //case kRecord:  // not passed into plugin somehow
      case kFastFwd:
      case kFastRew:
         if (cursorPos != 0) {
            //fully reset
            SetNumber(-3);
         }
         ExecuteAction(TranslateKey(Key));
         break;             

      default:
         break;
   }

   if (changedConfig) {
      zoomR = Display::GetZoom(); // remember zoom
      modeR = Display::mode; // remember mode
      if (ttSetup.configuredClrBackground != bgcSetup) {
         bgcR = ttSetup.configuredClrBackground; // color was changed during config
         DEBUG_OT_KEYS("osdteletext: recreate display with remembered mode=%d zoom=%d and setup configured bgc=%08x", modeR, zoomR, bgcR);
      } else {
         bgcR = Display::GetBackgroundColor(); // remember color
         DEBUG_OT_KEYS("osdteletext: recreate display with remembered mode=%d zoom=%d bgc=%08x", modeR, zoomR, bgcR);
      };
      Display::Delete();
      Display::SetMode(modeR, bgcR); // new with remembered mode and backgroud color
      Display::SetZoom(zoomR); // apply remembered zoom
      ShowPage();
   };

   return osContinue;
}

bool TeletextBrowser::ExecuteActionConfig(eTeletextActionConfig e, int delta) {
   bool changedConfig = false;
   int BackTransVal;

#define COND_ADJ_VALUE(value, min, max, delta) \
   if (((value + delta) >= min) && ((value + delta) <= max)) { \
      value += delta; \
      changedConfig = true; \
   } else if ((value + delta) < min) { \
      value = min; \
      changedConfig = true; \
   } else if ((value + delta) > max) { \
      value = max; \
      changedConfig = true; \
   };

   switch (configMode) {
      case Left:
         COND_ADJ_VALUE(ttSetup.OSDleftPct, OSDleftPctMin, OSDleftPctMax, delta);
         break;

      case Top:
         COND_ADJ_VALUE(ttSetup.OSDtopPct, OSDtopPctMin, OSDtopPctMax, delta);
         break;

      case Width:
         COND_ADJ_VALUE(ttSetup.OSDwidthPct, OSDwidthPctMin, OSDwidthPctMax, delta);
         break;

      case Height:
         COND_ADJ_VALUE(ttSetup.OSDheightPct, OSDheightPctMin, OSDheightPctMax, delta);
         break;

      case Frame:
         COND_ADJ_VALUE(ttSetup.OSDframePix, OSDframePixMin, OSDframePixMax, delta);
         break;

      case Font:
         ttSetup.txtFontIndex++;
         if (ttSetup.txtFontIndex >= ttSetup.txtFontNames.Size()) ttSetup.txtFontIndex = 0; // rollover
         ttSetup.txtFontName = ttSetup.txtFontNames[ttSetup.txtFontIndex];
         changedConfig = true;
         break;

      case Voffset:
         COND_ADJ_VALUE(ttSetup.txtVoffset, txtVoffsetMin, txtVoffsetMax, delta);
         break;

      case BackTrans:
         BackTransVal = ((uint32_t) ttSetup.configuredClrBackground) >> 24;
         DEBUG_OT_KEYS("key action: 'Config->BackTrans' BackTransVal=%d BackTransMin=%d BackTransMax=%d delta=%d", BackTransVal, BackTransMin, BackTransMax, delta * 8);
         COND_ADJ_VALUE(BackTransVal, BackTransMin, BackTransMax, delta * 8);
         ttSetup.configuredClrBackground = ((uint32_t) BackTransVal) << 24;
         clrBackground = ttSetup.configuredClrBackground;
         break;

      default:
         // nothing todo
         break;
   };
   return (changedConfig);
};

void TeletextBrowser::ExecuteAction(eTeletextAction e) {
   cDisplay::enumZoom zoomR;
   Display::Mode modeR;

   switch (e) {
      case Zoom:
         DEBUG_OT_KEYS("key action: 'Zoom' Display::GetZoom=%d", Display::GetZoom());
         if (selectingChannel) {
             selectingChannel=false;
             Display::ClearMessage();
         }
               
         switch (Display::GetZoom()) {
               case cDisplay::Zoom_Off:
                  Display::SetZoom(cDisplay::Zoom_Upper);
                  break;
               case cDisplay::Zoom_Upper:
                  Display::SetZoom(cDisplay::Zoom_Lower);
                  break;
               case cDisplay::Zoom_Lower:
                  Display::SetZoom(cDisplay::Zoom_Off);
                  break;
         }
         break;

      case HalfPage:
         DEBUG_OT_KEYS("key action: 'Half Page' Display::mode=%d clrBackground=%08x", Display::mode, clrBackground);
         if (selectingChannel) {
             selectingChannel=false;
             Display::ClearMessage();
         }
                  
         switch (Display::mode) {
            case Display::HalfUpper:
               Display::SetMode(Display::HalfLower, clrBackground);
               break;
            case Display::HalfLower:
               Display::SetMode(Display::HalfUpperTop, clrBackground);
               break;
            case Display::HalfUpperTop:
               Display::SetMode(Display::HalfLowerTop, clrBackground);
               break;
            case Display::HalfLowerTop:
               Display::SetMode(Display::Full, clrBackground);
               break;
            case Display::Full:
               Display::SetMode(Display::HalfUpper, clrBackground);
               break;
            }
            ShowPage();
         break;

      case SwitchChannel:
         DEBUG_OT_KEYS("key action: 'SwitchChannel'");
         if (!selectingChannel) {
             selectingChannelNumber=0;
             selectingChannel=true;
             ShowAskForChannel();
         } else {
             selectingChannel=false;
             Display::ClearMessage();
         };
         break;

      /*case SuspendReceiving:
            if (!txtStatus)
               break;
            //if (suspendedReceiving)
             //  txtStatus->ForceSuspending(false);
            //else
             //  txtStatus->ForceSuspending(true);
            //suspendedReceiving=(!suspendedReceiving);
            break;*/

      case DarkScreen:
         DEBUG_OT_KEYS("key action: 'DarkScreen'");
         ChangeBackground();
         break;

      case LineMode24:
         DEBUG_OT_KEYS("key action: 'LineMode24' lineMode24=%d", ttSetup.lineMode24);
         // toggle LineMode24
         if (ttSetup.lineMode24 != 0) {
            ttSetup.lineMode24 = 0;
         } else {
            ttSetup.lineMode24 = 1;
         };
         zoomR = Display::GetZoom(); // remember zoom
         modeR = Display::mode; // remember mode
         Display::Delete();
         Display::SetMode(modeR, clrBackground); // new with remembered mode and background color
         Display::SetZoom(zoomR); // apply remembered zoom
         ShowPage();
         break;

      case Config:
         DEBUG_OT_KEYS("key action: 'Config' lineMode24=%d configMode=%d", ttSetup.lineMode24, configMode);
         if (ttSetup.lineMode24) {
            // config mode is only supported in 25-line mode
            needClearMessage=true;
            delayClearMessage = 3;
            Display::DrawMessage(tr("*** Config mode is not supported in 24-line mode ***"), ttcYellow);
            break;
         };
         switch(configMode) {
            case NotActive : configMode = Left     ; break; // start config mode
            case Left      : configMode = Top      ; break;
            case Top       : configMode = Width    ; break;
            case Width     : configMode = Height   ; break;
            case Height    : configMode = Frame    ; break;
            case Frame     : configMode = Font     ; break;
            case Font      : configMode = Voffset  ; break;
            case Voffset   : configMode = BackTrans; break;
            case BackTrans : configMode = NotActive; break; // stop config mode
         };
         ShowPage();
         break;

      case ToggleConceal:
         DEBUG_OT_KEYS("key action: 'ToggleConceal' Concealed=%d -> %d", Display::GetConceal(), not(Display::GetConceal()));
         Display::SetConceal(not(Display::GetConceal()));
         ShowPage();
         break;

      case TogglePause:
         if ((ChannelInfo == ChannelIsLive) || (ChannelInfo == ChannelIsTuned)) {
            DEBUG_OT_KEYS("key action: 'TogglePause' paused=%d -> %d", Display::GetPaused(), not(Display::GetPaused()));
            // toggle paused status only if LIVE or TUNED channel (otherwise useless)
            Display::SetPaused(not(Display::GetPaused()));
            ShowPage();
         } else {
            DEBUG_OT_KEYS("key action: 'TogglePause' useless, currently not a LIVE or TUNED channel on OSD");
         };
         break;

      default:
         //In osdteletext.c, numbers are thought to be decimal, the setup page
         //entries will display them in this way. It is a lot easier to do the
         //conversion to hexadecimal here.
         //This means, we convert the number to what it would be if the string
         //had been parsed with hexadecimal base.
         int pageNr=PSEUDO_HEX_TO_DECIMAL((int)e);
         if (0x100<=pageNr && pageNr<=0x899) {
            if (selectingChannel) {
                selectingChannel=false;
                Display::ClearMessage();
            }
            SetPreviousPage(currentPage, currentSubPage, pageNr);
            currentPage=pageNr;
            cursorPos=0;
            currentSubPage=0;

            Display::ShowUpperHalf();
            ShowPage();
         }
         break;
   }
}

// 3-state toggling between configured->transparent->black.
// If configured is black or transparent, do 2-state transparent->black only.
void TeletextBrowser::ChangeBackground()
{
   tColor clrConfig = (tColor)ttSetup.configuredClrBackground;
   tColor clrCurrent = clrBackground;

   if (clrCurrent == clrConfig)
      if (clrConfig == clrTransparent)
         clrBackground = clrBlack;
      else
         clrBackground = clrTransparent;
   else if (clrCurrent == clrBlack)
      if (clrConfig == clrBlack)
         clrBackground = clrTransparent;
      else
         clrBackground = clrConfig;
   else // clrCurrent == clrTransparent
      clrBackground = clrBlack;

   Display::SetBackgroundColor(clrBackground);
}

eTeletextAction TeletextBrowser::TranslateKey(eKeys Key) {
   switch(Key) {
      case kRed:     return (eTeletextAction)ttSetup.mapKeyToAction[ActionKeyRed];
      case kGreen:   return (eTeletextAction)ttSetup.mapKeyToAction[ActionKeyGreen];
      case kYellow:  return (eTeletextAction)ttSetup.mapKeyToAction[ActionKeyYellow];
      case kBlue:    return (eTeletextAction)ttSetup.mapKeyToAction[ActionKeyBlue];
      case kPlay:    return (eTeletextAction)ttSetup.mapKeyToAction[ActionKeyPlay];
      //case kPause:   return (eTeletextAction)ttSetup.mapKeyToAction[ActionKeyPause]; // not passed into plugin somehow
      case kOk:      return (eTeletextAction)ttSetup.mapKeyToAction[ActionKeyOk];
      case kStop:    return (eTeletextAction)ttSetup.mapKeyToAction[ActionKeyStop];
      //case kRecord:  return (eTeletextAction)ttSetup.mapKeyToAction[ActionKeyRecord]; // not passed into plugin somehow
      case kFastFwd: return (eTeletextAction)ttSetup.mapKeyToAction[ActionKeyFastFwd];
      case kFastRew: return (eTeletextAction)ttSetup.mapKeyToAction[ActionKeyFastRew];
      default:       return (eTeletextAction)100; //just to keep gcc quiet
   }
}


void TeletextBrowser::SetNumber(int i) {
   //cursorPos means insertion after, 0<=cursorPos<=2
   if (selectingChannel) {
      selectingChannelNumber = selectingChannelNumber*10+i;  
      ShowAskForChannel();
      return;
   }
   
   //i<0 means revert cursor position
   if (i<0) {
      for (;i<0;i++) {
         switch (cursorPos) {
         case 0:
            return;
         case 1:
            currentPage = currentPage-256*GET_HUNDREDS(currentPage)+256*GET_HUNDREDS(pageBeforeNumberInput);
            break;
         case 2:
            currentPage = currentPage-16*GET_TENS(currentPage)+16*GET_TENS(pageBeforeNumberInput);
            break;
         }
         cursorPos--;
      }
      ShowPageNumber();
      return;
   }

   
   static int tempPage;
   switch (cursorPos) {
   case 0:
      if (i<1) i=1;
      //accept no 9 when cursorPos==0
      if (i>8) i=8;
      tempPage= currentPage;
      pageBeforeNumberInput = currentPage;
      currentPage = currentPage-256*GET_HUNDREDS(currentPage)+256*i;
      break;
   case 1:
      if (i<0) i=0;
      if (i>9) i=9;
      currentPage = currentPage-16*GET_TENS(currentPage)+16*i;
      break;
   case 2:
      if (i<0) i=0;
      if (i>9) i=9;
      currentPage = currentPage-GET_ONES(currentPage)+i;
      pageBeforeNumberInput = currentPage;
      SetPreviousPage(tempPage, currentSubPage, currentPage);
      break;
   }
   pageFound=true; //so that "page ... not found" is not displayed, but e.g. 1**-00
   if (++cursorPos>2) {
      cursorPos=0;
      CheckFirstSubPage(0);
      Display::ShowUpperHalf();
      ShowPage();
   } else {
      ShowPageNumber();
   }
}

//returns whether x, when written in hexadecimal form,
//will only contain the digits 0...9 and not A...F
//in the first three digits.
static inline bool onlyDecimalDigits(int x) {
   return ((  x      & 0xE) < 0xA) &&
          (( (x>>4)  & 0xE) < 0xA) &&
          (( (x>>8)  & 0xE) < 0xA);
}

//after 199 comes 1A0, but if these pages exist, they contain no useful data, so filter them out
int TeletextBrowser::nextValidPageNumber(int start, Direction direction) {
   do {
      switch (direction) {
      case DirectionForward:
         start++;
         break;
      case DirectionBackward:
         start--;
         break;
      }
   } while (!onlyDecimalDigits(start));
   return start;
}

void TeletextBrowser::ChangePageRelative(Direction direction)
{
   int oldpage = currentPage;
   int oldSubPage = currentSubPage;

   do  {
      /*if (back)
         currentPage--;
      else
         currentPage++;*/
      currentPage=nextValidPageNumber(currentPage, direction);
      if (currentPage>0x899) currentPage=0x100;
      if (currentPage<0x100) currentPage=0x899;
      // sub page is always 0 if you change the page
      if (CheckFirstSubPage(0)) {
         SetPreviousPage(oldpage, oldSubPage, currentPage);
         return;
      }
   } while (currentPage != oldpage);

   return;
}

void TeletextBrowser::ChangeSubPageRelative(Direction direction)
{
   int oldsubpage = currentSubPage;

   do  {
      /*if (back)
         currentSubPage--;
      else
         currentSubPage++;*/
      currentSubPage=nextValidPageNumber(currentSubPage, direction);
      if (currentSubPage > 0x99) currentSubPage=0;
      if (currentSubPage < 0) currentSubPage=0x99;

      if (CheckPage())
         return;
   } while (currentSubPage != oldsubpage);

   return;
}

bool TeletextBrowser::CheckFirstSubPage(int startWith) {
   int oldsubpage = currentSubPage;

   do  {
      if (CheckPage())
         return true;
      //currentSubPage++;
      currentSubPage=nextValidPageNumber(currentSubPage, DirectionForward);
      
      if (currentSubPage > 0x99) currentSubPage=0;
      if (currentSubPage < 0) currentSubPage=0x99;

   } while (currentSubPage != oldsubpage);
   
   return false;
}

bool TeletextBrowser::CheckPage()
{
   StorageHandle fd;
   
   if (!(fd=storage->openForReading(PageID(channel, currentPage, currentSubPage), false)) )
      return false;

   storage->close(fd);
   return true;
}

//sets the previousPage variables if and only if new page is different from old page
void TeletextBrowser::SetPreviousPage(int oldPage, int oldSubPage, int newPage)  {
   if (oldPage != newPage) {
      previousPage=oldPage;
      previousSubPage=oldSubPage;
   }
}




void TeletextBrowser::ShowPage() {
   if ((pageFound=DecodePage())) {
      if (ttSetup.autoUpdatePage)
         checkSum=PageCheckSum();
   }
}

void TeletextBrowser::ShowPageNumber() {
   DEBUG_OT_DRPI("called with currentPage=%03x currentSubPage=%02x", currentPage, currentSubPage);
   char str[9];
   snprintf(str, sizeof(str), "%3x-%02x  ", currentPage, currentSubPage);
   if (cursorPos>0) {
      str[2]='*';
      if (cursorPos==1)
         str[1]='*';
   }

   if (ChannelInfo == ChannelIsTuned) {
      str[7]='t';
      Display::DrawPageId(str, ttcMagenta, true); // colored
   }
   else if (liveChannelNumber != currentChannelNumber) {
      str[7]='c';
      Display::DrawPageId(str, ttcCyan, true); // colored
   }
   else
      Display::DrawPageId(str);
}

void TeletextBrowser::ShowAskForChannel() {
   if (selectingChannel) {
      cString str = cString::sprintf(selectingChannelNumber > 0 ? "%s%d" : "%s", tr("Channel (press OK): "), selectingChannelNumber);
      Display::DrawMessage(str, ttcBlue);
   }
}

//this is taken and adapted from the teletext plugin since it uses its data
bool TeletextBrowser::DecodePage() {
   // Load the page and decodes it
   unsigned char cache[sizeof(TelePageData)];
   StorageHandle fd;
   // Take a look if there is a xxx-00 page
   if (currentSubPage==0) {
      if ( !(fd=storage->openForReading(PageID(channel, currentPage,currentSubPage), false)) ) {
         // There is no subpage 0 so look if there is subpage 1
         currentSubPage++;
         // Generate file string
      } else {
         // yes file exists
         storage->close(fd);
      }
   }
   
   if ( (fd=storage->openForReading(PageID(channel, currentPage, currentSubPage), true)) )
   {
      storage->read(cache,sizeof cache,fd); // Read full page data
      storage->close(fd);
      
      Display::HoldFlush();
      Display::ClearMessage();
      Display::RenderTeletextCode(cache);
      ShowPageNumber();
      UpdateClock();
      UpdateFooter();
      Display::ReleaseFlush();
   } else {
      // page doesn't exist
      currentSubPage--;

      Display::HoldFlush();
      char str[80];
      char str2[80];
      snprintf(str2, sizeof(str2), "%d: %s", channelClass.Number(), channelClass.Name());
      enumTeletextColor color = ttcYellow;
      if (ChannelInfo == ChannelHasNoTeletext) {
         snprintf(str, sizeof(str), "%s %s (%s %s)", tr("Switch to"), tr("Channel"), tr("without"), tr("Teletext"));
         color = ttcRed;
      } else {
         ShowPageNumber();
         snprintf(str, sizeof(str), "%s %3x-%02x%s%s %s%s%s%s",tr("Page"),currentPage, currentSubPage
            , (ChannelInfo == ChannelIsCached) ? " " : ""
            , (ChannelInfo == ChannelIsCached) ? tr("in cache") : ""
            , tr("not found")
            , ((ChannelInfo == ChannelIsTuned) || (ChannelInfo == ChannelIsLive)) ? " (" : ""
            , ((ChannelInfo == ChannelIsTuned) || (ChannelInfo == ChannelIsLive)) ? tr("please wait") : ""
            , ((ChannelInfo == ChannelIsTuned) || (ChannelInfo == ChannelIsLive)) ? ")" : ""
         );
         if (ChannelInfo == ChannelIsTuned) {
            color = ttcMagenta;
         } else if (ChannelInfo == ChannelIsCached) {
            color = ttcRed;
         };
      };
      needClearMessage = false;
      Display::DrawMessage(str, str2, color);
      UpdateFooter();
      Display::ReleaseFlush();

      return false;
   }
   return true;
}



int TeletextBrowser::PageCheckSum() {
   int retSum=0;
   StorageHandle fd;
   
   CheckFirstSubPage(currentSubPage);
   
   if ((fd=storage->openForReading(PageID(channel, currentPage, currentSubPage), false)) ) {
      uchar cache[960];
      storage->read(cache, 12, fd); //skip
      storage->read(cache, sizeof(cache), fd);
      storage->close(fd);
      memset(cache+12, 0, 8); //it seems that there the clock is transmitted, ignore changes
      for (uint i=0;i<sizeof(cache); i++)
         retSum+=cache[i];
   }
   return retSum;
}

void TeletextBrowser::UpdateClock() {
   if ( ttSetup.showClock )
      Display::DrawClock();
}

void TeletextBrowser::UpdateFooter() {
   DEBUG_OT_FOOT("called with lineMode24=%d", ttSetup.lineMode24);

   if ( ttSetup.lineMode24 ) return; // nothing to do

   char textRed[81]= "", textGreen[81] = "", textYellow[81] = "", textBlue[81] = ""; // 40x UTF-8 char + \0
   FooterFlags flag = FooterNormal; // default
   eTeletextActionValueType valueType = None;

   if (configMode == NotActive) {
      eTeletextAction AkRed    = TranslateKey(kRed);
      eTeletextAction AkGreen  = TranslateKey(kGreen);
      eTeletextAction AkYellow = TranslateKey(kYellow);
      eTeletextAction AkBlue   = TranslateKey(kBlue);
      DEBUG_OT_FOOT("AkRed=%d AkGreen=%d AkYellow=%d AkBlue=%d", AkRed, AkGreen, AkYellow, AkBlue);

#define CONVERT_ACTION_TO_TEXT(text, mode) \
      if ((int) mode < 100) { \
         snprintf(text, sizeof(text), "%s", tr(st_modesFooter[mode])); \
      } else if ((int) mode < 999) { \
         snprintf(text, sizeof(text), "-> %03d", mode); \
      } else { \
         snprintf(text, sizeof(text), "ERROR"); \
      }; \

      CONVERT_ACTION_TO_TEXT(textRed   , AkRed   );
      CONVERT_ACTION_TO_TEXT(textGreen , AkGreen );
      CONVERT_ACTION_TO_TEXT(textYellow, AkYellow);
      CONVERT_ACTION_TO_TEXT(textBlue  , AkBlue  );
   } else {
      switch (configMode) {
         case Left:
         case Top:
         case Width:
         case Height:
         case Frame:
         case Voffset:
         case BackTrans:
            snprintf(textRed   , sizeof(textRed)   , "%s-", tr(config_modes[configMode])); // <mode>-
            snprintf(textGreen , sizeof(textGreen) , "%s+", tr(config_modes[configMode])); // <mode>+
            flag = FooterYellowValue;
            break;

         case Font:
            snprintf(textRed   , sizeof(textRed)   , "%s" , tr(config_modes[configMode])); // <mode>
            DEBUG_OT_FOOT("ttSetup.txtFontIndex=%d ttSetup.txtFontNames[%d]='%s'", ttSetup.txtFontIndex, ttSetup.txtFontIndex, ttSetup.txtFontNames[ttSetup.txtFontIndex]);
            snprintf(textGreen, sizeof(textGreen)  , "%s", ttSetup.txtFontNames[ttSetup.txtFontIndex]); // FontName
            flag = FooterGreenYellowValue;
            break;

         default:
            break;
      };

      int valueInt = 0;
      char *valueStr = NULL;
      switch (configMode) {
         case Left:
            valueInt = ttSetup.OSDleftPct;
            valueType = Pct;
            break;

         case Top:
            valueInt = ttSetup.OSDtopPct;
            valueType = Pct;
            break;

         case Width:
            valueInt = ttSetup.OSDwidthPct;
            valueType = Pct;
            break;

         case Height:
            valueInt = ttSetup.OSDheightPct;
            valueType = Pct;
            break;

         case Frame:
            valueInt = ttSetup.OSDframePix;
            valueType = Pix;
            break;

         case Voffset:
            valueInt = ttSetup.txtVoffset;
            valueType = Pix;
            break;

         case BackTrans:
            valueInt = ((uint32_t) ttSetup.configuredClrBackground) >> 24;
            valueType = Int;
            break;

         default:
            break;
      };

      switch(valueType) {
         case Pct:
            snprintf(textYellow, sizeof(textYellow), "%d %%", valueInt);
            break;

         case Pix:
            snprintf(textYellow, sizeof(textYellow), "%d Px", valueInt);
            break;

         case Int:
            snprintf(textYellow, sizeof(textYellow), "%d", valueInt);
            break;

         case Str:
            if (valueStr != NULL)
               snprintf(textYellow, sizeof(textYellow), "%s", valueStr);
            else
               snprintf(textYellow, sizeof(textYellow), "%s", "ERROR-STR"); // should not happen
            break;

         case None:
            // handled above directly
            break;

         default:
            snprintf(textYellow, sizeof(textYellow), "%s", "ERROR"); // should not happen
            break;
      };

      snprintf(textBlue  , sizeof(textBlue)  , "%s", tr(st_modesFooter[Config])); // option itself
   };

   DEBUG_OT_FOOT("textRed='%s' textGreen='%s' text Yellow='%s' textBlue='%s' flag=%d", textRed, textGreen, textYellow, textBlue, flag);
   Display::DrawFooter(textRed, textGreen, textYellow, textBlue, flag);
}

TeletextSetup ttSetup;

TeletextSetup::TeletextSetup()
   //Set default values for setup options
  : configuredClrBackground(clrGray50), showClock(true),
    suspendReceiving(false), autoUpdatePage(true),
    //OSDHeight+width default values given in Start()
    OSDheightPct(100), OSDwidthPct(100),
    OSDtopPct(0), OSDleftPct(0),
    //use the value set for VDR's min user inactivity.
    //Initially this value could be changed via the plugin's setup, but I removed that
    //because there is no advantage, but a possible problem when VDR's value is change
    //after the plugin has stored its own value.
    inactivityTimeout(Setup.MinUserInactivity),
    HideMainMenu(false),
    txtFontName("teletext2:Medium"),
    txtVoffset(0),
    colorMode4bpp(false),
    lineMode24(false)
{
   //init key bindings
   for (int i=0; i < LastActionKey; i++)
      mapKeyToAction[i]=(eTeletextAction)0;
   mapKeyToAction[ActionKeyRed]=DarkScreen;
   mapKeyToAction[ActionKeyGreen]=(eTeletextAction)100;
   mapKeyToAction[ActionKeyYellow]=HalfPage;
   mapKeyToAction[ActionKeyBlue]=Zoom;
   mapKeyToAction[ActionKeyStop]=Config;
   mapKeyToAction[ActionKeyFastRew]=LineMode24;
   mapKeyToAction[ActionKeyFastFwd]=ToggleConceal;
   mapKeyToAction[ActionKeyOk]=TogglePause;
}

// vim: ts=3 sw=3 et
