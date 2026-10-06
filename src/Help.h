// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include <QDialog>
#include <QUrl>
#include <QStringList>
#include "HelpSearch.h"
class QTextBrowser;
class QPrinter;
class HelpWindow;
namespace Help {
QUrl resolve(const QUrl &url, const QUrl &base = QUrl("qrc:/help/fm/index.htm"));
QStringList pages();
QByteArray resource(const QUrl &url);
bool external(const QUrl &url);
void show(QWidget *parent, const QString &topic = "fm/index.htm");
HelpWindow *currentWindow();
void about(QWidget *parent);
QList<HelpSearch::Topic> topics();
QStringList printTopics(const QUrl &source, bool subtopics);
}
class HelpWindow : public QDialog {
    Q_OBJECT
public:
    explicit HelpWindow(QWidget *parent = nullptr);
    bool navigate(const QUrl &url);
    QUrl source() const;
    bool print(QPrinter *printer, bool subtopics = false);
signals:
    void externalLinkRequested(QUrl url);
    void searchFinished(int topics);
private:
    QTextBrowser *browser;
};
class AboutDialog : public QDialog {
    Q_OBJECT
public:
    explicit AboutDialog(QWidget *parent = nullptr);
signals:
    void homePageRequested(QUrl url);
};
