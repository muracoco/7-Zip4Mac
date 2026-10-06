// SPDX-License-Identifier: LGPL-3.0-or-later
#include "FileToolDialogs.h"
#include "UiLanguage.h"
#include <QButtonGroup>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QGroupBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRadioButton>
#include <QSettings>
#include <QVBoxLayout>

namespace {
QWidget *folderControl(QComboBox *path, QWidget *parent) {
    auto row = new QWidget(parent); auto layout = new QHBoxLayout(row); layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(path, 1); auto browse = new QPushButton("...", row); browse->setFixedWidth(30); layout->addWidget(browse);
    QObject::connect(browse, &QPushButton::clicked, path, [path, parent] { auto chosen = QFileDialog::getExistingDirectory(parent, UiLanguage::text("Set Folder"), path->currentText()); if (!chosen.isEmpty()) path->setEditText(chosen); });
    return row;
}
void remember(QString key, QString path) {
    QSettings s; auto history = s.value(key).toStringList(); history.removeAll(path); history.prepend(path);
    while (history.size() > 32) history.removeLast(); s.setValue(key, history);
}
}
SplitDialog::SplitDialog(QString file, QString destination, QWidget *parent) : QDialog(parent), source(file) {
    setObjectName("splitDialog"); setWindowTitle("Split File"); resize(530, 195);
    auto layout = new QVBoxLayout(this); layout->addWidget(new QLabel(file, this));
    auto form = new QFormLayout; path = new QComboBox(this); path->setEditable(true); path->setObjectName("splitDestination"); path->addItems(QSettings().value("Split/History").toStringList()); path->setEditText(destination);
    form->addRow("Split to:", folderControl(path, this));
    sizes = new QComboBox(this); sizes->setEditable(true); sizes->setObjectName("splitSizes");
    sizes->addItems({"10M", "100M", "1000M", "650M - CD", "700M - CD", "4092M - FAT", "4480M - DVD", "8128M - DVD DL", "23040M - BD"});
    sizes->setEditText(QSettings().value("Split/Sizes", "10M").toString()); form->addRow("Split to volumes, bytes:", sizes); layout->addLayout(form);
    auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this); connect(buttons, &QDialogButtonBox::accepted, this, &SplitDialog::accept); connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject); layout->addWidget(buttons); UiLanguage::apply(this);
}
QString SplitDialog::destination() const { return path->currentText(); }
QList<quint64> SplitDialog::volumeSizes() const { return FileOperations::parseVolumeSizes(sizes->currentText()); }
void SplitDialog::accept() {
    QString error; const auto values = FileOperations::parseVolumeSizes(sizes->currentText(), &error);
    if (error.isEmpty() && quint64(QFileInfo(source).size()) <= values.first()) error = "File must be larger than the first volume size.";
    if (error.isEmpty() && destination().trimmed().isEmpty()) error = "Specify a destination folder.";
    if (!error.isEmpty()) { QMessageBox::warning(this, "7-Zip", error); return; }
    const auto count = FileOperations::volumeCount(quint64(QFileInfo(source).size()), values);
    if (count >= 100 && QMessageBox::question(this, "7-Zip", "Number of volumes: " + QString::number(count), QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) return;
    remember("Split/History", destination()); QSettings().setValue("Split/Sizes", sizes->currentText()); QDialog::accept();
}
CombineDialog::CombineDialog(QString firstVolume, QString destination, QWidget *parent) : CopyDialog(UiLanguage::resource(7400) + ' ' + QFileInfo(firstVolume).fileName(), UiLanguage::resource(7401), destination, combineItemsInfo(FileOperations::inspectVolumes(firstVolume).volumes, FileOperations::inspectVolumes(firstVolume).totalSize, QFileInfo(firstVolume).absolutePath() + '/'), {}, parent, QFileInfo(firstVolume).absolutePath()) {
    setObjectName("combineDialog"); path->setObjectName("combineDestination");
}
QString CombineDialog::destination() const { return textValue(); }
LinkDialog::LinkDialog(QString source, QString directory, QWidget *parent, QString currentDirectory) : QDialog(parent), base(currentDirectory.isEmpty() ? QFileInfo(source).absolutePath() : currentDirectory) {
    setObjectName("linkDialog"); setWindowTitle("Link"); resize(530, 340); auto layout = new QVBoxLayout(this);
    original = FileOperations::inspectSymbolicLink(source);
    auto pathControl = [this, layout](QString label, QString name, QString value, QLineEdit *&edit) {
        layout->addWidget(new QLabel(label, this)); auto row = new QHBoxLayout;
        auto combo = new QComboBox(this); combo->setObjectName(name + "Combo"); combo->setEditable(true); combo->setEditText(value); edit = combo->lineEdit(); edit->setObjectName(name); row->addWidget(combo, 1);
        auto browse = new QPushButton("...", this); browse->setObjectName(name + "Browse"); browse->setFixedWidth(30); row->addWidget(browse); layout->addLayout(row);
        connect(browse, &QPushButton::clicked, this, [this, combo] {
            auto chosen = QFileDialog::getExistingDirectory(this, UiLanguage::text("Set Folder"), QDir(base).absoluteFilePath(combo->currentText()));
            if (!chosen.isEmpty()) combo->setEditText(chosen.endsWith('/') ? chosen : chosen + '/');
        });
    };
    pathControl("Link from:", "linkFrom", original.symbolic ? source : directory.endsWith('/') ? directory : directory + '/', from);
    pathControl("Link to:", "linkTo", original.symbolic ? QString::fromUtf8(original.target) : source, to);
    auto current = new QLabel(original.symbolic ? (original.error.isEmpty() ? QString::fromUtf8(original.target) : "ERROR: " + original.error) : QString(), this);
    current->setObjectName("linkCurrentTarget"); current->setProperty("uiLiteral", true); current->setTextFormat(Qt::PlainText); current->setWordWrap(true); current->setMinimumHeight(20); layout->addWidget(current);
    auto group = new QGroupBox("Link Type", this); auto groupLayout = new QVBoxLayout(group); layout->addWidget(group); types = new QButtonGroup(this);
    const QStringList labels{"Hard Link", "File Symbolic Link", "Directory Symbolic Link"};
    for (int n = 0; n < labels.size(); ++n) { auto radio = new QRadioButton(labels[n], group); radio->setObjectName("linkType" + QString::number(n)); types->addButton(radio, n); groupLayout->addWidget(radio); }
    types->button(QFileInfo(source).isDir() ? 2 : original.symbolic ? 1 : 0)->setChecked(true);
    auto buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, this); auto create = buttons->addButton("Link", QDialogButtonBox::AcceptRole); create->setObjectName("createLink"); connect(buttons, &QDialogButtonBox::accepted, this, &LinkDialog::accept); connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject); layout->addWidget(buttons); UiLanguage::apply(this);
}
void LinkDialog::accept() {
    if (from->text().isEmpty()) return;
    QString destination = QDir(base).absoluteFilePath(from->text()); QString target = to->text(); auto type = LinkType(types->checkedId());
    if (type == LinkType::Hard) target = QDir(base).absoluteFilePath(target);
    const auto error = original.symbolic && QDir::cleanPath(destination) == QDir::cleanPath(original.path) ? FileOperations::editSymbolicLink(original, target, type) : FileOperations::createLink(destination, target, type);
    if (!error.isEmpty()) { QMessageBox::warning(this, "7-Zip", error); return; } QDialog::accept();
}
