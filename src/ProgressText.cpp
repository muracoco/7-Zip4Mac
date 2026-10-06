// SPDX-License-Identifier: LGPL-3.0-or-later
#include "ProgressText.h"
#include <algorithm>
#include <cwchar>
#include <string>
namespace {
using UInt64 = quint64; using UInt32 = quint32; using Byte = unsigned char;
void ConvertUInt64ToString(UInt64 value, wchar_t *destination) { const auto text = std::to_wstring(value); std::copy(text.cbegin(), text.cend(), destination); destination[text.size()] = 0; }
unsigned MyStringLen(const wchar_t *text) { return unsigned(std::wcslen(text)); }
#include "upstream/ProgressText.inc"
}
QString officialProgressTime(quint64 seconds) { wchar_t text[64]; GetTimeString(seconds, text); return QString::fromWCharArray(text); }
QString officialProgressSize(quint64 bytes) { wchar_t text[64]; ConvertSizeToString(bytes, text); return QString::fromWCharArray(text); }
QString officialProgressSpeed(quint64 bytesPerSecond) { wchar_t text[64]; ConvertSpeedToString(bytesPerSecond, text); return QString::fromWCharArray(text); }
