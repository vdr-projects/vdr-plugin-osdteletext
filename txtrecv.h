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

#ifndef __TXTRECV_H
#define __TXTRECV_H

#include <vdr/status.h>
#include <vdr/receiver.h>
#include <vdr/thread.h>
#include <vdr/ringbuffer.h>

#include <stdio.h>
#include <unistd.h>

#include "storage.h"

class cTelePage {
 private:
  int mag;
  unsigned char flags;
  unsigned char lang;
 public:
  PageID page;
 private:
  TelePageData pagedata;
  Storage* storage;
 public:
  cTelePage(PageID page, uchar flags, uchar lang, int mag, Storage *s);
  ~cTelePage();
  void SetLine(int, uchar*, const char*);
  void save();
  bool IsTopTextPage();
 };

class cRingTxtFrames : public cRingBufferFrame {
 public:
  cRingTxtFrames(int Size) : cRingBufferFrame(Size, true) {};
  ~cRingTxtFrames() { Clear(); };
  void Wait(void) { WaitForGet(); };
  void Signal(void) { EnableGet(); };
};

class cTxtReceiver : public cReceiver, public cThread {
private:
   void DecodeTXT(uchar*);
   uchar unham16 (uchar*);
   cTelePage *TxtPage;
   void SaveAndDeleteTxtPage();
   bool storeTopText;
   cRingTxtFrames buffer;
   Storage *storage;
public:
   const cDevice *device;
private:
   const bool live;
   bool flagStopByLiveChannelSwitch;
   const cChannel* channel;
   long int statTxtReceiverPageCount;
   time_t statTxtReceiverTimeStart;
protected:
   virtual void Activate(bool On);
#if defined(APIVERSNUM) && APIVERSNUM >= 20301
   virtual void Receive(const uchar *Data, int Length);
#else
   virtual void Receive(uchar *Data, int Length);
#endif
   virtual void Action();
public:
   cTxtReceiver(const cDevice *dev, const bool live, const cChannel* chan, bool storeTopText, Storage* storage);
   virtual ~cTxtReceiver();
   virtual void Stop();
   void SetFlagStopByLiveChannelSwitch(bool flag) { flagStopByLiveChannelSwitch = flag; };
   bool Live() { return live; };
   bool ChannelNumber() { return channel->Number(); };
};

class cTxtStatus : public cStatus {
public:
   cTxtReceiver *receiver;
private:
   bool storeTopText;
   Storage* storage;
   int NonLiveChannelNumber;
protected:
   virtual void ChannelSwitch(const cDevice *Device, int ChannelNumber, bool LiveView);
public:
   cTxtStatus(bool storeTopText, Storage* storage);
   void SetNonLiveChannelNumber(const int ChannelNumber) { NonLiveChannelNumber = ChannelNumber; return; };
   ~cTxtStatus();
};


#endif
