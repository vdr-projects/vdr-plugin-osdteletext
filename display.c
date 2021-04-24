/*************************************************************** -*- c++ -*-
 *       Copyright (c) 2005      by Udo Richter                            *
 *       Copyright (c) 2021      by Peter Bieringer (extenions)            *
 *                                                                         *
 *   display.c - Actual implementation of OSD display variants and         *
 *               Display:: namespace that encapsulates a single cDisplay.  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <strings.h>
#include <vdr/config.h>
#include <vdr/skins.h>
#include "setup.h"
#include "display.h"
#include "txtfont.h"
#include "logging.h"

// Static variables of Display:: namespace
Display::Mode Display::mode=Display::Full;
cDisplay *Display::display=NULL;


void Display::SetMode(Display::Mode NewMode, tColor clrBackground) {
    // Background is optional and default predefined in display.h
    int hChars = 40;
    int vLines = (ttSetup.lineMode24 == true) ? 24 : 25;
    int OSDwidth;
    int OSDheight;
    int OSDleftFrame  = 0;
    int OSDrightFrame = 0;
    int OSDtopFrame  = 0;
    int OSDbottomFrame = 0;
    int x0 = cOsd::OsdLeft(); // start with general OSD offset
    int y0 = cOsd::OsdTop();  // start with general OSD offset

    // (re-)set display mode.

    if (display!=NULL && NewMode==mode) return; // No change, nothing to do

    // calculate from percentage and OSD maximum
    OSDwidth = (cOsd::OsdWidth()   * TTSETUPPRESET(Width) ) / 100;
    OSDheight = (cOsd::OsdHeight() * TTSETUPPRESET(Height)) / 100;

    // align with hChars/vLines
    if ((OSDwidth  % hChars) > 0) OSDwidth  = (OSDwidth  / hChars) * hChars;
    if ((OSDheight % vLines) > 0) OSDheight = (OSDheight / vLines) * vLines;

    if (TTSETUPPRESET(Width) < 100) {
        if (TTSETUPPRESET(Left) > 0) {
            // check offset not exceeding maximum possible
            if ((TTSETUPPRESET(Width) + TTSETUPPRESET(Left)) > 100) {
                // shift to maximum
                x0 += cOsd::OsdWidth() - OSDwidth;
            } else {
                // add configured offset
                x0 += (cOsd::OsdWidth() * TTSETUPPRESET(Left)) / 100;

                // add 50% of alignment offset to center proper
                x0 += ((cOsd::OsdWidth() * TTSETUPPRESET(Width)) / 100 - OSDwidth) / 2;
            };
        };

        if (TTSETUPPRESET(Frame) > 0) {
            OSDleftFrame  = TTSETUPPRESET(Frame);
            OSDrightFrame = TTSETUPPRESET(Frame);

            x0 -= OSDleftFrame;
            if (x0 < 0) {
                OSDleftFrame += x0;
                x0 = 0;
            };
            if (OSDleftFrame < 0) OSDleftFrame = 0;

            if (x0 + OSDwidth + OSDrightFrame + OSDleftFrame > cOsd::OsdWidth()) {
                // limit right frame instead drawing out-of-area
                OSDrightFrame = cOsd::OsdWidth() - OSDwidth - x0 - OSDleftFrame;
                if (OSDrightFrame < 0) OSDrightFrame = 0;
            };
        };
    } else {
        // horizontal center with black frame left/right
        OSDleftFrame = (cOsd::OsdWidth() - OSDwidth) / 2;
        OSDrightFrame = cOsd::OsdWidth() - OSDwidth - OSDleftFrame;
    };

    if (TTSETUPPRESET(Height) < 100) {
        if (TTSETUPPRESET(Top) > 0) {
            // check offset not exceeding maximum possible
            if ((TTSETUPPRESET(Height) + TTSETUPPRESET(Top)) > 100) {
                // shift to maximum
                y0 += cOsd::OsdHeight() - OSDheight;
            } else {
                // add configured offset
                y0 += (cOsd::OsdHeight() * TTSETUPPRESET(Top)) / 100;

                // add 50% of alignment offset to center proper
                y0 += ((cOsd::OsdHeight() * TTSETUPPRESET(Height)) / 100 - OSDheight) / 2;
            };
        };

        if (TTSETUPPRESET(Frame) > 0) {
            OSDtopFrame    = TTSETUPPRESET(Frame);
            OSDbottomFrame = TTSETUPPRESET(Frame);

            y0 -= OSDtopFrame;
            if (y0 < 0) {
                OSDtopFrame += y0;
                y0 = 0;
            };
            if (OSDtopFrame < 0) OSDtopFrame = 0;

            if (y0 + OSDheight + OSDtopFrame + OSDbottomFrame > cOsd::OsdHeight()) {
                // limit bottom frame instead drawing out-of-area
                OSDbottomFrame = cOsd::OsdHeight() - OSDheight - y0 - OSDtopFrame;
                if (OSDbottomFrame < 0) OSDbottomFrame = 0;
            };
        };
    } else {
        // vertical center with black frame top/bottom
        OSDtopFrame = (cOsd::OsdHeight() - OSDheight) / 2;
        OSDbottomFrame = cOsd::OsdHeight() - OSDheight - OSDtopFrame;
    };

    DEBUG_OT_AREA("osdteletext: OSD area calculated by percent values: OL=%d OT=%d OW=%d OH=%d OwP=%d%% OhP=%d%% OlP=%d%% OtP=%d%% OfPx=%d lineMode24=%d => x0=%d y0=%d Ow=%d Oh=%d OlF=%d OrF=%d OtF=%d ObF=%d"
        , cOsd::OsdLeft(), cOsd::OsdTop(), cOsd::OsdWidth(), cOsd::OsdHeight()
        , TTSETUPPRESET(Width), TTSETUPPRESET(Height), TTSETUPPRESET(Left), TTSETUPPRESET(Top)
        , TTSETUPPRESET(Frame)
        , ttSetup.lineMode24
        , x0, y0
        , OSDwidth, OSDheight
        , OSDleftFrame, OSDrightFrame
        , OSDtopFrame, OSDbottomFrame
    );

    switch (NewMode) {
      case Display::Full:
        // Need to re-initialize *display:
        DEBUG_OT_DBFC("osdteletext: OSD 'full' Full->reinit display");
        Delete();
        // Try 32BPP display first:
        display=new cDisplay32BPP(x0,y0,OSDwidth,OSDheight,OSDleftFrame,OSDrightFrame,OSDtopFrame,OSDbottomFrame,clrBackground);
        break;
      case Display::HalfUpper:
        // Shortcut to switch from HalfUpper to HalfLower:
        if (mode==Display::HalfLower) {
            DEBUG_OT_DBFC("osdteletext: OSD 'half' shortcut HalfUpper->HalfLower");
            // keep instance.
            ((cDisplay32BPPHalf*)display)->SetZoom(cDisplay::Zoom_Upper);
            ((cDisplay32BPPHalf*)display)->SetUpper(true);
            ((cDisplay32BPPHalf*)display)->SetTop(false);
            break;
        }
        // Need to re-initialize *display:
        DEBUG_OT_DBFC("osdteletext: OSD 'half' HalfUpper->reinit display");
        Delete();
        display=new cDisplay32BPPHalf(x0,y0,OSDwidth,OSDheight,OSDleftFrame,OSDrightFrame,OSDtopFrame,OSDbottomFrame,true,false,clrBackground);
        ((cDisplay32BPPHalf*)display)->SetZoom(cDisplay::Zoom_Upper);
        break;
      case Display::HalfUpperTop:
        // Shortcut to switch from HalfUpperTop to HalfLowerTop:
        if (mode==Display::HalfLowerTop) {
            // keep instance.
            ((cDisplay32BPPHalf*)display)->SetZoom(cDisplay::Zoom_Upper);
            ((cDisplay32BPPHalf*)display)->SetUpper(true);
            ((cDisplay32BPPHalf*)display)->SetTop(true);
            break;
        }
        // Need to re-initialize *display:
        DEBUG_OT_DBFC("osdteletext: OSD 'half' HalfUpperTop->reinit display");
        Delete();
        display=new cDisplay32BPPHalf(x0,y0,OSDwidth,OSDheight,OSDleftFrame,OSDrightFrame,OSDtopFrame,OSDbottomFrame,true,true,clrBackground);
        ((cDisplay32BPPHalf*)display)->SetZoom(cDisplay::Zoom_Upper);
        break;
      case Display::HalfLower:
        // Shortcut to switch from HalfUpper to HalfLower:
        if (mode==Display::HalfUpper) {
            DEBUG_OT_DBFC("osdteletext: OSD 'half' shortcut HalfLower->HalfUpper");
            // keep instance.
            ((cDisplay32BPPHalf*)display)->SetZoom(cDisplay::Zoom_Lower);
            ((cDisplay32BPPHalf*)display)->SetUpper(false);
            ((cDisplay32BPPHalf*)display)->SetTop(false);
            break;
        }
        // Need to re-initialize *display:
        DEBUG_OT_DBFC("osdteletext: OSD 'half' HalfLower->reinit display");
        Delete();
        display=new cDisplay32BPPHalf(x0,y0,OSDwidth,OSDheight,OSDleftFrame,OSDrightFrame,OSDtopFrame,OSDbottomFrame,false,false,clrBackground);
        ((cDisplay32BPPHalf*)display)->SetZoom(cDisplay::Zoom_Lower);
        break;
      case Display::HalfLowerTop:
        // Shortcut to switch from HalfUpperTop to HalfLowerTop:
        if (mode==Display::HalfUpperTop) {
            DEBUG_OT_DBFC("osdteletext: OSD 'half' shortcut HalfUpperTop->HalfLowerTop");
            // keep instance.
            ((cDisplay32BPPHalf*)display)->SetZoom(cDisplay::Zoom_Lower);
            ((cDisplay32BPPHalf*)display)->SetUpper(false);
            ((cDisplay32BPPHalf*)display)->SetTop(true);
            break;
        }
        // Need to re-initialize *display:
        DEBUG_OT_DBFC("osdteletext: OSD 'half' HalfLowerTop->reinit display");
        Delete();
        display=new cDisplay32BPPHalf(x0,y0,OSDwidth,OSDheight,OSDleftFrame,OSDrightFrame,OSDtopFrame,OSDbottomFrame,false,true,clrBackground);
        ((cDisplay32BPPHalf*)display)->SetZoom(cDisplay::Zoom_Lower);
        break;
    }
    mode=NewMode;
    // If display is invalid, clean up immediately:
    if (!display->Valid()) Delete();
}

void Display::ShowUpperHalf() {
    // Enforce upper half of screen to be visible
    if (GetZoom()==cDisplay::Zoom_Lower)
        SetZoom(cDisplay::Zoom_Upper);
    if (mode==HalfLower)
        SetMode(HalfUpper);
}


cDisplay32BPP::cDisplay32BPP(int x0, int y0, int width, int height, int leftFrame, int rightFrame, int topFrame, int bottomFrame, tColor clrBackground)
    : cDisplay(width,height) {
    // 32BPP display for True Color OSD providers

    osd = cOsdProvider::NewOsd(x0, y0);
    if (!osd) return;

    Background = clrBackground;

    width=(width+1)&~1;
    // Width has to end on byte boundary, so round up

    int bpp = 32;
    if (ttSetup.colorMode4bpp == true) {
        bpp = 4;
        dsyslog("osdteletext: OSD config forced to bpp=%d", bpp);
    };
    tArea Areas[] = { { 0, 0, width - 1 + leftFrame + rightFrame, height - 1 + topFrame + bottomFrame, bpp } };
    if (bpp == 32 && (osd->CanHandleAreas(Areas, sizeof(Areas) / sizeof(tArea)) != oeOk)) {
        bpp = 8;
        Areas[0].bpp = 8;
        DEBUG_OT_AREA("osdteletext: OSD is not providing TrueColor, fallback to bpp=%d", bpp);
    }
    if (osd->CanHandleAreas(Areas, sizeof(Areas) / sizeof(tArea)) != oeOk) {
        DELETENULL(osd);
        esyslog("osdteletext: can't create requested OSD area with x0=%d y0=%d width=%d height=%d bpp=%d osdPreset=%d", x0, y0, width, height, bpp
            , ttSetup.osdPreset
        );
        Skins.Message(mtError, "OSD-Teletext can't request OSD area, check plugin settings");
        return;
    }
    osd->SetAreas(Areas, sizeof(Areas) / sizeof(tArea));

    setOutputWidth(width);
    setOutputHeight(Height);
    setLeftFrame(leftFrame);
    setRightFrame(rightFrame);
    setTopFrame(topFrame);
    setBottomFrame(bottomFrame);

    isyslog("osdteletext: OSD area successful requested with x0=%d y0=%d width=%d height=%d bpp=%d lF=%d rF=%d tF=%d bF=%d bg=0x%08x osdPreset=%d"
        , x0, y0, width, height, bpp
        , leftFrame, rightFrame, topFrame, bottomFrame
        , clrBackground
        , ttSetup.osdPreset
    );

    InitScaler();

    CleanDisplay();
    Dirty=true;
    DirtyAll=true;
}


cDisplay32BPPHalf::cDisplay32BPPHalf(int x0, int y0, int width, int height, int leftFrame, int rightFrame, int topFrame, int bottomFrame, bool upper, bool top, tColor clrBackground)
    : cDisplay(width,height), leftFrame(leftFrame)
      , rightFrame(rightFrame), topFrame(topFrame), bottomFrame(bottomFrame)
      , Upper(upper), Top(top), OsdX0(x0), OsdY0(y0)
{
    osd=NULL;

    Background = clrBackground;

    // Redirect all real init work to method
    InitOSD();
}

void cDisplay32BPPHalf::InitOSD() {
    delete osd;
    int x0 = OsdX0;

    int height = Height / 2; // half heigth
    int vLines = (ttSetup.lineMode24 == true) ? 24 : 25;
    if ((height % vLines) > 0) height = (height / vLines) * vLines; // alignment

    int y0 = OsdY0;
    if (!Top)
        y0 += Height - height; // calculate y-offset

    osd = cOsdProvider::NewOsd(x0, y0);
    if (!osd) return;

    int width=(Width+1)&~1; // Width has to end on byte boundary, so round up

    int bpp = 32;
    if (ttSetup.colorMode4bpp == true) {
        bpp = 4;
        dsyslog("osdteletext: OSD config forced to bpp=%d", bpp);
    };
    tArea Areas[] = { { 0, 0, width - 1 + leftFrame + rightFrame, height - 1 + topFrame + bottomFrame, bpp } };
    if (bpp == 32 && (osd->CanHandleAreas(Areas, sizeof(Areas) / sizeof(tArea)) != oeOk)) {
        bpp = 8;
        Areas[0].bpp = 8;
        DEBUG_OT_AREA("osdteletext: OSD is not providing TrueColor, fallback to bpp=%d", bpp);
    }
    if (osd->CanHandleAreas(Areas, sizeof(Areas) / sizeof(tArea)) != oeOk) {
        DELETENULL(osd);
        esyslog("osdteletext: can't create requested OSD 'half' area with x0=%d y0=%d width=%d height=%d bpp=%d", x0, y0, width, height, bpp);
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

    isyslog("osdteletext: OSD 'half' area successful requested x0=%d y0=%d width=%d height=%d bpp=%d lF=%d rF=%d tF=%d bF=%d Upper=%s Top=%s"
        , x0, y0, width, height, bpp
        , leftFrame, rightFrame, topFrame, bottomFrame
        , (Upper == true) ? "yes" : "no"
        , (Top == true) ? "yes" : "no"
    );

    setOutputWidth(width);
    setOutputHeight(height);
    setLeftFrame(leftFrame);
    setRightFrame(rightFrame);
    setTopFrame(topFrame);
    setBottomFrame(bottomFrame);

    InitScaler();

    // In case we switched on the fly, do a full redraw
    CleanDisplay();
    Dirty=true;
    DirtyAll=true;
    Flush();
}

// vim: ts=4 sw=4 et
