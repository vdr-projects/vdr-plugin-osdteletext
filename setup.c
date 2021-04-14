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

#include "setup.h"

const char *st_modesFooter[] =
{
      // 1:1 relation between st_modes[] in osdteletext.c + eTeletextAction in setup.h + st_modesFooter in setup.c
      // maximum 10 chars used in line25 footer //
      trNOOP("Zoom"),
      trNOOP("Half Page"),
      trNOOP("ChangeChan"),
      trNOOP("SwitchBack"),
      //trNOOP("SuspendRecv"),
      trNOOP("Config"),
      trNOOP("24LineMode"),
      trNOOP("Answer"),
      trNOOP("Pause"),
      trNOOP("Jump to..."),
};

const char *config_modes[] =
{
   // maximum 9 chars, 10th is -/+
   trNOOP("Left"),
   trNOOP("Top"),
   trNOOP("Width"),
   trNOOP("Height"),
   trNOOP("Frame"),
   trNOOP("Text Font"),
   trNOOP("TxVoffset"),
   trNOOP("BackTrans"),
};

// vim: ts=3 sw=3 et
