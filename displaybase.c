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
#include "logging.h"

std::string cDisplay::TXTFontFootprint = "";
int cDisplay::realFontWidths[4] = {0};

cDisplay::cDisplay(int width, int height)
    : Zoom(Zoom_Off), Concealed(true), Blinked(false), FlushLock(0),
      Boxed(false), Width(width), Height(height), Background(clrGray50),
      Paused(false),
      osd(NULL),
      outputWidth(0), outputHeight(0),
      leftFrame(0), rightFrame(0), topFrame(0), bottomFrame(0),
      OffsetX(0), OffsetY(0),
      MessageFont(cFont::GetFont(fontSml)), MessageX(0), MessageY(0),
      MessageW(0), MessageH(0),
      TXTFont(0), TXTDblWFont(0), TXTDblHFont(0), TXTDblHWFont(0), TXTHlfHFont(0)
{
}

cDisplay::~cDisplay() {
    DELETENULL(osd);
    DELETENULL(TXTFont);
    DELETENULL(TXTDblWFont);
    DELETENULL(TXTDblHFont);
    DELETENULL(TXTDblHWFont);
    DELETENULL(TXTHlfHFont);
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
            esyslog("osdteletext: font %s returned realWidth of 0 (should not happen, please try a different font)", name);
        }
    }
    DEBUG_OT_FONT("font %s index %d probed size (w/h) = (%d/%d), char width: %d", name, fontIndex, width, height, font->Width("g"));
    return font;
}

std::string cDisplay::GetFontFootprint(const char *name) {
    return std::string(cString::sprintf("%s_%d_%d_%d", name, fontWidth, fontHeight, Zoom));
}

void cDisplay::InitScaler() {
    // Set up the scaling factors. Also do zoom mode by
    // scaling differently.

    // outputScaleX = (double)outputWidth/480.0; // EOL: no longer used
    // outputScaleY = (double)outputHeight / (((ttSetup.lineMode24 == true) ? 24 : 25) * 10.0); // EOL: no longer used

    int height=Height-6;
    // int width=Width-6; // EOL: no longer used
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

    fontWidth = (outputWidth * 2 / 40) & 0xfffe;
    if (Zoom == Zoom_Off) {
        fontHeight = (outputHeight * 2 / ((ttSetup.lineMode24 == true) ? 24 : 25)) & 0xfffe;
    } else {
        fontHeight = (outputHeight * 2 * 2 / ((ttSetup.lineMode24 == true) ? 24 : 25)) & 0xfffe;
    }
    // use even font size for double sized characters (prevents rounding errors during character display)
    fontWidth &= 0xfffe;
    fontHeight &= 0xfffe;

    dsyslog("osdteletext: InitScaler width=%d height=%d fontWidth*2=%d fontHeight=%d lineMode24=%d Zoom=%d", outputWidth, outputHeight, fontWidth, fontHeight, ttSetup.lineMode24, Zoom);

    int txtFontWidth = fontWidth;
    int txtFontHeight = fontHeight;
    const char *txtFontName = ttSetup.txtFontName;
    std::string footprint = GetFontFootprint(txtFontName);

    if (footprint.compare(TXTFontFootprint) == 0) {
        TXTFont      = cFont::CreateFont(txtFontName, txtFontHeight / 2, realFontWidths[0]);
        TXTDblWFont  = cFont::CreateFont(txtFontName, txtFontHeight / 2, realFontWidths[1]);
        TXTDblHFont  = cFont::CreateFont(txtFontName, txtFontHeight, realFontWidths[2]);
        TXTDblHWFont = cFont::CreateFont(txtFontName, txtFontHeight, realFontWidths[3]);
        TXTHlfHFont  = cFont::CreateFont(txtFontName, txtFontHeight / 4, realFontWidths[0]);
    } else {
        TXTFontFootprint = footprint;
        TXTFont      = GetFont(txtFontName, 0, txtFontHeight / 2, txtFontWidth / 2);
        TXTDblWFont  = GetFont(txtFontName, 1, txtFontHeight / 2, txtFontWidth);
        TXTDblHFont  = GetFont(txtFontName, 2, txtFontHeight, txtFontWidth / 2);
        TXTDblHWFont = GetFont(txtFontName, 3, txtFontHeight, txtFontWidth);
        TXTHlfHFont  = GetFont(txtFontName, 0, txtFontHeight / 4, txtFontWidth / 2);
    }
}

