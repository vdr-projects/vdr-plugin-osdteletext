#ifndef OSDTELETEXT_SERVICES_H
#define OSDTELETEXT_SERVICES_H

#include <stdint.h>

#define OSDTELETEXT_SERVICE_CAPABILITIES_V1 "OsdTeletext::Capabilities-v1"
#define OSDTELETEXT_SERVICE_GET_PAGE_V1 "OsdTeletext::GetPage-v1"

#define OSDTELETEXT_SERVICE_SCHEMA_V1 1U
#define OSDTELETEXT_CHANNEL_ID_MAX 64U
#define OSDTELETEXT_PAGE_ROWS 25U
#define OSDTELETEXT_PAGE_COLUMNS 40U
#define OSDTELETEXT_PAGE_CELL_COUNT (OSDTELETEXT_PAGE_ROWS * OSDTELETEXT_PAGE_COLUMNS)
#define OSDTELETEXT_SUBPAGE_AUTO 0xFFFFU

enum OsdTeletextServiceResultV1 {
   OSDTELETEXT_RESULT_OK = 0,
   OSDTELETEXT_RESULT_INVALID_REQUEST = 1,
   OSDTELETEXT_RESULT_NO_LIVE_SERVICE = 2,
   OSDTELETEXT_RESULT_CHANNEL_MISMATCH = 3,
   OSDTELETEXT_RESULT_NO_TELETEXT = 4,
   OSDTELETEXT_RESULT_RECEIVER_INACTIVE = 5,
   OSDTELETEXT_RESULT_PAGE_NOT_FOUND = 6
};

enum OsdTeletextSourceStateV1 {
   OSDTELETEXT_SOURCE_UNKNOWN = 0,
   OSDTELETEXT_SOURCE_LIVE = 1,
   OSDTELETEXT_SOURCE_CACHED = 2
};

enum OsdTeletextCellKindV1 {
   OSDTELETEXT_CELL_TEXT = 0,
   OSDTELETEXT_CELL_MOSAIC = 1,
   OSDTELETEXT_CELL_UNKNOWN = 2
};

enum OsdTeletextCellFlagsV1 {
   OSDTELETEXT_CELL_CONCEAL = 1U << 0,
   OSDTELETEXT_CELL_BLINK = 1U << 1,
   OSDTELETEXT_CELL_BOXED_OUT = 1U << 2,
   OSDTELETEXT_CELL_DOUBLE_HEIGHT_TOP = 1U << 3,
   OSDTELETEXT_CELL_DOUBLE_HEIGHT_BOTTOM = 1U << 4,
   OSDTELETEXT_CELL_DOUBLE_WIDTH_LEFT = 1U << 5,
   OSDTELETEXT_CELL_DOUBLE_WIDTH_RIGHT = 1U << 6,
   OSDTELETEXT_CELL_MOSAIC_SEPARATED = 1U << 7
};

struct OsdTeletextCapabilitiesV1 {
   uint32_t structSize;
   uint32_t schemaVersion;
   uint16_t rows;
   uint16_t columns;
   uint32_t maxSnapshots;
   uint8_t pageRead;
   uint8_t subpages;
   uint8_t level1;
   uint8_t x26Partial;
   uint8_t conceal;
   uint8_t blink;
   uint8_t doubleSize;
   uint8_t flof;
   uint8_t topNavigation;
   uint8_t x28;
   uint8_t m29;
   uint8_t reserved[5];
};

struct OsdTeletextCellV1 {
   uint32_t codepoint;
   uint8_t rawChar;
   uint8_t charset;
   uint8_t foreground;
   uint8_t background;
   uint8_t kind;
   uint8_t flags;
   uint8_t reserved[2];
};

struct OsdTeletextGetPageV1 {
   uint32_t structSize;

   /* request */
   char channelId[OSDTELETEXT_CHANNEL_ID_MAX];
   uint16_t pageNumber;
   uint16_t subpageCode;

   /* response */
   uint32_t schemaVersion;
   uint8_t result;
   uint8_t sourceState;
   uint8_t receiverActive;
   uint8_t teletextAvailable;
   uint64_t serviceEpoch;
   uint64_t pageRevision;
   uint64_t observedAt;
   uint16_t resolvedPageNumber;
   uint16_t resolvedSubpageCode;
   uint8_t pageFlags;
   uint8_t language;
   uint8_t magazine;
   uint8_t complete;
   uint16_t rows;
   uint16_t columns;
   OsdTeletextCellV1 cells[OSDTELETEXT_PAGE_CELL_COUNT];
};

#endif
