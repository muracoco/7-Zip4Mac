// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include <QString>
#include <QList>
#include <QStringList>
#include <limits>

namespace CompressionMath {
constexpr quint64 Automatic = std::numeric_limits<quint64>::max();
struct Input {
    QString format = "7z", method = "LZMA2";
    int level = 5, threads = 0, cpus = 1;
    quint64 dictionary = Automatic, solid = Automatic;
    quint64 ram = 0, memoryLimit = 0;
};
struct Result {
    quint64 dictionary = 0, solid = 0, compressMemory = Automatic, decompressMemory = Automatic;
    unsigned word = 0, threads = 1, maximumThreads = 1;
};
struct Choice { QString text; quint64 value; bool automatic = false; };
struct NumericChoices { QList<Choice> items; int selected = -1; };
QList<int> levels(QString format);
quint32 levelNameId(int level);
QStringList methods(QString format, int level);
NumericChoices dictionaryChoices(Input input, quint64 saved = Automatic);
NumericChoices wordChoices(Input input, quint32 saved = quint32(-1));
QList<unsigned> threadChoices(Input input);
bool hasMemoryControl(QString format);
bool hasSolidControl(QString format);
Result calculate(Input input);
quint64 systemRam();
quint64 parseSize(QString text);
QString sizeText(quint64 bytes, bool rounded = false);
QList<quint64> dictionaries(QString method, QString format);
QList<unsigned> words(QString method, QString format);
}
