// -*- c++ -*-

#ifndef __TXTFONT_H
#define __TXTFONT_H

#include "txtrender.h"

unsigned int* GetFontChar(cTeletextChar c, unsigned int *buffer);

unsigned int GetVTXChar(cTeletextChar c);

uint8_t X26_G0_CharWithDiacritcalMarkMapping(uint8_t c, uint8_t mark);

#endif
