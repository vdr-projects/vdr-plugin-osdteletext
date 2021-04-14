/*************************************************************** -*- c++ -*-
 *       Copyright (c) 2005      by Udo Richter                            *
 *       Copyright (c) 2021      by Peter Bieringer (extenions)            *
 *                                                                         *
 *   display.h - Actual implementation of OSD display variants and         *
 *               Display:: namespace that encapsulates a single cDisplay.  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef OSDTELETEXT_DISPLAY_H_
#define OSDTELETEXT_DISPLAY_H_

#include "displaybase.h"
#include <vdr/osd.h>

namespace Display {
    // The Display:: namespace mainly encapsulates a cDisplay *display variable
    // and allows NULL-safe access to display members.
    // Additionally, selects via mode the actually used instance for *display.

    enum Mode { Full, HalfUpper, HalfLower, HalfUpperTop, HalfLowerTop };
    // Full mode: 2BPP or 4BPP full screen display, depending on memory constrains
    // HalfUpper: 4BPP display of upper half, drop lower half if out of memory
    // HalfLower: 4BPP display of lower half, drop upper half if out of memory
    // *Top: display top of screen (default: bottom)

    extern Mode mode;
    extern cDisplay *display;

    // Access to display mode:
    void SetMode(Display::Mode mode, tColor clrBackground = (tColor)ttSetup.configuredClrBackground);
    inline void Delete()
        { if (display) { DELETENULL(display); } }

    void ShowUpperHalf();
    // Make sure the upper half of screen is visible
    // eg. for entering numbers etc.


    // Wrapper calls for various *display members:
    inline bool GetBlink()
        { if (display) return display->GetBlink(); else return false; }
    inline bool SetBlink(bool blink)
        { if (display) return display->SetBlink(blink); else return false; }
    inline bool GetConceal()
        { if (display) return display->GetConceal(); else return false; }
    inline bool SetConceal(bool conceal)
        { if (display) return display->SetConceal(conceal); else return false; }
    inline bool GetPaused()
        { if (display) return display->GetPaused(); else return false; }
    inline void SetPaused(bool paused)
        { if (display) return display->SetPaused(paused); else return; }
    inline bool HasConceal()
        { if (display) return display->HasConceal(); else return false; }
    inline cDisplay::enumZoom GetZoom()
        { if (display) return display->GetZoom(); else return cDisplay::Zoom_Off; }
    inline void SetZoom(cDisplay::enumZoom zoom)
        { if (display) display->SetZoom(zoom); }

    inline void SetBackgroundColor(tColor c)
        { if (display) display->SetBackgroundColor(c); }

    inline tColor GetBackgroundColor()
        { if (display) return display->GetBackgroundColor(); else return 0; }

    inline void HoldFlush()
        { if (display) display->HoldFlush(); }
    inline void ReleaseFlush()
        { if (display) display->ReleaseFlush(); }

    inline void RenderTeletextCode(unsigned char *PageCode)
        { if (display) display->RenderTeletextCode(PageCode); }

    inline void DrawClock()
        { if (display) display->DrawClock(); }

    inline void DrawPageId(const char *text)
        { if (display) display->DrawPageId(text); }
    inline void DrawPageId(const char *text, const enumTeletextColor cText)
        { if (display) display->DrawPageId(text, cText); }
    inline void DrawPageId(const char *text, const enumTeletextColor cText, const bool boxed_always)
        { if (display) display->DrawPageId(text, cText, boxed_always); }

    inline void DrawFooter(const char *textRed, const char *textGreen, const char* textYellow, const char *textBlue, const FooterFlags flag)
        { if (display) display->DrawFooter(textRed, textGreen, textYellow, textBlue, flag); }

    inline void DrawMessage(const char *txt)
        { if (display) display->DrawMessage(txt, NULL); }
    inline void DrawMessage(const char *txt, const char *txt2)
        { if (display) display->DrawMessage(txt, txt2); }

    inline void DrawMessage(const char *txt, const enumTeletextColor cFrame)
        { if (display) display->DrawMessage(txt, NULL, cFrame); }
    inline void DrawMessage(const char *txt, const char *txt2,  const enumTeletextColor cFrame)
        { if (display) display->DrawMessage(txt, txt2, cFrame); }

    inline void DrawMessage(const char *txt, const enumTeletextColor cFrame, const enumTeletextColor cText)
        { if (display) display->DrawMessage(txt, NULL, cFrame, cText); }
    inline void DrawMessage(const char *txt, const char *txt2, const enumTeletextColor cFrame, const enumTeletextColor cText)
        { if (display) display->DrawMessage(txt, txt2, cFrame, cText); }

    inline void DrawMessage(const char *txt, const enumTeletextColor cFrame, const enumTeletextColor cText, const enumTeletextColor cBackground)
        { if (display) display->DrawMessage(txt, NULL, cFrame, cText, cBackground); }
    inline void DrawMessage(const char *txt, const char *txt2, const enumTeletextColor cFrame, const enumTeletextColor cText, const enumTeletextColor cBackground)
        { if (display) display->DrawMessage(txt, txt2, cFrame, cText, cBackground); }

    inline void ClearMessage()
        { if (display) display->ClearMessage(); }
    inline void ClearPage()
        { if (display) display->ClearPage(); }
}


class cDisplay32BPP : public cDisplay {
    // True Color OSD display
    // No need for color mapping
    // Uses cPixmap instead of cBitmap
public:
    cDisplay32BPP(int x0, int y0, int width, int height, int leftFrame, int rightFrame, int topFrame, int bottomFrame, tColor clrBackground);
};



class cDisplay32BPPHalf : public cDisplay {
    int leftFrame, rightFrame, topFrame, bottomFrame;
    // frame border

    // 32BPP (true color) OSD display with auto size reduction on memory constrains
    // Automatically tries to make visible area as big as possible
    // No need for color mapping
    bool Upper;
    // Prefer to show upper half or lower half?
    bool Top;
    // Prefer to show half on top or bottom?

    int OsdX0,OsdY0;
    // Needed to re-initialize osd

public:
    cDisplay32BPPHalf(int x0, int y0, int width, int height, int leftFrame, int rightFrame, int topFrame, int bottomFrame, bool upper, bool top, tColor clrBackground);
    bool GetUpper() { return Upper; }
    void SetUpper(bool upper)
        { if (Upper!=upper) { Upper=upper; } }
    void SetTop(bool top)
        { if (Top!=top) { Top=top; } }
protected:
    void InitOSD();
};


#endif