bool cDisplay::SetBlink(bool blink) {
    DEBUG_OT_BLINK("called with blink=%d", blink);
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

bool cDisplay::HasConceal() {
    int x,y;
    bool HasConceal=false;

    // check all concealed chars
    for (y=0;y<25;y++) {
        for (x=0;x<40;x++) {
            if (Page[x][y].GetConceal()) {
                HasConceal=true;
            }
        }
    }

    return HasConceal;
}

void cDisplay::SetZoom(enumZoom zoom) {
    DEBUG_OT_DBFC("called: zoom=%d", zoom);

    if (!osd) return;
    if (Zoom==zoom) return;
    Zoom=zoom;

    // Re-initialize scaler to let zoom take effect
    InitScaler();

    // Clear screen - mainly clear border
    // CleanDisplay(); // called later after SetBackgroundColor
    Dirty=true;
    DirtyAll=true;

    Flush();
}

void cDisplay::SetBackgroundColor(tColor c) {
    DEBUG_OT_DBFC("called: tColor=0x%08x", c);
    Background=c;
    CleanDisplay();
    Flush();
}

void cDisplay::CleanDisplay() {
    enumTeletextColor bgc=(Boxed)?(ttcTransparent):(ttcBlack);
    if (!osd) return;

    DEBUG_OT_DBFC("called: outputWidth=%d outputHeight=%d boxed=%d color=0x%08x bgc=%d", outputWidth, outputHeight, Boxed, GetColorRGB(bgc,0), bgc);
    if (m_debugmask & DEBUG_MASK_OT_ACT_OSD_BACK_RED)
        osd->DrawRectangle(0, 0, outputWidth - 1 + leftFrame + rightFrame, outputHeight - 1 + topFrame + bottomFrame, GetColorRGB(ttcRed,0));
    else
        osd->DrawRectangle(0, 0, outputWidth - 1 + leftFrame + rightFrame, outputHeight - 1 + topFrame + bottomFrame, GetColorRGB(bgc,0));

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
    // PageCode must be a buffer containing TelePageData structure (see storage.h)

    HoldFlush();

    cRenderPage::ReadTeletextHeader(PageCode);

    DEBUG_OT_DBFC("called");

    if (!Boxed && (Flags&0x60)!=0) {
        Boxed=true;
        CleanDisplay();
    } else if (Boxed && (Flags&0x60)==0) {
        Boxed=false;
        CleanDisplay();
    } else
        CleanDisplay();

    if (memcmp(PageCode, "VTXV5", 5) != 0) {
        esyslog("osdteletext: cDisplay::RenderTeletextCode called with PageCode which is not starting with 'VTXV5' (not supported)");
        return;
    };

    cRenderPage::RenderTeletextCode(PageCode+12);

    ReleaseFlush();
}



void cDisplay::DrawDisplay() {
    DEBUG_OT_DD("called with Blinked=%d Concealed=%d", Blinked, Concealed);
    int x,y;
    int cnt=0;

    if (!IsDirty()) return; // nothing to do

    for (y = 0; y < ((ttSetup.lineMode24 == true) ? 24 : 25); y++) {
        for (x=0;x<40;x++) {
            if (IsDirty(x,y)) {
                // Need to draw char to osd
                cnt++;
                cTeletextChar c=Page[x][y];
                c.SetDirty(false);
                if ((Blinked && c.GetBlink()) || (Concealed && c.GetConceal())) {
                    DEBUG_OT_BLINK("blink by replacing char %08x with ' ' on x=%d y=%d", c.GetC(), x, y);
                    c.SetChar(0x20);
                    c.SetCharset(CHARSET_LATIN_G0_DE);
                }
                DrawChar(x,y,c);
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

    static int cache_txtVoffset   = 0;
    static int cache_outputHeight = 0;
    static int cache_OsdHeight    = 0;
    static int cache_Vshift = 0;
    static int cache_valid = 0;

    if (!osd) return;

    if (Boxed && c.GetBoxedOut()) {
        ttbg=ttcTransparent;
        ttfg=ttcTransparent;
    }

    tColor fg=GetColorRGB(ttfg, 0);
    tColor bg=GetColorRGB(ttbg, 0);

    char buf[5];
    uint t = GetVTXChar(c);
    int tl = Utf8CharSet(t, buf);
    buf[tl] = 0;

    const cFont *font = TXTFont; // FIXED: -Wmaybe-uninitialized
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
        if (c.GetChar() == 0x7f) // filled rectangle
            isGraphicsChar = true;
        else
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

    bool h_scale_div2 = false;

    if (Zoom == Zoom_Lower) {
        y -= 12;
        if (y < 0 || y > 11) {
            if ((ttSetup.lineMode24 == false) && (y == 12)) {
                // display special line 24 in half height
                h /= 2;
                h_scale_div2 = true;
                font = TXTHlfHFont;
            } else {
                // display only line 12-23 (12 lines)
                return;
            };
        };
    };

    if (Zoom == Zoom_Upper) {
        if (y > 11) {
            if ((ttSetup.lineMode24 == false) && (y == 24)) {
                // display special line 24 in half height
                y -= 12;
                h /= 2;
                h_scale_div2 = true;
                font = TXTHlfHFont;
            } else {
                // display only line 0-11 (12 lines)
                return;
            };
        };
    };

    if ((m_debugmask & DEBUG_MASK_OT_ACT_LIMIT_LINES) && (y > 8)) return;

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
            if (m_debugmask & DEBUG_MASK_OT_ACT_CHAR_BACK_BLUE)
                charBm.DrawRectangle(0, 0, w - 1, h - 1, GetColorRGB(ttcBlue,0));
            else
                charBm.DrawRectangle(0, 0, w - 1, h - 1, bg);

            // draw scaled graphics char
            int virtY = 0;
            while (virtY<=h) {
                int bitline;
                bitline=charmap[virtY * 10 / h / ((h_scale_div2 == true) ? 2 : 1)];

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
#if 0
            if (x == 0) {
#else
            if (y == 16) {
#endif
                DEBUG_OT_DCHR("x=%d y=%d vx=%d vy=%d w=%d h=%d h_scale_div2=%d ttfg=%d ttbg=%d BoxedOut=%d graphics charmap=0x%04x", x, y, vx, vy, w, h, h_scale_div2, ttfg, ttbg, c.GetBoxedOut(), *charmap);
            };

            osd->DrawBitmap(vx + leftFrame, vy + topFrame, charBm);
        } else {
#if 0
            // hi level osd devices (e.g. rpi and softhddevice openglosd currently do not support monospaced fonts with arbitrary width
            // osd->DrawRectangle(vx, vy, vx + w - 1, vy + h - 1, bg);
            osd->DrawText(vx, vy, buf, fg, bg, font);
#else
            cBitmap charBm(w, h, 24);
            if (m_debugmask & DEBUG_MASK_OT_ACT_CHAR_BACK_BLUE)
                charBm.DrawRectangle(0, 0, w - 1, h - 1, GetColorRGB(ttcBlue,0));
            else
                charBm.DrawRectangle(0, 0, w - 1, h - 1, bg);
//            charBm.DrawText(0, 0, buf, fg, bg, font);
            if (
                 (cache_valid == 0) || (
                 (cache_txtVoffset   != ttSetup.txtVoffset)
              || (cache_outputHeight != outputHeight      )
              || (cache_OsdHeight    != cOsd::OsdHeight() )
              )
            ) {
                cache_valid = 1;
                cache_txtVoffset   = ttSetup.txtVoffset;
                cache_outputHeight = outputHeight;
                cache_OsdHeight    = cOsd::OsdHeight();
                cache_Vshift       = (cache_txtVoffset * cache_outputHeight) / cache_OsdHeight;
                dsyslog("osdteletext: DrawText vertical shift cache updated: txtVoffset=%d outputHeight=%d OsdHeight=%d => Vshift=%d", cache_txtVoffset, cache_outputHeight, cache_OsdHeight, cache_Vshift);
            };
#if 0
            if (x == 0) {
#else
            if (y == 2 || y == 3) {
#endif
                DEBUG_OT_DCHR("x=%d y=%d vx=%d vy=%d w=%d h=%d h_scale_div2=%d ttfg=%d ttbg=%d BoxedOut=%d text charset=0x%04x char='%s'", x, y, vx, vy, w, h, h_scale_div2, ttfg, ttbg, c.GetBoxedOut(), charset, buf);
            };
            charBm.DrawText(0, cache_Vshift, buf, fg, 0, font, 0, h / ((h_scale_div2 == true) ? 2 : 1));
            osd->DrawBitmap(vx + leftFrame, vy + topFrame, charBm);
#endif
        }
    }
}

uint8_t UTF8toTeletextChar(const uint8_t c1, const uint8_t c2, enumCharsets *TeletextCharset) {
    // Convert UTF char into TeletextChar and set related TeletextCharset
    uint8_t TeletextChar = '?'; // default "unknown"
    *TeletextCharset = CHARSET_LATIN_G0; // default

    switch (c1) {
        case 0xc3:
            switch (c2) {
                // CHARSET_LATIN_G0_DE
                case 0x84: // LATIN CAPITAL LETTER A WITH DIAERESIS
                    TeletextChar = 0x5b; *TeletextCharset = CHARSET_LATIN_G0_DE;
                    break;
                case 0x96: // LATIN CAPITAL LETTER O WITH DIAERESIS
                    TeletextChar = 0x5c; *TeletextCharset = CHARSET_LATIN_G0_DE;
                    break;
                case 0x9c: // LATIN CAPITAL LETTER U WITH DIAERESIS
                    TeletextChar = 0x7d; *TeletextCharset = CHARSET_LATIN_G0_DE;
                    break;
                case 0xa4: // LATIN SMALL LETTER A WITH DIAERESIS
                    TeletextChar = 0x7b; *TeletextCharset = CHARSET_LATIN_G0_DE;
                    break;
                case 0xb6: // LATIN SMALL LETTER O WITH DIAERESIS
                    TeletextChar = 0x7c; *TeletextCharset = CHARSET_LATIN_G0_DE;
                    break;
                case 0xbc: // LATIN SMALL LETTER U WITH DIAERESIS
                    TeletextChar = 0x7d; *TeletextCharset = CHARSET_LATIN_G0_DE;
                    break;
                case 0x9f: // LATIN SMALL LETTER SHARP S
                    TeletextChar = 0x7e; *TeletextCharset = CHARSET_LATIN_G0_DE;
                    break;
             }
        // TODO: implement other required mapping
    }
    return (TeletextChar);
}

void cDisplay::DrawTextExtended(const int x, const int y, const char *text, const int len, const enumAlignment alignment, const enumTeletextColor ttcFg, enumTeletextColor const ttcBg) {
    // Copy text to teletext page with alignment and foreground/background color
    int len_text_utf8 = Utf8StrLen(text);
    int len_text = strlen(text);

    DEBUG_OT_DTXT("called with x=%d y=%d len=%d alignment=%d ttcFg=%d ttcBg=%d text='%s' strlen(text)=%d utf8len(text)=%d", x, y, len, alignment, ttcFg, ttcBg, text, len_text, len_text_utf8);

    int fill_left = 0;

    if (len_text_utf8 < len) {
        if (alignment == AlignmentRight) {
            fill_left = len - len_text_utf8;
        } else if (alignment == AlignmentCenter) {
            fill_left = (len - len_text_utf8) / 2;
        };
    };

    cTeletextChar c;
    c.SetFGColor(ttcFg);
    c.SetBGColor(ttcBg);

    int j = 0;
    for (int i = 0; i < len; i++) {
        if (i < fill_left) {
            // fill left with space
            c.SetChar(' ');
        } else if (i > fill_left + len_text_utf8)  {
            // fill right with space
            c.SetChar(' ');
        } else {
            c.SetCharset(CHARSET_LATIN_G0); // default
            uint8_t c1 = text[j];
            if (j +1  < len_text) {
                // check for UTF-8
                if ((text[j + 1] & 0xC0) == 0x80) {
                    // unicode
                    uint8_t c2 = text[j + 1];
                    enumCharsets Charset;
                    uint8_t Char = UTF8toTeletextChar(c1, c2, &Charset);
                    DEBUG_OT_DTXT("unicode mapped i=%d c1=%x c2=%x -> c=%02x cs=%04x", i, c1, c2, Char, Charset);
                    c.SetCharset(Charset);
                    c1 = Char;
                    j++;
                }
            };
            c.SetChar(c1);
            j++;
        };
        SetChar(x+i, y, c);
    };

    Flush();
};

void cDisplay::DrawText(int x, int y, const char *text, int len) {
    // Copy text to teletext page
    DEBUG_OT_DTXT("called with x=%d y=%d len=%d text='%s' strlen(text)=%ld", x, y, len, text, strlen(text));

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

void cDisplay::DrawPageId(const char *text) {
    // Draw Page ID string to OSD
    static char text_last[9] = ""; // remember
    static bool paused_last = false;
    cTeletextChar c;

    DEBUG_OT_DRPI("called with text='%s' text_last='%s' Boxed=%d HasConceal=%d GetConceal=%d", text, text_last, Boxed, HasConceal(), GetConceal());

    if (! GetPaused() && Boxed && (strcmp(text, text_last) == 0)) {
        // don't draw PageId a 2nd time on boxed pages
        for (int i = 0; i < 8; i++) {
            c.SetFGColor(ttcTransparent);
            c.SetBGColor(ttcTransparent);
            c.SetChar(0);
            SetChar(i,0,c);
        };
        return;
    };

    DrawText(0,0,text,8);
    strncpy(text_last, text, sizeof(text_last) - 1);

    if (HasConceal()) {
        c.SetBGColor(ttcBlack);
        if (GetConceal()) {
            c.SetFGColor(ttcYellow);
            c.SetChar('?');
        } else {
            c.SetFGColor(ttcGreen);
            c.SetChar('!');
        };
        DEBUG_OT_DRPI("trigger SetChar for Conceiled hint ttfg=%x ttbg=%x", c.GetFGColor(), c.GetBGColor());
        SetChar(6, 0, c);
    };

    if (GetPaused()) {
        paused_last = true;
        c.SetBGColor(ttcBlack);
        c.SetFGColor(ttcRed);
        c.SetChar('*');
        DEBUG_OT_DRPI("trigger SetChar for Paused hint ttfg=%x ttbg=%x", c.GetFGColor(), c.GetBGColor());
        SetChar(7, 0, c);
    } else if (paused_last == true) {
        paused_last = false;
        c.SetBGColor(ttcBlack);
        c.SetFGColor(ttcGreen);
        c.SetChar('>');
        DEBUG_OT_DRPI("trigger SetChar for Paused finished hint ttfg=%x ttbg=%x", c.GetFGColor(), c.GetBGColor());
        SetChar(7, 0, c);
    };
}

void cDisplay::DrawFooter(const char *textRed, const char *textGreen, const char* textYellow, const char *textBlue, const FooterFlags flag) {
    if (Boxed) return; // don't draw footer in on boxed pages

    switch(flag) {
        case FooterNormal:
            DrawTextExtended( 0, 24, textRed   , 10, AlignmentCenter, ttcWhite, ttcRed    );
            DrawTextExtended(10, 24, textGreen , 10, AlignmentCenter, ttcWhite, ttcGreen  );
            DrawTextExtended(20, 24, textYellow, 10, AlignmentCenter, ttcWhite, ttcYellow );
            DrawTextExtended(30, 24, textBlue  , 10, AlignmentCenter, ttcWhite, ttcBlue   );
            break;

        case FooterYellowValue:
            DrawTextExtended( 0, 24, textRed   , 10, AlignmentCenter, ttcWhite, ttcRed    );
            DrawTextExtended(10, 24, textGreen , 10, AlignmentCenter, ttcWhite, ttcGreen  );
            DrawTextExtended(20, 24, textYellow, 10, AlignmentCenter, ttcWhite, ttcMagenta);
            DrawTextExtended(30, 24, textBlue  , 10, AlignmentCenter, ttcWhite, ttcBlue   );
            break;

        case FooterGreenYellowValue:
            DrawTextExtended( 0, 24, textRed   , 10, AlignmentCenter, ttcWhite, ttcRed    );
            DrawTextExtended(10, 24, textGreen , 20, AlignmentCenter, ttcWhite, ttcMagenta);
            DrawTextExtended(30, 24, textBlue  , 10, AlignmentCenter, ttcWhite, ttcBlue   );
            break;
   };
}

void cDisplay::DrawClock() {
    if (Boxed) return; // don't draw Clock in on boxed pages

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

    DEBUG_OT_MSG("MessageX=%d MessageY=%d MessageW=%d MessageH=%d OffsetX=%d OffsetY=%d", MessageX, MessageY, MessageW, MessageH, OffsetX, OffsetY);

    // And flush all changes
    ReleaseFlush();
}

void cDisplay::ClearMessage() {
    if (!osd) return;
    if (MessageW==0 || MessageH==0) return;

    // NEW, reverse calculation based on how DrawChar
    // map to character x/y
    int x0 = (MessageX - OffsetX )         / (fontWidth  / 2);
    int y0 = (MessageY-OffsetY)            / (fontHeight / 2);
    int x1 = (MessageX+MessageW-1-OffsetX) / (fontWidth  / 2);
    int y1 = (MessageY+MessageH-1-OffsetY) / (fontHeight / 2);

    DEBUG_OT_MSG("MessageX=%d MessageY=%d MessageW=%d MessageH=%d OffsetX=%d OffsetY=%d => x0=%d/y0=%d x1=%d/y1=%d", MessageX, MessageY, MessageW, MessageH, OffsetX, OffsetY, x0, y0, x1, y1);

#define TESTOORX(X) (X < 0 || X >= 40)
#define TESTOORY(Y) (Y < 0 || Y >= 25)
    if ( TESTOORX(x0) || TESTOORX(x1) || TESTOORY(y0) || TESTOORY(y1) ) {
        // something out-of-range
	    esyslog("osdteletext: %s out-of-range detected(crop) MessageX=%d MessageY=%d MessageW=%d MessageH=%d OffsetX=%d OffsetY=%d => x0=%d%s y0=%d%s x1=%d%s y1=%d%s", __FUNCTION__, MessageX, MessageY, MessageW, MessageH, OffsetX, OffsetY,
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
    // DEBUG_OT_MSG("call MakeDirty with area x0=%d/y0=%d <-> x1=%d/y1=%d", x0, y0, x1, y1);
    for (int x=x0;x<=x1;x++) {
        for (int y=y0;y<=y1;y++) {
            MakeDirty(x,y);
        }
    }

    MessageW=0;
    MessageH=0;

    Flush();
}

// vim: ts=4 sw=4 et
