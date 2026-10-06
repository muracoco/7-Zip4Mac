// BrowseDialog2.cpp
// Copyright (C) 1999-2026 Igor Pavlov. GNU LGPL-2.1-or-later.
// Generated from official 7-Zip 26.03 by import-temp-browser.py.
// Original bounded enumerator; only the POSIX link accessor adapted.
// Original source SHA-256: d78eb24bc0a27fd49143a612c779d5d5d91a3ce6885249e3d8f4fe987288648e

static const UInt32 k_EnumerateDirsLimit = 200;
static const UInt32 k_EnumerateFilesLimit = 2000;

struct CBrowseItem
{
  unsigned MainFileIndex;
  int SubFileIndex;
  bool WasInterrupted;
  UInt32 NumFiles;
  UInt32 NumDirs;
  UInt32 NumRootItems;
  UInt64 Size;

  void Construct()
  {
    // MainFileIndex = 0;
    SubFileIndex = -1;
    WasInterrupted = false;
    NumFiles = 0;
    NumDirs = 0;
    NumRootItems = 0;
    Size = 0;
  }
};


struct CBrowseEnumerator
{
  FString Path; // folder path without slash at the end
  CFileInfo fi; // temp
  CFileInfo fi_SubFile;
  CBrowseItem bi;

  CBrowseEnumerator()
  {
    bi.Construct();
  }
  void Enumerate(unsigned level);
  bool NeedInterrupt() const
  {
    return bi.NumFiles >= k_EnumerateFilesLimit
        || bi.NumDirs  >= k_EnumerateDirsLimit;
  }
};

void CBrowseEnumerator::Enumerate(unsigned level)
{
  Path.Add_PathSepar();
  const unsigned len = Path.Len();
  CObjectVector<FString> names;
  {
    CEnumerator enumerator;
    enumerator.SetDirPrefix(Path);
    while (enumerator.Next(fi))
    {
      if (level == 0)
      {
        if (bi.NumRootItems == 0)
          fi_SubFile = fi;
        bi.NumRootItems++;
      }
      
      if (fi.IsDir())
      {
        bi.NumDirs++;
        if (!fi.IsOsSymLink())
          names.Add(fi.Name);
      }
      else
      {
        bi.NumFiles++;
        bi.Size += fi.Size;
      }

      if (level != 0 || bi.NumRootItems > 1)
        if (NeedInterrupt())
        {
          bi.WasInterrupted = true;
          return;
        }
    }
  }

  FOR_VECTOR (i, names)
  {
    if (NeedInterrupt())
    {
      bi.WasInterrupted = true;
      return;
    }
    Path.DeleteFrom(len);
    Path += names[i];
    Enumerate(level + 1);
  }
}

// BrowseDialog.cpp; original source SHA-256: 756dc2c4321e0343ae0649c36392acb522bceea9aa8f95c7f9a8e17000ba77db
wchar_t *Browse_ConvertSizeToString(UInt64 v, wchar_t *s)
{
  char c = 0;
       if (v >= ((UInt64)10000 << 20)) { v >>= 30; c = 'G'; }
  else if (v >= ((UInt64)10000 << 10)) { v >>= 20; c = 'M'; }
  else if (v >= ((UInt64)10000 <<  0)) { v >>= 10; c = 'K'; }
  s = ConvertUInt64ToString(v, s);
  if (c != 0)
  {
    *s++ = ' ';
    *s++ = (wchar_t)c;
    *s++ = 'B';
    *s = 0;
  }
  return s;
}
