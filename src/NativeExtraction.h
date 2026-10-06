// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include "CPP/Common/MyString.h"
namespace PortExtraction {
bool Interactive();
bool Move(const FString &from, const FString &to);
bool RemoveDirectory(const FString &path);
bool DeleteFile(const FString &path);
bool ExistsRaw(const FString &path);
HRESULT Begin(UInt32 index, const FString &path);
HRESULT DeferLink(UInt32 index, const FString &path);
HRESULT Record(UInt32 index, const FString &path, const FString &hardLink, bool split = false, bool linkComplete = false);
HRESULT PrepareHardLinkTarget(FString &path);
HRESULT Finish();
}
