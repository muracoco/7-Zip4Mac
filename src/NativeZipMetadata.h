// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include "CPP/Windows/PropVariant.h"
#include "CPP/7zip/Archive/IArchive.h"

// Comment/Rename change no file timestamps. The official ZIP handler's default
// disables ATime/CTime writes even though the Agent callback supplies them.
// Ask that same handler to retain those existing properties for metadata-only
// UI commands, without changing Add/Update compression or timestamp settings.
inline HRESULT retainZipMetadataTimes(IInArchive *archive) {
    CMyComPtr<ISetProperties> settings;
    const HRESULT result = archive->QueryInterface(IID_ISetProperties, reinterpret_cast<void **>(&settings));
    if (result != S_OK || !settings) return result == S_OK ? E_NOINTERFACE : result;
    const wchar_t *names[] = {L"ta", L"tc"};
    PROPVARIANT values[2]{};
    for (auto &value : values) { value.vt = VT_BOOL; value.boolVal = VARIANT_TRUE; }
    return settings->SetProperties(names, values, 2);
}
