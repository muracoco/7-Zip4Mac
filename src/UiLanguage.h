// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include <QString>
#include <QList>
#include <QPair>
#include <QStringList>
#include <QLocale>
class QWidget;
class QObject;
namespace UiLanguage {
void set(const QString &language);
QString text(const QString &english);
QString resource(quint32 id, bool localized = true);
QString localizedResource(quint32 id);
// Combo item roles for source resource IDs and literal prefixes (e.g. "5 - ").
constexpr int ResourceIdRole = 0x100 + 101, PrefixRole = 0x100 + 102;
QString propertyName(quint32 id, const QString &fallback, bool localized = true);
QList<QPair<QString, QString>> available();
QString information(const QString &language);
QStringList invalidFiles();
QString directory();
QString defaultLanguage(const QLocale &locale = QLocale());
void bind(QObject *object, quint32 resourceId, bool removeMnemonic = false);
void bindTitle(QWidget *widget, quint32 resourceId);
void bindFormLabel(QWidget *field, quint32 resourceId);
void apply(QWidget *widget);
}
