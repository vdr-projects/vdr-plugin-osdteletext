#!/usr/bin/env python3
from pathlib import Path

source = (Path(__file__).resolve().parent.parent / "teletextservice.h").read_text(encoding="utf-8")

required = (
    "if (nextActive && !receiverActive_ && !pages_.empty())",
    "++serviceEpoch_;",
    "pages_.clear();",
    "if (!receiverActive_ || !teletextAvailable_ || channelId_.empty() || sourceChannel != channelId_)",
)
for needle in required:
    if needle not in source:
        raise SystemExit(f"missing snapshot freshness fence: {needle}")

print("teletext snapshot freshness fencing: PASS")
