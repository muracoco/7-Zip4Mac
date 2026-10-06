// SPDX-License-Identifier: LGPL-3.0-or-later
#include <QLocale>
#include "MainWindow.h"
#include "PanelDrag.h"
#include "PanelMenu.h"
#include "PortStyle.h"
#include "progress-dialog-driver.h"
#include <QApplication>
#include <QContextMenuEvent>
#include <QCryptographicHash>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFile>
#include "input-driver.h"
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMouseEvent>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QTest>

static QString executable;
static bool write(const QString &path, QByteArray data) { QDir().mkpath(QFileInfo(path).absolutePath()); QFile file(path); return file.open(QIODevice::WriteOnly) && file.write(data) == data.size(); }
static QByteArray hash(const QString &path) { QFile file(path); if (!file.open(QIODevice::ReadOnly)) return {}; QCryptographicHash result(QCryptographicHash::Sha256); result.addData(&file); return result.result(); }
static FileList *files(MainWindow &window) { return window.findChild<FileList *>("fileList"); }
static bool select(MainWindow &window, QString name) { auto list = files(window); const auto rows = list->findItems(name, Qt::MatchExactly); if (rows.size() != 1) return false; list->clearSelection(); list->setCurrentItem(rows.first()); rows.first()->setSelected(true); return true; }
static bool sendDrop(QAbstractItemView *view, const QMimeData *mime, QPoint point, Qt::KeyboardModifiers keys = Qt::NoModifier, Qt::MouseButtons button = Qt::LeftButton) {
    QDragEnterEvent enter(point, Qt::CopyAction | Qt::MoveAction, mime, button, keys); QApplication::sendEvent(view->viewport(), &enter);
    if (!enter.isAccepted()) return false;
    QDropEvent drop(point, Qt::CopyAction | Qt::MoveAction, mime, button, keys); QApplication::sendEvent(view->viewport(), &drop); return drop.isAccepted();
}

