// PanelSelect.cpp / PanelItems.cpp
// Copyright (C) 1999-2026 Igor Pavlov. GNU LGPL-2.1-or-later.
// Generated from official 7-Zip 26.03 by scripts/import-panel-selection.py.
// Original bodies retained; Qt supplies the vector/list/notification boundary.
// PanelSelect.cpp SHA-256: 8c64e1607e84be906cd73755bcfeeb4b2ec20ae18e38ef1646811b6b19b0053e
// PanelItems.cpp SHA-256: 2dc5869e794788b366b1dc7fb6485b362e7b72352c1549303b944693765e2f73

void CPanel::OnShiftSelectMessage()
{
  if (!_mySelectMode)
    return;
  const int focusedItem = _listView.GetFocusedItem();
  if (focusedItem < 0)
    return;
  if (!_selectionIsDefined)
    return;
  int startItem = MyMin(focusedItem, _prevFocusedItem);
  int finishItem = MyMax(focusedItem, _prevFocusedItem);
    
  int numItems = _listView.GetItemCount();
  for (int i = 0; i < numItems; i++)
  {
    const unsigned realIndex = GetRealItemIndex(i);
    if (realIndex == kParentIndex)
      continue;
    if (i >= startItem && i <= finishItem)
      if (_selectedStatusVector[realIndex] != _selectMark)
      {
        _selectedStatusVector[realIndex] = _selectMark;
        _listView.RedrawItem(i);
      }
  }

  _prevFocusedItem = focusedItem;
}

void CPanel::OnArrowWithShift()
{
  if (!_mySelectMode)
    return;
  const int focusedItem = _listView.GetFocusedItem();
  if (focusedItem < 0)
    return;
  const unsigned realIndex = GetRealItemIndex(focusedItem);
  
  if (_selectionIsDefined)
  {
    if (realIndex != kParentIndex)
      _selectedStatusVector[realIndex] = _selectMark;
  }
  else
  {
    if (realIndex == kParentIndex)
    {
      _selectionIsDefined = true;
      _selectMark = true;
    }
    else
    {
      _selectionIsDefined = true;
      _selectMark = !_selectedStatusVector[realIndex];
      _selectedStatusVector[realIndex] = _selectMark;
    }
  }
  
  _prevFocusedItem = focusedItem;
  PostMsg(kShiftSelectMessage);
  _listView.RedrawItem(focusedItem);
}

void CPanel::OnInsert()
{
  /*
  const int kState = CDIS_MARKED; // LVIS_DROPHILITED;
  UINT state = (_listView.GetItemState(focusedItem, LVIS_CUT) == 0) ?
      LVIS_CUT : 0;
  _listView.SetItemState(focusedItem, state, LVIS_CUT);
  // _listView.SetItemState_Selected(focusedItem);
  */

  const int focusedItem = _listView.GetFocusedItem();
  if (focusedItem < 0)
    return;
  
  const unsigned realIndex = GetRealItemIndex(focusedItem);
  if (realIndex != kParentIndex)
  {
    bool isSelected = !_selectedStatusVector[realIndex];
    _selectedStatusVector[realIndex] = isSelected;
    if (!_mySelectMode)
      _listView.SetItemState_Selected(focusedItem, isSelected);
    _listView.RedrawItem(focusedItem);
  }

  int nextIndex = focusedItem + 1;
  if (nextIndex < _listView.GetItemCount())
  {
    _listView.SetItemState_FocusedSelected(nextIndex);
    _listView.EnsureVisible(nextIndex, false);
  }
}

void CPanel::UpdateSelection()
{
  if (!_mySelectMode)
  {
    bool enableTemp = _enableItemChangeNotify;
    _enableItemChangeNotify = false;
    int numItems = _listView.GetItemCount();
    for (int i = 0; i < numItems; i++)
    {
      const unsigned realIndex = GetRealItemIndex(i);
      if (realIndex != kParentIndex)
        _listView.SetItemState_Selected(i, _selectedStatusVector[realIndex]);
    }
    _enableItemChangeNotify = enableTemp;
  }
  _listView.RedrawAllItems();
}

void CPanel::SelectAll(bool selectMode)
{
  FOR_VECTOR (i, _selectedStatusVector)
    _selectedStatusVector[i] = selectMode;
  UpdateSelection();
}

