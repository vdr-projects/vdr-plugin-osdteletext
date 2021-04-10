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

const char *st_modes[] =
{
      tr("Zoom"),
      tr("Half page"),
      tr("Change channel"),
      tr("Switch background"),
      //tr("Suspend receiving"),
      tr("Config"),
      tr("24-LineMode"),
      tr("Answer"),
      tr("Pause"),
      tr("Jump to..."),
};

const char *st_modesFooter[] =
{
      // 1:1 relation to *st_modes[] from above and maximum 10 chars used in line25 footer //
      tr("Zoom"),
      tr("Half Page"),
      tr("ChangeChan"),
      tr("SwitchBack"),
      //tr("SuspendRecv"),
      tr("Config"),
      tr("24LineMode"),
      tr("Answer"),
      tr("Pause"),
      tr("Jump to..."),
};

const char *config_modes[] =
{
   // maximum 9 chars, 10th is -/+
   tr("Left"),
   tr("Top"),
   tr("Width"),
   tr("Height"),
   tr("Frame"),
   tr("Text Font"),
   tr("TxVoffset"),
   tr("BackTrans"),
};

// vim: ts=3 sw=3 et
