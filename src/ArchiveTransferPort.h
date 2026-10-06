// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include "CPP/Common/MyString.h"
#include "CPP/7zip/UI/Common/EnumDirItems.h"
namespace PortArchiveTransfer {
bool Enabled() noexcept;
UString Prefix();
HRESULT Enumerate(CDirItems &items);
}
