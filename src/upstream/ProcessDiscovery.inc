// Modified for 7-Zip Mac Port by the 7-Zip4Mac contributors.
// Port adaptation: 2026-10-05 to 2026-10-05 (recorded Git history).
// Modification notice added on 2026-10-06; original notices retained below.
// PanelItemOpen.cpp
// Copyright (C) 1999-2026 Igor Pavlov. GNU LGPL-2.1-or-later.
// Generated from official 7-Zip 26.03 by scripts/import-process-discovery.py.
// Original Update body retained; macOS supplies snapshot/handle adapters.
// PanelItemOpen.cpp SHA-256: e21c806419c66314bed2def3e4e91562991d323bf4530bd403e364b0104251a2
  void Update(bool needFindProcessByPath /* , const CPossibleProgs &progs */)
  {
    /*
    if (_ids.IsEmpty())
      return;
    */

    CObjectVector<CSnapshotProcess> sps;
    GetSnapshot(sps);

    const int separ = Path.ReverseFind_PathSepar();
    const UString mainName = Path.Ptr((unsigned)(separ + 1));
    if (mainName.IsEmpty())
      needFindProcessByPath = false;

    const DWORD currentProcessId = GetCurrentProcessId();

    for (;;)
    {
      bool wasAdded = false;
      
      FOR_VECTOR (i, sps)
      {
        const CSnapshotProcess &sp = sps[i];
        const DWORD id = sp.Id;
        
        if (id == currentProcessId)
          continue;
        if (_ids.FindInSorted(id) >= 0)
          continue;

        bool isSameName = false;
        const UString &name = sp.Name;
        
        if (needFindProcessByPath)
          isSameName = mainName.IsEqualTo_NoCase(name);

        bool needAdd = false;
        // bool isFromProgs = false;
        
        if (isSameName || _ids.FindInSorted(sp.ParentId) >= 0)
          needAdd = true;
        /*
        else if (progs.IsFromList(name))
        {
          needAdd = true;
          isFromProgs = true;
        }
        */

        if (needAdd)
        {
          DEBUG_PRINT("----- OpenProcess -----");
          DEBUG_PRINT_W(name);
          HANDLE hProcess = OpenProcess(SYNCHRONIZE, FALSE, id);
          if (hProcess)
          {
            DEBUG_PRINT("----- OpenProcess OK -----");
            // if (!isFromProgs)
              _ids.AddToUniqueSorted(id);
            Handles.Add(hProcess);
            NeedWait.Add(true);
            // Names.Add(name);
            wasAdded = true;
            // ProgsWereUsed = isFromProgs;
          }
        }
      }
      
      if (!wasAdded)
        break;
    }
  }
