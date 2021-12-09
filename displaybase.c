/*************************************************************** -*- c++ -*-
 *       Copyright (c) 2005      by Udo Richter                            *
 *       Copyright (c) 2021      by Peter Bieringer (extenions)            *
 *                                                                         *
 *   displaybase.c - Base class for rendering a teletext cRenderPage to    *
 *                   an actual VDR OSD.                                    *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
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
int cDisplay::realFontWidths[5] = {0};
int cDisplay::realFontHeights[5] = {0};

cDisplay::cDisplay(int width, int height)
    : Zoom(Zoom_Off), Concealed(true), Blinked(false), FlushLock(0),
      Boxed(false), Width(width), Height(height), Background(clrGray50),
      Paused(false),
      PageIdDisplayedEver(false),
      osd(NULL),
      outputWidth(0), outputHeight(0),
      leftFrame(0), rightFrame(0), topFrame(0), bottomFrame(0),
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

// This is an improved detection mechanism (still ugly hack, any ideas on how to get font size with characters (glyphs) of specified width/height?)
// TODO: shrinking in case of italic fonts
cFont *cDisplay::GetFont(const char *name, int fontIndex, int height, int width) {
    const int heightOrig = height;
    const int widthOrig = width;
    DEBUG_OT_FONT("called with font %s index %d with requested w=%3d h=%3d", name, fontIndex, widthOrig, heightOrig);
    cFont *font = cFont::CreateFont(name, height, width);
    if (font != NULL) {
        int realWidth = font->Width('W');
        int realHeight = font->Height();
        DEBUG_OT_FONT("INITIAL     fontWidth =%3d -> realWidth =%3d    requestedWidth =%3d", width, realWidth, widthOrig);
        if (realWidth > 0) {
            for (int i = (width * width) / realWidth; i < width * 4; i++) {
                DELETENULL(font);
                font = cFont::CreateFont(name, heightOrig, i);
                if (font != NULL) {
                    realWidth = font->Width('W');
                    DEBUG_OT_FONT("TEST result fontWidth =%3d -> realWidth =%3d %s requestedWidth =%3d"
                        , i, realWidth
                        , (realWidth > widthOrig) ? "> " : "<="
                        , widthOrig);
                    if (realWidth > widthOrig) {
                        // too large, select last one and finish
                        width = i - 1;
                        realFontWidths[fontIndex] = width; // store in cache
                        break;
                    }
                }
            }
        } else {
            esyslog("osdteletext: font %s returned realWidth of 0 (should not happen, please try a different font)", name);
        }

        DEBUG_OT_FONT("INITIAL     fontHeight=%3d -> realHeight=%3d    requestedHeight=%3d", height, realHeight, heightOrig);
        if (realHeight > 0) {
            for (int i = height * height / realHeight; i < height * 4; i++) {
                DELETENULL(font);
                font = cFont::CreateFont(name, i, widthOrig);
                if (font != NULL) {
                    realHeight = font->Height();
                    DEBUG_OT_FONT("TEST result fontHeight=%3d -> realHeight=%3d %s requestedHeight=%3d"
                        , i, realHeight
                        , (realHeight > heightOrig) ? "> " : "<="
                        , heightOrig);
                    if (realHeight > heightOrig) {
                        // too large, select last one and finish
                        height = i - 1;
                        realFontHeights[fontIndex] = height; // store in cache
                        break;
                    }
                }
            }
        } else {
            esyslog("osdteletext: font %s returned realHeight of 0 (should not happen, please try a different font)", name);
        }
    }

    DELETENULL(font);
    font = cFont::CreateFont(name, height, width); // recreate with final height and width from above

    DEBUG_OT_FONT("font %s index %d with requested w=%3d h=%3d => probed size w=%3d h=%3d", name, fontIndex, widthOrig, heightOrig, width, height);
    return font;
}

std::string cDisplay::GetFontFootprint(const char *name) {
    return std::string(cString::sprintf("%s_%d_%d_%d", name, fontWidth, fontHeight, Zoom));
}

void cDisplay::InitScaler() {
    // Set up the scaling factors. Also do zoom mode by
    // scaling differently.

    fontWidth = (outputWidth * 2 / 40) & 0xfffe;
    if (Zoom == Zoom_Off) {
        fontHeight = (outputHeight * 2 / TT_DISPLAY_LINES)  & 0xfffe;
    } else {
        fontHeight = (outputHeight * 2 * 2 / TT_DISPLAY_LINES) & 0xfffe;
    }
    // use even font size for double sized characters (prevents rounding errors during character display)
    fontWidth &= 0xfffe;
    fontHeight &= 0xfffe;

    DEBUG_OT_SCALER("osdteletext: InitScaler width=%d height=%d fontWidth*2=%d fontHeight=%d lineMode24=%d Zoom=%d", outputWidth, outputHeight, fontWidth, fontHeight, ttSetup.lineMode24, Zoom);

    int txtFontWidth = fontWidth;
    int txtFontHeight = fontHeight;
    const char *txtFontName = TTSETUPPRESET_FONTNAME(Font);
    std::string footprint = GetFontFootprint(txtFontName);
    if (footprint.compare(TXTFontFootprint) == 0) {
        // cached
        DEBUG_OT_FONT("call cFont::CreateFont for: %s", txtFontName);
        TXTFont      = cFont::CreateFont(txtFontName, realFontHeights[0], realFontWidths[0]);
        TXTDblWFont  = cFont::CreateFont(txtFontName, realFontHeights[1], realFontWidths[1]);
        TXTDblHFont  = cFont::CreateFont(txtFontName, realFontHeights[2], realFontWidths[2]);
        TXTDblHWFont = cFont::CreateFont(txtFontName, realFontHeights[3], realFontWidths[3]);
        TXTHlfHFont  = cFont::CreateFont(txtFontName, realFontHeights[4], realFontWidths[4]);
    } else {
        TXTFontFootprint = footprint;
        DEBUG_OT_FONT("call GetFont for: %s", txtFontName);
        TXTFont      = GetFont(txtFontName, 0, txtFontHeight / 2, txtFontWidth / 2);
        TXTDblWFont  = GetFont(txtFontName, 1, txtFontHeight / 2, txtFontWidth);
        TXTDblHFont  = GetFont(txtFontName, 2, txtFontHeight, txtFontWidth / 2);
        TXTDblHWFont = GetFont(txtFontName, 3, txtFontHeight, txtFontWidth);
        TXTHlfHFont  = GetFont(txtFontName, 4, txtFontHeight / 4, txtFontWidth / 2);
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
    tColor bgc;

    if (!osd) return;

    if (Boxed)
        bgc = GetColorRGB(ttcTransparent,0);
    else if (m_debugmask & DEBUG_MASK_OT_ACT_OSD_BACK_RED)
        bgc = GetColorRGB(ttcRed,0);
    else
        bgc = Background;

    DEBUG_OT_RENCLN("called: outputWidth=%d outputHeight=%d boxed=%s color=0x%08x", outputWidth, outputHeight, BOOLTOTEXT(Boxed), bgc);
    osd->DrawRectangle(0, 0, outputWidth - 1 + leftFrame + rightFrame, outputHeight - 1 + topFrame + bottomFrame, bgc);

    // repaint all
    Dirty=true;
    DirtyAll=true;
}

// AARRGGBB
#define COLOR_HALF(clr)  ((clr & 0xff000000) | ((clr & 0x00fe0000) >> 1) | ((clr & 0x0000fe00) >> 1) | ((clr & 0x000000fe) >> 1))

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
        case ttcHalfRed:     return COLOR_HALF(clrRed);
        case ttcHalfGreen:   return COLOR_HALF(clrGreen);
        case ttcHalfYellow:  return COLOR_HALF(clrYellow);
        case ttcHalfBlue:    return COLOR_HALF(clrBlue);
        case ttcHalfMagenta: return COLOR_HALF(clrMagenta);
        case ttcHalfCyan:    return COLOR_HALF(clrCyan);
        case ttcGrey:        return COLOR_HALF(clrWhite);
        // 16-31 according to  ETSI EN 300 706 V1.2.1 (2003-012.4) 12.4
        case ttcColor16:     return 0xFFFC005C;
        case ttcColor17:     return 0xFFFC7C00;
        case ttcColor18:     return 0xFF00FC7C;
        case ttcColor19:     return 0xFFFCFCBC;
        case ttcColor20:     return 0xFFFCFCBC;
        case ttcColor21:     return 0xFF00CCAC;
        case ttcColor22:     return 0xFF5C0000;
        case ttcColor23:     return 0xFF6C5C2C;
        case ttcColor24:     return 0xFFCC7C7C;
        case ttcColor25:     return 0xFF3C3C3C; // grey25
        case ttcColor26:     return 0xFFFC7C7C;
        case ttcColor27:     return 0xFF7CFC7C;
        case ttcColor28:     return 0xFFFCFC7C;
        case ttcColor29:     return 0xFF7C7CFC;
        case ttcColor30:     return 0xFF7CFCFC;
        case ttcColor31:     return 0xFFDCDCDC; // grey75
        default:             return Background;
    }
}

tColor cDisplay::GetColorRGBAlternate(enumTeletextColor ttc, int Area) {
    // TODO implement ??
    return GetColorRGB(ttc,Area);
}

void cDisplay::RenderTeletextCode(unsigned char *PageCode) {
    // Interprete teletext code referenced by PageCode
    // and draw the whole page content into OSD.
    // PageCode must be a buffer containing TelePageData structure (see storage.h)

    HoldFlush();

    cRenderPage::ReadTeletextHeader(PageCode);

    DEBUG_OT_RENCLN("called with Boxed=%s Flags=0x%02x", BOOLTOTEXT(Boxed), Flags);

    if (!Boxed && (Flags&0x60)!=0) {
        DEBUG_OT_RENCLN("Toggle Boxed: false->true");
        Boxed=true;
        CleanDisplay();
    } else if (Boxed && (Flags&0x60)==0) {
        DEBUG_OT_RENCLN("Toggle Boxed: true->false");
        Boxed=false;
        CleanDisplay();
    }

    if (memcmp(PageCode, "VTXV5", 5) != 0) {
        esyslog("osdteletext: cDisplay::RenderTeletextCode called with PageCode which is not starting with 'VTXV5' (not supported)");
        return;
    };

    cRenderPage::RenderTeletextCode(PageCode+12);

    ReleaseFlush();
}



void cDisplay::DrawDisplay() {
    DEBUG_OT_DD("called with Blinked=%d Concealed=%d Dirty=%s DirtyAll=%s IsDirty()=%s", Blinked, Concealed, BOOLTOTEXT(Dirty), BOOLTOTEXT(DirtyAll), BOOLTOTEXT(IsDirty()));
    int x,y;

    if (!IsDirty()) return; // nothing to do

    for (y = 0; y < TT_DISPLAY_LINES; y++) {
        for (x=0;x<40;x++) {
            if (IsDirty(x,y)) {
                // Need to draw char to osd
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
    int w = fontWidth  / 2;
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
    int lines_div2 = 0;

    if (Zoom == Zoom_Lower) {
        y -= 12;
        if (y < 0 || y > 11) {
            if ((ttSetup.lineMode24 == false) && (y >= 12)) {
                // display special line >= 25 in half height
                h /= 2;
                h_scale_div2 = true;
                font = TXTHlfHFont;
                lines_div2 = y - 12;
            } else {
                // display only line 12-23 (12 lines)
                return;
            };
        };
    };

    if (Zoom == Zoom_Upper) {
        if (y > 11) {
            if ((ttSetup.lineMode24 == false) && (y >= 24)) {
                // display special line >= 25 in half height
                y -= 12;
                h /= 2;
                h_scale_div2 = true;
                font = TXTHlfHFont;
                lines_div2 = y - 12;
            } else {
                // display only line 0-11 (12 lines)
                return;
            };
        };
    };

    if ((m_debugmask & DEBUG_MASK_OT_ACT_LIMIT_LINES) && (y > 8)) return;

    int vx = x * fontWidth  / 2;
    int vy = y * fontHeight / 2 - lines_div2 * h;

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

            osd->DrawBitmap(vx + leftFrame, vy + topFrame, charBm);

        } else {
            cBitmap charBm(w, h, 24);
            if (m_debugmask & DEBUG_MASK_OT_ACT_CHAR_BACK_BLUE)
                charBm.DrawRectangle(0, 0, w - 1, h - 1, GetColorRGB(ttcBlue,0));
            else
                charBm.DrawRectangle(0, 0, w - 1, h - 1, bg);
//            charBm.DrawText(0, 0, buf, fg, bg, font);
            if (
                 (cache_valid == 0) || (
                 (cache_txtVoffset   != TTSETUPPRESET(Voffset))
              || (cache_outputHeight != outputHeight      )
              || (cache_OsdHeight    != cOsd::OsdHeight() )
              )
            ) {
                cache_valid = 1;
                cache_txtVoffset   = TTSETUPPRESET(Voffset);
                cache_outputHeight = outputHeight;
                cache_OsdHeight    = cOsd::OsdHeight();
                cache_Vshift       = (cache_txtVoffset * cache_outputHeight) / cache_OsdHeight;
                DEBUG_OT_DTXT("osdteletext: DrawText vertical shift cache updated: txtVoffset=%d outputHeight=%d OsdHeight=%d => Vshift=%d", cache_txtVoffset, cache_outputHeight, cache_OsdHeight, cache_Vshift);
            };

            if ((m_debugline >= 0) && (y == m_debugline)) {
                DEBUG_OT_DCHR("y=%2d x=%2d vy=%4d vx=%4d w=%d h=%d cache_Vshift=%d ttfg=%d fg=0x%08x ttbg=%d bg=0x%08x BoxedOut=%d text charset=0x%04x char='%s'", y, x, vy, vx, w, h, cache_Vshift, ttfg, fg, ttbg, bg, c.GetBoxedOut(), charset, buf);
            };

            charBm.DrawText(0, cache_Vshift, buf, fg, 0, font, 0, 0);

            if (m_debugmask & DEBUG_MASK_OT_DCHR) {
                // draw a bunch of markers into bitmap
                tColor color = GetColorRGB(ttcRed,0);
                if(((x % 2 != 0) && ((y % 2) == 0)) || ((x % 2 == 0) && ((y % 2) != 0)))
                    color = GetColorRGB(ttcBlue,0);
                for (int i = 0; i < w; i++) {
                    charBm.DrawPixel(i , 0    , color); // horizontal top
                    charBm.DrawPixel(i , h - 1, color); // horizontal bottom
                    if ((h > 2) && ((i % 5) == 0)) {
                        // mark at 5
                        charBm.DrawPixel(i , 0 + 1, color); // horizontal top
                        charBm.DrawPixel(i , h - 2, color); // horizontal bottom
                    };
                    if ((h > 4) && ((i % 10) == 0)) {
                        // mark at 10
                        charBm.DrawPixel(i , 0 + 2, color); // horizontal top
                        charBm.DrawPixel(i , 0 + 3, color); // horizontal top
                        charBm.DrawPixel(i , h - 2, color); // horizontal bottom
                        charBm.DrawPixel(i , h - 3, color); // horizontal bottom
                    };
                };
                for (int i = 0; i < h; i++) {
                    charBm.DrawPixel(0     , i, color); // vertical left
                    charBm.DrawPixel(w - 1 , i, color); // vertical right
                    if ((w > 2) && ((i % 5) == 0)) {
                        // mark at 5
                        charBm.DrawPixel(0 + 1 , i, color); // vertical left
                        charBm.DrawPixel(w - 1 , i, color); // vertical right
                    };
                    if ((w > 4) && ((i % 10) == 0)) {
                        // mark at 10
                        charBm.DrawPixel(0 + 2 , i, color); // vertical left
                        charBm.DrawPixel(0 + 3 , i, color); // vertical left
                        charBm.DrawPixel(w - 2 , i, color); // vertical right
                        charBm.DrawPixel(w - 3 , i, color); // vertical right
                    };
                };
            };

            osd->DrawBitmap(vx + leftFrame, vy + topFrame, charBm);
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

void cDisplay::DrawText(int x, int y, const char *text, int len, const enumTeletextColor cText) {
    // Copy text to teletext page
    DEBUG_OT_DTXT("called with x=%d y=%d len=%d text='%s' strlen(text)=%d", x, y, len, text, (int) strlen(text));

    cTeletextChar c;
    c.SetFGColor(cText); // default ttcWhite
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


void cDisplay::ClearPage(void) {
    // Clear Teletext Page on OSD
    cTeletextChar c;
    c.SetFGColor(ttcTransparent); // no char
    c.SetBGColor(ttcBlack); // pass selected background
    c.SetChar(' ');

    // reset 40x24 area with space
    for (int y = 0; y < 24; y++)
        for (int x = 0; x < 40; x++)
            SetChar(x, y, c);

    return;
};


void cDisplay::DrawPageId(const char *text, const enumTeletextColor cText, const bool boxedAlwaysOn) {
    // Draw Page ID string to OSD
    // In case of "Boxed" page: only until 1st page update unless "boxedAlwaysOn" is set
    // "boxedAlwaysOn" also unhides the 1st line completly
    static char text_last[9] = ""; // remember
    static bool paused_last = false;
    cTeletextChar c;

    DEBUG_OT_DRPI("called with text='%s' text_last='%s' Boxed=%d HasConceal=%d GetConceal=%d boxedAlwaysOn=%s", text, text_last, Boxed, HasConceal(), GetConceal(), BOOLTOTEXT(boxedAlwaysOn));

    if ((! GetPaused()) && Boxed && (! boxedAlwaysOn) && PageIdDisplayedEver && (strcmp(text, text_last) == 0)) {
        // don't draw PageId a 2nd time on boxed pages
        for (int i = 0; i < 8; i++) {
            c.SetFGColor(ttcTransparent);
            c.SetBGColor(ttcTransparent);
            c.SetChar(0);
            SetChar(i,0,c);
        };
        return;
    };

    if (Boxed && boxedAlwaysOn) {
        for (int x = 8; x < 40; x++) {
            // de-boxing of 1st line in case OSD is not matching live channel
            c = GetChar(x, 0);
            c.SetBoxedOut(false);
            SetChar(x, 0, c);
        };
    };

    DrawText(0,0,text,8, cText);
    strncpy(text_last, text, sizeof(text_last) - 1);
    PageIdDisplayedEver = true;

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
        c.SetChar('!');
        DEBUG_OT_DRPI("trigger SetChar for Paused hint ttfg=%x ttbg=%x", c.GetFGColor(), c.GetBGColor());
        SetChar(3, 0, c);
    } else if (paused_last == true) {
        paused_last = false;
        c.SetBGColor(ttcBlack);
        c.SetFGColor(ttcGreen);
        c.SetChar('>');
        DEBUG_OT_DRPI("trigger SetChar for Paused finished hint ttfg=%x ttbg=%x", c.GetFGColor(), c.GetBGColor());
        SetChar(3, 0, c);
    };
}

void cDisplay::DrawHotkey(const char *textRed, const char *textGreen, const char* textYellow, const char *textBlue, const HotkeyFlags flag) {
    if (Boxed) return; // don't draw hotkey on boxed pages

    switch(flag) {
        case HotkeyNormal:
            DrawTextExtended( 0, 24, textRed   , 10, AlignmentCenter, ttcWhite, ttcRed    );
            DrawTextExtended(10, 24, textGreen , 10, AlignmentCenter, ttcBlack, ttcGreen  );
            DrawTextExtended(20, 24, textYellow, 10, AlignmentCenter, ttcBlack, ttcYellow );
            DrawTextExtended(30, 24, textBlue  , 10, AlignmentCenter, ttcWhite, ttcBlue   );
            break;

        case HotkeyYellowValue:
            DrawTextExtended( 0, 24, textRed   , 10, AlignmentCenter, ttcWhite, ttcRed    );
            DrawTextExtended(10, 24, textGreen , 10, AlignmentCenter, ttcBlack, ttcGreen  );
            DrawTextExtended(20, 24, textYellow, 10, AlignmentCenter, ttcWhite, ttcMagenta);
            DrawTextExtended(30, 24, textBlue  , 10, AlignmentCenter, ttcWhite, ttcBlue   );
            break;

        case HotkeyGreenYellowValue:
            DrawTextExtended( 0, 24, textRed   , 10, AlignmentCenter, ttcWhite, ttcRed    );
            DrawTextExtended(10, 24, textGreen , 20, AlignmentCenter, ttcWhite, ttcMagenta);
            DrawTextExtended(30, 24, textBlue  , 10, AlignmentCenter, ttcWhite, ttcBlue   );
            break;
   };
}

void cDisplay::DrawHints(const char *textH1, const char *textH2, const char* textH3, const char *textH4, const char *textH5, const HintsFlags flag) {
    if (Boxed) return; // don't draw hints on boxed pages

    switch(flag) {
        case HintsKey:
            DrawTextExtended( 0, 25, textH1, 8, AlignmentCenter, ttcHalfCyan, ttcGrey   );
            DrawTextExtended( 8, 25, textH2, 8, AlignmentCenter, ttcHalfCyan, ttcColor25);
            DrawTextExtended(16, 25, textH3, 8, AlignmentCenter, ttcHalfCyan, ttcGrey   );
            DrawTextExtended(24, 25, textH4, 8, AlignmentCenter, ttcHalfCyan, ttcColor25);
            DrawTextExtended(32, 25, textH5, 8, AlignmentCenter, ttcHalfCyan, ttcGrey   );
            break;

        case HintsValue:
            DrawTextExtended( 0, 26, textH1, 8, AlignmentCenter, ttcColor25 , ttcGrey   );
            DrawTextExtended( 8, 26, textH2, 8, AlignmentCenter, ttcColor31 , ttcColor25);
            DrawTextExtended(16, 26, textH3, 8, AlignmentCenter, ttcColor25 , ttcGrey   );
            DrawTextExtended(24, 26, textH4, 8, AlignmentCenter, ttcColor31 , ttcColor25);
            DrawTextExtended(32, 26, textH5, 8, AlignmentCenter, ttcColor25 , ttcGrey   );
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

void cDisplay::DrawMessage(const char *txt1, const char *txt2, const enumTeletextColor cFrame, const enumTeletextColor cText, const enumTeletextColor cBackground) {
    int border=6; // minimum
    if (outputWidth > 720) {
        // increase border
        border = ((border * outputWidth) / 720) & 0xfffe; // always even number
    };
    if (outputWidth > 1280) {
        // select larger font
        MessageFont = cFont::GetFont(fontOsd);
    };

    if (!osd) return;

    HoldFlush();
    // Hold flush until done

    ClearMessage();
    // Make sure old message is gone

    if (IsDirty()) DrawDisplay();
    // Make sure all characters are out, so we can draw on top

    // text w/h
    int w1 = MessageFont->Width(txt1);
    int h1 = MessageFont->Height(txt1);
    int w2 = 0;
    int h2 = 0;

    // remember for later
    int w1_orig = w1;
    int w2_orig = w2;

    // box w/h
    int w = w1;
    int h = h1;

    // text offset
    int o1 = 0;
    int o2 = 0;

    if (txt2 != NULL) {
        // 2nd line active
        w2 = MessageFont->Width(txt2);
        h2 = MessageFont->Height(txt2);

        h += h2 + border / 2; // increase height

        if (w2 > w1) {
            // 2nd line is longer
            w = w2;
            o1 = (w2 - w1) / 2;
        } else if (w2 < w1) {
            // 1st line is longer
            o2 = (w1 - w2) / 2;
        };
    }

    w += 4 * border;
    h += 4 * border;

    // limit to maximum
    if (w > outputWidth)  w = outputWidth;
    if (h > outputHeight) h = outputHeight;

    // center box
    int x = (outputWidth -w)/2 + leftFrame;
    int y = (outputHeight-h)/2 + topFrame;

    // Get local color mapping
    tColor fg=GetColorRGB(cText,0);
    tColor bg=GetColorRGB(cBackground,0);
    tColor fr=GetColorRGB(cFrame,0);
    if (fg==bg) bg=GetColorRGBAlternate(cBackground,0);

    // Draw framed box (2 outer pixel always background)
    osd->DrawRectangle(x           , y           , x+w-1         , y+h-1         , bg); // outer rectangle
    osd->DrawRectangle(x+(border/2), y+(border/2), x+w-1-border/2, y+h-1-border/2, fr); // inner rectangle
    osd->DrawRectangle(x+border    , y+border    , x+w-1-border  , y+h-1-border  , bg); // background for text

    // Remember box
    MessageW = w;
    MessageH = h;
    MessageX = x;
    MessageY = y;

    // limit width
    if ((w - 4 * border) < w1) {
        w1 = w - 4 * border;
        wsyslog_ot("text too long for box, apply width limit (%d->%d) for txt1='%s'", w1_orig, w1, txt1);
    };
    if (txt2 != NULL) {
        if ((w - 4 * border) < w2) {
            w2 = w - 4 * border;
            wsyslog_ot("text too long for box, apply width limit (%d->%d) for txt2='%s'", w2_orig, w2, txt2);
        };
    };

    // Draw text
    if (txt2 == NULL) {
        osd->DrawText(x + 2 * border + o1, y + 2 * border, txt1, fg, bg, MessageFont, w1, h1);
        DEBUG_OT_MSG("MX=%d MY=%d MW=%d MH=%d OW=%d OH=%d w1=%d h1=%d txt1='%s'", MessageX, MessageY, MessageW, MessageH, outputWidth, outputHeight, w1, h1, txt1);
    } else {
        osd->DrawText(x + 2 * border + o1, y + 2 * border                  , txt1, fg, bg, MessageFont, w1, h1);
        osd->DrawText(x + 2 * border + o2, y + 2 * border + h1 + border / 2, txt2, fg, bg, MessageFont, w2, h2);
        DEBUG_OT_MSG("MX=%d MY=%d MW=%d MH=%d OW=%d OH=%d w1=%d h1=%d w2=%d w2=%d txt1='%s' txt2='%s'", MessageX, MessageY, MessageW, MessageH, outputWidth, outputHeight, w1, h1, w2, h2, txt1, txt2);
    };

    // And flush all changes
    ReleaseFlush();
}

void cDisplay::ClearMessage() {
    if (!osd) return;
    if (MessageW==0 || MessageH==0) return;

    // NEW, reverse calculation based on how DrawChar
    // map to character x/y
    int x0 = (MessageX                - leftFrame) / (fontWidth  / 2);
    int y0 = (MessageY                - topFrame ) / (fontHeight / 2);
    int x1 = (MessageX + MessageW - 1 - leftFrame) / (fontWidth  / 2);
    int y1 = (MessageY + MessageH - 1 - topFrame ) / (fontHeight / 2);

    DEBUG_OT_MSG("MX=%d MY=%d MW=%d MH=%d => x0=%d/y0=%d x1=%d/y1=%d", MessageX, MessageY, MessageW, MessageH, x0, y0, x1, y1);

#define TESTOORX(X) (X < 0 || X >= 40)
#define TESTOORY(Y) (Y < 0 || Y >= 25)
    if ( TESTOORX(x0) || TESTOORX(x1) || TESTOORY(y0) || TESTOORY(y1) ) {
        // something out-of-range
	    esyslog("osdteletext: %s out-of-range detected(crop) MessageX=%d MessageY=%d MessageW=%d MessageH=%d => x0=%d%s y0=%d%s x1=%d%s y1=%d%s", __FUNCTION__, MessageX, MessageY, MessageW, MessageH,
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

    HoldFlush();

    // DEBUG_OT_MSG("call MakeDirty with area x0=%d/y0=%d <-> x1=%d/y1=%d", x0, y0, x1, y1);
    for (int x=x0;x<=x1;x++) {
        for (int y=y0;y<=y1;y++) {
            MakeDirty(x,y);
        }
    }

    MessageW=0;
    MessageH=0;

    ReleaseFlush();
}

// vim: ts=4 sw=4 et
