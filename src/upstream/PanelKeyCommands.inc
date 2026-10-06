// Modified for 7-Zip Mac Port by the 7-Zip4Mac contributors.
// Port adaptation: 2026-10-06 to 2026-10-06 (recorded Git history).
// Modification notice added on 2026-10-06; original notices retained below.
// Copyright (C) 1999-2026 Igor Pavlov. GNU LGPL-2.1-or-later.
// Generated from official 7-Zip 26.03 by scripts/import-panel-key.py.
// Original dispatch/name/path bodies; Qt supplies OS types and callbacks.
// PanelKey.cpp SHA-256: f0814d249a41d50ba1f4eb58ece67b2f4377d4df7bc59706e4acc4f73e180d7f
// PanelOperations.cpp SHA-256: fe88eab47e41a17c1694b38f36a5798c5e49fa34710e7dc8decb408ddec37bb3
// BrowseDialog.cpp SHA-256: 756dc2c4321e0343ae0649c36392acb522bceea9aa8f95c7f9a8e17000ba77db
// App.cpp SHA-256: ffeb0cd42e88bd29038039857631e584410323864aeae8ed361aaed897f50a9b
// PanelFolderChange.cpp SHA-256: 912bb44658a2efef68a47f42b43668bab70479f6d1bb41d6c96b797cd3c55acd
// Panel.cpp SHA-256: 0fb68a77ff5079044c01ccbca1336100d26024689bfc4bb51b70bdb5455ddc78
// FSFolder.cpp SHA-256: 59ff062ca1c186410c76d9d1f50992f865d37d7cce948d236f4a7e00a13823bd

struct CVKeyPropIDPair
{
  WORD VKey;
  PROPID PropID;
};

static const CVKeyPropIDPair g_VKeyPropIDPairs[] =
{
  { VK_F3, kpidName },
  { VK_F4, kpidExtension },
  { VK_F5, kpidMTime },
  { VK_F6, kpidSize },
  { VK_F7, kpidNoProperty }
};

static int FindVKeyPropIDPair(WORD vKey)
{
  for (unsigned i = 0; i < Z7_ARRAY_SIZE(g_VKeyPropIDPairs); i++)
    if (g_VKeyPropIDPairs[i].VKey == vKey)
      return (int)i;
  return -1;
}


