/*************************************************************** -*- c++ -*-
 *                                                                         *
 *   display.c - Actual implementation of OSD display variants and         *
 *               Display:: namespace that encapsulates a single cDisplay.  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   Changelog:                                                            *
 *     2005-03    initial version (c) Udo Richter                          *
 *                                                                         *
 ***************************************************************************/

#include <strings.h>
#include <vdr/config.h>
#include <vdr/skins.h>
#include "setup.h"
#include "display.h"
#include "txtfont.h"

// Static variables of Display:: namespace
Display::Mode Display::mode=Display::Full;
cDisplay *Display::display=NULL;


void Display::SetMode(Display::Mode NewMode) {
    int hChars = 40;
    int vLines = (ttSetup.lineMode24 == true) ? 24 : 25;
    int OSDwidth;
    int OSDheight;
    int x0 = cOsd::OsdLeft(); // start with general OSD offset
    int y0 = cOsd::OsdTop();  // start with general OSD offset

    // (re-)set display mode.

    if (display!=NULL && NewMode==mode) return; // No change, nothing to do

    // calculate from percentage and OSD maximum
    OSDwidth = (cOsd::OsdWidth() * ttSetup.OSDwidthPct) / 100;
    OSDheight = (cOsd::OsdHeight() * ttSetup.OSDheightPct) / 100;

    // align with hChars/vLines in case of less than 100 %
    if ((ttSetup.OSDwidthPct  < 100) && ((OSDwidth  % hChars) > 0)) OSDwidth  = (OSDwidth  / hChars) * hChars;
    if ((ttSetup.OSDheightPct < 100) && ((OSDheight % vLines) > 0)) OSDheight = (OSDheight / vLines) * vLines;

    if ((ttSetup.OSDwidthPct < 100) && (ttSetup.OSDleftPct > 0)) {
        // check offset not exceeding maximum possible
        if (ttSetup.OSDwidthPct + ttSetup.OSDleftPct > 100) {
            // shift to maximum
            x0 += cOsd::OsdWidth() - OSDwidth;
        } else {
            // add configured offset
            x0 += (cOsd::OsdWidth() * ttSetup.OSDleftPct) / 100;

            // add 50% of alignment offset to center proper
            x0 += ((cOsd::OsdWidth() * ttSetup.OSDwidthPct) / 100 - OSDwidth) / 2;
        };
    };

    if ((ttSetup.OSDtopPct < 100) && (ttSetup.OSDtopPct > 0)) {
        // check offset not exceeding maximum possible
        if (ttSetup.OSDheightPct + ttSetup.OSDtopPct > 100) {
            // shift to maximum
            y0 += cOsd::OsdHeight() - OSDheight;
        } else {
            // add configured offset
            y0 += cOsd::OsdHeight() * ttSetup.OSDtopPct / 100;

            // add 50% of alignment offset to center proper
            y0 += ((cOsd::OsdHeight() * ttSetup.OSDheightPct) / 100 - OSDheight) / 2;
        };
    };

    dsyslog("OSD-Teletext: OSD area calculated by percent values: OsdLeft=%d OsdTop=%d OsdWidth=%d OsdHeight=%d OSDwidthPct=%d%% OSDheightPct=%d%% OSDleftPct=%d%% OSDtopPct=%d%% ttSetup.lineMode24=%d => x0=%d y0=%d OSDwidth=%d OSDheight=%d", cOsd::OsdLeft(), cOsd::OsdTop(), cOsd::OsdWidth(), cOsd::OsdHeight(), ttSetup.OSDwidthPct, ttSetup.OSDheightPct, ttSetup.OSDleftPct, ttSetup.OSDtopPct, ttSetup.lineMode24, x0, y0, OSDwidth, OSDheight);

    switch (NewMode) {
      case Display::Full:
        // Need to re-initialize *display:
        Delete();
        // Try 32BPP display first:
        display=new cDisplay32BPP(x0,y0,OSDwidth,OSDheight);
        break;
      case Display::HalfUpper:
        // Shortcut to switch from HalfUpper to HalfLower:
        if (mode==Display::HalfLower) {
            // keep instance.
            ((cDisplay32BPPHalf*)display)->SetZoom(cDisplay::Zoom_Upper);
            ((cDisplay32BPPHalf*)display)->SetUpper(true);
            break;
        }
        // Need to re-initialize *display:
        Delete();
        display=new cDisplay32BPPHalf(x0,y0,OSDwidth,OSDheight,true);
        ((cDisplay32BPPHalf*)display)->SetZoom(cDisplay::Zoom_Upper);
        break;
      case Display::HalfLower:
        // Shortcut to switch from HalfUpper to HalfLower:
        if (mode==Display::HalfUpper) {
            // keep instance.
            ((cDisplay32BPPHalf*)display)->SetZoom(cDisplay::Zoom_Lower);
            ((cDisplay32BPPHalf*)display)->SetUpper(false);
            break;
        }
        // Need to re-initialize *display:
        Delete();
        display=new cDisplay32BPPHalf(x0,y0,OSDwidth,OSDheight,false);
        ((cDisplay32BPPHalf*)display)->SetZoom(cDisplay::Zoom_Lower);
        break;
    }
    mode=NewMode;
    // If display is invalid, clean up immediately:
    if (!display->Valid()) Delete();
    // Pass through OSD black transparency
    SetBackgroundColor((tColor)ttSetup.configuredClrBackground);
}

