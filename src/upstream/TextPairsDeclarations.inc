// TextPairs.h
// Copyright (C) 1999-2026 Igor Pavlov. GNU LGPL-2.1-or-later.
// Generated from official 7-Zip 26.03 by import-file-comments.py.
// QString UTF-16/method adapters; worker cancellation only.
// Original source SHA-256: ee3653ed58c3ea74225ef41fb2bdcfea35fe198562d60124faad6c8f94d641d1

class CPairsStorage
{
public:
  CObjectVector<CTextPair> Pairs;
  std::shared_ptr<OperationControl> control;
private:
  
  int FindID(const QString &id, unsigned &insertPos) const;
  int FindID(const QString &id) const;
  void Sort();
public:
  void Clear() { Pairs.Clear(); }
  bool ReadFromString(const QString &text);
  void SaveToString(QString &text) const;

  bool GetValue(const QString &id, QString &value) const;
  QString GetValue(const QString &id) const;
  void AddPair(const CTextPair &pair);
  void DeletePair(const QString &id);
};
