/*************************************************************** -*- c++ -*-
 *                                                                         *
 *   txtrender.c - Teletext display abstraction and teletext code          *
 *                 renderer                                                *
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
#include "txtrender.h"
#include "menu.h"
#include "logging.h"
#include "txtfont.h"

// Font tables

// teletext uses 7-bit numbers to identify a font set.
// There are three font sets involved:
// Primary G0, Secondary G0, and G2 font set.

// Font tables are organized in blocks of 8 fonts:

enumCharsets FontBlockG0_0000[8] = {
    CHARSET_LATIN_G0_EN,
    CHARSET_LATIN_G0_DE,
    CHARSET_LATIN_G0_SV_FI,
    CHARSET_LATIN_G0_IT,
    CHARSET_LATIN_G0_FR,
    CHARSET_LATIN_G0_PT_ES,
    CHARSET_LATIN_G0_CZ_SK,
    CHARSET_LATIN_G0
};

enumCharsets FontBlockG2Latin[8]={
    CHARSET_LATIN_G2,
    CHARSET_LATIN_G2,
    CHARSET_LATIN_G2,
    CHARSET_LATIN_G2,
    CHARSET_LATIN_G2,
    CHARSET_LATIN_G2,
    CHARSET_LATIN_G2,
    CHARSET_LATIN_G2
};

enumCharsets FontBlockG0_0001[8] = {
    CHARSET_LATIN_G0_PL,
    CHARSET_LATIN_G0_DE,
    CHARSET_LATIN_G0_SV_FI,
    CHARSET_LATIN_G0_IT,
    CHARSET_LATIN_G0_FR,
    CHARSET_LATIN_G0,
    CHARSET_LATIN_G0_CZ_SK,
    CHARSET_LATIN_G0
};

enumCharsets FontBlockG0_0010[8] = {
    CHARSET_LATIN_G0_EN,
    CHARSET_LATIN_G0_DE,
    CHARSET_LATIN_G0_SV_FI,
    CHARSET_LATIN_G0_IT,
    CHARSET_LATIN_G0_FR,
    CHARSET_LATIN_G0_PT_ES,
    CHARSET_LATIN_G0_TR,
    CHARSET_LATIN_G0
};


enumCharsets FontBlockG0_0011[8] = {
    CHARSET_LATIN_G0,
    CHARSET_LATIN_G0,
    CHARSET_LATIN_G0,
    CHARSET_LATIN_G0,
    CHARSET_LATIN_G0,
    CHARSET_LATIN_G0_SR_HR_SL,
    CHARSET_LATIN_G0,
    CHARSET_LATIN_G0_RO
};

enumCharsets FontBlockG0_0100[8] = {
    CHARSET_CYRILLIC_G0_SR_HR,
    CHARSET_LATIN_G0_DE,
    CHARSET_LATIN_G0_EE,
    CHARSET_LATIN_G0_LV_LT,
    CHARSET_CYRILLIC_G0_RU_BG,
    CHARSET_CYRILLIC_G0_UK,
    CHARSET_LATIN_G0_CZ_SK,
    CHARSET_INVALID
};

enumCharsets FontBlockG2_0100[8] = {
    CHARSET_CYRILLIC_G2,
    CHARSET_LATIN_G2,
    CHARSET_LATIN_G2,
    CHARSET_LATIN_G2,
    CHARSET_CYRILLIC_G2,
    CHARSET_CYRILLIC_G2,
    CHARSET_LATIN_G2,
    CHARSET_INVALID
};

enumCharsets FontBlockG0_0110[8] = {
    CHARSET_INVALID,
    CHARSET_INVALID,
    CHARSET_INVALID,
    CHARSET_INVALID,
    CHARSET_INVALID,
    CHARSET_INVALID,
    CHARSET_LATIN_G0_TR,
    CHARSET_GREEK_G0
};

enumCharsets FontBlockG2_0110[8] = {
    CHARSET_INVALID,
    CHARSET_INVALID,
    CHARSET_INVALID,
    CHARSET_INVALID,
    CHARSET_INVALID,
    CHARSET_INVALID,
    CHARSET_LATIN_G2,
    CHARSET_GREEK_G2
};

enumCharsets FontBlockG0_1000[8] = {
    CHARSET_LATIN_G0_EN,
    CHARSET_INVALID,
    CHARSET_INVALID,
    CHARSET_INVALID,
    CHARSET_LATIN_G0_FR,
    CHARSET_INVALID,
    CHARSET_INVALID,
    CHARSET_ARABIC_G0
};

enumCharsets FontBlockG2_1000[8] = {
    CHARSET_ARABIC_G2,
    CHARSET_INVALID,
    CHARSET_INVALID,
    CHARSET_INVALID,
    CHARSET_ARABIC_G2,
    CHARSET_INVALID,
    CHARSET_INVALID,
    CHARSET_ARABIC_G2
};

enumCharsets FontBlockG0_1010[8] = {
    CHARSET_INVALID,
    CHARSET_INVALID,
    CHARSET_INVALID,
    CHARSET_INVALID,
    CHARSET_INVALID,
    CHARSET_HEBREW_G0,
    CHARSET_INVALID,
    CHARSET_ARABIC_G0,
};

enumCharsets FontBlockG2_1010[8] = {
    CHARSET_INVALID,
    CHARSET_INVALID,
    CHARSET_INVALID,
    CHARSET_INVALID,
    CHARSET_INVALID,
    CHARSET_ARABIC_G2,
    CHARSET_INVALID,
    CHARSET_ARABIC_G2,
};

enumCharsets FontBlockInvalid[8] = {
    CHARSET_INVALID,
    CHARSET_INVALID,
    CHARSET_INVALID,
    CHARSET_INVALID,
    CHARSET_INVALID,
    CHARSET_INVALID,
    CHARSET_INVALID,
    CHARSET_INVALID
};



// The actual font table definition:
// Split the 7-bit number into upper 4 and lower 3 bits,
// use upper 4 bits for outer array,
// use lower 3 bits for inner array

struct structFontBlock {
    enumCharsets *G0Block;
    enumCharsets *G2Block;
};
    
structFontBlock FontTable[16] = {
    { FontBlockG0_0000, FontBlockG2Latin }, // 0000 block
    { FontBlockG0_0001, FontBlockG2Latin }, // 0001 block
    { FontBlockG0_0010, FontBlockG2Latin }, // 0010 block
    { FontBlockG0_0011, FontBlockG2Latin }, // 0011 block
    { FontBlockG0_0100, FontBlockG2_0100 }, // 0100 block
    { FontBlockInvalid, FontBlockInvalid }, // 0101 block
    { FontBlockG0_0110, FontBlockG2_0110 }, // 0110 block
    { FontBlockInvalid, FontBlockInvalid }, // 0111 block
    { FontBlockG0_1000, FontBlockG2_1000 }, // 1000 block
    { FontBlockInvalid, FontBlockInvalid }, // 1001 block
    { FontBlockG0_1010, FontBlockG2_1010 }, // 1010 block
    { FontBlockInvalid, FontBlockInvalid }, // 1011 block
    { FontBlockInvalid, FontBlockInvalid }, // 1100 block
    { FontBlockInvalid, FontBlockInvalid }, // 1101 block
    { FontBlockInvalid, FontBlockInvalid }, // 1110 block
    { FontBlockInvalid, FontBlockInvalid }  // 1111 block
};

inline enumCharsets GetG0Charset(int codepage) {
    return FontTable[codepage>>3].G0Block[codepage&7];
}
inline enumCharsets GetG2Charset(int codepage) {
    return FontTable[codepage>>3].G2Block[codepage&7];
}

    
cRenderPage::cRenderPage()
    : Dirty(false), DirtyAll(false),
      // Todo: make this configurable
      FirstG0CodePage(ttSetup.txtG0Block<<3), SecondG0CodePage(ttSetup.txtG2Block<<3)
{
}

enum enumSizeMode {
    // Possible size modifications of characters
    sizeNormal,
    sizeDoubleWidth,
    sizeDoubleHeight,
    sizeDoubleSize
};

// Debug only: List of teletext spacing code short names
const char *(names[0x20])={
    "AlBk","AlRd","AlGr","AlYl","AlBl","AlMg","AlCy","AlWh",
    "Flsh","Stdy","EnBx","StBx","SzNo","SzDh","SzDw","SzDs",
    "MoBk","MoRd","MoGr","MoYl","MoBl","MoMg","MoCy","MoWh",
    "Conc","GrCn","GrSp","ESC", "BkBl","StBk","HoMo","ReMo"};

void cRenderPage::ReadTeletextHeader(unsigned char *Header) {
    // Format of buffer, see also structure TelePageData in storage.h
    // Header: 12 bytes (0-11)
    //   0     String "VTXV5"
    //   5     always 0x01
    //   6     magazine number
    //   7     page number
    //   8     flags
    //   9     lang
    //   10    always 0x00
    //   11    always 0x00
    // Teletext base data starting from byte 12
    //   12    teletext data, 25x40 bytes
    // VTXV5 extension
    //   X25 ( 1x40)
    //   X26 (16x40)
    //   X27 (16x40)
    //   X28 (16x40)
    //   M29 (16x40)
    // Format of flags:
    //   0x80  C4 - Erase page
    //   0x40  C5 - News flash
    //   0x20  C6 - Subtitle
    //   0x10  C7 - Suppress Header
    //   0x08  C8 - Update
    //   0x04  C9 - Interrupt Sequence
    //   0x02  C9 (Bug?)
    //   0x01  C11 - Magazine Serial mode

    Flags=Header[8];
    Lang=Header[9];
}


void cRenderPage::RenderTeletextCode(unsigned char *PageCode) {
    int x,y;
    bool EmptyNextLine=false; // Skip one line, in case double height chars were/will be used

    // Get code pages:
    int LocalG0CodePage=(FirstG0CodePage & 0x78) 
            | ((Lang & 0x04)>>2) | (Lang & 0x02) | ((Lang & 0x01)<<2);
        
    enumCharsets FirstG0=GetG0Charset(LocalG0CodePage);
    enumCharsets SecondG0=GetG0Charset(SecondG0CodePage);
    // Reserved for later use:
    // enumCharsets FirstG2=GetG2Charset(LocalG0CodePage);

    for (y=0;y<24;(EmptyNextLine?y+=2:y++)) {
        // Start of line: Set start of line defaults
        
        if (m_debugmask & DEBUG_MASK_OT_TXTRD) {
            printf("y=%02d ", y);
        };

        // Hold Mosaics mode: Remember last mosaic char/charset 
        // for next spacing code
        bool HoldMosaics=false;
        unsigned char HoldMosaicChar=' ';
        enumCharsets HoldMosaicCharset=FirstG0;

        enumSizeMode Size=sizeNormal;
        // Font size modification
        bool SecondCharset=false;
        // Use primary or secondary G0 charset
        bool GraphicCharset=false;
        // Graphics charset used?
        bool SeparateGraphics=false;
        // Use separated vs. contiguous graphics charset
        bool NoNextChar=false;
        // Skip display of next char, for double-width
        EmptyNextLine=false;
        // Skip next line, for double-height

        cTeletextChar c;
        // auto.initialized to everything off
        c.SetFGColor(ttcWhite);
        c.SetBGColor(ttcBlack);
        c.SetCharset(FirstG0);
        
        if (y==0 && (Flags&0x10)) {
            DEBUG_OT_BOXED("set c.SetBoxed(true) Flags=%02x x=%d y=%d", Flags, x, y);
            c.SetBoxedOut(true);    
        }
        if (Flags&0x60) {
            DEBUG_OT_BOXED("set c.SetBoxed(true) Flags=%02x x=%d y=%d", Flags, x, y);
            c.SetBoxedOut(true);    
        }

        // Pre-scan for double-height and double-size codes
        for (x=0;x<40;x++) {
            if (y==0 && x<8) x=8;
            if ((PageCode[x+40*y] & 0x7f)==0x0D || (PageCode[x+40*y] & 0x7f)==0x0F)
                EmptyNextLine=true;
        }

        // Move through line
        for (x=0;x<40;x++) {
            unsigned char ttc=PageCode[x+40*y];

            if (y==0 && x<8) continue;
            // no displayable data here...

            if (m_debugmask & DEBUG_MASK_OT_TXTRD) {
                // Debug only: Output line data and spacing codes
                if (ttc<0x20)
                    printf("%s(%02x) ",names[ttc], ttc);
                else
                    printf("%02x ",ttc);
                if (x==39) printf("\n");
            };
            
            // Handle all 'Set-At' spacing codes
            switch (ttc) {
            case 0x09: // Steady
                DEBUG_OT_BLINK("set bc.SetBlink(false) ttc=%d x=%d y=%d", ttc, x, y);
                c.SetBlink(false);
                break;
            case 0x0C: // Normal Size
                if (Size!=sizeNormal) {
                    Size=sizeNormal;
                    HoldMosaicChar=' ';
                    HoldMosaicCharset=FirstG0;
                }                   
                break;
            case 0x18: // Conceal
                c.SetConceal(true);
                break;
            case 0x19: // Contiguous Mosaic Graphics
                SeparateGraphics=false;
                if (GraphicCharset)
                    c.SetCharset(CHARSET_GRAPHICS_G1);
                break;
            case 0x1A: // Separated Mosaic Graphics
                SeparateGraphics=true;
                if (GraphicCharset)
                    c.SetCharset(CHARSET_GRAPHICS_G1_SEP);
                break;
            case 0x1C: // Black Background
                c.SetBGColor(ttcBlack);
                break;
            case 0x1D: // New Background
                c.SetBGColor(c.GetFGColor());
                break;
            case 0x1E: // Hold Mosaic
                HoldMosaics=true;               
                break;
            }

            // temporary copy of character data:
            cTeletextChar c2=c;
            // c2 will be text character or space character or hold mosaic
            // c2 may also have temporary flags or charsets
            
            if (ttc<0x20) {
                // Spacing code, display space or hold mosaic
                if (HoldMosaics) {
                    c2.SetChar(HoldMosaicChar);
                    c2.SetCharset(HoldMosaicCharset);
                } else {
                    c2.SetChar(' ');
                }
            } else {
                // Character code               
                c2.SetChar(ttc);
                if (GraphicCharset) {
                    if (ttc&0x20) {
                        // real graphics code, remember for HoldMosaics
                        HoldMosaicChar=ttc;
                        HoldMosaicCharset=c.GetCharset();
                    } else {
                        // invalid code, pass-through to G0
                        c2.SetCharset(SecondCharset?SecondG0:FirstG0);
                    }   
                }
            }
            
            // Handle double-height and double-width extremes
            if (y>=23) {
                if (Size==sizeDoubleHeight) Size=sizeNormal;
                if (Size==sizeDoubleSize) Size=sizeDoubleWidth;
            }
            if (x>=38) {
                if (Size==sizeDoubleWidth) Size=sizeNormal;
                if (Size==sizeDoubleSize) Size=sizeDoubleHeight;
            }
            
            // Now set character code
            
            if (NoNextChar) {
                // Suppress this char due to double width last char
                NoNextChar=false;
            } else {
                switch (Size) {
                case sizeNormal:
                    // Normal sized
                    SetChar(x,y,c2);
                    if (EmptyNextLine && y<23) {
                        // Clean up next line
                        SetChar(x,y+1,c2.ToChar(' ').ToCharset(FirstG0));
                    }
                    break;
                case sizeDoubleWidth:
                    // Double width
                    SetChar(x,y,c2.ToDblWidth(dblw_Left));
                    SetChar(x+1,y,c2.ToDblWidth(dblw_Right));
                    if (EmptyNextLine && y<23) {
                        // Clean up next line
                        SetChar(x  ,y+1,c2.ToChar(' ').ToCharset(FirstG0));
                        SetChar(x+1,y+1,c2.ToChar(' ').ToCharset(FirstG0));
                    }
                    NoNextChar=true;
                    break;
                case sizeDoubleHeight:
                    // Double height
                    SetChar(x,y,c2.ToDblHeight(dblh_Top));
                    SetChar(x,y+1,c2.ToDblHeight(dblh_Bottom));
                    break;
                case sizeDoubleSize:
                    // Double Size
                    SetChar(x  ,  y,c2.ToDblHeight(dblh_Top   ).ToDblWidth(dblw_Left ));
                    SetChar(x+1,  y,c2.ToDblHeight(dblh_Top   ).ToDblWidth(dblw_Right));
                    SetChar(x  ,y+1,c2.ToDblHeight(dblh_Bottom).ToDblWidth(dblw_Left ));
                    SetChar(x+1,y+1,c2.ToDblHeight(dblh_Bottom).ToDblWidth(dblw_Right));
                    NoNextChar=true;
                    break;
                }
            }
                
            // Handle all 'Set-After' spacing codes
            switch (ttc) {
            case 0x00 ... 0x07: // Set FG color
                if (GraphicCharset) {
                    // Actual switch from graphics charset
                    HoldMosaicChar=' ';
                    HoldMosaicCharset=FirstG0;
                }
                c.SetFGColor((enumTeletextColor)ttc);
                c.SetCharset(SecondCharset?SecondG0:FirstG0);
                GraphicCharset=false;
                c.SetConceal(false);
                break;
            case 0x08: // Flash
                DEBUG_OT_BLINK("set c.SetBlink(true) ttc=%02x x=%d y=%d", ttc, x, y);
                c.SetBlink(true);
                break;
            case 0x0A: // End Box
                DEBUG_OT_BOXED("set c.SetBoxed(true)  End Box  ttc=%02x x=%d y=%d", ttc, x, y);
                c.SetBoxedOut(true);
                break;
            case 0x0B: // Start Box
                DEBUG_OT_BOXED("set c.SetBoxed(false) StartBox ttc=%02x x=%d y=%d", ttc, x, y);
                c.SetBoxedOut(false);
                break;
            case 0x0D: // Double Height
                if (Size!=sizeDoubleHeight) {
                    Size=sizeDoubleHeight;
                    HoldMosaicChar=' ';
                    HoldMosaicCharset=FirstG0;
                }                   
                break;
            case 0x0E: // Double Width
                if (Size!=sizeDoubleWidth) {
                    Size=sizeDoubleWidth;
                    HoldMosaicChar=' ';
                    HoldMosaicCharset=FirstG0;
                }                   
                break;
            case 0x0F: // Double Size
                if (Size!=sizeDoubleSize) {
                    Size=sizeDoubleSize;
                    HoldMosaicChar=' ';
                    HoldMosaicCharset=FirstG0;
                }                   
                break;
            case 0x10 ... 0x17: // Mosaic FG Color
                if (!GraphicCharset) {
                    // Actual switch to graphics charset
                    HoldMosaicChar=' ';
                    HoldMosaicCharset=FirstG0;
                }
                c.SetFGColor((enumTeletextColor)(ttc-0x10));
                c.SetCharset(SeparateGraphics?CHARSET_GRAPHICS_G1_SEP:CHARSET_GRAPHICS_G1);
                GraphicCharset=true;
                c.SetConceal(false);
                break;
            case 0x1B: // ESC Switch
                SecondCharset=!SecondCharset;
                if (!GraphicCharset) c.SetCharset(SecondCharset?SecondG0:FirstG0);
                break;
            case 0x1F: // Release Mosaic
                HoldMosaics=false;
                break;
            }
        } // end for x
    } // end for y
    
    for (x=0;x<40;x++) {
        // Clean out last line
        cTeletextChar c;
        c.SetFGColor(ttcWhite);
        c.SetBGColor(ttcBlack);
        c.SetCharset(FirstG0);
        c.SetChar(' ');
        if (Flags&0x60) {
            DEBUG_OT_BOXED("set c.SetBoxed(true) Flags=%02x ttc=(space) x=%d y=%d", Flags, x, y);
            c.SetBoxedOut(true);    
        }
        SetChar(x,24,c);
    }

    /* VTXV5 handling starts here */

    DEBUG_OT_TXTRDT("start X/26 handling"); /* X/26 */
    unsigned char* PageCode_X26 = PageCode + 25*40 + 40; // X/1-24 + X/25
    for (int row = 0; row <= 15; row++) {
        // convert X/26/0-15 into triplets
        if (PageCode_X26[row*40] == 0) {
            // row empty
            continue;
        } else if (PageCode_X26[row*40] & 0x80 != 0x80) {
            DEBUG_OT_TXTRDT("invalid X/26 row (DesignationCode flag not valid)");
            continue;
        };
        for (int triplet = 0; triplet < 13; triplet++) {
            uint8_t addr = PageCode_X26[row*40 + 1 + triplet*3];
            uint8_t mode = PageCode_X26[row*40 + 2 + triplet*3];
            uint8_t data = PageCode_X26[row*40 + 3 + triplet*3];

            int found = 0;
            const char* info;

            if ((mode == 0x04) && (addr >= 40) && (addr <= 63)) {
                // 0x04 = 0b00100
                // "Set Active Position"
                found = 1;
                if (addr == 40) {
                    y = 24;
                } else {
                    y = addr - 40;
                };
                x = data;
                DEBUG_OT_TXTRDT("X/26 triplet found: row=%d triplet=%d SetActivePosition y=%d x=%d\n", row, triplet, y, x);
            } else if ((mode == 0x04) && (addr >= 0) && (addr <= 39)) {
                // 0x04 = 0b00100
                // RESERVED
                found = 1;
            } else if ((mode == 0x1f) && (addr == 0x3f)) {
                // 0x1f =  0b11111
                // "Termination Marker"
                found = 1;
                if (m_debugmask & DEBUG_MASK_OT_TXTRDT) {
                    switch(data & 0x07) {
                        case 0x00: // 0b000
                            info = "Intermediate (G)POP sub-page. End of object, more objects follow on this page.";
                            break;
                        case 0x01: // 0b001
                            info = "Intermediate (G)POP sub-page. End of last object on this page.";
                            break;
                        case 0x02: // 0b010
                            info = "Last (G)POP sub-page. End of object, more objects follow on this page.";
                            break;
                        case 0x03: // 0b011
                            info = "Last (G)POP sub-page. End of last object on this page.";
                            break;
                        case 0x04: // 0b100
                            info = "Local Object definitions. End of object, more objects follow on this page.";
                            break;
                        case 0x05: // 0b101
                            info = "Local Object definitions. End of last object on this page.";
                            break;
                        case 0x06: // 0b110
                            info = "Local enhancement data. End of enhancement data, Local Object definitions follow.";
                            break;
                        case 0x07: // 0b111
                            info = "Local enhancement data. End of enhancement data, no Local Object definitions follow.";
                            break;
                    };
                    DEBUG_OT_TXTRDT("X/26 triplet found: row=%d triplet=%d TerminationMarker: %s\n", row, triplet, info);
                };
            } else if (((mode & 0x10) == 0x10) && (addr >= 0) && (addr <= 39)) {
                // 0x1x =  0b1xxxx
                // Characters Including Diacritical Marks
                x = addr;
                cTeletextChar c = GetChar(x, y);
                if (mode == 0x1000) {
                    info = "character without diacritical mark";
                    // No diacritical mark exists for mode description value 10000. An unmodified G0 character is then displayed unless the 7 bits of the data field have the value 0101010 (2/A) when the symbol "@" shall be displayed.
                    if (data == 0x2a) {
                        // set char to '@'
                        c.SetChar(0x80);
                    } else {
                        c.SetChar(data);
                    };
                    DEBUG_OT_TXTRDT("X/26 triplet found: row=%d triplet=%d: %s\n", row, triplet, info);
                    found = 1;
                } else {
                    info = "G0 character with diacritical mark";
                    found = 1;
                    uint8_t mark = mode & 0x0f;
                    DEBUG_OT_TXTRDT("X/26 triplet found: row=%d triplet=%d: %s x=%d mark=%d data=0x%02x\n", row, triplet, info, x, mark, data);
                    if (data >= 0x20) {
                        DEBUG_OT_TXTRDT("X/26 triplet exec : y=%02d x=%02d change Char=0x%02x Charset=0x%04x mark=%x => Char=0x%02x Charset=0x%04x\n", y, x, c.GetChar(), c.GetCharset(), mark, X26_G0_CharWithDiacritcalMarkMapping(data, mark), CHARSET_LATIN_G0);
                        c.SetCharset(CHARSET_LATIN_G0);
                        c.SetChar(X26_G0_CharWithDiacritcalMarkMapping(data, mark));
                    } else {
                        // ignore: Data field values < 20 hex are reserved but decoders should still set the column co-ordinate of the Active Position to the value of the address field.
                    };
                };
                SetChar(x, y, c);
            };

            if (found == 0)
                DEBUG_OT_TXTRDT("X/26 triplet found: row=%d triplet=%d UNSUPPORTED addr=0x%02x mode=0x%02x data=0x%02x\n", row, triplet, addr, mode, data);
        };
    };
}

// vim: ts=4 sw=4 et
