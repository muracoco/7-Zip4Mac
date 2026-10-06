// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include "CPP/Windows/FileFind.h"
namespace PortFileState {
bool Enabled();
HRESULT Find(const FString &path, NWindows::NFile::NFind::CFileInfo &info, bool &found, bool probe = false);
bool Exists(const FString &path);
}