bool CPanel::OnKeyDown(LPNMLVKEYDOWN keyDownInfo, LRESULT &result)
{
  if (keyDownInfo->wVKey == VK_TAB && keyDownInfo->hdr.hwndFrom == _listView)
  {
    _panelCallback->OnTab();
    return false;
  }
  const bool alt = IsKeyDown(VK_MENU);
  const bool ctrl = IsKeyDown(VK_CONTROL);
  // const bool leftCtrl = IsKeyDown(VK_LCONTROL);
  const bool rightCtrl = IsKeyDown(VK_RCONTROL);
  const bool shift = IsKeyDown(VK_SHIFT);
  result = 0;

  if (keyDownInfo->wVKey >= '0' &&
      keyDownInfo->wVKey <= '9' &&
      (rightCtrl || alt))
  {
    const unsigned index = (unsigned)(keyDownInfo->wVKey - '0');
    if (shift)
    {
      SetBookmark(index);
      return true;
    }
    else
    {
      OpenBookmark(index);
      return true;
    }
  }

  if ((keyDownInfo->wVKey == VK_F2 ||
       keyDownInfo->wVKey == VK_F1)
       && alt && !ctrl && !shift)
  {
    _panelCallback->SetFocusToPath(keyDownInfo->wVKey == VK_F1 ? 0 : 1);
    return true;
  }

  if ((keyDownInfo->wVKey == VK_F9) && !alt && !ctrl && !shift)
  {
    g_App.SwitchOnOffOnePanel();
  }

  if (keyDownInfo->wVKey >= VK_F3 && keyDownInfo->wVKey <= VK_F12 && ctrl)
  {
    const int index = FindVKeyPropIDPair(keyDownInfo->wVKey);
    if (index >= 0)
      SortItemsWithPropID(g_VKeyPropIDPairs[index].PropID);
  }

  switch (keyDownInfo->wVKey)
  {
    case VK_SHIFT:
    {
      _selectionIsDefined = false;
      _prevFocusedItem = _listView.GetFocusedItem();
      break;
    }
    /*
    case VK_F1:
    {
      // ShowHelpWindow(NULL, kHelpTopic);
      break;
    }
    */
    case VK_F2:
    {
      if (!alt && !ctrl &&!shift)
      {
        RenameFile();
        return true;
      }
      break;
    }
    case VK_F3:
    {
      if (!alt && !ctrl && !shift)
      {
        EditItem(false);
        return true;
      }
      break;
    }
    case VK_F4:
    {
      if (!alt && !ctrl && !shift)
      {
        EditItem(true);
        return true;
      }
      if (!alt && !ctrl && shift)
      {
        CreateFile();
        return true;
      }
      break;
    }
    case VK_F5:
    {
      if (!alt && !ctrl)
      {
        _panelCallback->OnCopy(false, shift);
        return true;
      }
      break;
    }
    case VK_F6:
    {
      if (!alt && !ctrl)
      {
        _panelCallback->OnCopy(true, shift);
        return true;
      }
      break;
    }
    case VK_F7:
    {
      if (!alt && !ctrl && !shift)
      {
        /* we can process F7 via menu ACCELERATOR.
          But menu loading can be slow in case of UNC paths and system menu.
          So we use don't use ACCELERATOR */
        CreateFolder();
        return true;
      }
      break;
    }
    case VK_DELETE:
    {
      DeleteItems(!shift);
      return true;
    }
    case VK_INSERT:
    {
      if (!alt)
      {
        if (ctrl && !shift)
        {
          EditCopy();
          return true;
        }
        if (shift && !ctrl)
        {
          EditPaste();
          return true;
        }
        if (!shift && !ctrl && _mySelectMode)
        {
          OnInsert();
          return true;
        }
      }
      return false;
    }
    case VK_DOWN:
    {
      if (shift)
        OnArrowWithShift();
      return false;
    }
    case VK_UP:
    {
      if (alt)
        _panelCallback->OnSetSameFolder();
      else if (shift)
        OnArrowWithShift();
      return false;
    }
    case VK_RIGHT:
    {
      if (alt)
        _panelCallback->OnSetSubFolder();
      else if (shift)
        OnArrowWithShift();
      return false;
    }
    case VK_LEFT:
    {
      if (alt)
        _panelCallback->OnSetSubFolder();
      else if (shift)
        OnArrowWithShift();
      return false;
    }
    case VK_NEXT:
    {
      if (ctrl && !alt && !shift)
      {
        // EnterToFocused();
        return true;
      }
      break;
    }
    case VK_ADD:
    {
      if (alt)
        SelectByType(true);
      else if (shift)
        SelectAll(true);
      else if (!ctrl)
        SelectSpec(true);
      return true;
    }
    case VK_SUBTRACT:
    {
      if (alt)
        SelectByType(false);
      else if (shift)
        SelectAll(false);
      else
        SelectSpec(false);
      return true;
    }
    /*
    case VK_DELETE:
      CommandDelete();
      return 0;
    case VK_F1:
      CommandHelp();
      return 0;
    */
    case VK_BACK:
      OpenParentFolder();
      return true;
    /*
    case VK_DIVIDE:
    case '\\':
    case '/':
    case VK_OEM_5:
    {
      // OpenRootFolder();
      OpenDrivesFolder();

      return true;
    }
    */
    case 'A':
      if (ctrl)
      {
        SelectAll(true);
        return true;
      }
      return false;
    case 'X':
      if (ctrl)
      {
        EditCut();
        return true;
      }
      return false;
    case 'C':
      if (ctrl)
      {
        EditCopy();
        return true;
      }
      return false;
    case 'V':
      if (ctrl)
      {
        EditPaste();
        return true;
      }
      return false;
    case 'N':
      if (ctrl)
      {
        CreateFile();
        return true;
      }
      return false;
    case 'R':
      if (ctrl)
      {
        OnReload();
        return true;
      }
      return false;
    case 'W':
      if (ctrl)
      {
        // SendMessage();
        PostMessage(g_HWND, WM_COMMAND, IDCLOSE, 0);
        return true;
      }
      return false;
    case 'Z':
      if (ctrl)
      {
        ChangeComment();
        return true;
      }
      return false;
    case '1':
    case '2':
    case '3':
    case '4':
      if (ctrl)
      {
        const unsigned styleIndex = (unsigned)(keyDownInfo->wVKey - '1');
        SetListViewMode(styleIndex);
        return true;
      }
      return false;
    case VK_MULTIPLY:
      {
        InvertSelection();
        return true;
      }
    case VK_F12:
      if (alt && !ctrl && !shift)
      {
        FoldersHistory();
        return true;
      }
  }
  return false;
}
