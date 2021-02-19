/*************************************************************** -*- c++ -*-
 *                                                                         *
 *   displaybase.c - Base class for rendering a teletext cRenderPage to    *
 *                   an actual VDR OSD.                                    *
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
#include <time.h>
#include <vdr/tools.h>
#include "displaybase.h"
#include "txtfont.h"
#include <iostream>

std::string cDisplay::TXTFontFootprint = "";
int cDisplay::realFontWidths[4] = {0};

cDisplay::cDisplay(int width, int height)
    : Zoom(Zoom_Off), Concealed(false), Blinked(false), FlushLock(0),
      Boxed(false), Width(width), Height(height), Background(clrGray50),
      osd(NULL), outputWidth(0), outputScaleX(1.0),
      outputHeight(0), outputScaleY(1.0),
      ScaleX(1), ScaleY(1), OffsetX(0), OffsetY(0),
      MessageFont(cFont::GetFont(fontSml)), MessageX(0), MessageY(0),
      MessageW(0), MessageH(0),
      TXTFont(0), TXTDblWFont(0), TXTDblHFont(0), TXTDblHWFont(0)
{
}

cDisplay::~cDisplay() {
    DELETENULL(osd);
    DELETENULL(TXTFont);
    DELETENULL(TXTDblWFont);
    DELETENULL(TXTDblHFont);
    DELETENULL(TXTDblHWFont);
}

// This is an ugly hack, any ideas on how to get font size with characters (glyphs) of specified width/height?
cFont *cDisplay::GetFont(const char *name, int fontIndex, int height, int width) {
    cFont *font = cFont::CreateFont(name, height, width);
    if (font != NULL) {
        int realWidth = font->Width('g');
        if (realWidth > 0) {
            for (int i = width * width / realWidth; i < width * 4; i++) {
                DELETENULL(font);
                font = cFont::CreateFont(name, height, i);
                if (font != NULL) {
                        realWidth = font->Width('g');
                    if (realWidth > width) {
                        DELETENULL(font);
                        width = i - 1;
                        font = cFont::CreateFont(name, height, width);
                        realFontWidths[fontIndex] = width;
                        break;
                    }
                }
            }
        } else {
            esyslog("OSD-Teletext: font %s returned realWidth of 0 (should not happen, please try a different font)", name);
        }
    }
    dsyslog("OSD-Teletext: font %s index %d probed size (w/h) = (%d/%d), char width: %d", name, fontIndex, width, height, font->Width("g"));
    return font;
}

std::string cDisplay::GetFontFootprint(const char *name) {
    return std::string(cString::sprintf("%s_%d_%d_%d", name, fontWidth, fontHeight, Zoom));
}

void cDisplay::InitScaler() {
    // Set up the scaling factors. Also do zoom mode by
    // scaling differently.

    outputScaleX = (double)outputWidth/480.0;
    outputScaleY = (double)outputHeight/250.0;

    int height=Height-6;
    int width=Width-6;
    OffsetX=3;
    OffsetY=3;

    switch (Zoom) {
    case Zoom_Upper:
        height=height*2;
        break;
    case Zoom_Lower:
        OffsetY=OffsetY-height;
        height=height*2;
        break;
    default:;
    }

    ScaleX=(480<<16)/width;
    ScaleY=(250<<16)/height;

    fontWidth = (outputWidth * 2 / 40) & 0xfffe;
    if (Zoom == Zoom_Off) {
        fontHeight = (outputHeight * 2 / 25) & 0xfffe;
    } else {
        fontHeight = (outputHeight * 2 / 13) & 0xfffe;
    }
    // use even font size for double sized characters (prevents rounding errors during character display)
    fontWidth &= 0xfffe;
    fontHeight &= 0xfffe;


    dsyslog("OSD-Teletext: OSD width = %d, height = %d", outputWidth, outputHeight);
    dsyslog("OSD-Teletext: font width * 2 = %d, height = %d", fontWidth, fontHeight);

    int txtFontWidth = fontWidth;
    int txtFontHeight = fontHeight;
    const char *txtFontName = ttSetup.txtFontName;
    std::string footprint = GetFontFootprint(txtFontName);

    if (footprint.compare(TXTFontFootprint) == 0) {
        TXTFont      = cFont::CreateFont(txtFontName, txtFontHeight / 2, realFontWidths[0]);
        TXTDblWFont  = cFont::CreateFont(txtFontName, txtFontHeight / 2, realFontWidths[1]);
        TXTDblHFont  = cFont::CreateFont(txtFontName, txtFontHeight, realFontWidths[2]);
        TXTDblHWFont = cFont::CreateFont(txtFontName, txtFontHeight, realFontWidths[3]);
    } else {
        TXTFontFootprint = footprint;
        TXTFont      = GetFont(txtFontName, 0, txtFontHeight / 2, txtFontWidth / 2);
        TXTDblWFont  = GetFont(txtFontName, 1, txtFontHeight / 2, txtFontWidth);
        TXTDblHFont  = GetFont(txtFontName, 2, txtFontHeight, txtFontWidth / 2);
        TXTDblHWFont = GetFont(txtFontName, 3, txtFontHeight, txtFontWidth);
    }
}

bool cDisplay::SetBlink(bool blink) {
    int x,y;
    bool Change=false;

    if (blink==Blinked) return false;

    // touch all blinking chars
    for (y=0;y<25;y++) {
        for (x=0;x<40;x++) {
            if (Page[x][y].GetBlink())  {
                Page[x][y].SetDirty(true);
                Change=true;
            }
        }
    }
    Blinked=blink;
    if (Change) Dirty=true;

    Flush();

    return Change;
}

bool cDisplay::SetConceal(bool conceal) {
    int x,y;
    bool Change=false;

    if (conceal==Concealed) return false;

    // touch all concealed chars
    for (y=0;y<25;y++) {
        for (x=0;x<40;x++) {
            if (Page[x][y].GetConceal()) {
                Page[x][y].SetDirty(true);
                Change=true;
            }
        }
    }
    Concealed=conceal;
    if (Change) Dirty=true;

    Flush();

    return Change;
}

void cDisplay::SetZoom(enumZoom zoom) {

    if (!osd) return;
    if (Zoom==zoom) return;
    Zoom=zoom;

    // Re-initialize scaler to let zoom take effect
    InitScaler();

    // Clear screen - mainly clear border
    CleanDisplay();

    Flush();
}

void cDisplay::SetBackgroundColor(tColor c) {
    Background=c;
    CleanDisplay();
    Flush();
}

void cDisplay::CleanDisplay() {
    enumTeletextColor bgc=(Boxed)?(ttcTransparent):(ttcBlack);
    if (!osd) return;

    osd->DrawRectangle(0, 0, outputWidth, outputHeight, GetColorRGB(bgc,0));

    // repaint all
    Dirty=true;
    DirtyAll=true;
}


tColor cDisplay::GetColorRGB(enumTeletextColor ttc, int Area) {
    switch (ttc) {
    case ttcBlack:       return Background;
    case ttcRed:         return clrRed;
    case ttcGreen:       return clrGreen;
    case ttcYellow:      return clrYellow;
    case ttcBlue:        return clrBlue;
    case ttcMagenta:     return clrMagenta;
    case ttcCyan:        return clrCyan;
    case ttcWhite:       return clrWhite;
    case ttcTransparent: return clrTransparent;
    default:             return Background;
    }
}

tColor cDisplay::GetColorRGBAlternate(enumTeletextColor ttc, int Area) {
    return GetColorRGB(ttc,Area);
}

void cDisplay::RenderTeletextCode(unsigned char *PageCode) {
    // Interprete teletext code referenced by PageCode
    // and draw the whole page content into OSD.
    // PageCode must be a 40*24+12 bytes buffer

    HoldFlush();

    cRenderPage::ReadTeletextHeader(PageCode);

    if (!Boxed && (Flags&0x60)!=0) {
        Boxed=true;
        CleanDisplay();
    } else if (Boxed && (Flags&0x60)==0) {
        Boxed=false;
        CleanDisplay();
    } else
        CleanDisplay();

    cRenderPage::RenderTeletextCode(PageCode+12);

    ReleaseFlush();
}



void cDisplay::DrawDisplay() {
    int x,y;
    int cnt=0;

    if (!IsDirty()) return;
    // nothing to do

    for (y=0;y<25;y++) {
        for (x=0;x<40;x++) {
            if (IsDirty(x,y)) {
                // Need to draw char to osd
                cnt++;
                cTeletextChar c=Page[x][y];
                c.SetDirty(false);
                if ((Blinked && c.GetBlink()) || (Concealed && c.GetConceal())) {
                    c.SetChar(0x20);
                    c.SetCharset(CHARSET_LATIN_G0_DE);
                }
                DrawChar(x,y,c);
                Page[x][y]=c;
            }
        }
    }

    Dirty=false;
    DirtyAll=false;
}


inline bool IsPureChar(unsigned int *bitmap) {
    // Check if character is pure foreground or
    // pure background color
    int i;
    if (bitmap[0]==0x0000) {
        for (i=1;i<10;i++) {
            if (bitmap[i]!=0x0000) return false;
        }
    } else if (bitmap[0]==0xfff0) {
        for (i=1;i<10;i++) {
            if (bitmap[i]!=0xfff0) return false;
        }
    } else {
        return false;
    }
    return true;
}



void cDisplay::DrawChar(int x, int y, cTeletextChar c) {
    // Get colors
    enumTeletextColor ttfg=c.GetFGColor();
    enumTeletextColor ttbg=c.GetBGColor();

    if (c.GetBoxedOut()) {
        ttbg=ttcTransparent;
        ttfg=ttcTransparent;
    }

    if (!osd) return;

    tColor fg=GetColorRGB(ttfg, 0);
    tColor bg=GetColorRGB(ttbg, 0);

    char buf[5];
    uint t = GetVTXChar(c);
    int tl = Utf8CharSet(t, buf);
    buf[tl] = 0;

    const cFont *font;
    int charset = c.GetCharset();
    int fontType = 0;
    int w = fontWidth / 2;
    int h = fontHeight / 2;
    if (c.GetDblWidth() != dblw_Normal) {
        fontType |= 1;
        w = fontWidth;
    }

    if (c.GetDblHeight() != dblh_Normal) {
        fontType |= 2;
        h = fontHeight;
    }

    bool isGraphicsChar;
    if (charset == CHARSET_GRAPHICS_G1 || charset == CHARSET_GRAPHICS_G1_SEP) {
        isGraphicsChar = true;
    } else {
        isGraphicsChar = false;
        switch(fontType) {
            case 0:
                font = TXTFont;
                break;
            case 1:
                font = TXTDblWFont;
                break;
            case 2:
                font = TXTDblHFont;
                break;
            case 3:
                font = TXTDblHWFont;
                break;
        }
    }

    if (Zoom == Zoom_Lower) {
        y -= 11;
    }

    int vx = x * fontWidth / 2;
    int vy = y * fontHeight / 2;

    bool drawChar = true;
    if (c.GetDblWidth() == dblw_Right) {
        drawChar = false;
    }
    if (c.GetDblHeight() == dblh_Bottom) {
        drawChar = false;
    }

    if (drawChar) {
        if (isGraphicsChar) {
            unsigned int buffer[10];
            unsigned int *charmap;

            // Get character face:
            charmap=GetFontChar(c,buffer);
            if (!charmap) {
                // invalid - clear buffer
                bzero(&buffer,sizeof buffer);
                charmap=buffer;
            }

            cBitmap charBm(w, h, 24);
            charBm.DrawRectangle(0, 0, w, h, bg);

            // draw scaled graphics char
            int virtY = 0;
            while (virtY<=h) {
                int bitline;
                bitline=charmap[virtY * 10 / h];

                int virtX=0;
                while (virtX < w) {
                    int bit=(virtX * 12 / w);
                    if (bitline&(0x8000>>bit)) {
                        charBm.DrawPixel(virtX,virtY,fg);
//                    } else {
//                        charBm.DrawPixel(virtX,virtY,bg);
                    }
                    virtX++;
                }
                virtY++;
            }

            osd->DrawBitmap(vx, vy, charBm);
        } else {
#if 0
            // hi level osd devices (e.g. rpi and softhddevice openglosd currently do not support monospaced fonts with arbitrary width
//            osd->DrawRectangle(vx, vy, vx + w - 1, vy + h - 1, bg);
        osd->DrawText(vx, vy, buf, fg, bg, font);
#else
            cBitmap charBm(w, h, 24);
            charBm.DrawRectangle(0, 0, w, h, bg);
//            charBm.DrawText(0, 0, buf, fg, bg, font);
            charBm.DrawText(0, ttSetup.txtVoffset, buf, fg, 0, font);
            osd->DrawBitmap(vx, vy, charBm);
#endif
        }
    }

}

void cDisplay::DrawText(int x, int y, const char *text, int len) {
    // Copy text to teletext page

    cTeletextChar c;
    c.SetFGColor(ttcWhite);
    c.SetBGColor(ttcBlack);
    c.SetCharset(CHARSET_LATIN_G0);

    // Copy chars up to len or null char
    while (len>0 && *text!=0x00) {
        c.SetChar(*text);
        SetChar(x,y,c);
        text++;
        x++;
        len--;
    }

    // Fill remaining chars with spaces
    c.SetChar(' ');
    while (len>0) {
        SetChar(x,y,c);
        x++;
        len--;
    }
    // .. and display
    Flush();
}

void cDisplay::DrawClock() {
    char text[9];
    time_t t=time(0);
    struct tm loct;

    localtime_r(&t, &loct);
    sprintf(text, "%02d:%02d:%02d", loct.tm_hour, loct.tm_min, loct.tm_sec);

    DrawText(32,0,text,8);
}

void cDisplay::DrawMessage(const char *txt) {
    const int border=5;

    if (!osd) return;

    HoldFlush();
    // Hold flush until done

    ClearMessage();
    // Make sure old message is gone

    if (IsDirty()) DrawDisplay();
    // Make sure all characters are out, so we can draw on top

    int w=MessageFont->Width(txt)+4*border;
    int h=MessageFont->Height(txt)+4*border;
    int x=(outputWidth-w)/2;
    int y=(outputHeight-h)/2;

    // Get local color mapping
    tColor fg=GetColorRGB(ttcWhite,0);
    tColor bg=GetColorRGB(ttcBlack,0);
    if (fg==bg) bg=GetColorRGBAlternate(ttcBlack,0);

    // Draw framed box
    osd->DrawRectangle(x         ,y         ,x+w-1       ,y+border-1  ,fg);
    osd->DrawRectangle(x         ,y+h-border,x+w-1       ,y+h-1       ,fg);
    osd->DrawRectangle(x         ,y         ,x+border-1  ,y+h-1       ,fg);
    osd->DrawRectangle(x+w-border,y         ,x+w-1       ,y+h-1       ,fg);
    osd->DrawRectangle(x+border  ,y+border  ,x+w-border-1,y+h-border-1,bg);

    // Draw text
    osd->DrawText(x+2*border,y+2*border,txt, fg, bg, MessageFont);

    // Remember box
    MessageW=w;
    MessageH=h;
    MessageX=x;
    MessageY=y;

#if 0
    dsyslog("OSD-Teletext/%s: display with MessageX=%d MessageY=%d MessageW=%d MessageH=%d OffsetX=%d OffsetY=%d ScaleX=%d ScaleY=%d", __FUNCTION__, MessageX, MessageY, MessageW, MessageH, OffsetX, OffsetY, ScaleX, ScaleY);
#else
    dsyslog("OSD-Teletext/%s: display with MessageX=%d MessageY=%d MessageW=%d MessageH=%d OffsetX=%d OffsetY=%d", __FUNCTION__, MessageX, MessageY, MessageW, MessageH, OffsetX, OffsetY);
#endif

    // And flush all changes
    ReleaseFlush();
}

void cDisplay::ClearMessage() {
    if (!osd) return;
    if (MessageW==0 || MessageH==0) return;

#if 0
    // BUGGY, resulting in out-of-range
    // map OSD pixel to virtual coordinate, use center of pixel
    int x0=(MessageX-OffsetX)*ScaleX+ScaleX/2;
    int y0=(MessageY-OffsetY)*ScaleY+ScaleY/2;
    int x1=(MessageX+MessageW-1-OffsetX)*ScaleX+ScaleX/2;
    int y1=(MessageY+MessageH-1-OffsetY)*ScaleY+ScaleY/2;

    // map to character
    x0=x0/(12<<16);
    y0=y0/(10<<16);
    x1=(x1+(12<<16)-1)/(12<<16);
    y1=(y1+(10<<16)-1)/(10<<16);

    dsyslog("OSD-Teletext/%s: called with MessageX=%d MessageY=%d MessageW=%d MessageH=%d OffsetX=%d OffsetY=%d ScaleX=%d ScaleY=%d => x0=%d/y0=%d x1=%d/y1=%d", __FUNCTION__, MessageX, MessageY, MessageW, MessageH, OffsetX, OffsetY, ScaleX, ScaleY, x0, y0, x1, y1);
#else
    // NEW, reverse calculation based on how DrawChar
    // map to character x/y
    int x0 = (MessageX - OffsetX )         / (fontWidth  / 2);
    int y0 = (MessageY-OffsetY)            / (fontHeight / 2);
    int x1 = (MessageX+MessageW-1-OffsetX) / (fontWidth  / 2);
    int y1 = (MessageY+MessageH-1-OffsetY) / (fontHeight / 2);

    dsyslog("OSD-Teletext/%s: called with MessageX=%d MessageY=%d MessageW=%d MessageH=%d OffsetX=%d OffsetY=%d => x0=%d/y0=%d x1=%d/y1=%d", __FUNCTION__, MessageX, MessageY, MessageW, MessageH, OffsetX, OffsetY, x0, y0, x1, y1);
#endif

#define TESTOORX(X) (X < 0 || X >= 40)
#define TESTOORY(Y) (Y < 0 || Y >= 25)
    if ( TESTOORX(x0) || TESTOORX(x1) || TESTOORY(y0) || TESTOORY(y1) ) {
	// something out-of-range
#if 0
	esyslog("OSD-Teletext/%s: out-of-range detected(crop) MessageX=%d MessageY=%d MessageW=%d MessageH=%d OffsetX=%d OffsetY=%d ScaleX=%d ScaleY=%d => x0=%d%s y0=%d%s x1=%d%s y1=%d%s", __FUNCTION__, MessageX, MessageY, MessageW, MessageH, OffsetX, OffsetY, ScaleX, ScaleY,
#else
	esyslog("OSD-Teletext/%s: out-of-range detected(crop) MessageX=%d MessageY=%d MessageW=%d MessageH=%d OffsetX=%d OffsetY=%d => x0=%d%s y0=%d%s x1=%d%s y1=%d%s", __FUNCTION__, MessageX, MessageY, MessageW, MessageH, OffsetX, OffsetY,
#endif
		x0, TESTOORX(x0) ? "!" : "",
		y0, TESTOORY(y0) ? "!" : "",
		x1, TESTOORX(x1) ? "!" : "",
		y1, TESTOORY(y1) ? "!" : ""
	);
	// crop to limits
	if (x0 < 0) x0 = 0;
	if (x1 < 0) x1 = 0;
	if (y0 < 0) y0 = 0;
	if (y1 < 0) y1 = 0;
	if TESTOORX(x0) x0 = 40 - 1;
	if TESTOORX(x1) x1 = 40 - 1;
	if TESTOORY(y0) y0 = 25 - 1;
	if TESTOORY(y1) y1 = 25 - 1;
    }
    // dsyslog("OSD-Teletext/%s: call MakeDirty with area x0=%d/y0=%d <-> x1=%d/y1=%d", __FUNCTION__, x0, y0, x1, y1);
    for (int x=x0;x<=x1;x++) {
        for (int y=y0;y<=y1;y++) {
            MakeDirty(x,y);
        }
    }

    MessageW=0;
    MessageH=0;

    Flush();
}
