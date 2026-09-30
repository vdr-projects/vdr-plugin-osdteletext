#ifndef OSDTELETEXT_TELETEXTSERVICE_H
#define OSDTELETEXT_TELETEXTSERVICE_H

#include "services.h"
#include "storage.h"
#include "txtrender.h"
#include "txtfont.h"

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <ctime>
#include <map>
#include <mutex>
#include <string>
#include <utility>

namespace TeletextService {

static const std::size_t kMaximumSnapshots = 2048;

struct RawPageSnapshot {
   unsigned int pageCode;
   unsigned int subpageCode;
   unsigned char flags;
   unsigned char language;
   unsigned char magazine;
   uint64_t revision;
   uint64_t observedAt;
   TelePageData data;
};

struct ServiceStateSnapshot {
   std::string channelId;
   uint64_t serviceEpoch;
   bool teletextAvailable;
   bool receiverActive;
};

class SnapshotStore {
   typedef std::pair<unsigned int, unsigned int> PageKey;

public:
   SnapshotStore()
      : serviceEpoch_(0), observationSequence_(0), teletextAvailable_(false), receiverActive_(false)
   {
   }

   void ClearLiveService()
   {
      std::lock_guard<std::mutex> lock(mutex_);
      if (!channelId_.empty() || teletextAvailable_ || receiverActive_ || !pages_.empty())
         ++serviceEpoch_;
      channelId_.clear();
      teletextAvailable_ = false;
      receiverActive_ = false;
      pages_.clear();
   }

   void SetLiveService(const char *channelId, bool teletextAvailable)
   {
      const std::string nextChannel = channelId ? channelId : "";
      std::lock_guard<std::mutex> lock(mutex_);

      const bool changed = channelId_ != nextChannel || teletextAvailable_ != teletextAvailable;
      if (changed) {
         ++serviceEpoch_;
         pages_.clear();
         receiverActive_ = false;
      }

      channelId_ = nextChannel;
      teletextAvailable_ = teletextAvailable && !channelId_.empty();
      if (!teletextAvailable_)
         receiverActive_ = false;
   }

   void SetReceiverActive(bool active)
   {
      std::lock_guard<std::mutex> lock(mutex_);
      const bool nextActive = active && teletextAvailable_ && !channelId_.empty();
      if (nextActive && !receiverActive_ && !pages_.empty()) {
         // A fresh receiver lifetime must not make pages from the previous
         // receiver lifetime look live. Keep the fencing monotonic and wait
         // for new broadcast observations.
         ++serviceEpoch_;
         pages_.clear();
      }
      receiverActive_ = nextActive;
   }

   void Publish(
      const char *channelId,
      unsigned int pageCode,
      unsigned int subpageCode,
      unsigned char flags,
      unsigned char language,
      unsigned char magazine,
      const TelePageData &data)
   {
      const std::string sourceChannel = channelId ? channelId : "";
      std::lock_guard<std::mutex> lock(mutex_);

      if (!receiverActive_ || !teletextAvailable_ || channelId_.empty() || sourceChannel != channelId_)
         return;

      RawPageSnapshot snapshot;
      snapshot.pageCode = pageCode;
      snapshot.subpageCode = subpageCode;
      snapshot.flags = flags;
      snapshot.language = language;
      snapshot.magazine = magazine;
      snapshot.revision = ++observationSequence_;
      snapshot.observedAt = static_cast<uint64_t>(std::time(NULL));
      std::memcpy(&snapshot.data, &data, sizeof(snapshot.data));

      pages_[std::make_pair(pageCode, subpageCode)] = snapshot;
      EvictIfNeeded();
   }

   uint8_t Read(
      const std::string &requestedChannel,
      unsigned int pageCode,
      unsigned int subpageCode,
      RawPageSnapshot &snapshot,
      ServiceStateSnapshot &state) const
   {
      std::lock_guard<std::mutex> lock(mutex_);

      state.channelId = channelId_;
      state.serviceEpoch = serviceEpoch_;
      state.teletextAvailable = teletextAvailable_;
      state.receiverActive = receiverActive_;

      if (channelId_.empty())
         return OSDTELETEXT_RESULT_NO_LIVE_SERVICE;
      if (requestedChannel != channelId_)
         return OSDTELETEXT_RESULT_CHANNEL_MISMATCH;
      if (!teletextAvailable_)
         return OSDTELETEXT_RESULT_NO_TELETEXT;

      if (subpageCode != OSDTELETEXT_SUBPAGE_AUTO) {
         const std::map<PageKey, RawPageSnapshot>::const_iterator it = pages_.find(std::make_pair(pageCode, subpageCode));
         if (it == pages_.end())
            return receiverActive_ ? OSDTELETEXT_RESULT_PAGE_NOT_FOUND : OSDTELETEXT_RESULT_RECEIVER_INACTIVE;
         snapshot = it->second;
         return OSDTELETEXT_RESULT_OK;
      }

      const RawPageSnapshot *latest = NULL;
      for (std::map<PageKey, RawPageSnapshot>::const_iterator it = pages_.begin(); it != pages_.end(); ++it) {
         if (it->first.first != pageCode)
            continue;
         if (!latest || it->second.revision > latest->revision)
            latest = &it->second;
      }

      if (!latest)
         return receiverActive_ ? OSDTELETEXT_RESULT_PAGE_NOT_FOUND : OSDTELETEXT_RESULT_RECEIVER_INACTIVE;

      snapshot = *latest;
      return OSDTELETEXT_RESULT_OK;
   }

private:
   void EvictIfNeeded()
   {
      while (pages_.size() > kMaximumSnapshots) {
         std::map<PageKey, RawPageSnapshot>::iterator oldest = pages_.begin();
         for (std::map<PageKey, RawPageSnapshot>::iterator it = pages_.begin(); it != pages_.end(); ++it) {
            if (it->second.revision < oldest->second.revision)
               oldest = it;
         }
         pages_.erase(oldest);
      }
   }

