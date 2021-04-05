/*************************************************************** -*- c++ -*-
 *       Copyright (c) < 2021    by TODO                                   *
 *       Copyright (c) 2021      by Peter Bieringer (extenions)            *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef __TXTFONT_H
#define __TXTFONT_H

#include "txtrender.h"

unsigned int* GetFontChar(cTeletextChar c, unsigned int *buffer);

unsigned int GetVTXChar(cTeletextChar c);

uint8_t X26_G0_CharWithDiacritcalMarkMapping(uint8_t c, uint8_t mark);

#endif
