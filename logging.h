
#ifndef LOGGING_H_INCLUDED
#define LOGGING_H_INCLUDED

#include <vdr/tools.h>

extern int m_debugmask;

#define DEBUG_MASK_OT		0x00000001	// general
#define DEBUG_MASK_OT_KNONE	0x00000010	// Knone action
#define DEBUG_MASK_OT_KEYS	0x00000020	// Keys action
#define DEBUG_MASK_OT_NEPG	0x00000100	// new cTelePage
#define DEBUG_MASK_OT_COPG	0x00000200	// regular log amount of new cTelePage
#define DEBUG_MASK_OT_DD	0x00001000	// DrawDisplay
#define DEBUG_MASK_OT_MSG	0x00002000	// Draw/Clear Message
#define DEBUG_MASK_OT_DRPI	0x00004000	// DrawPageId
#define DEBUG_MASK_OT_FOOT	0x00008000	// DrawFooter
#define DEBUG_MASK_OT_FONT	0x00010000	// Font
#define DEBUG_MASK_OT_DBFC	0x00040000	// DisplayBase Function Call
#define DEBUG_MASK_OT_BLINK	0x00100000	// Text Blink
#define DEBUG_MASK_OT_DCHR	0x00200000	// DrawChar
#define DEBUG_MASK_OT_DTXTE	0x00400000	// DrawTextExtended
#define DEBUG_MASK_OT_CACHE	0x01000000	// Caching System

// special action mask
#define DEBUG_MASK_OT_ACT_LIMIT_LINES		0x10000000	// Limit Lines (debugging for detecting pixel offset issues)
#define DEBUG_MASK_OT_ACT_OSD_BACK_RED		0x20000000	// OSD Background Red (debugging for detecting pixel offset issues)
#define DEBUG_MASK_OT_ACT_CHAR_BACK_BLUE	0x40000000	// Char Background Blue (debugging for detecting pixel offset issues)

#define dsyslog_ot(format, arg...) dsyslog("osdteletext: DEBUG %s/%s: " format, __FILE__, __FUNCTION__, ## arg)

#define DEBUG_OT	if (m_debugmask & DEBUG_MASK_OT)        dsyslog_ot
#define DEBUG_OT_FONT	if (m_debugmask & DEBUG_MASK_OT_FONT)   dsyslog_ot
#define DEBUG_OT_DBFC	if (m_debugmask & DEBUG_MASK_OT_DBFC)   dsyslog_ot
#define DEBUG_OT_NEPG   if (m_debugmask & DEBUG_MASK_OT_NEPG)   dsyslog_ot
#define DEBUG_OT_COPG   if (m_debugmask & DEBUG_MASK_OT_COPG)   dsyslog_ot
#define DEBUG_OT_BLINK  if (m_debugmask & DEBUG_MASK_OT_BLINK)  dsyslog_ot
#define DEBUG_OT_DD     if (m_debugmask & DEBUG_MASK_OT_DD)     dsyslog_ot
#define DEBUG_OT_KNONE  if (m_debugmask & DEBUG_MASK_OT_KNONE)  dsyslog_ot
#define DEBUG_OT_KEYS   if (m_debugmask & DEBUG_MASK_OT_KEYS)   dsyslog_ot
#define DEBUG_OT_MSG    if (m_debugmask & DEBUG_MASK_OT_MSG)    dsyslog_ot
#define DEBUG_OT_DRPI   if (m_debugmask & DEBUG_MASK_OT_DRPI)   dsyslog_ot
#define DEBUG_OT_FOOT   if (m_debugmask & DEBUG_MASK_OT_FOOT)   dsyslog_ot
#define DEBUG_OT_DCHR   if (m_debugmask & DEBUG_MASK_OT_DCHR)   dsyslog_ot
#define DEBUG_OT_DTXTE  if (m_debugmask & DEBUG_MASK_OT_DTXTE)  dsyslog_ot
#define DEBUG_OT_CACHE  if (m_debugmask & DEBUG_MASK_OT_CACHE)  dsyslog_ot

#endif
