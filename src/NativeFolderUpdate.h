// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include "CPP/7zip/UI/Common/OpenArchive.h"
#include "CPP/7zip/UI/Console/OpenCallbackConsole.h"
namespace PortFolderUpdate {
bool Requested();
HRESULT Run(const CArchiveLink &link, const CCodecs *codecs, const COpenCallbackConsole &openCallback);
}
