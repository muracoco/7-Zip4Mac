// Copyright (C) 2026 7-Zip Mac Port contributors
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include <QDialog>
#include <QMap>
#include <QPointer>
#include <QTimer>
#include <functional>

class MainWindow;
class QMenu;
struct OpenWithItem { QString id, label; };
struct OpenWithSettings {
    QStringList enabled;
    bool icons = true;
    static QList<OpenWithItem> items();
    static OpenWithSettings load();
    bool save() const;
};
QString openWithArchiveName(const QStringList &paths);
QString openWithExtractName(const QString &path);
QString openWithCommonParent(const QStringList &paths);
bool openWithArchiveCandidate(const QString &path);
QString openWithItemLabel(const OpenWithItem &item, QString name = "<name>");
QList<QPair<QString, QString>> checksumMethods();
void populateOpenWithContextMenu(QMenu *menu, const QStringList &paths, std::function<void(QString)> chosen);

class OpenWithMenu : public QDialog {
    Q_OBJECT
public:
    explicit OpenWithMenu(QStringList paths, QWidget *parent = nullptr);
    QStringList paths() const { return targets; }
signals:
    void commandChosen(QString command, QStringList paths);
protected:
    void showEvent(QShowEvent *) override;
private:
    QStringList targets;
};

class OpenWithController : public QObject {
    Q_OBJECT
public:
    explicit OpenWithController(MainWindow *window, bool quitWhenIdle = false);
    void enqueue(QStringList paths);
    bool hasRequests() const { return !pending.isEmpty() || menu; }
    bool receivedRequests() const { return received; }
    OpenWithMenu *currentMenu() const { return menu; }
protected:
    bool eventFilter(QObject *, QEvent *) override;
private:
    void flush();
    void checkIdleExit();
    MainWindow *window;
    bool quitWhenIdle;
    bool received = false;
    QStringList pending;
    QTimer timer, idleExitTimer;
    QPointer<OpenWithMenu> menu;
};