   mutable std::mutex mutex_;
   std::string channelId_;
   uint64_t serviceEpoch_;
   uint64_t observationSequence_;
   bool teletextAvailable_;
   bool receiverActive_;
   std::map<PageKey, RawPageSnapshot> pages_;
};

inline SnapshotStore &Store()
{
   static SnapshotStore store;
   return store;
}

inline void ClearLiveService()
{
   Store().ClearLiveService();
}

inline void SetLiveService(const char *channelId, bool teletextAvailable)
{
   Store().SetLiveService(channelId, teletextAvailable);
}

inline void SetReceiverActive(bool active)
{
   Store().SetReceiverActive(active);
}

inline void Publish(
   const char *channelId,
   unsigned int pageCode,
   unsigned int subpageCode,
   unsigned char flags,
   unsigned char language,
   unsigned char magazine,
   const TelePageData &data)
{
   Store().Publish(channelId, pageCode, subpageCode, flags, language, magazine, data);
}

inline bool DecimalPageToCode(uint16_t pageNumber, unsigned int &pageCode)
{
   if (pageNumber < 100 || pageNumber > 899)
      return false;

   const unsigned int hundreds = pageNumber / 100;
   const unsigned int tens = (pageNumber / 10) % 10;
   const unsigned int ones = pageNumber % 10;
   pageCode = (hundreds << 8) | (tens << 4) | ones;
   return true;
}

inline uint16_t PageCodeToDecimal(unsigned int pageCode)
{
   return static_cast<uint16_t>(((pageCode >> 8) & 0x0F) * 100 + ((pageCode >> 4) & 0x0F) * 10 + (pageCode & 0x0F));
}

inline std::string FixedString(const char *value, std::size_t capacity)
{
   std::size_t length = 0;
   while (length < capacity && value[length] != '\0')
      ++length;
   return std::string(value, length);
}

inline void FillCapabilities(OsdTeletextCapabilitiesV1 &reply)
{
   const uint32_t structSize = reply.structSize;
   std::memset(&reply, 0, sizeof(reply));
   reply.structSize = structSize;
   reply.schemaVersion = OSDTELETEXT_SERVICE_SCHEMA_V1;
   reply.rows = OSDTELETEXT_PAGE_ROWS;
   reply.columns = OSDTELETEXT_PAGE_COLUMNS;
   reply.maxSnapshots = static_cast<uint32_t>(kMaximumSnapshots);
   reply.pageRead = 1;
   reply.subpages = 1;
   reply.level1 = 1;
   reply.x26Partial = 1;
   reply.conceal = 1;
   reply.blink = 1;
   reply.doubleSize = 1;
   reply.flof = 0;
   reply.topNavigation = 0;
   reply.x28 = 0;
   reply.m29 = 0;
}

inline void NormalizeCell(const cTeletextChar &input, OsdTeletextCellV1 &output)
{
   cTeletextChar cell = input;
   std::memset(&output, 0, sizeof(output));

   const enumCharsets charset = cell.GetCharset();
   output.rawChar = cell.GetChar();
   output.charset = static_cast<uint8_t>((static_cast<unsigned int>(charset) >> 8) & 0xFF);
   output.foreground = static_cast<uint8_t>(cell.GetFGColor());
   output.background = static_cast<uint8_t>(cell.GetBGColor());

   const bool mosaic = charset == CHARSET_GRAPHICS_G1 || charset == CHARSET_GRAPHICS_G1_SEP;
   output.kind = mosaic ? OSDTELETEXT_CELL_MOSAIC : OSDTELETEXT_CELL_TEXT;
   output.codepoint = mosaic ? 0U : GetVTXChar(cell);

   if (cell.GetConceal())
      output.flags |= OSDTELETEXT_CELL_CONCEAL;
   if (cell.GetBlink())
      output.flags |= OSDTELETEXT_CELL_BLINK;
   if (cell.GetBoxedOut())
      output.flags |= OSDTELETEXT_CELL_BOXED_OUT;
   if (cell.GetDblHeight() == dblh_Top)
      output.flags |= OSDTELETEXT_CELL_DOUBLE_HEIGHT_TOP;
   if (cell.GetDblHeight() == dblh_Bottom)
      output.flags |= OSDTELETEXT_CELL_DOUBLE_HEIGHT_BOTTOM;
   if (cell.GetDblWidth() == dblw_Left)
      output.flags |= OSDTELETEXT_CELL_DOUBLE_WIDTH_LEFT;
   if (cell.GetDblWidth() == dblw_Right)
      output.flags |= OSDTELETEXT_CELL_DOUBLE_WIDTH_RIGHT;
   if (charset == CHARSET_GRAPHICS_G1_SEP)
      output.flags |= OSDTELETEXT_CELL_MOSAIC_SEPARATED;
}

inline void FillPageReply(OsdTeletextGetPageV1 &reply)
{
   const uint32_t structSize = reply.structSize;
   const std::string requestedChannel = FixedString(reply.channelId, sizeof(reply.channelId));
   const uint16_t requestedPage = reply.pageNumber;
   const uint16_t requestedSubpage = reply.subpageCode;

   std::memset(&reply, 0, sizeof(reply));
   reply.structSize = structSize;
   reply.schemaVersion = OSDTELETEXT_SERVICE_SCHEMA_V1;
   reply.pageNumber = requestedPage;
   reply.subpageCode = requestedSubpage;
   if (!requestedChannel.empty()) {
      const std::size_t copyLength = std::min(requestedChannel.size(), sizeof(reply.channelId) - 1);
      std::memcpy(reply.channelId, requestedChannel.data(), copyLength);
      reply.channelId[copyLength] = '\0';
   }

   unsigned int pageCode = 0;
   if (requestedChannel.empty() || !DecimalPageToCode(requestedPage, pageCode)) {
      reply.result = OSDTELETEXT_RESULT_INVALID_REQUEST;
      return;
   }

   RawPageSnapshot snapshot = {};
   ServiceStateSnapshot state = {};
   reply.result = Store().Read(requestedChannel, pageCode, requestedSubpage, snapshot, state);
   reply.serviceEpoch = state.serviceEpoch;
   reply.receiverActive = state.receiverActive ? 1 : 0;
   reply.teletextAvailable = state.teletextAvailable ? 1 : 0;
   reply.sourceState = state.receiverActive ? OSDTELETEXT_SOURCE_LIVE : OSDTELETEXT_SOURCE_CACHED;

   if (reply.result != OSDTELETEXT_RESULT_OK)
      return;

   reply.pageRevision = snapshot.revision;
   reply.observedAt = snapshot.observedAt;
   reply.resolvedPageNumber = PageCodeToDecimal(snapshot.pageCode);
   reply.resolvedSubpageCode = static_cast<uint16_t>(snapshot.subpageCode);
   reply.pageFlags = snapshot.flags;
   reply.language = snapshot.language;
   reply.magazine = snapshot.magazine;
   reply.complete = 0; // The legacy decoder does not track an authoritative received-line mask yet.
   reply.rows = OSDTELETEXT_PAGE_ROWS;
   reply.columns = OSDTELETEXT_PAGE_COLUMNS;

   TelePageData renderData;
   std::memcpy(&renderData, &snapshot.data, sizeof(renderData));
   cRenderPage renderer;
   unsigned char *renderBytes = reinterpret_cast<unsigned char *>(&renderData);
   renderer.ReadTeletextHeader(renderBytes);
   renderer.RenderTeletextCode(renderBytes + sizeof(renderData.pageheader));

   for (unsigned int y = 0; y < OSDTELETEXT_PAGE_ROWS; ++y) {
      for (unsigned int x = 0; x < OSDTELETEXT_PAGE_COLUMNS; ++x) {
         NormalizeCell(renderer.GetChar(x, y), reply.cells[y * OSDTELETEXT_PAGE_COLUMNS + x]);
      }
   }
}

inline bool Handle(const char *id, void *data)
{
   if (!id)
      return false;

   if (std::strcmp(id, OSDTELETEXT_SERVICE_CAPABILITIES_V1) == 0) {
      if (!data)
         return true;
      OsdTeletextCapabilitiesV1 *reply = static_cast<OsdTeletextCapabilitiesV1 *>(data);
      if (reply->structSize != sizeof(*reply))
         return true;
      FillCapabilities(*reply);
      return true;
   }

   if (std::strcmp(id, OSDTELETEXT_SERVICE_GET_PAGE_V1) == 0) {
      if (!data)
         return true;
      OsdTeletextGetPageV1 *reply = static_cast<OsdTeletextGetPageV1 *>(data);
      if (reply->structSize != sizeof(*reply))
         return true;
      FillPageReply(*reply);
      return true;
   }

   return false;
}

} // namespace TeletextService

#endif
