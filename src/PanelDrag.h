// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include <QMimeData>
#include <QPointer>
#include <QPoint>
#include <QObject>
#include <functional>
#include <memory>
class MainWindow;
class QAbstractItemView;
class QDrag;
class QDropEvent;
class QTemporaryDir;
class PanelDragMimeData : public QMimeData {
    Q_OBJECT
public:
    PanelDragMimeData(MainWindow *source, const QList<QUrl> &urls, bool archive);
    QPointer<MainWindow> source;
    bool archive;
    bool rightButton = false;
};
Qt::DropAction officialPanelDropEffect(Qt::KeyboardModifiers keys, Qt::DropActions allowed, bool sameDrive);
class PanelDragController : public QObject {
    Q_OBJECT
public:
    explicit PanelDragController(MainWindow *window);
    void start(QStringList paths, std::shared_ptr<QTemporaryDir> temporary = {});
    // Native drag execution is replaceable so locked-session tests can inspect
    // the real MIME payload and feed it through another production viewport.
    void setExecutor(std::function<Qt::DropAction(QDrag *)> value) { executor = std::move(value); }
protected:
    bool eventFilter(QObject *object, QEvent *event) override;
private:
    MainWindow *window;
    QPointer<MainWindow> transferSource;
    std::function<Qt::DropAction(QDrag *)> executor;
    QPoint pressPosition;
    bool rightPressed = false, rightStarted = false, suppressContext = false;
    Qt::DropAction effect(QAbstractItemView *view, QDropEvent *event) const;
    void drop(QAbstractItemView *view, QDropEvent *event);
    QString target(QAbstractItemView *view, QPoint position) const;
};
