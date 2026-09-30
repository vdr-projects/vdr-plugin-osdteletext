#include "services.h"

#include <cassert>
#include <cstring>
#include <type_traits>

int main()
{
   static_assert(std::is_standard_layout<OsdTeletextCapabilitiesV1>::value, "capability contract must stay standard-layout");
   static_assert(std::is_standard_layout<OsdTeletextCellV1>::value, "cell contract must stay standard-layout");
   static_assert(std::is_standard_layout<OsdTeletextGetPageV1>::value, "page contract must stay standard-layout");

   assert(std::strcmp(OSDTELETEXT_SERVICE_CAPABILITIES_V1, "OsdTeletext::Capabilities-v1") == 0);
   assert(std::strcmp(OSDTELETEXT_SERVICE_GET_PAGE_V1, "OsdTeletext::GetPage-v1") == 0);
   assert(OSDTELETEXT_SERVICE_SCHEMA_V1 == 1U);
   assert(OSDTELETEXT_PAGE_ROWS == 25U);
   assert(OSDTELETEXT_PAGE_COLUMNS == 40U);
   assert(OSDTELETEXT_PAGE_CELL_COUNT == 1000U);
   assert(OSDTELETEXT_SUBPAGE_AUTO == 0xFFFFU);
   assert(sizeof(((OsdTeletextGetPageV1 *)0)->channelId) == OSDTELETEXT_CHANNEL_ID_MAX);
   assert(sizeof(((OsdTeletextGetPageV1 *)0)->cells) / sizeof(OsdTeletextCellV1) == OSDTELETEXT_PAGE_CELL_COUNT);

   return 0;
}
