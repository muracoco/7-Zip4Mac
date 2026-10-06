// TextPairs.cpp
// Copyright (C) 1999-2026 Igor Pavlov. GNU LGPL-2.1-or-later.
// Generated from official 7-Zip 26.03 by import-file-comments.py.
// QString UTF-16/method adapters; worker cancellation only.
// Original source SHA-256: 578937fb064effd9977944d42d0a5602ab10ccc81b8724959b8063073d8f6478

static const QChar kNewLineChar = '\n';
static const QChar kQuoteChar = '\"';

static const QChar kBOM = QChar(0xFEFF);

static bool IsSeparatorChar(QChar c)
{
  return (c == ' ' || c == '\t');
}

static void RemoveCr(QString &s)
{
  s.remove(QChar('\r'));
}

static QString GetIDString(const QChar *srcString, unsigned &finishPos)
{
  QString result;
  bool quotes = false;
  for (finishPos = 0;;)
  {
    QChar c = srcString[finishPos];
    if (c.isNull())
      break;
    finishPos++;
    bool isSeparatorChar = IsSeparatorChar(c);
    if (c == kNewLineChar || (isSeparatorChar && !quotes)
        || (c == kQuoteChar && quotes))
      break;
    else if (c == kQuoteChar)
      quotes = true;
    else
      result += c;
  }
  result = trimmed(result);
  RemoveCr(result);
  return result;
}

static QString GetValueString(const QChar *srcString, unsigned &finishPos)
{
  QString result;
  for (finishPos = 0;;)
  {
    QChar c = srcString[finishPos];
    if (c.isNull())
      break;
    finishPos++;
    if (c == kNewLineChar)
      break;
    result += c;
  }
  result = trimmed(result);
  RemoveCr(result);
  return result;
}

static bool GetTextPairs(const QString &srcString, CObjectVector<CTextPair> &pairs, const std::shared_ptr<OperationControl> &control)
{
  pairs.Clear();
  unsigned pos = 0;
  
  if (srcString.size() > 0)
  {
    if (srcString[0] == kBOM)
      pos++;
  }
  while (pos < srcString.size())
  {
    if (stopped(control)) return false;
    unsigned finishPos;
    QString id = GetIDString(srcString.constData() + pos, finishPos);
    pos += finishPos;
    if (id.isEmpty())
      continue;
    QString value = GetValueString(srcString.constData() + pos, finishPos);
    pos += finishPos;
    if (!id.isEmpty())
    {
      CTextPair pair;
      pair.name = id;
      pair.value = value;
      pairs.Add(pair);
    }
  }
  return true;
}

static int ComparePairIDs(const QString &s1, const QString &s2)
  { return MyStringCompareNoCase(s1, s2); }

static int ComparePairItems(const CTextPair &p1, const CTextPair &p2)
  { return ComparePairIDs(p1.name, p2.name); }

static int ComparePairItems(void *const *a1, void *const *a2, void * /* param */)
  { return ComparePairItems(**(const CTextPair *const *)a1, **(const CTextPair *const *)a2); }

void CPairsStorage::Sort() { Pairs.Sort(ComparePairItems, NULL); }

int CPairsStorage::FindID(const QString &id, unsigned &insertPos) const
{
  unsigned left = 0, right = Pairs.Size();
  while (left != right)
  {
    const unsigned mid = (left + right) / 2;
    const int compResult = ComparePairIDs(id, Pairs[mid].name);
    if (compResult == 0)
    {
      insertPos = mid; // to disable GCC warning
      return (int)mid;
    }
    if (compResult < 0)
      right = mid;
    else
      left = mid + 1;
  }
  insertPos = left;
  return -1;
}

int CPairsStorage::FindID(const QString &id) const
{
  unsigned pos;
  return FindID(id, pos);
}

void CPairsStorage::AddPair(const CTextPair &pair)
{
  unsigned insertPos;
  const int pos = FindID(pair.name, insertPos);
  if (pos >= 0)
    Pairs[pos] = pair;
  else
    Pairs.Insert(insertPos, pair);
}

void CPairsStorage::DeletePair(const QString &id)
{
  const int pos = FindID(id);
  if (pos >= 0)
    Pairs.Delete((unsigned)pos);
}

bool CPairsStorage::GetValue(const QString &id, QString &value) const
{
  value.clear();
  const int pos = FindID(id);
  if (pos < 0)
    return false;
  value = Pairs[pos].value;
  return true;
}

QString CPairsStorage::GetValue(const QString &id) const
{
  const int pos = FindID(id);
  if (pos < 0)
    return QString();
  return Pairs[pos].value;
}

bool CPairsStorage::ReadFromString(const QString &text)
{
  bool result = GetTextPairs(text, Pairs, control);
  if (result)
    Sort();
  else
    Pairs.Clear();
  return result;
}

void CPairsStorage::SaveToString(QString &text) const
{
  FOR_VECTOR (i, Pairs)
  {
    const CTextPair &pair = Pairs[i];
    bool multiWord = (pair.name.indexOf(QChar(' ')) >= 0);
    if (multiWord)
      text.append(QChar('\"'));
    text += pair.name;
    if (multiWord)
      text.append(QChar('\"'));
    text.append(QChar(' '));
    text += pair.value;
    text.append(QChar('\x0D'));
    text.append(QChar('\n'));
  }
}
