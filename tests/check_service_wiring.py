#!/usr/bin/env python3
from pathlib import Path

root = Path(__file__).resolve().parent.parent
osdteletext = (root / "osdteletext.c").read_text(encoding="utf-8")
txtrecv = (root / "txtrecv.c").read_text(encoding="utf-8")
service = (root / "teletextservice.h").read_text(encoding="utf-8")

required_osd = (
    '#include "teletextservice.h"',
    'virtual bool Service(const char *Id, void *Data = NULL);',
    'return TeletextService::Handle(Id, Data);',
)
required_receiver = (
    '#include "teletextservice.h"',
    'TeletextService::Publish(',
    'TeletextService::SetLiveService(',
    'TeletextService::ClearLiveService();',
    'TeletextService::SetReceiverActive(',
)
required_service_rendering = (
    'unsigned char *renderBytes = reinterpret_cast<unsigned char *>(&renderData);',
    'renderer.ReadTeletextHeader(renderBytes);',
    'renderer.RenderTeletextCode(renderBytes + sizeof(renderData.pageheader));',
)
forbidden_service = (
    'openForReading(',
    'getFilename(',
    '/var/cache/vdr/vtx',
    'renderer.RenderTeletextCode(reinterpret_cast<unsigned char *>(&renderData));',
)

for needle in required_osd:
    if needle not in osdteletext:
        raise SystemExit(f"missing osdteletext service wiring: {needle}")
for needle in required_receiver:
    if needle not in txtrecv:
        raise SystemExit(f"missing receiver service wiring: {needle}")
for needle in required_service_rendering:
    if needle not in service:
        raise SystemExit(f"missing service renderer wiring: {needle}")
for needle in forbidden_service:
    if needle in service:
        raise SystemExit(f"forbidden service wiring: {needle}")

print("teletext service wiring: PASS")
