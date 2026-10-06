// PanelSelect.cpp / PanelItems.cpp
// Copyright (C) 1999-2026 Igor Pavlov. GNU LGPL-2.1-or-later.
// Generated from official 7-Zip 26.03 by scripts/import-panel-selection.py.
// Original bodies retained; Qt supplies the vector/list/notification boundary.
// PanelSelect.cpp SHA-256: 8c64e1607e84be906cd73755bcfeeb4b2ec20ae18e38ef1646811b6b19b0053e
// PanelItems.cpp SHA-256: 2dc5869e794788b366b1dc7fb6485b362e7b72352c1549303b944693765e2f73

void CPanel::OpenSelectedItems(bool tryInternal)
{
  CRecordVector<UInt32> indices;
  Get_ItemIndices_Operated(indices);
  if (indices.Size() > 20)
  {
    MessageBox_Error_LangID(IDS_TOO_MANY_ITEMS);
    return;
  }
  
  const int focusedItem = _listView.GetFocusedItem();
  if (focusedItem >= 0)
  {
    const unsigned realIndex = GetRealItemIndex(focusedItem);
    if (realIndex == kParentIndex && (tryInternal || indices.Size() == 0) && _listView.IsItemSelected(focusedItem))
      indices.Insert(0, realIndex);
  }

  // bool dirIsStarted = false;
  FOR_VECTOR (i, indices)
  {
    UInt32 index = indices[i];
    // CFileInfo &aFile = m_Files[index];
    if (IsItem_Folder(index))
    {
      // if (!dirIsStarted)
      {
        if (tryInternal)
        {
          OpenFolder(index);
          // dirIsStarted = true;
          break;
        }
        else
          OpenFolderExternal(index);
      }
    }
    else
      OpenItem(index, (tryInternal && indices.Size() == 1), true);
  }
}
