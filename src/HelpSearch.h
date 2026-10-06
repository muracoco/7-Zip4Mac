// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include <QStringList>
#include <QList>
#include <QHash>
namespace HelpSearch {
struct Topic { QString path, title, text; };
struct Options { bool titlesOnly = false, similarWords = true; QStringList previous; bool withinPrevious = false; };
struct Hit { QString path, title; int occurrences = 0; };
struct Results { QList<Hit> hits; QString error; };
Results search(const QString &query, const QList<Topic> &topics, const Options &options = {});
QStringList words(const QString &text);
QString lemma(const QString &word);
QHash<QString, QString> lemmas(const QStringList &words);
}
