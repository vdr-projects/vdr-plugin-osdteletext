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

#include <dirent.h>

#include "txtrecv.h"
#include "tables.h"
#include "setup.h"
#include "menu.h"
#include "logging.h"

#include <vdr/channels.h>
#include <vdr/device.h>
#include <vdr/config.h>

#include <pthread.h> 
#include <signal.h> 
#include <errno.h>
#include <sys/vfs.h> 
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

cTelePage::cTelePage(PageID t_page, uchar t_flags, uchar t_lang,int t_mag, Storage *s)
  : mag(t_mag), flags(t_flags), lang(t_lang), page(t_page), storage(s)
{
   memset(pagedata.pagebuf    ,' ',25 * 40);  // clear X/0-24 buffer with spaces
   memset(pagedata.pagebuf_X25, 0,  1 * 40);  // clear X25 buffer
   memset(pagedata.pagebuf_X26, 0, 16 * 40);  // clear X26 buffer
   memset(pagedata.pagebuf_X27, 0, 16 * 40);  // clear X27 buffer
   memset(pagedata.pagebuf_X28, 0, 16 * 40);  // clear X28 buffer
   memset(pagedata.pagebuf_M29, 0, 16 * 40);  // clear M29 buffer
}

cTelePage::~cTelePage() {
}


void cTelePage::SetLine(const int line, uchar *myptr, const char *debugPrefix)
{
   unsigned char *buf = NULL;
   int dc = 0;

   if (strlen(debugPrefix) > 0) {
      printf("DEBUG: %s#%02d <:", debugPrefix, line);
      for (int i = 0; i < 40; i++) {
         printf(" %02x", myptr[i]);
      };
      printf("\n");
   };

   switch(line) {
      case 0:
         // VTX header
         buf = pagedata.pagebuf;
         memcpy(buf, myptr, 8); // copy first 8 bytes raw
         // copy 32 VTX
         for (int i = 8; i < 40; i++) {
            buf[i] = myptr[i] & 0x7f; // clear of parity bit
         };
         break;

      case 1 ... 24:
         // standard VTX, clear parity bit
         buf = pagedata.pagebuf + 40 * line;
         for (int i = 0; i < 40; i++) {
            buf[i] = myptr[i] & 0x7f; // clear of parity bit
         };
         break;

      case 25:
         buf = pagedata.pagebuf_X25;
         for (int i = 0; i < 40; i++) {
            buf[i] = myptr[i] & 0x7f; // clear of parity bit
         };
         break;

      case 26:
         buf = pagedata.pagebuf_X26;
         // further handling below
         break;

      case 27:
         buf = pagedata.pagebuf_X27;
         // further handling below
         break;

      case 28:
         buf = pagedata.pagebuf_X28;
         // further handling below
         break;

      case 29:
         buf = pagedata.pagebuf_M29;
         // further handling below
         break;

      default:
         // esyslog("osdteletext: cTelePage::SetLine called with unsupported line=%d (code issue)\n", line);
         break;
   };

   switch(line) {
      case 26 ... 29:
         // line 26-29 contain DesignationCode
         dc = unhamtab[myptr[0]] & 0x0f;
         buf += dc * 40; // shift buffer start to DesignationCode row
         buf[0] = dc | 0x80; // store unhammed DesignationCode | 0x80 (shows "used")
         break;
   };

   switch(line) {
      case 26:
      case 28:
      case 29:
         // unhamming 24/18 triplet
         for (int triplet = 0; triplet < 13; triplet++) {
            switch(line) {
               case 26:
                  // ETSI 8.3 unhamming 24/18
                  //  1   2   3   4   5   6   7   8 |  9  10  11  12  13  14  15  16 |  17  18  19  20  21  22  23  24
                  // P1  P2  D1  P3  D2  D3  D4  P4 | D5  D6  D7  D8  D9 D10 D11  P5 | D12 D13 D14 D15 D16 D17 D18  P6
                  // ETSI 12.3.1 X/26 address / mode / data
                  //         A0      A1  A2  A3     | A4  A5  M0  M1  M2  M3  M4     |  D0  D1  D2  D3  D4  D5  D6
                  buf[triplet*3 + 1] = ((myptr[triplet*3 + 1] & 0x04) >> 2)  // A0   mask 3           and shift to 1
                                     | ((myptr[triplet*3 + 1] & 0x70) >> 3)  // A1-3 mask 5-7         and shift to 2-4
                                     | ((myptr[triplet*3 + 2] & 0x03) << 4); // A4-5 mask 1-2 (9-10)  and shift to 5-6
                  buf[triplet*3 + 2] = ((myptr[triplet*3 + 2] & 0x7c) >> 2); // M0-4 mask 3-7 (11-15) and shift to 1-5
                  buf[triplet*3 + 3] = ((myptr[triplet*3 + 3] & 0x7f)     ); // D0-6 mask 1-7         and nothing to shift
#if 0
                  if (strlen(debugPrefix) > 0)
                     printf("DEBUG: %s#%02d T: t=%d i1=%02x i2=%02x i3=%02x -> o1=%02x o2=%02x o3=%02x\n", debugPrefix, line, triplet
                           , myptr[1 + triplet*3], myptr[2 + triplet*3], myptr[3 + triplet*3]
                           , buf[triplet*3 + 1], buf[triplet*3 + 2], buf[triplet*3 + 3]
                     );
#endif
                  break;

               case 28:
                  // TODO implement
                  break;
               case 29:
                  // TODO implement
                  break;
            };
         };
         break;

      case 27:
         // line 27 contain only a 16-bit CRC at the end
         memcpy(buf + 40 * dc + 1, myptr + 1, 39); // store byte 2-40
         break;

      default:
         // no storage
         break;
   };

   if (strlen(debugPrefix) > 0) {
      if (buf != NULL) {
         printf("DEBUG: %s#%02d >:", debugPrefix, line);
         for (int i = 0; i < 40; i++) {
            printf(" %02x", buf[i]);
         };
         printf("\n");
      } else {
         printf("DEBUG: %s#%02d >: NOT-SELECTED-TO-STORE\n", debugPrefix, line);
      };
   };
}

