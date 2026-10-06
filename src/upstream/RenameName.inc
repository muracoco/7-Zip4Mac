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

static bool IsCorrectFsName(const UString &name)
{
  const UString lastPart = name.Ptr((unsigned)(name.ReverseFind_PathSepar() + 1));
  return
      !lastPart.IsEqualTo(".") &&
      !lastPart.IsEqualTo("..");
}

bool CorrectFsPath(const UString & /* relBase */, const UString &path, UString &result)
{
  result = path;
  return true;
}
