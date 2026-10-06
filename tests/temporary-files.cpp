// SPDX-License-Identifier: LGPL-3.0-or-later
#include <QLocale>
#include "TemporaryFilesDialog.h"
#include "MainWindow.h"
#include "PropertiesDialog.h"
#include "PortStyle.h"
#include "UiLanguage.h"
#include <QApplication>
#include <QAbstractButton>
#include <QComboBox>
#include <QContextMenuEvent>
#include <QElapsedTimer>
#include <QFile>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QLabel>
#include <QMessageBox>
#include <QMenu>
#include <QProcess>
#include <QPushButton>
#include <QScopeGuard>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <QTreeWidget>
#include <unistd.h>
#include <sys/stat.h>

static QString executable, helper;
static bool write(QString path, QByteArray data) { QDir().mkpath(QFileInfo(path).absolutePath()); QFile file(path); return file.open(QIODevice::WriteOnly) && file.write(data) == data.size(); }
static QByteArray read(QString path) { QFile file(path); return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray(); }
static QTreeWidget *tree(TemporaryFilesDialog &dialog) { return dialog.findChild<QTreeWidget *>("temporaryList"); }
static QTreeWidgetItem *select(TemporaryFilesDialog &dialog, QString name) {
    auto list = tree(dialog); auto found = list->findItems(name, Qt::MatchExactly); if (found.size() != 1) return nullptr;
    list->clearSelection(); list->setCurrentItem(found.first(), 0, QItemSelectionModel::NoUpdate); found.first()->setSelected(true); return found.first();
}
class TemporaryFilesTests : public QObject {
    Q_OBJECT
    QTemporaryDir preferences;
    QJsonArray native(QString root, QString path = {}, int expectedCode = 0) {
        QProcess process; auto environment = QProcessEnvironment::systemEnvironment();
        environment.insert("SEVENZIP_PORT_TEMP_ROOT", QFileInfo(root).canonicalFilePath());
        environment.insert("SEVENZIP_PORT_TEMP_FOLDER", path.isEmpty() ? QFileInfo(root).canonicalFilePath() : path);
        process.setProcessEnvironment(environment); process.start(helper, {"i"});
        if (!process.waitForFinished(10000) || process.exitStatus() != QProcess::NormalExit || process.exitCode() != expectedCode) { qWarning() << process.readAllStandardError(); return {}; }
        return QJsonDocument::fromJson(process.readAllStandardOutput()).array();
    }
private slots:
    void initTestCase() {
        QVERIFY(preferences.isValid()); QVERIFY(QFileInfo::exists(helper));
        QSettings::setDefaultFormat(QSettings::IniFormat); QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, preferences.path());
    }
    void init() { QSettings().clear(); FileManagerSettings settings; settings.realIcons = false; settings.language = "en"; QVERIFY(settings.save()); UiLanguage::set("en"); }
    void originalFilterAndBoundedCounts() {
        QTemporaryDir fixture; QVERIFY(fixture.isValid());
        const QString root = fixture.path(); const QStringList included{"7zO1234aBcD", "7ZEabcdef01", "7zs01234567", ".7zip-open-aBc123", ".7zip-nested-ABC123"};
        for (const auto &name : included) QVERIFY(write(root + '/' + name + "/日本語 space.txt", "payload"));
        for (const auto &name : {"other", "7zO1234", "7zX12345678", "7zO123456789", ".7zip-other-ABC123", ".7zip-open-ABC1234"}) QVERIFY(write(root + '/' + name + "/do not delete.txt", "unrelated"));
        auto rows = native(root); QCOMPARE(rows.size(), included.size());
        for (const auto &value : rows) { const auto row = value.toObject(); QVERIFY(included.contains(row["name"].toString())); QCOMPARE(row["size"].toString(), QString("7")); QCOMPARE(row["files"].toInt(), 1); QCOMPARE(row["subName"].toString(), QString("日本語 space.txt")); QVERIFY(!row["interrupted"].toBool()); }
        QVERIFY(QDir().mkpath(root + "/7zE00000000")); for (int n = 0; n < 2001; ++n) QVERIFY(write(root + "/7zE00000000/" + QString::number(n), "x"));
        rows = native(root); bool limited = false;
        for (const auto &value : rows) if (value.toObject()["name"] == "7zE00000000") { const auto row = value.toObject(); QCOMPARE(row["files"].toInt(), 2000); QCOMPARE(row["size"].toString(), QString("2000")); QVERIFY(row["interrupted"].toBool()); QVERIFY(row["subName"].toString().isEmpty()); limited = true; }
        QVERIFY(limited);
    }
    void layoutNavigationSortAndOpen_data() { QTest::addColumn<bool>("dots"); QTest::newRow("without-dots") << false; QTest::newRow("with-dots") << true; }
    void layoutNavigationSortAndOpen() {
        QFETCH(bool, dots); auto preferences = FileManagerSettings::load(); preferences.showDots = dots; QVERIFY(preferences.save());
        QTemporaryDir fixture; QVERIFY(write(fixture.filePath("7zO11111111/日本語 space.txt"), "payload")); QVERIFY(write(fixture.filePath("7zE22222222/sub/file.txt"), "different"));
        TemporaryFilesDialog dialog(helper, nullptr, fixture.path()); QSignalSpy loaded(&dialog, &TemporaryFilesDialog::listed), opened(&dialog, &TemporaryFilesDialog::openRequested); dialog.show();
        QTRY_VERIFY_WITH_TIMEOUT(!loaded.isEmpty(), 10000); QVERIFY(loaded.last()[0].toBool()); QCOMPARE(tree(dialog)->columnCount(), 6);
        QCOMPARE(tree(dialog)->headerItem()->text(0), QString("Name")); QCOMPARE(tree(dialog)->headerItem()->text(5), QString("Name-2"));
        QVERIFY(dialog.findChild<QLineEdit *>("temporaryPath")->isReadOnly()); QVERIFY(!dialog.findChild<QComboBox *>("temporaryFilter")->isEnabled()); QVERIFY(!dialog.findChild<QPushButton *>("temporaryParent")->isEnabled());
        auto item = select(dialog, "7zO11111111"); QVERIFY(item); QCOMPARE(item->text(5), QString("日本語 space.txt"));
        QTest::keyClick(tree(dialog), Qt::Key_Return); QTRY_COMPARE_WITH_TIMEOUT(loaded.size(), 2, 10000); QCOMPARE(QFileInfo(dialog.currentDirectory()).fileName(), QString("7zO11111111"));
        QCOMPARE(tree(dialog)->findItems("..", Qt::MatchExactly).size(), dots ? 1 : 0);
        QVERIFY(select(dialog, "日本語 space.txt")); QTest::keyClick(tree(dialog), Qt::Key_Return, Qt::ShiftModifier); QCOMPARE(opened.size(), 1); QCOMPARE(read(opened.first()[0].toString()), QByteArray("payload")); QVERIFY(!opened.first()[1].toBool());
        QTest::keyClick(tree(dialog), Qt::Key_Return, Qt::AltModifier); QTRY_VERIFY(dialog.findChild<QDialog *>("temporaryProperties")); dialog.findChild<QDialog *>("temporaryProperties")->close();
        QTest::keyClick(tree(dialog), Qt::Key_Backspace); QTRY_COMPARE_WITH_TIMEOUT(loaded.size(), 3, 10000); QCOMPARE(dialog.currentDirectory(), QFileInfo(fixture.path()).canonicalFilePath());
        QCOMPARE(tree(dialog)->selectedItems().size(), 1); QCOMPARE(tree(dialog)->currentItem()->text(0), QString("7zO11111111"));
        QTest::keyClick(tree(dialog), Qt::Key_F6, Qt::ControlModifier); QCOMPARE(tree(dialog)->sortColumn(), 2); QCOMPARE(tree(dialog)->header()->sortIndicatorOrder(), Qt::DescendingOrder);
        QTest::keyClick(tree(dialog), Qt::Key_R, Qt::ControlModifier); QTRY_COMPARE_WITH_TIMEOUT(loaded.size(), 4, 10000); QCOMPARE(tree(dialog)->currentItem()->text(0), QString("7zO11111111"));
        QTest::keyClick(tree(dialog), Qt::Key_A, Qt::ControlModifier); QCOMPARE(tree(dialog)->selectedItems().size(), 2);
    }
    void contextMenuSettingsAndTranslation() {
        QTemporaryDir fixture; QVERIFY(write(fixture.filePath("7zO00000001/one.txt"), QByteArray(20000, 'x')));
        auto preferences = FileManagerSettings::load(); preferences.fullRow = true; preferences.grid = true; preferences.singleClick = true; QVERIFY(preferences.save());
        TemporaryFilesDialog dialog(helper, nullptr, fixture.path()); QSignalSpy loaded(&dialog, &TemporaryFilesDialog::listed), opened(&dialog, &TemporaryFilesDialog::openRequested); dialog.show();
        QTRY_VERIFY_WITH_TIMEOUT(!loaded.isEmpty(), 10000); QCOMPARE(tree(dialog)->selectionBehavior(), QAbstractItemView::SelectRows); QVERIFY(!tree(dialog)->styleSheet().isEmpty());
        QVERIFY(select(dialog, "7zO00000001")); QCOMPARE(tree(dialog)->currentItem()->text(2), QString("19 KB"));
        tree(dialog)->clearSelection(); QStringList menuTexts; bool menuFound = false;
        QTimer choose; choose.setInterval(10); connect(&choose, &QTimer::timeout, &dialog, [&] {
            auto menu = dialog.findChild<QMenu *>(); if (!menu) return; menuFound = true;
            for (auto action : menu->actions()) menuTexts << action->text();
            if (menu->actions().size() >= 2) menu->actions()[1]->trigger(); menu->close(); choose.stop();
        }); choose.start();
        const QPoint point(12, tree(dialog)->viewport()->height() - 5); QContextMenuEvent event(QContextMenuEvent::Mouse, point, tree(dialog)->viewport()->mapToGlobal(point)); QApplication::sendEvent(tree(dialog)->viewport(), &event);
        QVERIFY(menuFound); QCOMPARE(opened.size(), 1); QCOMPARE(opened.first()[0].toString(), dialog.currentDirectory()); QVERIFY(opened.first()[1].toBool()); QVERIFY(menuTexts.first().startsWith("Open Outside")); QVERIFY(menuTexts[1].endsWith("7-Zip"));
        auto item = select(dialog, "7zO00000001"); QVERIFY(item); QTest::mouseClick(tree(dialog)->viewport(), Qt::LeftButton, Qt::NoModifier, tree(dialog)->visualItemRect(item).center());
        QTRY_COMPARE_WITH_TIMEOUT(loaded.size(), 2, 10000); QCOMPARE(QFileInfo(dialog.currentDirectory()).fileName(), QString("7zO00000001"));
        UiLanguage::set("ja"); TemporaryFilesDialog japanese(helper, nullptr, fixture.path()); QVERIFY(japanese.windowTitle() != "Delete Temporary Files"); QCOMPARE(tree(japanese)->headerItem()->text(5), UiLanguage::text("Name") + "-2"); japanese.reject(); UiLanguage::set("en");
    }
    void propertiesFreshRescanAndAttributes() {
        QTemporaryDir fixture; const auto folder = fixture.filePath("7zO00000001"); QVERIFY(write(folder + "/日本語 <b> space.txt", "one"));
        TemporaryFilesDialog dialog(helper, nullptr, fixture.path()); QSignalSpy loaded(&dialog, &TemporaryFilesDialog::listed); dialog.show(); QTRY_VERIFY_WITH_TIMEOUT(!loaded.isEmpty(), 10000); QVERIFY(select(dialog, "7zO00000001"));
        QVERIFY(write(folder + "/second.txt", "four")); QVERIFY(::chmod(QFile::encodeName(folder).constData(), 0700) == 0);
        QTest::keyClick(tree(dialog), Qt::Key_Return, Qt::AltModifier); QTRY_VERIFY_WITH_TIMEOUT(dialog.findChild<QDialog *>("temporaryProperties"), 10000);
        auto properties = dialog.findChild<QDialog *>("temporaryProperties"); const auto text = properties->findChild<QLabel *>("temporaryPropertyText")->text();
        QVERIFY2(text.contains("Files: 2"), qPrintable(text)); QVERIFY2(text.contains("Size: 7 bytes"), qPrintable(text)); QVERIFY2(text.contains("Attributes:") && text.contains("drwx------"), qPrintable(text)); QVERIFY(text.contains("Modified:"));
        QCOMPARE(properties->findChild<QDialogButtonBox *>("temporaryPropertiesButtons")->standardButtons(), QDialogButtonBox::StandardButtons(QDialogButtonBox::Ok)); QCOMPARE(properties->findChild<QLabel *>("temporaryPropertyText")->textFormat(), Qt::PlainText); properties->close(); QTRY_VERIFY(!dialog.findChild<QDialog *>("temporaryProperties"));
        QFile removed(folder + "/second.txt"); QVERIFY(removed.remove()); QVERIFY(::chmod(QFile::encodeName(folder + "/日本語 <b> space.txt").constData(), 0400) == 0);
        QTest::keyClick(tree(dialog), Qt::Key_Return, Qt::AltModifier); QTRY_VERIFY_WITH_TIMEOUT(dialog.findChild<QDialog *>("temporaryProperties"), 10000);
        properties = dialog.findChild<QDialog *>("temporaryProperties"); const auto second = properties->findChild<QLabel *>("temporaryPropertyText")->text();
        QVERIFY2(second.contains("Files: 1") && second.contains("Size: 3 bytes"), qPrintable(second)); QVERIFY(second.contains("----------------")); QVERIFY(second.contains("日本語 <b> space.txt")); QVERIFY2(second.contains("-r--------"), qPrintable(second));
        properties->close(); dialog.reject();
    }
    void propertiesLocalizedAndBounded() {
        QTemporaryDir fixture; const auto folder = fixture.filePath("7zO00000002");
        for (int i = 0; i < 2001; ++i) QVERIFY(write(folder + '/' + QString::number(i) + ".txt", "x"));
        UiLanguage::set("ja"); TemporaryFilesDialog dialog(helper, nullptr, fixture.path()); QSignalSpy loaded(&dialog, &TemporaryFilesDialog::listed); dialog.show(); QTRY_VERIFY_WITH_TIMEOUT(!loaded.isEmpty(), 10000); QVERIFY(select(dialog, "7zO00000002"));
        QTest::keyClick(tree(dialog), Qt::Key_Return, Qt::AltModifier); QTRY_VERIFY_WITH_TIMEOUT(dialog.findChild<QDialog *>("temporaryProperties"), 10000);
        auto properties = dialog.findChild<QDialog *>("temporaryProperties"); QCOMPARE(properties->windowTitle(), UiLanguage::text("Properties"));
        const auto text = properties->findChild<QLabel *>("temporaryPropertyText")->text(); QVERIFY2(text.contains(UiLanguage::text("Files") + ": 2000+"), qPrintable(text)); QVERIFY2(text.contains(UiLanguage::text("Attributes") + ":"), qPrintable(text));
        QVERIFY2(text.contains(UiLanguage::text("{0} bytes").replace("{0}", "2000")), qPrintable(text)); properties->close(); dialog.reject(); UiLanguage::set("en");
    }
    void propertiesMissingItemRefreshes() {
        QTemporaryDir fixture; const auto folder = fixture.filePath("7zO00000003"); QVERIFY(write(folder + "/one.txt", "x"));
        TemporaryFilesDialog dialog(helper, nullptr, fixture.path()); QSignalSpy loaded(&dialog, &TemporaryFilesDialog::listed), failed(&dialog, &TemporaryFilesDialog::failed); dialog.show(); QTRY_VERIFY_WITH_TIMEOUT(!loaded.isEmpty(), 10000); QVERIFY(select(dialog, "7zO00000003"));
        QVERIFY(QDir(folder).removeRecursively()); QTest::keyClick(tree(dialog), Qt::Key_Return, Qt::AltModifier); QTRY_VERIFY(!failed.isEmpty()); QTRY_COMPARE_WITH_TIMEOUT(loaded.size(), 2, 10000); QVERIFY(!dialog.findChild<QDialog *>("temporaryProperties")); QCOMPARE(tree(dialog)->topLevelItemCount(), 0); QVERIFY(failed.last()[0].toString().contains("Item no longer exists")); dialog.reject();
    }
    void closeDuringPropertiesAndReuse() {
        QTemporaryDir fixture; QVERIFY(write(fixture.filePath("7zO00000004/one.txt"), "x"));
        for (int i = 0; i < 5; ++i) {
            auto dialog = new TemporaryFilesDialog(helper, nullptr, fixture.path()); QSignalSpy loaded(dialog, &TemporaryFilesDialog::listed); dialog->show(); QTRY_VERIFY_WITH_TIMEOUT(!loaded.isEmpty(), 10000); QVERIFY(select(*dialog, "7zO00000004"));
            QTest::keyClick(tree(*dialog), Qt::Key_Return, Qt::AltModifier); QVERIFY(dialog->findChild<QProcess *>("temporaryPropertiesReader")); dialog->reject(); delete dialog;
        }
        TemporaryFilesDialog reused(helper, nullptr, fixture.path()); QSignalSpy loaded(&reused, &TemporaryFilesDialog::listed); reused.show(); QTRY_VERIFY_WITH_TIMEOUT(!loaded.isEmpty(), 10000); QVERIFY(loaded.last()[0].toBool()); QVERIFY(select(reused, "7zO00000004")); QTest::keyClick(tree(reused), Qt::Key_Return, Qt::AltModifier); QTRY_VERIFY_WITH_TIMEOUT(reused.findChild<QDialog *>("temporaryProperties"), 10000); reused.findChild<QDialog *>("temporaryProperties")->close(); reused.reject();
    }
    void linksAndBoundaryAreNotFollowed() {
        QTemporaryDir fixture, outside; QVERIFY(write(outside.filePath("do not touch.txt"), "outside"));
        const auto link = fixture.filePath("7zOaaaaaaaa"); QVERIFY(::symlink(QFile::encodeName(outside.path()).constData(), QFile::encodeName(link).constData()) == 0);
        TemporaryFilesDialog dialog(helper, nullptr, fixture.path()); QSignalSpy loaded(&dialog, &TemporaryFilesDialog::listed), failed(&dialog, &TemporaryFilesDialog::failed), opened(&dialog, &TemporaryFilesDialog::openRequested); dialog.show();
        QTRY_VERIFY_WITH_TIMEOUT(!loaded.isEmpty(), 10000); QVERIFY(select(dialog, "7zOaaaaaaaa")); QTest::keyClick(tree(dialog), Qt::Key_Return);
        QCOMPARE(failed.size(), 1); QVERIFY(failed.last()[0].toString().contains("Link")); QCOMPARE(opened.size(), 0); QCOMPARE(dialog.currentDirectory(), QFileInfo(fixture.path()).canonicalFilePath());
        QProcess process; auto env = QProcessEnvironment::systemEnvironment(); env.insert("SEVENZIP_PORT_TEMP_ROOT", QFileInfo(fixture.path()).canonicalFilePath()); env.insert("SEVENZIP_PORT_TEMP_FOLDER", link); process.setProcessEnvironment(env); process.start(helper, {"i"}); QVERIFY(process.waitForFinished(10000)); QCOMPARE(process.exitCode(), 2); QVERIFY(process.readAllStandardError().contains("Cannot read temporary folder"));
        QCOMPARE(read(outside.filePath("do not touch.txt")), QByteArray("outside"));
    }
    void activeTempsAndChangedItemsAreRetained() {
        QTemporaryDir fixture; const auto path = fixture.filePath("7zO12345678"); QVERIFY(write(path + "/file.txt", "original"));
        QStringList active{QFileInfo(path).canonicalFilePath() + "/file.txt"};
        TemporaryFilesDialog dialog(helper, nullptr, fixture.path(), [&] { return active; }); QSignalSpy loaded(&dialog, &TemporaryFilesDialog::listed), failed(&dialog, &TemporaryFilesDialog::failed), moved(&dialog, &TemporaryFilesDialog::trashed); dialog.show();
        QTRY_VERIFY_WITH_TIMEOUT(!loaded.isEmpty(), 10000); QVERIFY(select(dialog, "7zO12345678")); QVERIFY(!dialog.findChild<QPushButton *>("deleteTemporary")->isEnabled());
        QTest::keyClick(tree(dialog), Qt::Key_Delete); QCOMPARE(failed.size(), 1); QCOMPARE(read(path + "/file.txt"), QByteArray("original"));
        active.clear(); QTest::keyClick(tree(dialog), Qt::Key_R, Qt::ControlModifier); QTRY_COMPARE_WITH_TIMEOUT(loaded.size(), 2, 10000); QVERIFY(select(dialog, "7zO12345678"));
        QTimer decision; decision.setInterval(10); connect(&decision, &QTimer::timeout, &dialog, [&] { if (auto box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget())) { QVERIFY(write(path + "/added.txt", "concurrent modification")); box->button(QMessageBox::Yes)->click(); decision.stop(); } }); decision.start();
        QTest::mouseClick(dialog.findChild<QPushButton *>("deleteTemporary"), Qt::LeftButton); QCOMPARE(moved.size(), 0); QCOMPARE(failed.size(), 2); QVERIFY(QFileInfo::exists(path + "/added.txt"));
    }
    void confirmedTrashAndCancelKeepUnrelatedFiles() {
        QTemporaryDir fixture; const auto folder = fixture.filePath("7zO87654321"), file = fixture.filePath("7zE12345678");
        QVERIFY(write(folder + "/日本語 space.txt", "trash payload")); QVERIFY(write(file, "standalone")); QVERIFY(write(fixture.filePath("unrelated.txt"), "preserved"));
        TemporaryFilesDialog dialog(helper, nullptr, fixture.path()); QSignalSpy loaded(&dialog, &TemporaryFilesDialog::listed), moved(&dialog, &TemporaryFilesDialog::trashed); dialog.show();
        QList<FileSnapshot> ownedTrash; auto cleanup = qScopeGuard([&] { for (const auto &owned : ownedTrash) if (FileInstall::unchanged(owned)) FileInstall::removeMovedSource(owned); });
        connect(&dialog, &TemporaryFilesDialog::trashed, &dialog, [&](QString, QString target) { ownedTrash << FileInstall::capture(target, true); });
        QTRY_VERIFY_WITH_TIMEOUT(!loaded.isEmpty(), 10000); QVERIFY(select(dialog, "7zO87654321"));
        bool yes = false, defaultWasNo = false; QTimer decision; decision.setInterval(10);
        connect(&decision, &QTimer::timeout, &dialog, [&] { if (auto box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget())) { defaultWasNo = box->defaultButton() == box->button(QMessageBox::No); box->button(yes ? QMessageBox::Yes : QMessageBox::Cancel)->click(); } }); decision.start();
        QTest::mouseClick(dialog.findChild<QPushButton *>("deleteTemporary"), Qt::LeftButton); QVERIFY(defaultWasNo); QCOMPARE(moved.size(), 0); QCOMPARE(read(folder + "/日本語 space.txt"), QByteArray("trash payload"));
        tree(dialog)->selectAll(); yes = true; QTest::mouseClick(dialog.findChild<QPushButton *>("deleteTemporary"), Qt::LeftButton); QTRY_COMPARE_WITH_TIMEOUT(moved.size(), 2, 10000); QTRY_COMPARE_WITH_TIMEOUT(loaded.size(), 2, 10000); decision.stop();
        QCOMPARE(ownedTrash.size(), 2); QVERIFY(!QFileInfo::exists(folder)); QVERIFY(!QFileInfo::exists(file)); QCOMPARE(read(fixture.filePath("unrelated.txt")), QByteArray("preserved"));
        for (const auto &entry : moved) { const auto source = entry[0].toString(), target = entry[1].toString(); QVERIFY(QFileInfo::exists(target)); QCOMPARE(read(source.endsWith("87654321") ? target + "/日本語 space.txt" : target), source.endsWith("87654321") ? QByteArray("trash payload") : QByteArray("standalone")); }
    }
    void processFailureAndMenuRemainUsable() {
        QTemporaryDir fixture; TemporaryFilesDialog broken(fixture.filePath("no helper"), nullptr, fixture.path()); QSignalSpy loaded(&broken, &TemporaryFilesDialog::listed); broken.show();
        QTRY_VERIFY_WITH_TIMEOUT(!loaded.isEmpty(), 10000); QVERIFY(!loaded.last()[0].toBool()); QVERIFY(broken.findChild<QLabel *>("temporaryStatus")->text().contains("List temporary files")); broken.reject();
        QSettings().setValue("View/LastPath", fixture.path()); MainWindow window(executable); window.show(); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000);
        auto action = window.findChild<QAction *>("temporaryFilesAction"); QVERIFY(action); QVERIFY(action->isEnabled()); bool seen = false;
        QTimer closer; closer.setInterval(10); connect(&closer, &QTimer::timeout, &window, [&] { if (auto dialog = qobject_cast<TemporaryFilesDialog *>(QApplication::activeModalWidget())) { seen = true; dialog->reject(); closer.stop(); } }); closer.start();
        action->trigger(); QVERIFY(seen); QVERIFY(action->isEnabled());
    }
    void closeWhileReadingAndReuse() {
        QTemporaryDir fixture; QVERIFY(write(fixture.filePath("7zO00000000/file.txt"), "payload"));
        QElapsedTimer elapsed; elapsed.start();
        for (int n = 0; n < 5; ++n) { auto dialog = new TemporaryFilesDialog(helper, nullptr, fixture.path()); dialog->reject(); delete dialog; }
        QVERIFY(elapsed.elapsed() < 500);
        QTRY_VERIFY_WITH_TIMEOUT(qApp->findChildren<QProcess *>(QString(), Qt::FindDirectChildrenOnly).isEmpty(), 5000);
        TemporaryFilesDialog again(helper, nullptr, fixture.path()); QSignalSpy loaded(&again, &TemporaryFilesDialog::listed); again.show(); QTRY_VERIFY_WITH_TIMEOUT(!loaded.isEmpty(), 10000); QVERIFY(loaded.last()[0].toBool()); QVERIFY(select(again, "7zO00000000"));
    }
    void unreadableChildDoesNotHideOtherTemps() {
        QTemporaryDir fixture; const auto path = fixture.filePath("7zO99999999"); QVERIFY(write(path + "/file.txt", "retained")); QVERIFY(write(fixture.filePath("7zO00000000/file.txt"), "visible"));
        const auto restore = qScopeGuard([&] { ::chmod(QFile::encodeName(path).constData(), 0700); }); QVERIFY(::chmod(QFile::encodeName(path).constData(), 0000) == 0);
        const auto rows = native(fixture.path()); QCOMPARE(rows.size(), 2); bool warning = false;
        for (const auto &value : rows) if (value.toObject()["name"] == "7zO99999999") { QVERIFY(value.toObject()["interrupted"].toBool()); QVERIFY(value.toObject()["error"].toString().contains("Permission denied")); warning = true; }
        QVERIFY(warning);
    }
};
int main(int argc, char **argv) {
    QLocale::setDefault(QLocale(QLocale::English));
    if (argc < 2) return 1; executable = QString::fromLocal8Bit(argv[1]); helper = QFileInfo(executable).absolutePath() + "/7zz-progress";
    if (!qEnvironmentVariable("PORT_TEST_PLUGIN_ROOT").isEmpty()) QCoreApplication::setLibraryPaths({qEnvironmentVariable("PORT_TEST_PLUGIN_ROOT")});
    QCoreApplication::setAttribute(Qt::AA_MacDontSwapCtrlAndMeta); QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    QApplication app(argc, argv); applyPortAppearance(app); app.setOrganizationName("SevenZipMacPortTests"); app.setApplicationName("TemporaryFiles"); app.setQuitOnLastWindowClosed(false);
    TemporaryFilesTests tests; QList<char *> args{argv[0]}; for (int n = 2; n < argc; ++n) args << argv[n]; args << nullptr; return QTest::qExec(&tests, args.size() - 1, args.data());
}
#include "temporary-files.moc"