void CPanel::InvertSelection()
{
  if (!_mySelectMode)
  {
    /*
    unsigned numSelected = 0;
    FOR_VECTOR (i, _selectedStatusVector)
      if (_selectedStatusVector[i])
        numSelected++;
    */
    // 17.02: fixed : now we invert item even, if single item is selected
    /*
    if (numSelected == 1)
    {
      int focused = _listView.GetFocusedItem();
      if (focused >= 0)
      {
        const unsigned realIndex = GetRealItemIndex(focused);
        if (realIndex >= 0)
          if (_selectedStatusVector[realIndex])
            _selectedStatusVector[realIndex] = false;
      }
    }
    */
  }
  FOR_VECTOR (i, _selectedStatusVector)
    _selectedStatusVector[i] = !_selectedStatusVector[i];
  UpdateSelection();
}

void CPanel::KillSelection()
{
  SelectAll(false);
  // ver 20.01: now we don't like that focused will be selected item.
  //   So the following code was disabled:
  /*
  if (!_mySelectMode)
  {
    int focused = _listView.GetFocusedItem();
    if (focused >= 0)
    {
      // CPanel::OnItemChanged notify for LVIS_SELECTED change doesn't work here. Why?
      // so we change _selectedStatusVector[realIndex] here.
      const unsigned realIndex = GetRealItemIndex(focused);
      if (realIndex != kParentIndex)
        _selectedStatusVector[realIndex] = true;
      _listView.SetItemState_Selected(focused);
    }
  }
  */
}

void CPanel::OnLeftClick(MY_NMLISTVIEW_NMITEMACTIVATE *itemActivate)
{
  if (itemActivate->hdr.hwndFrom != HWND(_listView))
    return;
  // It will work only for Version 4.71 (IE 4);
  int indexInList = itemActivate->iItem;
  if (indexInList < 0)
    return;
  
  #ifndef UNDER_CE
  if ((itemActivate->uKeyFlags & LVKF_SHIFT) != 0)
  {
    // int focusedIndex = _listView.GetFocusedItem();
    const int focusedIndex = _startGroupSelect;
    if (focusedIndex < 0)
      return;
    const int startItem = MyMin(focusedIndex, indexInList);
    const int finishItem = MyMax(focusedIndex, indexInList);

    const int numItems = _listView.GetItemCount();
    for (int i = 0; i < numItems; i++)
    {
      const unsigned realIndex = GetRealItemIndex(i);
      if (realIndex == kParentIndex)
        continue;
      const bool selected = (i >= startItem && i <= finishItem);
      if (_selectedStatusVector[realIndex] != selected)
      {
        _selectedStatusVector[realIndex] = selected;
        _listView.RedrawItem(i);
      }
    }
  }
  else
  #endif
  {
    _startGroupSelect = indexInList;
  
    #ifndef UNDER_CE
    if ((itemActivate->uKeyFlags & LVKF_CONTROL) != 0)
    {
      const unsigned realIndex = GetRealItemIndex(indexInList);
      if (realIndex != kParentIndex)
      {
        _selectedStatusVector[realIndex] = !_selectedStatusVector[realIndex];
        _listView.RedrawItem(indexInList);
      }
    }
    #endif
  }

  return;
}

void CPanel::Get_ItemIndices_Selected(CRecordVector<UInt32> &indices) const
{
  indices.Clear();
  /*
  int itemIndex = -1;
  while ((itemIndex = _listView.GetNextSelectedItem(itemIndex)) != -1)
  {
    LPARAM param;
    if (_listView.GetItemParam(itemIndex, param))
      indices.Add(param);
  }
  HeapSort(&indices.Front(), indices.Size());
  */
  const bool *v = _selectedStatusVector.ConstData();
  const unsigned size = _selectedStatusVector.Size();
  for (unsigned i = 0; i < size; i++)
    if (v[i])
      indices.Add(i);
}

void CPanel::Get_ItemIndices_Operated(CRecordVector<UInt32> &indices) const
{
  Get_ItemIndices_Selected(indices);
  if (!indices.IsEmpty())
    return;
  if (_listView.GetSelectedCount() == 0)
    return;
  const int focusedItem = _listView.GetFocusedItem();
  if (focusedItem >= 0)
  {
    if (_listView.IsItemSelected(focusedItem))
    {
      const unsigned realIndex = GetRealItemIndex(focusedItem);
      if (realIndex != kParentIndex)
        indices.Add(realIndex);
    }
  }
}
