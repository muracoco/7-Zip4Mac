// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include "CPP/7zip/UI/Common/OpenArchive.h"
#include "NativeAgentProperties.h"
#include <memory>
#include <string>

namespace PortSelection {
struct Resolved {
  // Keep the official proxy/folder alive for Agent update operations too.
  CProxyArc proxy;
  CProxyArc2 proxy2;
  PortAgentContext context{nullptr, {}};
  std::unique_ptr<PortAgentFolder> folder;
  CUIntVector localIndices;
  CUIntVector indices;
  UString selectedPath;
  UStringVector pathParts;
  bool alternateFolder = false, tree = false, preservePaths = false;
  UInt32 baseParent = UInt32(-1);
};
bool Requested();
std::string Snapshot(const CArchiveLink &link);
HRESULT Resolve(const CArchiveLink &link, const CCodecs *codecs, Resolved &result);
void Normalize(CUIntVector &indices);
}
