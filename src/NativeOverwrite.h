// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include "CPP/Common/MyWindows.h"
#include "CPP/Common/MyString.h"
namespace PortOverwrite {
bool Enabled();
HRESULT Ask(const UString &existing, const FILETIME *existingTime, const UInt64 *existingSize,
            const UString &incoming, const FILETIME *incomingTime, const UInt64 *incomingSize,
            bool existingDirectory, bool incomingDirectory, Int32 *answer);
}