void Display::ShowUpperHalf() {
    // Enforce upper half of screen to be visible
    if (GetZoom()==cDisplay::Zoom_Lower)
        SetZoom(cDisplay::Zoom_Upper);
    if (mode==HalfLower)
        SetMode(HalfUpper);
}


cDisplay32BPP::cDisplay32BPP(int x0, int y0, int width, int height)
    : cDisplay(width,height) {
    // 32BPP display for True Color OSD providers

    osd = cOsdProvider::NewOsd(x0, y0);
    if (!osd) return;

    width=(width+1)&~1;
    // Width has to end on byte boundary, so round up

    int bpp = 32;
    if (ttSetup.colorMode4bpp == true) {
        bpp = 4;
        dsyslog("OSD-Teletext: OSD config forced to bpp=%d", bpp);
    };
    tArea Areas[] = { { 0, 0, width - 1, height - 1, bpp } };
    if (bpp == 32 && (osd->CanHandleAreas(Areas, sizeof(Areas) / sizeof(tArea)) != oeOk)) {
        bpp = 8;
        Areas[0].bpp = 8;
        dsyslog("OSD-Teletext: OSD is not providing TrueColor, fallback to bpp=%d", bpp);
    }
    if (osd->CanHandleAreas(Areas, sizeof(Areas) / sizeof(tArea)) != oeOk) {
        DELETENULL(osd);
        esyslog("OSD-Teletext: can't create requested OSD area with x0=%d y0=%d width=%d height=%d bpp=%d", x0, y0, width, height, bpp);
        Skins.Message(mtError, "OSD-Teletext can't request OSD area, check plugin settings");
        return;
    }
    osd->SetAreas(Areas, sizeof(Areas) / sizeof(tArea));

    setOutputWidth(width);
    setOutputHeight(Height);

    isyslog("OSD-Teletext: OSD area successful requested with x0=%d y0=%d width=%d height=%d bpp=%d", x0, y0, width, height, bpp);

    InitScaler();

    // CleanDisplay(); // called later after SetBackgroundColor
    Dirty=true;
    DirtyAll=true;
}


cDisplay32BPPHalf::cDisplay32BPPHalf(int x0, int y0, int width, int height, bool upper)
    : cDisplay(width,height), Upper(upper), OsdX0(x0), OsdY0(y0)
{
    osd=NULL;

    // Redirect all real init work to method
    InitOSD();
}

void cDisplay32BPPHalf::InitOSD() {
    delete osd;
    int x0 = OsdX0;
    int y0 = OsdY0 + Height / 2;
    osd = cOsdProvider::NewOsd(x0, y0);
    if (!osd) return;

    int width=(Width+1)&~1; // Width has to end on byte boundary, so round up
    int height=Height / 2;

    int bpp = 32;
    if (ttSetup.colorMode4bpp == true) {
        bpp = 4;
        dsyslog("OSD-Teletext: OSD config forced to bpp=%d", bpp);
    };
    tArea Areas[] = { { 0, 0, width - 1, height - 1, bpp } };
    if (bpp == 32 && (osd->CanHandleAreas(Areas, sizeof(Areas) / sizeof(tArea)) != oeOk)) {
        bpp = 8;
        Areas[0].bpp = 8;
        dsyslog("OSD-Teletext: OSD is not providing TrueColor, fallback to bpp=%d", bpp);
    }
    if (osd->CanHandleAreas(Areas, sizeof(Areas) / sizeof(tArea)) != oeOk) {
        DELETENULL(osd);
        esyslog("OSD-Teletext: can't create requested OSD 'half' area with x0=%d y0=%d width=%d height=%d bpp=%d", x0, y0, width, height, bpp);
        Skins.Message(mtError, "OSD-Teletext can't request OSD 'half' area, check plugin settings");
        return;
    }
/*
    // Try full-size area first
    while (osd->CanHandleAreas(Areas, sizeof(Areas) / sizeof(tArea)) != oeOk) {
        // Out of memory, so shrink
        if (Upper) {
            // Move up lower border
            Areas[0].y2=Areas[0].y2-1;
        } else {
            // Move down upper border
            Areas[0].y1=Areas[0].y1+1;
        }
        if (Areas[0].y1>Areas[0].y2) {
            // Area is empty, fail miserably
            DELETENULL(osd);
            return;
        }
    }
    // Add some backup
    // CanHandleAreas is not accurate enough
    if (Upper) {
        Areas[0].y2=Areas[0].y2-10;
    } else {
        Areas[0].y1=Areas[0].y1+10;
    }
*/
    osd->SetAreas(Areas, sizeof(Areas) / sizeof(tArea));

    isyslog("OSD-Teletext: OSD 'half' area successful requested x0=%d y0=%d width=%d height=%d bpp=%d upper=%d", x0, y0, width, height, bpp, Upper);

    setOutputWidth(width);
    setOutputHeight(height);

    InitScaler();

    // CleanDisplay(); // called later after SetBackgroundColor

    // In case we switched on the fly, do a full redraw
    Dirty=true;
    DirtyAll=true;
    Flush();
}

// vim: ts=4 sw=4 et
