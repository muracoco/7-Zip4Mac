// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include "FileOperations.h"
#include <QDialog>
#include <QMap>
#include <QPointer>
#include <functional>
class QTreeWidget;
class QTreeWidgetItem;
class QLineEdit;
class QPushButton;
class QLabel;
class QProcess;

class TemporaryFilesDialog final : public QDialog {
    Q_OBJECT
public:
    explicit TemporaryFilesDialog(QString helper, QWidget *parent = nullptr, QString root = {},
        std::function<QStringList()> protectedPaths = {});
    ~TemporaryFilesDialog() override;
    QString currentDirectory() const { return current; }
    void reject() override;
signals:
    void listed(bool success);
    void failed(QString message);
    void openRequested(QString path, bool fileManager);
    void trashed(QString source, QString trashPath);
protected:
    bool eventFilter(QObject *, QEvent *) override;
private:
    void load(QString path, QStringList selected = {}, QString focused = {});
    void stopScanner();
    void refresh();
    void up();
    void enter(Qt::KeyboardModifiers modifiers = {});
    void removeSelected();
    void showProperties();
    void openOutside(bool manager);
    void contextMenu(QPoint position);
    void updateButtons();
    bool protectedItem(QString path) const;
    QStringList selectedPaths() const;
    void report(QString message);
    QString executable, boundary, current, operationError;
    FileSnapshot rootSnapshot;
    QMap<QString, FileSnapshot> snapshots;
    std::function<QStringList()> protection;
    QPointer<QProcess> scanner, propertiesReader;
    FileOperations operations;
    QTreeWidget *files;
    QLineEdit *pathBox;
    QPushButton *removeButton, *refreshButton, *upButton;
    QLabel *status;
    bool reading = false, inspecting = false, closing = false, ascending = true, showDots = false;
    int sortedColumn = 1;
};