class PanelMenuDragTests : public QObject {
    Q_OBJECT
    QTemporaryDir temporary;
    ProgressDialogDriver resultDialogs;
    ArchiveResult execute(ArchiveRequest request) {
        SevenZipProcessBackend backend(executable); QSignalSpy done(&backend, &ArchiveBackend::finished); backend.start(request);
        if (done.isEmpty() && !done.wait(20000)) return {};
        return qvariant_cast<ArchiveResult>(done.last()[0]);
    }
private slots:
    void initTestCase() { QVERIFY(temporary.isValid()); QSettings::setDefaultFormat(QSettings::IniFormat); QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, temporary.filePath("preferences")); }
    void init() { resultDialogs.clear(); QSettings().clear(); QSettings().setValue("View/LastPath", temporary.path()); QSettings().setValue("View/AutoRefresh", false); }
    void effects_data() {
        QTest::addColumn<int>("keys"); QTest::addColumn<bool>("same"); QTest::addColumn<int>("expected");
        QTest::newRow("same-drive") << int(Qt::NoModifier) << true << int(Qt::MoveAction);
        QTest::newRow("other-drive") << int(Qt::NoModifier) << false << int(Qt::CopyAction);
        QTest::newRow("Ctrl-copy") << int(Qt::ControlModifier) << true << int(Qt::CopyAction);
        QTest::newRow("Shift-move") << int(Qt::ShiftModifier) << false << int(Qt::MoveAction);
        QTest::newRow("Ctrl-Shift-link-refused") << int(Qt::ControlModifier | Qt::ShiftModifier) << true << int(Qt::IgnoreAction);
        QTest::newRow("Alt-link-refused") << int(Qt::AltModifier) << false << int(Qt::IgnoreAction);
        QTest::newRow("Ctrl-Alt-default") << int(Qt::ControlModifier | Qt::AltModifier) << true << int(Qt::MoveAction);
        QTest::newRow("Shift-Alt-default") << int(Qt::ShiftModifier | Qt::AltModifier) << false << int(Qt::CopyAction);
    }
    void effects() { QFETCH(int, keys); QFETCH(bool, same); QFETCH(int, expected); QCOMPARE(int(officialPanelDropEffect(Qt::KeyboardModifiers(keys), Qt::CopyAction | Qt::MoveAction, same)), expected); QCOMPARE(officialPanelDropEffect(Qt::NoModifier, Qt::CopyAction, true), Qt::CopyAction); }
    void sourceMenuOrderAndConditions() {
        MainWindow window(executable); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000);
        auto source = window.menuBar()->actions().first()->menu(); FileMenuState state; state.count = 1;
        QMenu menu; appendOfficialFileMenu(&menu, source, state);
        QStringList names;
        for (auto action : menu.actions()) if (!action->isSeparator()) names << (action->menu() ? "CRC" : action->objectName());
        QCOMPARE(names, QStringList({"openAction","insideAction","insideOneAction","insideParserAction","outsideAction","viewAction","editAction","renameAction","copyAction","moveAction","deleteAction","splitAction","combineAction","infoAction","commentAction","CRC","folderAction","newfileAction","linkAction","alternateStreamsAction"}));
        auto crc = menu.findChildren<QMenu *>().first(); QCOMPARE(crc->actions().size(), 11);
        state.diff = "/owned/diff"; state.readOnly = true; QMenu readOnly; appendOfficialFileMenu(&readOnly, source, state);
        QVERIFY(readOnly.findChild<QAction *>("diffAction"));
        for (const auto &key : {"renameAction", "moveAction", "deleteAction", "commentAction", "folderAction", "newfileAction"}) QVERIFY(!readOnly.findChild<QAction *>(key)->isEnabled());
        state.largeScreen = false; QMenu small; appendOfficialFileMenu(&small, source, state);
        QVERIFY(!small.findChild<QAction *>("renameAction")); for (auto action : small.actions()) QVERIFY(!action->isSeparator());
        state.largeScreen = true; state.hashFolder = true; QMenu sums; appendOfficialFileMenu(&sums, source, state);
        QVERIFY(!sums.findChild<QAction *>("openAction")->isEnabled());
        QVERIFY(!sums.findChild<QAction *>("linkAction")->isEnabled());
    }
    void contextCrcClick() {
        const auto root = temporary.filePath("context"); QVERIFY(write(root + "/日本語 space.bin", "context crc bytes"));
        MainWindow window(executable); window.show(); window.openPath(root); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); QVERIFY(select(window, "日本語 space.bin"));
        auto backend = window.findChild<SevenZipProcessBackend *>(); QSignalSpy done(backend, &ArchiveBackend::finished);
        bool submenu = false, clicked = false; QTimer responder; responder.setInterval(10);
        connect(&responder, &QTimer::timeout, &window, [&] {
            for (auto menu : window.findChildren<QMenu *>()) if (menu->objectName() == "fileContextMenu" && menu->isVisible()) {
                for (auto action : menu->actions()) if (auto crc = action->menu(); crc && crc->findChild<QAction *>("hashSHA256Action")) {
                    if (!submenu) { submenu = true; QTimer::singleShot(0, menu, [menu, action] { QTest::mouseClick(menu, Qt::LeftButton, Qt::NoModifier, menu->actionGeometry(action).center()); }); }
                    if (crc->isVisible() && !clicked) for (auto method : crc->actions()) if (method->objectName() == "hashSHA256Action") { clicked = true; QTimer::singleShot(0, crc, [crc, method] { QTest::mouseClick(crc, Qt::LeftButton, Qt::NoModifier, crc->actionGeometry(method).center()); }); }
                }
            }
        }); responder.start();
        QTimer::singleShot(5000, &window, [&] { if (!clicked) for (auto menu : window.findChildren<QMenu *>()) menu->close(); });
        const auto point = files(window)->visualItemRect(files(window)->currentItem()).center(); QContextMenuEvent context(QContextMenuEvent::Mouse, point, files(window)->viewport()->mapToGlobal(point)); QApplication::sendEvent(files(window)->viewport(), &context);
        QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); QVERIFY(clicked); QVERIFY(!done.isEmpty());
        const auto result = qvariant_cast<ArchiveResult>(done.last()[0]); QVERIFY2(result.success, qPrintable(result.details)); QVERIFY(result.details.contains(QString::fromLatin1(hash(root + "/日本語 space.bin").toHex()), Qt::CaseInsensitive));
    }
    void filesystemPanelDrop_data() {
        QTest::addColumn<bool>("move"); QTest::addColumn<int>("mode");
        QTest::newRow("Details-Copy") << false << 3; QTest::newRow("Details-Move") << true << 3;
        QTest::newRow("Large-Icons-Copy") << false << 0; QTest::newRow("Large-Icons-Move") << true << 0;
        QTest::newRow("Small-Icons-Copy") << false << 1; QTest::newRow("Small-Icons-Move") << true << 1;
        QTest::newRow("List-Copy") << false << 2; QTest::newRow("List-Move") << true << 2;
    }
    void filesystemPanelDrop() {
        QFETCH(bool, move); QFETCH(int, mode); const bool icon = mode != 3; const auto root = temporary.filePath(QString("panels-%1-%2").arg(move).arg(mode));
        QVERIFY(write(root + "/source/日本語 space.bin", QByteArray(1024 * 1024 + 13, 'p'))); QVERIFY(QDir().mkpath(root + "/destination/child"));
        MainWindow source(executable), destination(executable); source.show(); destination.show(); source.openPath(root + "/source"); destination.openPath(root + "/destination"); QTRY_VERIFY_WITH_TIMEOUT(!source.operationBusy() && !destination.operationBusy(), 10000);
        destination.findChild<QAction *>("mode" + QString::number(mode) + "Action")->trigger();
        const auto row = files(destination)->findItems("child", Qt::MatchExactly).first(); QAbstractItemView *view = icon ? static_cast<QAbstractItemView *>(destination.findChild<IconFileList *>("iconFileList")) : files(destination);
        const auto point = view->visualRect(files(destination)->indexFromItem(row)).center();
        PanelDragMimeData mime(&source, {QUrl::fromLocalFile(root + "/source/日本語 space.bin")}, false);
        const auto original = hash(root + "/source/日本語 space.bin"); QVERIFY(sendDrop(view, &mime, point, move ? Qt::ShiftModifier : Qt::ControlModifier));
        QTRY_VERIFY_WITH_TIMEOUT(!destination.operationBusy() && !source.operationBusy(), 10000); QCOMPARE(hash(root + "/destination/child/日本語 space.bin"), original); QCOMPARE(QFileInfo::exists(root + "/source/日本語 space.bin"), !move);
        QVERIFY(!sendDrop(files(source), &mime, QPoint(4, 4))); QVERIFY(!QFileInfo::exists(root + "/destination/日本語 space.bin"));
    }
    void archiveDrag_data() {
        QTest::addColumn<QString>("format"); QTest::addColumn<bool>("encrypted"); QTest::addColumn<bool>("accept");
        QTest::newRow("7z-to-FS") << QString("7z") << false << true;
        QTest::newRow("ZIP-to-FS") << QString("zip") << false << true;
        QTest::newRow("encrypted-7z-to-FS") << QString("7z") << true << true;
        QTest::newRow("drag-cancel") << QString("7z") << false << false;
    }
    void defaultDropMovesSymlinkOnly() {
        const auto root = temporary.filePath("default-link-move"); QVERIFY(write(root + "/payload.bin", "link target stays")); QVERIFY(QDir().mkpath(root + "/source")); QVERIFY(QDir().mkpath(root + "/out")); QVERIFY(QFile::link(root + "/payload.bin", root + "/source/link.bin"));
        const auto original = hash(root + "/payload.bin"); MainWindow source(executable), target(executable); source.show(); target.show(); source.openPath(root + "/source"); target.openPath(root + "/out"); QTRY_VERIFY_WITH_TIMEOUT(!source.operationBusy() && !target.operationBusy(), 10000);
        PanelDragMimeData mime(&source, {QUrl::fromLocalFile(root + "/source/link.bin")}, false); QVERIFY(sendDrop(files(target), &mime, QPoint(4, 4)));
        QTRY_VERIFY_WITH_TIMEOUT(!source.operationBusy() && !target.operationBusy(), 10000); QVERIFY(!QFileInfo(root + "/source/link.bin").isSymLink()); QVERIFY(QFileInfo(root + "/out/link.bin").isSymLink()); QCOMPARE(hash(root + "/payload.bin"), original); QCOMPARE(hash(root + "/out/link.bin"), original);
    }
    void rightDropChoices_data() {
        QTest::addColumn<QString>("choice");
        for (const auto &name : {"Copy", "Move", "Add", "Cancel"}) QTest::newRow(name) << QString(name);
    }
    void rightDragGesture() {
        const auto root = temporary.filePath("right-gesture"); QVERIFY(write(root + "/source/owned.bin", "right gesture bytes")); QVERIFY(QDir().mkpath(root + "/out"));
        MainWindow source(executable), target(executable); source.show(); target.show(); source.openPath(root + "/source"); target.openPath(root + "/out"); QTRY_VERIFY_WITH_TIMEOUT(!source.operationBusy() && !target.operationBusy(), 10000); QVERIFY(select(source, "owned.bin"));
        bool invoked = false, selected = false, copied = false; QTimer responder; responder.setInterval(10);
        connect(&responder, &QTimer::timeout, &target, [&] { for (auto menu : target.findChildren<QMenu *>("dragContextMenu")) if (menu->isVisible() && !selected) for (auto action : menu->actions()) if (action->objectName() == "dragCopy") { selected = true; QTimer::singleShot(0, menu, [menu, action] { QTest::mouseClick(menu, Qt::LeftButton, Qt::NoModifier, menu->actionGeometry(action).center()); }); } }); responder.start();
        source.findChild<PanelDragController *>()->setExecutor([&](QDrag *drag) {
            const auto mime = qobject_cast<const PanelDragMimeData *>(drag->mimeData()); invoked = mime && mime->rightButton;
            copied = sendDrop(files(target), drag->mimeData(), QPoint(4, 4)); return copied ? Qt::CopyAction : Qt::IgnoreAction;
        });
        auto viewport = files(source)->viewport(); const auto point = files(source)->visualItemRect(files(source)->currentItem()).center();
        QMouseEvent press(QEvent::MouseButtonPress, point, viewport->mapToGlobal(point), Qt::RightButton, Qt::RightButton, Qt::NoModifier); QApplication::sendEvent(viewport, &press);
        const auto moved = point + QPoint(QApplication::startDragDistance() + 4, 0); QMouseEvent movement(QEvent::MouseMove, moved, viewport->mapToGlobal(moved), Qt::NoButton, Qt::RightButton, Qt::NoModifier); QApplication::sendEvent(viewport, &movement);
        QTRY_VERIFY_WITH_TIMEOUT(!source.operationBusy() && !target.operationBusy(), 10000); QVERIFY(invoked && selected && copied); QCOMPARE(hash(root + "/out/owned.bin"), hash(root + "/source/owned.bin"));
    }
    void rightDropChoices() {
        QFETCH(QString, choice); const auto root = temporary.filePath("right-" + choice);
        QVERIFY(write(root + "/source/日本語 space.bin", "right drop bytes")); QVERIFY(QDir().mkpath(root + "/out/child"));
        const auto original = hash(root + "/source/日本語 space.bin"); MainWindow window(executable); window.show(); window.openPath(root + "/out"); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000);
        auto row = files(window)->findItems("child", Qt::MatchExactly).first(); const auto point = files(window)->visualItemRect(row).center();
        bool chosen = false, addDialog = false; QString error, archiveParent; QTimer responder; responder.setInterval(10);
        connect(&responder, &QTimer::timeout, &window, [&] {
            for (auto menu : window.findChildren<QMenu *>("dragContextMenu")) if (menu->isVisible() && !chosen) for (auto action : menu->actions()) if (action->objectName() == "drag" + choice) {
                chosen = true; QTimer::singleShot(0, menu, [menu, action] { QTest::mouseClick(menu, Qt::LeftButton, Qt::NoModifier, menu->actionGeometry(action).center()); });
            }
            for (auto dialog : window.findChildren<AddDialog *>()) if (dialog->isVisible()) { addDialog = true; archiveParent = QFileInfo(dialog->findChild<QLineEdit *>("archivePath")->text()).absolutePath(); dialog->accept(); }
            for (auto dialog : window.findChildren<QMessageBox *>()) if (dialog->isVisible()) { error += dialog->text(); dialog->accept(); }
        }); responder.start();
        QMimeData mime; mime.setUrls({QUrl::fromLocalFile(root + "/source/日本語 space.bin")});
        QCOMPARE(sendDrop(files(window), &mime, point, Qt::NoModifier, Qt::RightButton), choice != "Cancel");
        QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); QVERIFY(chosen); QVERIFY2(error.isEmpty(), qPrintable(error)); QCOMPARE(addDialog, choice == "Add");
        QCOMPARE(QFileInfo::exists(root + "/source/日本語 space.bin"), choice != "Move");
        if (choice == "Add") {
            QCOMPARE(archiveParent, root + "/out/child");
            ArchiveRequest request; request.operation = ArchiveOperation::Extract; request.archive = root + "/out/child/日本語 space.7z"; request.outputDirectory = root + "/decode"; request.overwriteMode = "overwrite"; QVERIFY(execute(request).success); QCOMPARE(hash(root + "/decode/日本語 space.bin"), original);
        } else if (choice != "Cancel") QCOMPARE(hash(root + "/out/child/日本語 space.bin"), original);
        else QVERIFY(QDir(root + "/out/child").entryList(QDir::AllEntries | QDir::NoDotAndDotDot).isEmpty());
    }
    void archiveDropConfirmation_data() {
        QTest::addColumn<QString>("format"); QTest::addColumn<bool>("accept");
        QTest::newRow("7z-Yes") << QString("7z") << true; QTest::newRow("7z-Cancel") << QString("7z") << false;
        QTest::newRow("ZIP-Yes") << QString("zip") << true; QTest::newRow("ZIP-Cancel") << QString("zip") << false;
    }
    void archiveDropConfirmation() {
        QFETCH(QString, format); QFETCH(bool, accept); const auto root = temporary.filePath(QString("confirm-%1-%2").arg(format).arg(accept));
        QVERIFY(write(root + "/seed/folder/seed.txt", "seed bytes")); QVERIFY(write(root + "/source/added 日本語.txt", "added bytes"));
        ArchiveRequest request; request.operation = ArchiveOperation::Add; request.archive = root + "/target." + format; request.format = format; request.workingDirectory = root + "/seed"; request.files = {"folder"}; request.method = format == "zip" ? "Deflate" : "LZMA2"; QVERIFY(execute(request).success);
        const auto original = hash(request.archive); MainWindow window(executable); window.show(); window.openPath(request.archive + "/folder"); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000);
        int confirmations = 0; QString errors; QTimer responder; responder.setInterval(10);
        connect(&responder, &QTimer::timeout, &window, [&] {
            for (auto box : window.findChildren<QMessageBox *>()) if (box->isVisible()) {
                if (box->objectName() == "archiveDropConfirmation") { ++confirmations; box->done(accept ? QMessageBox::Yes : QMessageBox::Cancel); }
                else { errors += box->text(); box->accept(); }
            }
        }); responder.start();
        QMimeData mime; mime.setUrls({QUrl::fromLocalFile(root + "/source/added 日本語.txt")}); QCOMPARE(sendDrop(files(window), &mime, QPoint(4, 4), Qt::ShiftModifier), accept);
        QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); QCOMPARE(confirmations, 1); QVERIFY2(errors.isEmpty(), qPrintable(errors)); QVERIFY(QFileInfo::exists(root + "/source/added 日本語.txt"));
        if (accept) { request.operation = ArchiveOperation::Extract; request.files.clear(); request.outputDirectory = root + "/decode"; request.overwriteMode = "overwrite"; QVERIFY(execute(request).success); QCOMPARE(hash(root + "/decode/folder/added 日本語.txt"), hash(root + "/source/added 日本語.txt")); }
        else QCOMPARE(hash(request.archive), original);
    }
    void archiveDrag() {
        QFETCH(QString, format); QFETCH(bool, encrypted); QFETCH(bool, accept);
        const auto root = temporary.filePath(QString("archive-%1-%2-%3").arg(format).arg(encrypted).arg(accept));
        QVERIFY(write(root + "/input/日本語 space.bin", QByteArray(32 * 1024 * 1024 + 17, 'a'))); QVERIFY(QDir().mkpath(root + "/out"));
        ArchiveRequest request; request.operation = ArchiveOperation::Add; request.archive = root + "/source." + format; request.format = format; request.workingDirectory = root + "/input"; request.files = {"日本語 space.bin"}; request.method = format == "zip" ? "Deflate" : "LZMA2"; request.password = encrypted ? "owned-password" : ""; request.encryptNames = encrypted; QVERIFY(execute(request).success);
        auto settings = FileManagerSettings::load(); settings.workMode = 2; settings.workPath = root; settings.removableOnly = false; QVERIFY(settings.save());
        auto source = std::make_unique<MainWindow>(executable); MainWindow destination(executable); source->show(); destination.show();
        QTimer password; password.setInterval(10); connect(&password, &QTimer::timeout, source.get(), [&] { for (auto dialog : testInputs(source.get())) if (dialog->isVisible()) { dialog->setTextValue("owned-password"); dialog->accept(); } }); password.start();
        source->openPath(request.archive); destination.openPath(root + "/out"); QTRY_VERIFY_WITH_TIMEOUT(!source->operationBusy() && !destination.operationBusy(), 10000); QVERIFY(select(*source, "日本語 space.bin"));
        QString extracted; bool invoked = false, copied = false; int ticks = 0; QTimer heartbeat; heartbeat.setInterval(5); connect(&heartbeat, &QTimer::timeout, [&] { ++ticks; }); heartbeat.start();
        source->findChild<PanelDragController *>()->setExecutor([&](QDrag *drag) {
            invoked = true; const auto mime = qobject_cast<const PanelDragMimeData *>(drag->mimeData()); if (!mime || !mime->archive || mime->source != source.get() || mime->urls().size() != 1) return Qt::IgnoreAction;
            extracted = mime->urls().first().toLocalFile(); if (hash(extracted) != hash(root + "/input/日本語 space.bin")) return Qt::IgnoreAction;
            if (!accept) return Qt::IgnoreAction;
            copied = sendDrop(files(destination), mime, QPoint(4, 4)); if (!copied) return Qt::IgnoreAction;
            auto operations = destination.findChild<FileOperations *>(); QSignalSpy finished(operations, &FileOperations::finished); if (operations->busy()) finished.wait(10000);
            return Qt::CopyAction;
        });
        files(*source)->archiveDragRequested(); QTRY_VERIFY_WITH_TIMEOUT(invoked && !source->operationBusy() && !destination.operationBusy(), 15000); QVERIFY(ticks > 1); QVERIFY(!extracted.isEmpty());
        QCOMPARE(QFileInfo::exists(extracted), accept);
        if (accept) { QVERIFY(copied); QCOMPARE(hash(root + "/out/日本語 space.bin"), hash(root + "/input/日本語 space.bin")); }
        password.stop(); source.reset(); QCOMPARE(QFileInfo::exists(extracted), accept); // accepted payload survives Manager destruction
    }
};
int main(int argc, char **argv) {
    QLocale::setDefault(QLocale(QLocale::English));
    if (argc < 2) return 2; executable = argv[1];
    const auto plugins = qEnvironmentVariable("PORT_TEST_PLUGIN_ROOT"); if (!plugins.isEmpty()) QCoreApplication::setLibraryPaths({plugins});
    QCoreApplication::setAttribute(Qt::AA_MacDontSwapCtrlAndMeta); QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    QApplication app(argc, argv); applyPortAppearance(app); app.setOrganizationName("SevenZipMacPortTests"); app.setApplicationName("PanelMenuDrag"); app.setQuitOnLastWindowClosed(false);
    PanelMenuDragTests tests; QList<char *> args{argv[0]}; for (int n = 2; n < argc; ++n) args << argv[n]; args << nullptr; return QTest::qExec(&tests, args.size() - 1, args.data());
}
#include "panel-menu-drag.moc"