void cTelePage::save()
{
   unsigned char buf;
   StorageHandle fd;
   if ( (fd=storage->openForWriting(page)) ) {
      // page header (12)
      memcpy(pagedata.pageheader, "VTXV5", 5);      // prefix (5)  "VTXV4" < 2.0.0
      buf=0x01;      pagedata.pageheader[5]  = buf; // fixed 0x01 (1)
      buf=mag;       pagedata.pageheader[6]  = buf; // mag (1)
      buf=page.page; pagedata.pageheader[7]  = buf; // page (1)
      buf=flags;     pagedata.pageheader[8]  = buf; // flags (1)
      buf=lang;      pagedata.pageheader[9]  = buf; // lang (1)
      buf=0x00;      pagedata.pageheader[10] = buf; // fixed 0x00 (1)
      buf=0x00;      pagedata.pageheader[11] = buf; // fixed 0x00 (1)
      storage->write(&pagedata, sizeof(TelePageData), fd);
      storage->close(fd);
   }
}

bool cTelePage::IsTopTextPage()
{
   return (page.page & 0xFF) > 0x99 || (page.page & 0x0F) > 0x9;
}

cTxtStatus::cTxtStatus(bool storeTopText, Storage* storage)
   : receiver(NULL), storeTopText(storeTopText), storage(storage)
     , NonLiveChannelNumber(0)
{
}

cTxtStatus::~cTxtStatus()
{
    delete receiver;
}

