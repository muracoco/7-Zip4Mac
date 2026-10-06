// SPDX-License-Identifier: LGPL-3.0-or-later
#include <QLocale>
#include "FileToolDialogs.h"
#include "MainWindow.h"
#include "PortStyle.h"
#include "UiLanguage.h"
#include <QApplication>
#include <QComboBox>
#include <QFile>
#include <QFileDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRadioButton>
#include <QSettings>
#include <QScopeGuard>
#include <QTemporaryDir>
#include <QTest>
#include <QUuid>
#include <cstring>
#include <fcntl.h>
#include <sys/xattr.h>
#include <unistd.h>

static QString executable;
static bool write(QString path, QByteArray bytes) { QDir().mkpath(QFileInfo(path).absolutePath()); QFile file(path); return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size(); }
static QByteArray read(QString path) { QFile file(path); return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray(); }
static bool symlink(QString path, QByteArray target) { return ::symlink(target.constData(), QFile::encodeName(path).constData()) == 0; }
static QByteArray raw(QString path) { QByteArray bytes(4096, Qt::Uninitialized); const auto count = ::readlink(QFile::encodeName(path).constData(), bytes.data(), bytes.size()); if (count < 0) return {}; bytes.resize(count); return bytes; }
static bool focus(MainWindow &window, QString name, bool selected = true) {
    auto second = window.findChild<MainWindow *>("secondPanel"); FileList *tree = nullptr;
    for (auto list : window.findChildren<FileList *>("fileList")) if (!second || !second->isAncestorOf(list)) tree = list;
    if (!tree) return false; const auto rows = tree->findItems(name, Qt::MatchExactly); if (rows.size() != 1) return false;
    tree->clearSelection(); tree->setCurrentItem(rows.first(), 0, QItemSelectionModel::NoUpdate); rows.first()->setSelected(selected); tree->setFocus(); return true;
}
class LinkTests : public QObject {
    Q_OBJECT
    QTemporaryDir temporary;
    QString root;
    QString file(QString name) const { return root + '/' + name; }
    void untouchedTargets() { QCOMPARE(read(file("first.txt")), QByteArray("first data\n")); QCOMPARE(read(file("日本語 second.txt")), QByteArray("second data\n")); }
private slots:
    void initTestCase() {
        QVERIFY(temporary.isValid()); QSettings::setDefaultFormat(QSettings::IniFormat); QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, temporary.filePath("preferences"));
        qInfo() << "Owned symbolic-link fixtures:" << temporary.path();
    }
    void init() {
        root = temporary.filePath(QUuid::createUuid().toString(QUuid::WithoutBraces)); QVERIFY(QDir().mkpath(root));
        QVERIFY(write(file("first.txt"), "first data\n")); QVERIFY(write(file("日本語 second.txt"), "second data\n")); QVERIFY(QDir().mkpath(file("folder one"))); QVERIFY(QDir().mkpath(file("folder two")));
        QSettings().clear(); QSettings().setValue("View/LastPath", root); QSettings().setValue("View/AutoRefresh", false); FileManagerSettings settings; settings.realIcons = false; settings.language = "en"; QVERIFY(settings.save()); UiLanguage::set("en");
    }
    void relativeAbsoluteDanglingAndLoop_data() {
        QTest::addColumn<QByteArray>("before"); QTest::addColumn<bool>("absolute");
        QTest::newRow("relative") << QByteArray("first.txt") << false; QTest::newRow("absolute") << QByteArray("first.txt") << true;
        QTest::newRow("dangling") << QByteArray("missing.txt") << false; QTest::newRow("self-loop") << QByteArray("link") << false; QTest::newRow("mutual-loop") << QByteArray("loop peer") << false;
    }
    void relativeAbsoluteDanglingAndLoop() {
        QFETCH(QByteArray, before); QFETCH(bool, absolute); if (absolute) before = QFile::encodeName(file(QString::fromUtf8(before)));
        if (before == "loop peer") QVERIFY(symlink(file("loop peer"), "link"));
        const auto path = file("link"); QVERIFY(symlink(path, before)); auto original = FileOperations::inspectSymbolicLink(path); QVERIFY2(original.error.isEmpty(), qPrintable(original.error)); QVERIFY(original.symbolic); QCOMPARE(original.target, before);
        const auto to = absolute ? file("日本語 second.txt") : QString("日本語 second.txt"); const auto error = FileOperations::editSymbolicLink(original, to, LinkType::SymbolicFile); QVERIFY2(error.isEmpty(), qPrintable(error)); QCOMPARE(raw(path), QFile::encodeName(to)); QCOMPARE(read(path), QByteArray("second data\n")); untouchedTargets();
        QVERIFY(QDir(root).entryList({".7zip-link-edit-*"}, QDir::Dirs | QDir::Hidden).isEmpty());
    }
    void directoryAndTypeProtection() {
        const auto path = file("dir link"); QVERIFY(symlink(path, "folder one")); auto original = FileOperations::inspectSymbolicLink(path);
        QVERIFY(!FileOperations::editSymbolicLink(original, "first.txt", LinkType::SymbolicDirectory).isEmpty()); QCOMPARE(raw(path), QByteArray("folder one"));
        QVERIFY(FileOperations::editSymbolicLink(original, "folder two", LinkType::SymbolicDirectory).isEmpty()); QCOMPARE(raw(path), QByteArray("folder two")); QVERIFY(QFileInfo(path).isDir());
        original = FileOperations::inspectSymbolicLink(path); QVERIFY(!FileOperations::editSymbolicLink(original, "folder one", LinkType::SymbolicFile).isEmpty()); QVERIFY(!FileOperations::editSymbolicLink(original, "first.txt", LinkType::Hard).isEmpty());
        QVERIFY(!FileOperations::editSymbolicLink(original, {}, LinkType::SymbolicDirectory).isEmpty()); QCOMPARE(raw(path), QByteArray("folder two")); untouchedTargets();
    }
    void externalMutationRefused_data() { QTest::addColumn<QString>("change"); for (auto name : {"target", "file", "directory", "removed", "parent", "metadata"}) QTest::newRow(name) << QString(name); }
    void externalMutationRefused() {
        QFETCH(QString, change); const auto path = file("link"); QVERIFY(symlink(path, "first.txt")); const auto original = FileOperations::inspectSymbolicLink(path);
        if (change == "target") { QVERIFY(QFile::remove(path)); QVERIFY(symlink(path, "external missing")); }
        else if (change == "file") { QVERIFY(QFile::remove(path)); QVERIFY(write(path, "external data")); }
        else if (change == "directory") { QVERIFY(QFile::remove(path)); QVERIFY(QDir().mkdir(path)); QVERIFY(write(path + "/keep.txt", "external directory data")); }
        else if (change == "removed") QVERIFY(QFile::remove(path));
        else if (change == "parent") { QVERIFY(QDir().rename(root, root + "-old")); QVERIFY(QDir().mkpath(root)); QVERIFY(symlink(path, "parent replacement")); }
        else { const timespec times[2]{{1700000000, 100}, {1700000000, 200}}; QVERIFY(::utimensat(AT_FDCWD, QFile::encodeName(path).constData(), times, AT_SYMLINK_NOFOLLOW) == 0); }
        const auto error = FileOperations::editSymbolicLink(original, "日本語 second.txt", LinkType::SymbolicFile); QVERIFY2(error.contains("changed"), qPrintable(error));
        if (change == "file") QCOMPARE(read(path), QByteArray("external data")); else if (change == "directory") QCOMPARE(read(path + "/keep.txt"), QByteArray("external directory data"));
        else if (change == "target") QCOMPARE(raw(path), QByteArray("external missing")); else if (change == "parent") { QCOMPARE(raw(path), QByteArray("parent replacement")); QCOMPARE(raw(root + "-old/link"), QByteArray("first.txt")); }
        else if (change == "removed") QVERIFY(!QFileInfo(path).isSymLink()); else QCOMPARE(raw(path), QByteArray("first.txt"));
    }
    void failedPreparationAndHardLinkedProtection() {
        const auto path = file("link"); QVERIFY(symlink(path, "first.txt")); auto original = FileOperations::inspectSymbolicLink(path);
        auto error = FileOperations::editSymbolicLink(original, QString(6000, 'x'), LinkType::SymbolicFile); QVERIFY(!error.isEmpty()); QCOMPARE(raw(path), original.target);
        QVERIFY(::linkat(AT_FDCWD, QFile::encodeName(path).constData(), AT_FDCWD, QFile::encodeName(file("second name")).constData(), 0) == 0); original = FileOperations::inspectSymbolicLink(path); QCOMPARE(original.link.st_nlink, nlink_t(2));
        QVERIFY(FileOperations::editSymbolicLink(original, "日本語 second.txt", LinkType::SymbolicFile).contains("hard-linked")); QCOMPARE(raw(path), QByteArray("first.txt")); QCOMPARE(raw(file("second name")), QByteArray("first.txt")); untouchedTargets();
    }
    void installationFailuresAndRaces_data() {
        QTest::addColumn<QString>("fault");
        for (auto name : {"parent-before", "parent-after", "rename-failure", "rollback", "rollback-io-failure", "rollback-foreign-entry", "stage-name-replaced"}) QTest::newRow(name) << QString(name);
    }
    void installationFailuresAndRaces() {
        QFETCH(QString, fault); const auto path = file("link"); QVERIFY(symlink(path, "first.txt")); const auto original = FileOperations::inspectSymbolicLink(path);
        QString stagePath, foreignStage; bool injected = false;
        const auto error = FileOperations::editSymbolicLink(original, "missing new target", LinkType::SymbolicFile, [&](LinkEditPhase phase, const QString &stage) {
            stagePath = stage;
            if ((fault == "parent-before" && phase == LinkEditPhase::Prepared) || (fault == "parent-after" && phase == LinkEditPhase::Exchanged)) {
                QVERIFY(QDir().rename(root, root + "-old")); QVERIFY(QDir().mkpath(root)); QVERIFY(write(path, "foreign parent data"));
                foreignStage = file(QFileInfo(stage).fileName()); QVERIFY(write(foreignStage + "/keep.txt", "foreign stage data")); injected = true;
            } else if (fault == "rename-failure" && phase == LinkEditPhase::Validated) { QVERIFY(QFile::remove(stage + "/replacement")); injected = true; }
            else if (QStringList{"rollback", "rollback-io-failure", "rollback-foreign-entry"}.contains(fault) && phase == LinkEditPhase::Validated) { QVERIFY(QFile::remove(path)); QVERIFY(write(path, "external data")); injected = true; }
            else if (fault == "rollback-io-failure" && phase == LinkEditPhase::Rollback) { QVERIFY(::geteuid() != 0); QVERIFY(::chmod(QFile::encodeName(root).constData(), 0555) == 0); }
            else if (fault == "rollback-foreign-entry" && phase == LinkEditPhase::Rollback) { QVERIFY(QFile::remove(path)); QVERIFY(write(path, "later external data")); }
            else if (fault == "stage-name-replaced" && phase == LinkEditPhase::Prepared) { QVERIFY(QDir().rename(stage, stage + "-moved")); QVERIFY(write(stage + "/keep.txt", "foreign stage data")); foreignStage = stage; injected = true; }
        });
        QVERIFY(injected); if (fault == "rollback-io-failure") QVERIFY(::chmod(QFile::encodeName(root).constData(), 0700) == 0);
        if (fault == "stage-name-replaced") { QVERIFY2(error.isEmpty(), qPrintable(error)); QCOMPARE(raw(path), QByteArray("missing new target")); QCOMPARE(read(foreignStage + "/keep.txt"), QByteArray("foreign stage data")); return; }
        QVERIFY2(!error.isEmpty(), qPrintable(error));
        if (fault.startsWith("parent-")) {
            QCOMPARE(read(path), QByteArray("foreign parent data")); QCOMPARE(read(foreignStage + "/keep.txt"), QByteArray("foreign stage data")); QCOMPARE(raw(root + "-old/link"), QByteArray("first.txt"));
            QVERIFY(QDir(root + "-old").entryList({".7zip-link-edit-*"}, QDir::Dirs | QDir::Hidden).isEmpty());
        } else if (fault == "rename-failure") { QVERIFY(error.contains("Cannot install")); QCOMPARE(raw(path), QByteArray("first.txt")); }
        else if (fault == "rollback") { QVERIFY(error.contains("previous entry restored")); QCOMPARE(read(path), QByteArray("external data")); }
        else { QVERIFY2(error.contains("Recovery files retained"), qPrintable(error)); QCOMPARE(read(stagePath + "/replacement"), QByteArray("external data")); if (fault == "rollback-foreign-entry") QCOMPARE(read(path), QByteArray("later external data")); else QCOMPARE(raw(path), QByteArray("missing new target")); }
        if (QStringList{"rename-failure", "rollback"}.contains(fault)) QVERIFY(QDir(root).entryList({".7zip-link-edit-*"}, QDir::Dirs | QDir::Hidden).isEmpty());
    }
    void metadataPreservedWithoutFollowingTargets() {
        const auto path = file("link"); QVERIFY(symlink(path, "first.txt")); const auto native = QFile::encodeName(path);
        const timespec times[2]{{1700000000, 100}, {1700000000, 200}}; QVERIFY(::utimensat(AT_FDCWD, native.constData(), times, AT_SYMLINK_NOFOLLOW) == 0);
        // A permitted macOS metadata name on a symlink; neither target receives it.
        const char attribute[] = "com.apple.metadata:7ZipMacPortLinkTest", value[] = "link attribute";
        QVERIFY2(::setxattr(native.constData(), attribute, value, sizeof(value), 0, XATTR_NOFOLLOW) == 0, strerror(errno));
        const auto original = FileOperations::inspectSymbolicLink(path); struct stat first{}, second{}; QVERIFY(::stat(QFile::encodeName(file("first.txt")).constData(), &first) == 0); QVERIFY(::stat(QFile::encodeName(file("日本語 second.txt")).constData(), &second) == 0);
        const auto error = FileOperations::editSymbolicLink(original, "日本語 second.txt", LinkType::SymbolicFile); QVERIFY2(error.isEmpty(), qPrintable(error)); struct stat after{}, firstAfter{}, secondAfter{}; QVERIFY(::lstat(native.constData(), &after) == 0);
        QCOMPARE(after.st_uid, original.link.st_uid); QCOMPARE(after.st_gid, original.link.st_gid); QCOMPARE(after.st_mode, original.link.st_mode); QCOMPARE(after.st_mtimespec.tv_sec, original.link.st_mtimespec.tv_sec); QCOMPARE(after.st_mtimespec.tv_nsec, original.link.st_mtimespec.tv_nsec);
        char stored[128]{}; QCOMPARE(::getxattr(native.constData(), attribute, stored, sizeof(stored), 0, XATTR_NOFOLLOW), ssize_t(sizeof(value))); QCOMPARE(QByteArray(stored, sizeof(value)), QByteArray(value, sizeof(value)));
        QVERIFY(::stat(QFile::encodeName(file("first.txt")).constData(), &firstAfter) == 0); QVERIFY(::stat(QFile::encodeName(file("日本語 second.txt")).constData(), &secondAfter) == 0); QCOMPARE(first.st_mtimespec.tv_nsec, firstAfter.st_mtimespec.tv_nsec); QCOMPARE(second.st_mtimespec.tv_nsec, secondAfter.st_mtimespec.tv_nsec);
        QVERIFY(::getxattr(QFile::encodeName(file("first.txt")).constData(), attribute, stored, sizeof(stored), 0, 0) < 0); QVERIFY(::getxattr(QFile::encodeName(file("日本語 second.txt")).constData(), attribute, stored, sizeof(stored), 0, 0) < 0); untouchedTargets();
    }
    void invalidBytesAndOrdinaryFiles() {
        const auto path = file("invalid link"); QVERIFY(symlink(path, QByteArray("bad\xff", 4))); auto original = FileOperations::inspectSymbolicLink(path); QVERIFY(original.symbolic); QVERIFY(!original.error.isEmpty()); QVERIFY(!FileOperations::editSymbolicLink(original, "first.txt", LinkType::SymbolicFile).isEmpty()); QCOMPARE(raw(path), QByteArray("bad\xff", 4));
        original = FileOperations::inspectSymbolicLink(file("first.txt")); QVERIFY(!original.symbolic); QVERIFY(!FileOperations::editSymbolicLink(original, "日本語 second.txt", LinkType::SymbolicFile).isEmpty());
        QVERIFY(!FileOperations::createLink(file("first.txt"), "日本語 second.txt", LinkType::SymbolicFile).isEmpty()); QVERIFY(!FileOperations::createLink(file("folder one"), "folder two", LinkType::SymbolicDirectory).isEmpty());
        QVERIFY(!FileOperations::createLink(file("NUL link"), QString("first.txt") + QChar::Null + "extra", LinkType::SymbolicFile).isEmpty()); QVERIFY(!QFileInfo(file("NUL link")).isSymLink()); untouchedTargets();
    }
    void dialogDefaultsEditAndCancel() {
        LinkDialog regular(file("first.txt"), file("folder one")); QCOMPARE(regular.findChild<QLineEdit *>("linkFrom")->text(), file("folder one") + '/'); QVERIFY(regular.findChild<QRadioButton *>("linkType0")->isChecked()); QVERIFY(regular.findChild<QComboBox *>("linkFromCombo")->isEditable());
        LinkDialog directory(file("folder one"), root); QVERIFY(directory.findChild<QRadioButton *>("linkType2")->isChecked());
        const auto path = file("link"); QVERIFY(symlink(path, "first.txt")); LinkDialog edit(path, file("folder one")); QCOMPARE(edit.findChild<QLineEdit *>("linkFrom")->text(), path); QCOMPARE(edit.findChild<QLineEdit *>("linkTo")->text(), QString("first.txt")); QCOMPARE(edit.findChild<QLabel *>("linkCurrentTarget")->text(), QString("first.txt")); QVERIFY(edit.findChild<QRadioButton *>("linkType1")->isChecked());
        edit.findChild<QLineEdit *>("linkTo")->setText("日本語 second.txt"); edit.show(); QTest::mouseClick(edit.findChild<QPushButton *>("createLink"), Qt::LeftButton); QCOMPARE(edit.result(), int(QDialog::Accepted)); QCOMPARE(raw(path), QFile::encodeName("日本語 second.txt"));
        LinkDialog cancel(path, root); cancel.findChild<QLineEdit *>("linkTo")->setText("first.txt"); cancel.reject(); QCOMPARE(raw(path), QFile::encodeName("日本語 second.txt")); untouchedTargets();
        QVERIFY(symlink(file("translated link"), "Cancel")); UiLanguage::set("ja"); LinkDialog translated(file("translated link"), root); QCOMPARE(translated.findChild<QLabel *>("linkCurrentTarget")->text(), QString("Cancel")); UiLanguage::set("en");
        QVERIFY(symlink(file("markup link"), "<b>missing</b>")); LinkDialog markup(file("markup link"), root); auto label = markup.findChild<QLabel *>("linkCurrentTarget"); QCOMPARE(label->text(), QString("<b>missing</b>")); QCOMPARE(label->textFormat(), Qt::PlainText);
    }
    void folderBrowse_data() { QTest::addColumn<QString>("field"); QTest::newRow("from") << QString("linkFrom"); QTest::newRow("to") << QString("linkTo"); }
    void folderBrowse() {
        QFETCH(QString, field);
        // A programmatic QFileDialog::accept does not click the native NSOpenPanel.
        // Exercise Qt folder choice and the Link adapter; physical native choice is separate.
        const bool nativeDisabled = QApplication::testAttribute(Qt::AA_DontUseNativeDialogs);
        QApplication::setAttribute(Qt::AA_DontUseNativeDialogs, true);
        auto restoreDialogs = qScopeGuard([&] { QApplication::setAttribute(Qt::AA_DontUseNativeDialogs, nativeDisabled); });
        LinkDialog dialog(file("first.txt"), root); dialog.show(); bool chosen = false;
        QTimer select, limit; select.setInterval(10); limit.setSingleShot(true); connect(&select, &QTimer::timeout, &dialog, [&] {
            if (auto chooser = dialog.findChild<QFileDialog *>(); chooser && chooser->isVisible()) { select.stop(); QCOMPARE(chooser->fileMode(), QFileDialog::Directory); auto name = chooser->findChild<QLineEdit *>("fileNameEdit"); QVERIFY(name); name->setText(file("folder two")); QMetaObject::invokeMethod(chooser, "accept", Qt::QueuedConnection); chosen = true; }
        }); connect(&limit, &QTimer::timeout, &dialog, [&] { for (auto chooser : dialog.findChildren<QFileDialog *>()) chooser->reject(); }); select.start(); limit.start(5000);
        QTest::mouseClick(dialog.findChild<QPushButton *>(field + "Browse"), Qt::LeftButton); select.stop(); limit.stop(); QVERIFY(chosen); const auto path = dialog.findChild<QLineEdit *>(field)->text(); QVERIFY(path.endsWith('/')); QCOMPARE(QFileInfo(path).canonicalFilePath(), QFileInfo(file("folder two")).canonicalFilePath()); dialog.reject(); untouchedTargets();
    }
    void guiFocusedFlatAndTwoPanels() {
        QVERIFY(write(file("nested/flat.txt"), "flat payload")); QVERIFY(symlink(file("editable link"), "first.txt"));
        MainWindow window(executable); window.show(); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); QVERIFY(focus(window, "editable link", false)); auto action = window.findChild<QAction *>("linkAction"); QVERIFY(!action->isEnabled()); QVERIFY(focus(window, "editable link")); QVERIFY(action->isEnabled());
        bool edited = false; QTimer::singleShot(20, &window, [&] { auto dialog = window.findChild<LinkDialog *>("linkDialog"); QVERIFY(dialog); dialog->findChild<QLineEdit *>("linkTo")->setText("日本語 second.txt"); dialog->accept(); edited = true; }); action->trigger(); QVERIFY(edited); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); QCOMPARE(raw(file("editable link")), QFile::encodeName("日本語 second.txt"));
        auto ownTree = window.findChild<FileList *>("fileList"); QVERIFY(ownTree->currentItem()); QCOMPARE(ownTree->currentItem()->data(0, Qt::UserRole).toString(), file("editable link")); QCOMPARE(ownTree->selectedItems().size(), 1);
        window.findChild<QAction *>("flatAction")->trigger(); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); QVERIFY(focus(window, "flat.txt")); bool checked = false;
        QTimer::singleShot(20, &window, [&] { auto dialog = window.findChild<LinkDialog *>("linkDialog"); QVERIFY(dialog); QCOMPARE(dialog->findChild<QLineEdit *>("linkFrom")->text(), file("nested") + '/'); dialog->findChild<QLineEdit *>("linkFrom")->setText("flat hard link.txt"); dialog->accept(); checked = true; }); action->trigger(); QVERIFY(checked); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000);
        struct stat source{}, linked{}; QVERIFY(::stat(QFile::encodeName(file("nested/flat.txt")).constData(), &source) == 0); QVERIFY(::stat(QFile::encodeName(file("flat hard link.txt")).constData(), &linked) == 0); QCOMPARE(source.st_ino, linked.st_ino); QCOMPARE(read(file("flat hard link.txt")), QByteArray("flat payload"));
        window.findChild<QAction *>("twoPanelsAction")->trigger(); auto panels = window.findChildren<MainWindow *>(); QCOMPARE(panels.size(), 1); auto second = panels.first(); second->openPath(file("folder two")); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); QVERIFY(focus(window, "flat.txt")); checked = false;
        QTimer::singleShot(20, &window, [&] { auto dialog = window.findChild<LinkDialog *>("linkDialog"); QVERIFY(dialog); QCOMPARE(dialog->findChild<QLineEdit *>("linkFrom")->text(), file("folder two") + '/'); dialog->reject(); checked = true; }); action->trigger(); QVERIFY(checked); untouchedTargets();
    }
};
int main(int argc, char **argv) {
    QLocale::setDefault(QLocale(QLocale::English));
    if (argc < 2) return 1; executable = QString::fromLocal8Bit(argv[1]); if (!qEnvironmentVariable("PORT_TEST_PLUGIN_ROOT").isEmpty()) QCoreApplication::setLibraryPaths({qEnvironmentVariable("PORT_TEST_PLUGIN_ROOT")});
    QCoreApplication::setAttribute(Qt::AA_MacDontSwapCtrlAndMeta); QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs); QApplication app(argc, argv); applyPortAppearance(app); app.setOrganizationName("SevenZipMacPortTests"); app.setApplicationName("Links"); app.setQuitOnLastWindowClosed(false);
    LinkTests tests; QList<char *> args{argv[0]}; for (int n = 2; n < argc; ++n) args << argv[n]; args << nullptr; return QTest::qExec(&tests, args.size() - 1, args.data());
}
#include "file-links.moc"
