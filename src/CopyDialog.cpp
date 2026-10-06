// SPDX-License-Identifier: LGPL-3.0-or-later
#include "CopyDialog.h"
#include "ResourceDialogs.h"
#include "UiLanguage.h"
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSettings>

CopyDialog::CopyDialog(QString title, QString text, QString value, QString info, QStringList history, QWidget *parent, QString base) : QDialog(parent), path(new QComboBox(this)) {
    setObjectName("copyDialog"); setWindowTitle(title);
    auto label = new QLabel(text, this); label->setObjectName("copyLabel");
    path->setObjectName("copyDestination"); path->setEditable(true); path->setInsertPolicy(QComboBox::NoInsert); path->addItems(history); path->setEditText(value); label->setBuddy(path);
    auto browse = new QPushButton("...", this); browse->setObjectName("copyBrowse");
    auto summary = new QLabel(info, this); summary->setObjectName("copyInfo"); summary->setProperty("uiLiteral", true); summary->setTextFormat(Qt::PlainText);
    auto ok = new QPushButton(this), cancel = new QPushButton(this); ok->setObjectName("copyOk"); cancel->setObjectName("copyCancel"); UiLanguage::bind(ok, 401); UiLanguage::bind(cancel, 402);
    connect(ok, &QPushButton::clicked, this, &QDialog::accept); connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(browse, &QPushButton::clicked, this, [this, base] {
        // MyBrowseForFolder uses the OS Shell browser for ordinary Windows
        // paths. QFileDialog supplies the corresponding macOS folder browser.
        const auto initial = base.isEmpty() ? path->currentText() : QDir(base).absoluteFilePath(path->currentText());
        const auto chosen = QFileDialog::getExistingDirectory(this, UiLanguage::resource(6007), initial);
        if (!chosen.isEmpty()) { path->setCurrentIndex(-1); path->setEditText(chosen.endsWith('/') ? chosen : chosen + '/'); }
    });
    auto layout = new ResourceDialogLayout(this, 96); layout->bind("IDT_COPY", label); layout->bind("IDC_COPY", path); layout->bind("IDB_COPY_SET_PATH", browse); layout->bind("IDT_COPY_INFO", summary); layout->bind("IDOK", ok); layout->bind("IDCANCEL", cancel); layout->install();
    // CopyDialog.rc explicitly uses SS_LEFTNOWORDWRAP | SS_NOPREFIX.
    summary->setWordWrap(false); summary->setTextInteractionFlags(Qt::NoTextInteraction);
    ok->setDefault(true); path->setFocus(); path->lineEdit()->selectAll(); UiLanguage::apply(this);
}
void CopyDialog::remember(QString destination) {
    QSettings preferences; auto history = preferences.value("Copy/History").toStringList(); history.removeAll(destination); history.prepend(destination); while (history.size() > 20) history.removeLast(); preferences.setValue("Copy/History", history);
}
