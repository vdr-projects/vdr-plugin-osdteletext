
#ifndef LOGGING_H_INCLUDED
#define LOGGING_H_INCLUDED

#include <vdr/tools.h>

extern int m_debugmask;

#define DEBUG_MASK_OT		0x00000001	// OSD_Teletext general
#define DEBUG_MASK_OT_FONT	0x00010000	// OSD Teletext Font
#define DEBUG_MASK_OT_DBFC	0x00040000	// OSD Teletext DisplayBase Function Call

// special action mask
#define DEBUG_MASK_OT_ACT_BACK_RED	0x20000000	// OSD Teletext Background Red (debugging for detecting pixel offset issues)
#define DEBUG_MASK_OT_ACT_LIMIT_LINES	0x10000000	// OSD Teletext Limit Lines (debugging for detecting pixel offset issues)

#define dsyslog_ot(format, arg...) dsyslog("osdteletext: DEBUG %s/%s: " format, __FILE__, __FUNCTION__, ## arg)

#define DEBUG_OT	if (m_debugmask & DEBUG_MASK_OT)        dsyslog_ot
#define DEBUG_OT_FONT	if (m_debugmask & DEBUG_MASK_OT_FONT)   dsyslog_ot
#define DEBUG_OT_DBFC	if (m_debugmask & DEBUG_MASK_OT_DBFC)   dsyslog_ot

#endif
