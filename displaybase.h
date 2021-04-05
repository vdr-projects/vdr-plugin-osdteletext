/*************************************************************** -*- c++ -*-
 *       Copyright (c) 2005      by Udo Richter                            *
 *       Copyright (c) 2021      by Peter Bieringer (extenions)            *
 *                                                                         *
 *   displaybase.h - Base class for rendering a teletext cRenderPage to    *
 *                   an actual VDR OSD.                                    *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef OSDTELETEXT_DISPLAYBASE_H_
#define OSDTELETEXT_DISPLAYBASE_H_

#include "txtrender.h"
#include "setup.h"
#include <vdr/osd.h>
#include <string>

// #define dsyslog_osdteletext(format, arg...)	dsyslog(format, ## arg)
#define dsyslog_osdteletext(format, arg...)	{ }

class cDisplay : public cRenderPage {
    // Class that extends the virtual cRenderPage with the capability
    // to render its contents to an OSD of variable size.
    // Renders incrementally - just changes
    // plus adds some more display features like message display.

public:
    enum enumZoom {
        // Zoom up upper/lower half of page
        Zoom_Off,
        Zoom_Upper,
        Zoom_Lower
    } Zoom;

    enum enumAlignment { AlignmentLeft, AlignmentCenter, AlignmentRight } Alignment;

protected:
    bool Concealed;
    // Hidden text internal state

    bool Blinked;
    // Blinking text internal state

    int FlushLock;
    // Lock counter for bundeling OSD flushes

    bool Boxed;
    // Page is 'boxed mode' transparent

    int Width;
    int Height;
    // OSD pixel dimension

    tColor Background;
    // Color to be used for black background
    // - allow transparency

    bool Paused;
    // Paused internal state

    cOsd *osd;
    // The osd object. If creation fails, may be NULL

    int outputWidth;
    int outputHeight;
    // for 32bpp true color, If creation fails, may be NULL

    int leftFrame, rightFrame, topFrame, bottomFrame;
    // frame border

    int OffsetX,OffsetY;
    // Virtual coordinate system, see InitScaler

    const cFont *MessageFont;
    int MessageX,MessageY,MessageW,MessageH;

    const cFont *TXTFont;
    const cFont *TXTDblWFont;
    const cFont *TXTDblHFont;
    const cFont *TXTDblHWFont;
    const cFont *TXTHlfHFont;
    int fontHeight;
    int fontWidth;

    static int realFontWidths[4];


    class cBox {
        // helper class. Represents a character's box in virtual coordinates
    public:
        int XMin,YMin,XMax,YMax;
        inline void SetToCharacter(int x, int y);
    };
    friend class cBox;

    class cVirtualCoordinate {
        // helper class. Represents a coordinate in virtual display space
        // and in OSD pixel coordinates.
    public:
        int OsdX,OsdY;
        int VirtX,VirtY;
        inline void VirtualToPixel(cDisplay *Display, int x, int y);
        inline void IncPixelX(cDisplay *Display);
        inline void IncPixelY(cDisplay *Display);
    };
    friend class cVirtualCoordinate;

public:
    cDisplay(int width, int height);
    virtual ~cDisplay();
    bool Valid() { return (osd!=NULL); }
    // After creation, check for Valid(). Destroy, if not valid.

    void setOutputWidth(int w) { outputWidth = w; };
    void setOutputHeight(int h) { outputHeight = h; };
    void setLeftFrame  (int lF) { leftFrame = lF;   };
    void setRightFrame (int rF) { rightFrame = rF;  };
    void setTopFrame   (int tF) { topFrame = tF;    };
    void setBottomFrame(int bF) { bottomFrame = bF; };

    static std::string GFXFontFootprint;
    static std::string TXTFontFootprint;

protected:
    void InitScaler();
    // Initialize transformation for OSD->Virtual coordinates
    // Some words about scaling:

    // OSD display is variable width x height, with 3 pixels border
    // on all sides. There is a virtual coordinate system projected
    // on this, with (3,3) mapped to (0,0) and (width-3,height-3)
    // mapped to (480<<16,250<<16).
    // The idea is, that each font pixel uses a virtual rectangle
    // of (1<<16,1<<16) size.

    // ScaleX,ScaleY represent the (virtual) width and height of a
    // physical OSD pixel.
    // OffsetX,OffsetY default to 3,3 to represent the border offset,
    // but may be used differently.

public:
    bool GetBlink() { return Blinked; }
    bool SetBlink(bool blink);
    // Switch blink frequently to get blinking chars
    // Returns true if there are blinking characters.

    bool GetConceal() { return Concealed; }
    bool SetConceal(bool conceal);
    // Hidden text. Set to true to see hidden text.
    // Returns true if there are concealed characters.
    bool HasConceal();
    // Returns true if there are concealed characters.

    void SetPaused(bool paused) { Paused = paused; return; };
    bool GetPaused() { return Paused; };
    // Returns true if paused

    enumZoom GetZoom() { return Zoom; }
    void SetZoom(enumZoom zoom);
    // Zoom to upper/lower half of page

    void SetBackgroundColor(tColor c);
    tColor GetBackgroundColor() { return Background; }
    // Set the background color for black. Allows transparent black.

    // Color mapping interface.
    virtual tColor GetColorRGB(enumTeletextColor ttc, int Area);
    // Map a teletext color to an OSD color in #Area.

    virtual tColor GetColorRGBAlternate(enumTeletextColor ttc, int Area);
    // For color collision:
    // Map this teletext color to an OSD color in #Area, but dont
    // return same as GetColorRGB(). Used to solve conflicts if
    // foreground and background are mapped to same color.
    // Defaults to 1:1 identity. Not needed if all colors actually
    // supported by OSD.

protected:

    void DrawDisplay();
    // Draw all dirty characters from cRenderPage buffer to OSD

    void CleanDisplay();
    // Clean OSD completely

    virtual void DrawChar(int x, int y, cTeletextChar c);
    // Draw a single character to OSD


public:
    void HoldFlush() { FlushLock++; }
    // Hold all OSD flush updates to bundle operations.

    void ReleaseFlush() { FlushLock--; Flush(); }
    // Release hold of flush updates. After last release,
    // the flush will be done

protected:
    void Flush() {
        // Commit all changes from OSD internal bitmaps to device
        // All draw operations inside cDisplay should call it,
        // no one outside should need to call it.
        if (FlushLock>0) return;
        if (!osd) return;
        if (IsDirty()) DrawDisplay();
        osd->Flush();
    }

public:
    void RenderTeletextCode(unsigned char *PageCode);
    // Interprete teletext code referenced by PageCode
    // and draw the whole page content into OSD.
    // PageCode must be a 40*24+12 bytes buffer

    void DrawText(int x, int y, const char *text, int len);
    // Draw some characters in teletext page.
    // Max len chars, fill up with spaces

    void DrawTextExtended(const int x, const int y, const char *text, int len, const enumAlignment alignment, const enumTeletextColor ttcFg, const enumTeletextColor ttcBg);
    // Draw some characters in teletext page.
    // Max len chars, fill up with spaces
    // with alignment, foreground and background color

    void DrawFooter(const char *textRed, const char *textGreen, const char* textYellow, const char *textBlue);
    // Draw footer to OSD

    void DrawClock();
    // Draw current time to OSD

    void DrawPageId(const char *text);
    // Draw Page ID string to OSD

    void DrawMessage(const char *txt);
    // Draw a framed, centered message box to OSD

    void ClearMessage();
    // Remove message box and redraw hidden content

private:
    cFont *GetFont(const char *name, int index, int height, int width);
    std::string GetFontFootprint(const char *name);
};



inline void cDisplay::cBox::SetToCharacter(int x, int y) {
    // Virtual box area of a character
    XMin=(x*12)<<16;
    YMin=(y*10)<<16;
    XMax=XMin+(12<<16)-1;
    YMax=YMin+(10<<16)-1;
}

/*
inline void cDisplay::cVirtualCoordinate::VirtualToPixel(cDisplay *Display, int x, int y) {
    // Map virtual coordinate to OSD pixel
    OsdX=x/Display->ScaleX+Display->OffsetX;
    OsdY=y/Display->ScaleY+Display->OffsetY;

    // map OSD pixel back to virtual coordinate, use center of pixel
    VirtX=(OsdX-Display->OffsetX)*Display->ScaleX+Display->ScaleX/2;
    VirtY=(OsdY-Display->OffsetY)*Display->ScaleY+Display->ScaleY/2;
}

inline void cDisplay::cVirtualCoordinate::IncPixelX(cDisplay *Display) {
    // Move one OSD pixel
    OsdX++;
    VirtX+=Display->ScaleX;
}
inline void cDisplay::cVirtualCoordinate::IncPixelY(cDisplay *Display) {
    // Move one OSD pixel
    OsdY++;
    VirtY+=Display->ScaleY;
}
*/

#endif
