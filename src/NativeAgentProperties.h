// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include "CPP/Common/ComTry.h"
#include "CPP/Windows/PropVariant.h"
#include "CPP/7zip/UI/Agent/AgentProxy.h"

// The imported Agent methods only require this opened handler/proxy boundary.
// COM ownership and Windows host/update state stay outside the read snapshot.
struct PortAgentContext {
  IInArchive *archive;
  UString ArchiveType;
  IInArchive *GetArchive() const { return archive; }
};
struct PortProxyItem { unsigned DirIndex, Index; };
class PortAgentFolder {
  CProxyArc *_proxy;
  CProxyArc2 *_proxy2;
  unsigned _proxyDirIndex;
  bool _flatMode;
  CRecordVector<PortProxyItem> _items;
  PortAgentContext *_agentSpec;
public:
  PortAgentFolder(PortAgentContext &context, CProxyArc *proxy, CProxyArc2 *proxy2,
                  unsigned dir, bool flat = false) : _proxy(proxy), _proxy2(proxy2),
    _proxyDirIndex(dir), _flatMode(flat), _agentSpec(&context) {}
  void SetItem(unsigned index) { _items.Clear(); _items.Add({_proxyDirIndex, index}); }
  void SetItems(const CRecordVector<PortProxyItem> &items) { _items = items; }
  UString GetName(UInt32 index) const;
  UString GetItemName_for_Copy(unsigned itemIndex);
  void GetPrefix(UInt32 index, UString &prefix) const;
  UString GetFullPrefix(UInt32 index) const;
  int GetRealIndex(unsigned index) const;
  void GetRealIndices(const UInt32 *indices, UInt32 numItems, bool includeAltStreams,
                      bool includeFolderSubItemsInFlatMode, CUIntVector &realIndices) const;
  HRESULT GetProperty(UInt32 index, PROPID propID, PROPVARIANT *value);
  HRESULT GetFolderProperty(PROPID propID, PROPVARIANT *value);
};
