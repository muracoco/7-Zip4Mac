// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include "CPP/7zip/UI/Common/OpenArchive.h"
#include <string>
namespace PortMetadata {
// These adapters call the existing official console name/error formatters.
UString PropertyName(PROPID id, const wchar_t *name);
UString ErrorFlags(UInt32 flags);
std::string RawProperty(PROPID id, const Byte *data, UInt32 size, bool list);
HRESULT Write(const CArchiveLink &link, const CCodecs *codecs);
HRESULT WriteFormats(const CCodecs *codecs);
}