void cTxtStatus::ChannelSwitch(const cDevice *Device, int ChannelNumber, bool LiveView)
{
   // ignore if channel is 0
   if (ChannelNumber == 0) {
      DEBUG_OT_TXTRCVC("IGNORE channel=0 switch on DVB %d for channel %d LiveView=%s\n", Device->DeviceNumber(), ChannelNumber, BOOLTOTEXT(LiveView));
      return;
   };

   // ignore if channel is invalid (highly unlikely, this will ever
   // be the case, but defensive coding rules!)
#if defined(APIVERSNUM) && APIVERSNUM >= 20301
   LOCK_CHANNELS_READ;
   const cChannel* newChannel = Channels->GetByNumber(ChannelNumber);
#else
   const cChannel* newChannel = Channels.GetByNumber(ChannelNumber);
#endif
   if (newChannel == NULL) {
      DEBUG_OT_TXTRCVC("IGNORE invalid channel on DVB %d for channel %d LiveView=%s\n", Device->DeviceNumber(), ChannelNumber, BOOLTOTEXT(LiveView));
      return;
   };

   if (!LiveView) {
      if ((NonLiveChannelNumber > 0) && (NonLiveChannelNumber == ChannelNumber)) {
         // don't ignore non-live-channel-switching in case of NonLiveChannelNumber was hit
         DEBUG_OT_TXTRCVC("PASSED selected NON-LIVE channel switch detected on DVB %d for channel %d '%s'\n", Device->DeviceNumber(), newChannel->Number(), newChannel->Name());
      } else if (
            (NonLiveChannelNumber > 0) // currently on tuned channel
         && (NonLiveChannelNumber != ChannelNumber) // channel is not matching
         && (receiver->device->DeviceNumber() == Device->DeviceNumber()) // device matching
      ) {
         // don't ignore non-live-channel-switching in case of Device was hit
         DEBUG_OT_TXTRCVC("STOPRC not matching NON-LIVE channel switch detected on DVB %d for channel %d '%s'\n", Device->DeviceNumber(), newChannel->Number(), newChannel->Name());
         DELETENULL(receiver);
         return;
      } else {
         // ignore other non-live-channel-switching
         DEBUG_OT_TXTRCVC("IGNORE not matching NON-LIVE channel switch on DVB %d for channel %d '%s'\n", Device->DeviceNumber(), newChannel->Number(), newChannel->Name());
         return;
      };
   } else {
      // ignore non-live-channel-switching
      if (ChannelNumber != cDevice::CurrentChannel()) {
         DEBUG_OT_TXTRCVC("IGNORE not current device LIVE channel switch on DVB %d for channel %d '%s'\n", Device->DeviceNumber(), newChannel->Number(), newChannel->Name());
         return;
      };

      // process live channel switch
      DEBUG_OT_TXTRCVC("PASSED LIVE channel switch detected on DVB %d for channel %d '%s'\n", Device->DeviceNumber(), newChannel->Number(), newChannel->Name());
   };

   // now re-attach the receiver to the new channel
   int TPid = newChannel->Tpid();

   if (LiveView && TPid && receiver) {
      // tell still running receiver thread that it will be deleted and new channel is live
      // will be used during deleting the receiver to signal that status via TeletextBrowser::ChannelSwitched
      receiver->SetFlagStopByLiveChannelSwitch(true);
   };

   // channel was changed, delete the running receiver
   DELETENULL(receiver);

   if (TPid) {
      if (LiveView) {
         // attach to actual device
         receiver = new cTxtReceiver(cDevice::ActualDevice(), LiveView, newChannel, storeTopText, storage);
         cDevice::ActualDevice()->AttachReceiver(receiver);
         DEBUG_OT_TXTRCVC("ATTACH receiver to DVB %d for LIVE channel %d '%s'\n", cDevice::ActualDevice()->DeviceNumber(), newChannel->Number(), newChannel->Name());
         TeletextBrowser::ChannelSwitched(ChannelNumber, ChannelIsLive);
         NonLiveChannelNumber = 0; // clear non-live channel number
      } else {
         cDevice *device = cDevice::GetDevice(Device->DeviceNumber());
         receiver = new cTxtReceiver(device, LiveView, newChannel, storeTopText, storage);
         device->AttachReceiver(receiver);
         DEBUG_OT_TXTRCVC("ATTACH receiver to DVB %d for TUNED channel %d '%s'\n", Device->DeviceNumber(), newChannel->Number(), newChannel->Name());
         TeletextBrowser::ChannelSwitched(ChannelNumber, ChannelIsTuned);
      };
   } else {
      DEBUG_OT_TXTRCVC("NOOP  do not attach receiver (MISSING teletext) on DVB %d for channel %d '%s'\n", Device->DeviceNumber(), newChannel->Number(), newChannel->Name());
      TeletextBrowser::ChannelSwitched(ChannelNumber, ChannelHasNoTeletext);
   }
}


cTxtReceiver::cTxtReceiver(const cDevice *dev, const bool live, const cChannel* chan, bool storeTopText, Storage* storage)
 : cReceiver(chan, -1), cThread("osdteletext-receiver", true),
   TxtPage(0), storeTopText(storeTopText), buffer((188+60)*75), storage(storage)
   , device(dev)
   , live(live)
   , flagStopByLiveChannelSwitch(false)
   , channel(chan), statTxtReceiverPageCount(0)
{
   isyslog("osdteletext: cTxtReceiver started on DVB %d for channel %d '%s' ID=%s storeTopText=%s LiveView=%s\n", device->DeviceNumber(), channel->Number(), channel->Name(), *ChannelID().ToString(), BOOLTOTEXT(storeTopText), BOOLTOTEXT(live));
   SetPids(NULL);
   AddPid(chan->Tpid());
   storage->prepareDirectory(ChannelID());
   time(&statTxtReceiverTimeStart); // record start time

   // 10 ms timeout on getting TS frames
   buffer.SetTimeouts(0, 10);
}


cTxtReceiver::~cTxtReceiver()
{
   Detach();
   Activate(false);
   buffer.Clear();
   delete TxtPage;

   // calculate and log statistics
   time_t statTxtReceiverTimeStop;
   time(&statTxtReceiverTimeStop);
   double time_diff = difftime(statTxtReceiverTimeStop, statTxtReceiverTimeStart);
   isyslog("osdteletext: cTxtReceiver stopped after %.0lf sec: cTelePage received on DVB %d for channel %d '%s' ID=%s: %ld (%.3lf/sec)\n", time_diff, device->DeviceNumber(), channel->Number(), channel->Name(), *ChannelID().ToString(), statTxtReceiverPageCount, statTxtReceiverPageCount / time_diff);

   if (!live) {
      // tuned channel
      if (flagStopByLiveChannelSwitch == false) {
         TeletextBrowser::ChannelSwitched(channel->Number(), ChannelWasTuned); // trigger TeletextBrowser that channel is no longer tuned
      } else {
         TeletextBrowser::ChannelSwitched(channel->Number(), ChannelWasTunedNewChannelIsLive); // trigger TeletextBrowser that channel is no longer tuned but new channel is live
      };
   };
}

void cTxtReceiver::Stop()
{
   Activate(false);
}

void cTxtReceiver::Activate(bool On)
{
  if (On) {
     if (!Running()) {
        Start();
        }
     }
  else if (Running()) {
     buffer.Signal();
     Cancel(2);
     }
}

#if defined(APIVERSNUM) && APIVERSNUM >= 20301
void cTxtReceiver::Receive(const uchar *Data, int Length)
#else
void cTxtReceiver::Receive(uchar *Data, int Length)
#endif
{
   cFrame *frame=new cFrame(Data, Length);
   if (!buffer.Put(frame)) {
      // Buffer overrun
      delete frame;
      buffer.Signal();
   }
}

void cTxtReceiver::Action() {

   while (Running()) {
      cFrame *frame=buffer.Get();
      if (frame) {
         uchar *Datai=frame->Data();

         for (int i=0; i < 4; i++) {
            if (Datai[4+i*46]==2 || Datai[4+i*46]==3) {
               for (int j=(8+i*46);j<(50+i*46);j++)
                  Datai[j]=invtab[Datai[j]];
               DecodeTXT(&Datai[i*46]);
            }
         }

         buffer.Drop(frame);
      } else
         buffer.Wait();
   }

   buffer.Clear();
}

uchar cTxtReceiver::unham16 (uchar *p)
{
  unsigned short c1,c2;
  c1=unhamtab[p[0]];
  c2=unhamtab[p[1]];
  return (c1 & 0x0F) | (c2 & 0x0F) *16;
}

void cTxtReceiver::SaveAndDeleteTxtPage()
{
  if (TxtPage) {
     if (storeTopText || !TxtPage->IsTopTextPage()) {
        TxtPage->save();
        DELETENULL(TxtPage);
     }
  }
}

void cTxtReceiver::DecodeTXT(uchar* TXT_buf)
{
   // Format of buffer:
   //   0x00-0x04  ?
   //   0x05-0x06  Clock Run-In?
   //   0x07       Framing Code?
   //   0x08       Magazine number (100-digit of page number)
   //   0x09       Line number
   //   0x0A..0x31 Line data
   // Line 0 only:
   //   0x0A       10-digit of page number
   //   0x0B       1-digit of page number
   //   0x0C       Sub-Code bits 0..3
   //   0x0D       Sub-Code bits 4..6 + C4 flag
   //   0x0E       Sub-Code bits 8..11
   //   0x0F       Sub-Code bits 12..13 + C5,C6 flag
   //   0x10       C7-C10 flags
   //   0x11       C11-C14 flags
   //
   // Flags:
   //   C4 - Erase last page, new page transmitted
   //   C5 - News flash, boxed display
   //   C6 - Subtitle, boxed display
   //   C7 - Suppress Header, dont show line 0
   //   C8 - Update, page has changed
   //   C9 - Interrupt Sequence, page number is out of order
   //   C10 - Inhibit Display
   //   C11 - Magazine Serial mode
   //   C12-C14 - Language selection, lower 3 bits


   int hdr,mag,mag8,line;
   uchar *ptr;
   uchar flags,lang;

   hdr = unham16 (&TXT_buf[0x8]);
   mag = hdr & 0x07;
   mag8 = mag ?: 8;
   line = (hdr>>3) & 0x1f;
   ptr = &TXT_buf[10];

   static int stat_pagecount = 0;
   static long int stat_pagecount_total = 0;
   static time_t stat_time_last;
   static time_t stat_time_start;
   static int init = 0;
   if (init == 0) {
      time(&stat_time_last);
      time(&stat_time_start);
      init = 1;
   }
   time_t stat_time_now;
   double stat_time_diff_last;
   double stat_time_diff_start;

   static char debugPrefix[16];

   if (line == 0) {
      unsigned char b1, b2, b3, b4;
      int pgno, subno;
      b1 = unham16 (ptr);    
      // Page no, 10- and 1-digit

      if (b1 == 0xff) return;

      SaveAndDeleteTxtPage();
      snprintf(debugPrefix, sizeof(debugPrefix), "%s", ""); // clear debug status

      b2 = unham16 (ptr+2); // Sub-code 0..6 + C4
      b3 = unham16 (ptr+4); // Sub-code 8..13 + C5,C6
      b4 = unham16 (ptr+6); // C7..C14

      // flags:
      //   0x80  C4 - Erase page
      //   0x40  C5 - News flash
      //   0x20  C6 - Subtitle
      //   0x10  C7 - Suppress Header
      //   0x08  C8 - Update
      //   0x04  C9 - Interrupt Sequence
      //   0x02  C9 (Bug?)
      //   0x01  C11 - Magazine Serial mode
      flags=b2 & 0x80;
      flags|=(b3&0x40)|((b3>>2)&0x20); //??????
      flags|=((b4<<4)&0x10)|((b4<<2)&0x08)|(b4&0x04)|((b4>>1)&0x02)|((b4>>4)&0x01);
      lang=((b4>>5) & 0x07);

      pgno = mag8 * 256 + b1;
      subno = (b2 + b3 * 256) & 0x3f7f;         // Sub Page Number

      TxtPage = new cTelePage(PageID(ChannelID(), pgno, subno), flags, lang, mag, storage);
      DEBUG_OT_NEPG("new cTelePage pgno=%03x subno=%02x flags=0x%02x lang=0x%02x\n", pgno, subno, flags, lang);
      stat_pagecount++;
      stat_pagecount_total++;
      statTxtReceiverPageCount++;
      time(&stat_time_now);
      stat_time_diff_last = difftime(stat_time_now, stat_time_last);
      if (stat_time_diff_last >= 10) { // every 10 seconds
         stat_time_diff_start = difftime(stat_time_now, stat_time_start);
         DEBUG_OT_COPG("received on DVB %d channel %d '%s' cTelePages: %d in %.0lf sec (total: %ld in %.0lf sec -> %.3lf/sec)\n"
            , device->DeviceNumber()
            , channel->Number()
            , channel->Name()
            , stat_pagecount
            , stat_time_diff_last
            , stat_pagecount_total
            , stat_time_diff_start
            , stat_pagecount_total / stat_time_diff_start
         );
         stat_pagecount = 0;
         stat_time_last = stat_time_now;
      };

      if (m_debugmask & DEBUG_MASK_OT_TXTRCVD) {
         if (m_debugpsub == 0) {
            if (m_debugpage == TxtPage->page.page) {
               // select debug for all sub-pages
               snprintf(debugPrefix, sizeof(debugPrefix), "p=%03x*%02x", TxtPage->page.page, TxtPage->page.subPage);
            } else if (m_debugline >= 0) {
               // all pages, but specific line only
               snprintf(debugPrefix, sizeof(debugPrefix), "p=%03x!%02x", TxtPage->page.page, TxtPage->page.subPage);
            };
         } else {
            if ((m_debugpage == TxtPage->page.page) && (m_debugpsub == TxtPage->page.subPage)) {
               if ((m_debugline < 0) || (m_debugline == line)) {
                  // select debug only for matching sub-page
                  snprintf(debugPrefix, sizeof(debugPrefix), "p=%03x-%02x", TxtPage->page.page, TxtPage->page.subPage);
               };
            };
         };
      };
   };

   if (TxtPage) {
      if ((strlen(debugPrefix) > 0) && ((m_debugline < 0) || (m_debugline == line))) {
         TxtPage->SetLine(line,(uchar *)ptr, debugPrefix);
      } else {
         TxtPage->SetLine(line,(uchar *)ptr, "");
      };
   };
}

// vim: ts=3 sw=3 et
