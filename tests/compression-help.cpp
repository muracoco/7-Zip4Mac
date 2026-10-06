// SPDX-License-Identifier: LGPL-3.0-or-later
#include <QLocale>
#include "CompressionMath.h"
#include "Dialogs.h"
#include "Help.h"
#include "MainWindow.h"
#include "OptionsDialog.h"
#include "UiLanguage.h"
#include "ArchiveFormats.h"
#include <QApplication>
#include <QCryptographicHash>
#include <QDialogButtonBox>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QSignalSpy>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTest>
#include <QTextBrowser>
#include <QTreeWidget>
#include <QPrinter>
#include <QShortcut>
#include <QToolButton>
#include <QMenu>
#include <QThread>
#include <QDirIterator>
#include <QAction>
#include <QPointer>
#include <QWindow>
static QString executable;
class CompressionHelpTests : public QObject {
    Q_OBJECT
    QTemporaryDir temp;
    ArchiveResult execute(SevenZipProcessBackend &backend, const ArchiveRequest &request) { QSignalSpy done(&backend, &ArchiveBackend::finished); backend.start(request); if (done.isEmpty() && !done.wait(30000)) qFatal("7-Zip timed out"); return qvariant_cast<ArchiveResult>(done.last().first()); }
private slots:
    void initTestCase() { QVERIFY(temp.isValid()); QSettings::setDefaultFormat(QSettings::IniFormat); QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, temp.filePath("settings")); QSettings().setValue("View/LastPath", temp.path()); UiLanguage::set("en"); }
    void defaultsAndMemory() {
        CompressionMath::Input input; input.cpus = 12; input.ram = quint64(32) << 30; auto normal = CompressionMath::calculate(input);
        QCOMPARE(normal.dictionary, quint64(32) << 20); QCOMPARE(normal.word, 32u); QCOMPARE(normal.solid, quint64(8) << 30); QCOMPARE(normal.threads, 12u); QVERIFY(normal.compressMemory > normal.decompressMemory);
        input.level = 9; auto ultra = CompressionMath::calculate(input); QCOMPARE(ultra.dictionary, quint64(256) << 20); QCOMPARE(ultra.word, 64u); QCOMPARE(ultra.solid, quint64(16) << 30);
        input.memoryLimit = quint64(1) << 30; auto limited = CompressionMath::calculate(input); QVERIFY(limited.threads < ultra.threads); QCOMPARE(limited.threads, 2u);
        input.format = "zip"; input.method = "Deflate"; input.level = 9; auto zip = CompressionMath::calculate(input); QCOMPARE(zip.dictionary, quint64(32) << 10); QCOMPARE(zip.word, 128u); QCOMPARE(zip.decompressMemory, quint64(2) << 20); QCOMPARE(zip.maximumThreads, 128u);
        input.level = 0; auto store = CompressionMath::calculate(input); QCOMPARE(store.compressMemory, quint64(1) << 20); QCOMPARE(store.decompressMemory, quint64(1) << 20);
        input.format = "7z"; input.method = "PPMd"; input.level = 5; auto ppmd = CompressionMath::calculate(input); QCOMPARE(ppmd.dictionary, quint64(16) << 20); QCOMPARE(ppmd.word, 6u); QCOMPARE(ppmd.threads, 1u);
        QCOMPARE(CompressionMath::dictionaries("PPMd", "zip").first(), quint64(1) << 20); QCOMPARE(CompressionMath::dictionaries("PPMd", "zip").last(), quint64(256) << 20); QCOMPARE(CompressionMath::dictionaries("BZip2", "7z").last(), quint64(900) << 10);
        QCOMPARE(CompressionMath::words("Deflate", "zip").last(), 258u); QCOMPARE(CompressionMath::words("Deflate64", "zip").last(), 257u);
        QCOMPARE(CompressionMath::parseSize("1 TB"), quint64(1) << 40); QCOMPARE(CompressionMath::parseSize("18446744073709551615 GB"), CompressionMath::Automatic);
    }
    void automaticAndManualPersistence() {
        QSettings().remove("Compression"); AddDialog dialog(temp.filePath("defaults.7z")); auto value = dialog.options(); QVERIFY(value.methodAutomatic); QVERIFY(value.dictionary.isEmpty()); QVERIFY(value.wordSize.isEmpty()); QVERIFY(value.solid.isEmpty()); QCOMPARE(value.threads, 0);
        auto dictionary = dialog.findChild<QComboBox *>("compressionDictionary"), level = dialog.findChild<QComboBox *>("compressionLevel"); QVERIFY(dictionary->currentText().startsWith("*  32 MB")); level->setCurrentIndex(9); QCOMPARE(dictionary->currentText(), QString("*  256 MB")); QCOMPARE(dialog.findChild<QComboBox *>("compressionWord")->currentText(), QString("*  64"));
        dictionary->setCurrentText("1 MB"); dialog.findChild<QComboBox *>("compressionWord")->setCurrentText("24"); dialog.findChild<QComboBox *>("compressionThreads")->setCurrentText("2"); dialog.findChild<QComboBox *>("compressionSolid")->setCurrentText("Non-solid"); dialog.findChild<QComboBox *>("compressionMemory")->setCurrentText("4 GB"); dialog.accept();
        AddDialog reopened(temp.filePath("manual.7z")); value = reopened.options(); QCOMPARE(value.dictionary, QString("1m")); QCOMPARE(value.wordSize, QString("24")); QCOMPARE(value.threads, 2); QCOMPARE(value.solid, QString("off")); QCOMPARE(value.compressionMemory, QString("4g"));
        reopened.findChild<QComboBox *>("compressionDictionary")->setCurrentIndex(0); reopened.findChild<QComboBox *>("compressionWord")->setCurrentIndex(0); reopened.findChild<QComboBox *>("compressionSolid")->setCurrentIndex(0); reopened.findChild<QComboBox *>("compressionThreads")->setCurrentIndex(0); reopened.findChild<QComboBox *>("compressionMemory")->setCurrentIndex(0); reopened.accept();
        AddDialog autoReopened(temp.filePath("auto.7z")); value = autoReopened.options(); QVERIFY(value.dictionary.isEmpty()); QVERIFY(value.wordSize.isEmpty()); QVERIFY(value.solid.isEmpty()); QVERIFY(value.compressionMemory.isEmpty()); QCOMPARE(value.threads, 0);
        autoReopened.findChild<QComboBox *>("compressionMethod")->setCurrentText("PPMd"); QCOMPARE(autoReopened.findChild<QComboBox *>("compressionWord")->currentText(), QString("*  32")); QCOMPARE(autoReopened.options().method, QString("PPMd")); QVERIFY(!autoReopened.options().methodAutomatic);
        autoReopened.findChild<QComboBox *>("archiveFormat")->setCurrentText("zip"); QCOMPARE(autoReopened.options().method, QString("Deflate")); QVERIFY(!autoReopened.findChild<QComboBox *>("compressionSolid")->isEnabled()); QCOMPARE(autoReopened.findChild<QLabel *>("decompressionMemoryRequired")->text(), QString("2 MB")); QSettings().remove("Compression");
    }
    void officialControlInventory() {
        QSettings().remove("Compression"); AddDialog dialog(temp.filePath("inventory.7z"));
        auto format = dialog.findChild<QComboBox *>("archiveFormat"), method = dialog.findChild<QComboBox *>("compressionMethod"), level = dialog.findChild<QComboBox *>("compressionLevel"), threads = dialog.findChild<QComboBox *>("compressionThreads"), solid = dialog.findChild<QComboBox *>("compressionSolid"), memory = dialog.findChild<QComboBox *>("compressionMemory");
        QCOMPARE(method->count(), 4); QCOMPARE(method->itemText(0), QString("*  LZMA2")); QVERIFY(method->findText("Deflate") < 0); QVERIFY(method->findText("Copy") < 0);
        const auto cpus = qMax(1, QThread::idealThreadCount()); QCOMPARE(threads->count(), qMin(512, cpus * 2) + 1);
        method->setCurrentText("PPMd"); QCOMPARE(threads->count(), 1); QVERIFY(!threads->isEnabled()); QVERIFY(dialog.findChild<QComboBox *>("compressionWord")->isEnabled());
        level->setCurrentIndex(level->findData(0)); QCOMPARE(method->count(), 0); QVERIFY(!method->isEnabled()); QVERIFY(memory->isEnabled()); QVERIFY(solid->count() == 0);
        format->setCurrentText("zip"); QCOMPARE(level->count(), 6); QCOMPARE(level->currentData().toInt(), 5); QCOMPARE(method->count(), 5); QCOMPARE(method->itemText(0), QString("*  Deflate")); QVERIFY(!dialog.findChild<QComboBox *>("compressionDictionary")->isEnabled());
        format->setCurrentText("gzip"); QCOMPARE(level->count(), 4); QCOMPARE(method->count(), 1); QVERIFY(!method->isEnabled()); QCOMPARE(threads->count(), 0);
        format->setCurrentText("xz"); QCOMPARE(level->count(), 9); QCOMPARE(method->count(), 1); QVERIFY(!method->isEnabled()); QVERIFY(solid->findText("Non-solid") < 0); QVERIFY(solid->findText("Solid") > 0);
        format->setCurrentText("tar"); QCOMPARE(level->count(), 1); QVERIFY(!level->isEnabled()); QCOMPARE(method->count(), 2); QVERIFY(!memory->isEnabled()); QVERIFY(dialog.findChild<QLabel *>("decompressionMemoryRequired")->isHidden());
        format->setCurrentText("wim"); QCOMPARE(method->count(), 0); QVERIFY(!memory->isEnabled()); format->setCurrentText("Hash"); QCOMPARE(level->count(), 0); QCOMPARE(method->count(), 2);
        QCOMPARE(CompressionMath::parseSize(memory->itemText(memory->count() - 1)), quint64(16) << 40);
        CompressionMath::Input input; input.cpus = 3; input.ram = quint64(32) << 30; QCOMPARE(CompressionMath::threadChoices(input), QList<unsigned>({1,2,3,4,5,6})); input.method = "PPMd"; QVERIFY(CompressionMath::threadChoices(input).isEmpty());
    }
    void formatDraftsCommitAndCancel() {
        QSettings().remove("Compression"); const auto archive = temp.filePath("drafts.7z");
        { AddDialog dialog(archive); auto format = dialog.findChild<QComboBox *>("archiveFormat");
          dialog.findChild<QComboBox *>("compressionDictionary")->setCurrentText("1 MB"); dialog.findChild<QComboBox *>("compressionWord")->setCurrentText("24"); dialog.findChild<QLineEdit *>("compressionParameters")->setText("fb=24"); dialog.findChild<QLineEdit *>("addPassword")->setText("never saved secret");
          format->setCurrentText("zip"); dialog.findChild<QComboBox *>("compressionMethod")->setCurrentText("BZip2"); dialog.findChild<QComboBox *>("compressionDictionary")->setCurrentText("100 KB");
          format->setCurrentText("7z"); QCOMPARE(dialog.options().dictionary, QString("1m")); QCOMPARE(dialog.options().wordSize, QString("24")); QCOMPARE(dialog.options().parameters, QString("fb=24"));
          QVERIFY(!QSettings().contains("Compression/7z/DictionaryText")); dialog.reject(); }
        QVERIFY(!QSettings().contains("Compression/7z/DictionaryText")); QVERIFY(!QSettings().contains("Compression/zip/Method"));
        { AddDialog dialog(archive); auto format = dialog.findChild<QComboBox *>("archiveFormat"); dialog.findChild<QComboBox *>("compressionDictionary")->setCurrentText("1 MB"); format->setCurrentText("zip"); dialog.findChild<QComboBox *>("compressionMethod")->setCurrentText("BZip2"); dialog.findChild<QComboBox *>("compressionDictionary")->setCurrentText("100 KB"); dialog.accept(); }
        QCOMPARE(QSettings().value("Compression/LastFormat").toString(), QString("zip")); AddDialog seven(archive); QCOMPARE(seven.options().dictionary, QString("1m")); AddDialog zip(temp.filePath("drafts.zip")); QCOMPARE(zip.options().method, QString("BZip2")); QCOMPARE(zip.options().dictionary, QString("100k"));
        AddDialog preferred(temp.filePath("no-extension")); QCOMPARE(preferred.options().format, QString("zip")); AddDialog explicitSeven(archive); QCOMPARE(explicitSeven.options().format, QString("7z"));
        for (const auto &key : QSettings().allKeys()) QVERIFY(!QSettings().value(key).toString().contains("never saved secret")); QSettings().remove("Compression");
    }
    void localizedDynamicControls() {
        QSettings().remove("Compression"); UiLanguage::set("ja");
        AddDialog dialog(temp.filePath("localized.7z")); auto format = dialog.findChild<QComboBox *>("archiveFormat");
        auto level = dialog.findChild<QComboBox *>("compressionLevel"), solid = dialog.findChild<QComboBox *>("compressionSolid");
        QVERIFY(level->currentText().startsWith("5 - ")); QVERIFY(!level->currentText().contains("Normal"));
        solid->setCurrentIndex(solid->findData("off")); QCOMPARE(dialog.options().solid, QString("off")); QVERIFY(solid->currentText() != "Non-solid");
        format->setCurrentText("zip"); QVERIFY(!level->currentText().contains("Normal")); format->setCurrentText("7z"); QCOMPARE(dialog.options().solid, QString("off"));
        UiLanguage::set("en"); UiLanguage::apply(&dialog); QCOMPARE(level->currentText(), QString("5 - Normal")); QCOMPARE(solid->currentText(), QString("Non-solid")); QCOMPARE(dialog.options().solid, QString("off"));
        solid->setEditText("13 MB"); UiLanguage::set("ja"); UiLanguage::apply(&dialog); QCOMPARE(solid->currentText(), QString("13 MB")); QCOMPARE(dialog.options().solid, QString("13m"));
        dialog.accept(); UiLanguage::set("en"); AddDialog reopened(temp.filePath("restored.7z")); QCOMPARE(reopened.options().solid, QString("13m"));
        reopened.findChild<QComboBox *>("compressionMethod")->setCurrentText("PPMd");
        for (auto label : reopened.findChildren<QLabel *>()) QVERIFY(label->text() != "&Order:");
        QSettings().remove("Compression");
    }
    void originalTimeAndLinkOptions() {
        QSettings().remove("Compression");
        const auto zip = ArchiveFormats::find("zip"), tar = ArchiveFormats::find("tar");
        QCOMPARE(zip.precisionMask, quint32(7)); QCOMPARE(tar.precisionMask, quint32(11)); QCOMPARE(tar.defaultPrecision, 1);
        QVERIFY(tar.symbolicLinks && tar.hardLinks); QVERIFY(!zip.symbolicLinks && !zip.hardLinks);
        for (const auto &format : QStringList{"7z", "zip", "tar", "gzip", "xz", "bzip2", "wim"}) {
            AddDialog add(temp.filePath("time-options.7z")); add.findChild<QComboBox *>("archiveFormat")->setCurrentText(format);
            if (format == "tar") add.findChild<QComboBox *>("compressionMethod")->setCurrentText("GNU");
            int visited = 0;
            QTimer::singleShot(0, &add, [&] {
                auto options = add.findChild<QDialog *>("compressOptionsDialog"); QVERIFY(options); ++visited;
                auto precision = options->findChild<QComboBox *>("timestampPrecision"); auto specified = options->findChild<QCheckBox *>("timestampPrecisionSet");
                auto created = options->findChild<QCheckBox *>("storeCreationTimeSet"), accessed = options->findChild<QCheckBox *>("storeAccessTimeSet");
                if (format == "7z" || format == "wim") { QCOMPARE(precision->count(), 1); QVERIFY(specified->isHidden()); QVERIFY(!precision->isEnabled()); }
                if (format == "xz" || format == "bzip2") { QVERIFY(precision->isHidden()); QVERIFY(options->findChild<QCheckBox *>("storeModificationTime")->isHidden()); }
                if (format == "gzip") { QCOMPARE(precision->currentData().toInt(), 1); QVERIFY(!options->findChild<QCheckBox *>("storeModificationTimeSet")->isHidden()); }
                if (format == "tar") { QVERIFY(created->isHidden()); QVERIFY(accessed->isHidden()); QCOMPARE(precision->currentData().toInt(), 1); QVERIFY(options->findChild<QLabel *>("compressionTimeInfo")->text().endsWith(": GNU")); }
                if (format == "zip") {
                    QVERIFY(!created->isHidden()); specified->setChecked(true); precision->setCurrentIndex(precision->findData(1)); QVERIFY(created->isHidden()); QVERIFY(accessed->isHidden());
                    specified->setChecked(false); QCOMPARE(precision->currentData().toInt(), 0); QVERIFY(!created->isHidden());
                    created->setChecked(true); auto creation = options->findChild<QCheckBox *>("storeCreationTime"); creation->setChecked(true); created->setChecked(false); QVERIFY(!creation->isChecked()); QVERIFY(!creation->isEnabled());
                }
                auto latest = options->findChild<QCheckBox *>("latestArchiveTime"), latestSet = options->findChild<QCheckBox *>("latestArchiveTimeSet");
                QVERIFY(!latest->isEnabled()); latestSet->setChecked(true); QVERIFY(latest->isEnabled()); latest->setChecked(false);
                options->accept();
            });
            add.findChild<QPushButton *>("compressionOptions")->click(); QCOMPARE(visited, 1); QVERIFY(add.options().latestArchiveTimeSpecified); QVERIFY(!add.options().latestArchiveTime);
        }
        AddDialog add(temp.filePath("posix.tar")); add.findChild<QComboBox *>("archiveFormat")->setCurrentText("tar"); add.findChild<QComboBox *>("compressionMethod")->setCurrentText("POSIX");
        QTimer::singleShot(0, &add, [&] { auto options = add.findChild<QDialog *>("compressOptionsDialog"); QVERIFY(options); QVERIFY(!options->findChild<QCheckBox *>("storeAccessTimeSet")->isHidden()); QVERIFY(options->findChild<QCheckBox *>("storeCreationTimeSet")->isHidden()); options->reject(); });
        add.findChild<QPushButton *>("compressionOptions")->click(); QSettings().remove("Compression");
    }
    void explicitLatestTimeOffRoundtrip() {
        QSettings().remove("Compression"); QFile source(temp.filePath("latest off 日本語.txt")); QVERIFY(source.open(QIODevice::WriteOnly)); source.write("original content"); source.close();
        QVERIFY(source.open(QIODevice::ReadWrite)); QVERIFY(source.setFileTime(QDateTime::fromSecsSinceEpoch(946684800), QFileDevice::FileModificationTime)); source.close();
        AddDialog dialog(temp.filePath("latest-off.7z"));
        QTimer::singleShot(0, &dialog, [&] { auto options = dialog.findChild<QDialog *>("compressOptionsDialog"); QVERIFY(options); options->findChild<QCheckBox *>("latestArchiveTimeSet")->setChecked(true); options->findChild<QCheckBox *>("latestArchiveTime")->setChecked(false); options->accept(); });
        dialog.findChild<QPushButton *>("compressionOptions")->click(); dialog.accept();
        AddDialog reopened(temp.filePath("latest-off.7z")); auto request = reopened.options(); QVERIFY(request.latestArchiveTimeSpecified); QVERIFY(!request.latestArchiveTime);
        request.workingDirectory = temp.path(); request.files = {"latest off 日本語.txt"}; SevenZipProcessBackend backend(executable); auto result = execute(backend, request); QVERIFY2(result.success, qPrintable(result.message + result.details));
        QVERIFY(QFileInfo(request.archive).lastModified().toSecsSinceEpoch() > 946684800);
        request.operation = ArchiveOperation::Extract; request.outputDirectory = temp.filePath("latest-off-out"); request.overwriteMode = "overwrite"; request.files.clear(); result = execute(backend, request); QVERIFY2(result.success, qPrintable(result.message + result.details));
        QFile restored(request.outputDirectory + "/latest off 日本語.txt"); QVERIFY(restored.open(QIODevice::ReadOnly)); QCOMPARE(restored.readAll(), QByteArray("original content")); QCOMPARE(QFileInfo(restored).lastModified().toSecsSinceEpoch(), qint64(946684800)); QSettings().remove("Compression");
    }
    void upstreamResetTransitions() {
        QSettings().remove("Compression"); AddDialog dialog(temp.filePath("resets.7z"));
        auto dictionary = dialog.findChild<QComboBox *>("compressionDictionary"), word = dialog.findChild<QComboBox *>("compressionWord"), solid = dialog.findChild<QComboBox *>("compressionSolid"), threads = dialog.findChild<QComboBox *>("compressionThreads"), method = dialog.findChild<QComboBox *>("compressionMethod"), level = dialog.findChild<QComboBox *>("compressionLevel");
        dictionary->setCurrentText("1 MB"); solid->setCurrentText("16 MB"); dictionary->setCurrentText("2 MB"); QVERIFY(dialog.options().solid.isEmpty());
        solid->setCurrentText("Non-solid"); dictionary->setCurrentText("4 MB"); QCOMPARE(dialog.options().solid, QString("off")); solid->setCurrentText("Solid"); dictionary->setCurrentText("8 MB"); QCOMPARE(dialog.options().solid, QString("on"));
        method->setCurrentText("PPMd"); QVERIFY(dialog.options().dictionary.isEmpty()); dictionary->setCurrentText("1 MB"); word->setCurrentText("16"); threads->setCurrentText("2");
        level->setCurrentIndex(level->findData(9)); auto options = dialog.options(); QCOMPARE(options.method, QString("LZMA2")); QVERIFY(options.methodAutomatic); QVERIFY(options.dictionary.isEmpty()); QVERIFY(options.wordSize.isEmpty()); QVERIFY(options.solid.isEmpty()); QCOMPARE(options.threads, 0); QCOMPARE(dictionary->currentText(), QString("*  256 MB"));
        dialog.findChild<QComboBox *>("archiveFormat")->setCurrentText("zip"); method->setCurrentText("PPMd"); word->setCurrentText("17"); int errors = 0;
        QTimer::singleShot(0, &dialog, [&] { auto box = dialog.findChild<QMessageBox *>(); QVERIFY(box); QVERIFY(box->text().contains("selected method")); ++errors; box->accept(); }); QTest::mouseClick(dialog.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok), Qt::LeftButton); QCOMPARE(errors, 1); QSettings().remove("Compression");
    }
    void methodGuardAndNearestSavedChoices() {
        QSettings().remove("Compression"); QSettings().setValue("Compression/7z/Method", "PPMd"); QSettings().setValue("Compression/7z/MethodAutomatic", false); QSettings().setValue("Compression/7z/DictionaryText", "3 MB"); QSettings().setValue("Compression/7z/DictionaryAutomatic", false); QSettings().setValue("Compression/7z/Word", "15"); QSettings().setValue("Compression/7z/WordAutomatic", false);
        AddDialog dialog(temp.filePath("guard.7z")); QCOMPARE(dialog.options().dictionary, QString("3m")); QCOMPARE(dialog.options().wordSize, QString("14"));
        auto method = dialog.findChild<QComboBox *>("compressionMethod"); method->setCurrentText("LZMA"); QVERIFY(dialog.options().dictionary.isEmpty()); QVERIFY(dialog.options().wordSize.isEmpty()); method->setCurrentText("PPMd"); QCOMPARE(dialog.options().dictionary, QString("3m")); QCOMPARE(dialog.options().wordSize, QString("14"));
        auto choices = CompressionMath::dictionaryChoices({"7z", "LZMA2"}, quint64(4) << 30); QCOMPARE(choices.items.at(choices.selected).value, quint64(15) << 28); QSettings().remove("Compression");
    }
    void historyAndToolbarInitialFormat() {
        QSettings().remove("Compression"); QSettings().setValue("Compression/LastFormat", "zip"); QStringList saved;
        for (int n = 0; n < 30; ++n) saved << temp.filePath(QString::number(n) + ".7z"); QSettings().setValue("Compression/History", saved);
        { AddDialog dialog(temp.filePath("history.zip")); auto history = dialog.findChild<QComboBox *>("archiveHistory"); QCOMPARE(history->count(), 20); history->setCurrentIndex(0); QVERIFY(QMetaObject::invokeMethod(history, "activated", Q_ARG(int, 0))); QCOMPARE(dialog.options().format, QString("7z"));
          for (auto check : dialog.findChildren<QCheckBox *>()) if (check->text() == "Show Password") { check->setChecked(true); QVERIFY(dialog.findChild<QLineEdit *>("repeatPassword")->isHidden()); check->setChecked(false); QVERIFY(!dialog.findChild<QLineEdit *>("repeatPassword")->isHidden()); }
          dialog.accept(); QCOMPARE(QSettings().value("Compression/History").toStringList().size(), 20); }
        QSettings().setValue("Compression/LastFormat", "zip"); const auto source = temp.filePath("toolbar source.txt"); QFile file(source); QVERIFY(file.open(QIODevice::WriteOnly)); QCOMPARE(file.write("toolbar format"), qint64(14)); file.close();
        MainWindow window(executable); window.openPath(temp.path()); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 20000); auto list = window.findChild<FileList *>("fileList"); QVERIFY(list); QTreeWidgetItem *selected = nullptr;
        for (int n = 0; n < list->topLevelItemCount(); ++n) if (list->topLevelItem(n)->text(0) == "toolbar source.txt") selected = list->topLevelItem(n); QVERIFY(selected); list->setCurrentItem(selected); selected->setSelected(true);
        bool checked = false; QTimer::singleShot(0, &window, [&] { auto dialog = window.findChild<AddDialog *>(); QVERIFY(dialog); QCOMPARE(dialog->options().format, QString("zip")); QVERIFY(dialog->options().archive.endsWith(".zip")); checked = true; dialog->reject(); }); auto action = window.findChild<QAction *>("addAction"); QVERIFY(action); action->trigger(); QVERIFY(checked); window.close(); QSettings().remove("Compression");
    }
    void translatedSolidCommands() {
        QSettings().remove("Compression"); UiLanguage::set("ja");
        { AddDialog dialog(temp.filePath("translated.7z")); auto solid = dialog.findChild<QComboBox *>("compressionSolid"); const auto off = solid->findData("off"); QVERIFY(off >= 0); solid->setCurrentIndex(off); QCOMPARE(dialog.options().solid, QString("off")); dialog.findChild<QComboBox *>("compressionDictionary")->setCurrentText("1 MB"); QCOMPARE(dialog.options().solid, QString("off")); dialog.accept(); }
        { AddDialog reopened(temp.filePath("translated-restored.7z")); QCOMPARE(reopened.options().solid, QString("off")); auto solid = reopened.findChild<QComboBox *>("compressionSolid"); solid->setCurrentIndex(solid->findData("on")); QCOMPARE(reopened.options().solid, QString("on")); }
        UiLanguage::set("en"); QSettings().remove("Compression");
    }
    void selectedMethodRoundtrip_data() {
        QTest::addColumn<QString>("format"); QTest::addColumn<QString>("method");
        for (const auto &choice : QStringList{"7z/LZMA2", "7z/LZMA", "7z/PPMd", "7z/BZip2", "7z/Copy", "zip/Deflate", "zip/Deflate64", "zip/BZip2", "zip/LZMA", "zip/PPMd", "zip/Copy", "tar/GNU", "tar/POSIX", "wim/Copy", "xz/LZMA2", "gzip/Deflate", "bzip2/BZip2", "Hash/SHA256", "Hash/SHA1"}) {
            const auto fields = choice.split('/'); QTest::newRow(qPrintable(choice)) << fields[0] << fields[1];
        }
    }
    void selectedMethodRoundtrip() {
        QFETCH(QString, format); QFETCH(QString, method); QSettings().remove("Compression");
        const QString sourceName = "method 日本語 space.txt"; QFile source(temp.filePath(sourceName)); QVERIFY(source.open(QIODevice::WriteOnly)); const QByteArray content = QByteArray("official GUI method roundtrip\n") + QByteArray(1 << 20, 'x'); QCOMPARE(source.write(content), qint64(content.size())); source.close();
        const auto extension = format == "Hash" ? method == "SHA1" ? QString("sha1") : QString("sha256") : format;
        AddDialog dialog(temp.filePath("selected-" + format + '-' + method + '.' + extension)); auto formats = dialog.findChild<QComboBox *>("archiveFormat"); formats->setCurrentText(format);
        auto methods = dialog.findChild<QComboBox *>("compressionMethod"), levels = dialog.findChild<QComboBox *>("compressionLevel");
        if (method == "Copy" && format != "wim") levels->setCurrentIndex(levels->findData(0)); else if (format != "wim") { const auto index = methods->findData(method); QVERIFY(index >= 0); methods->setCurrentIndex(index); }
        auto dictionary = dialog.findChild<QComboBox *>("compressionDictionary"); if (dictionary->isEnabled()) dictionary->setCurrentText(method == "BZip2" ? "100 KB" : "1 MB");
        auto request = dialog.options(); QCOMPARE(request.method, method); request.workingDirectory = temp.path(); request.files = {sourceName}; SevenZipProcessBackend backend(executable);
        auto result = execute(backend, request); QVERIFY2(result.success, qPrintable(result.message + result.details));
        request.operation = ArchiveOperation::Test; request.files.clear(); request.hashTest = format == "Hash"; result = execute(backend, request); QVERIFY2(result.success, qPrintable(result.message + result.details));
        if (format == "Hash") { QFile hashFile(request.archive); QVERIFY(hashFile.open(QIODevice::ReadOnly)); QVERIFY(hashFile.readAll().toLower().contains(QCryptographicHash::hash(content, method == "SHA1" ? QCryptographicHash::Sha1 : QCryptographicHash::Sha256).toHex())); return; }
        request.operation = ArchiveOperation::Extract; request.outputDirectory = temp.filePath("selected-out-" + format + '-' + method); request.overwriteMode = "overwrite"; result = execute(backend, request); QVERIFY2(result.success, qPrintable(result.message + result.details));
        QDirIterator files(request.outputDirectory, QDir::Files, QDirIterator::Subdirectories); QVERIFY(files.hasNext()); QFile restored(files.next()); QVERIFY(restored.open(QIODevice::ReadOnly)); QCOMPARE(QCryptographicHash::hash(restored.readAll(), QCryptographicHash::Sha256), QCryptographicHash::hash(content, QCryptographicHash::Sha256));
    }
    void automaticRoundtrip_data() { QTest::addColumn<QString>("format"); QTest::addColumn<int>("level"); for (auto format : {QString("7z"), QString("zip")}) { QTest::newRow(qPrintable(format + "-normal")) << format << 5; QTest::newRow(qPrintable(format + "-store")) << format << 0; } }
    void automaticRoundtrip() {
        QFETCH(QString, format); QFETCH(int, level); QSettings().remove("Compression"); const QString name = "日本語 space " + format + ".txt"; QFile source(temp.filePath(name)); QVERIFY(source.open(QIODevice::WriteOnly)); const QByteArray content(2 << 20, 'a'); QCOMPARE(source.write(content), qint64(content.size())); source.close();
        AddDialog dialog(temp.filePath("auto-" + format + '-' + QString::number(level) + '.' + format)); auto levels = dialog.findChild<QComboBox *>("compressionLevel"); levels->setCurrentIndex(levels->findData(level)); auto request = dialog.options(); request.workingDirectory = temp.path(); request.files = {name}; SevenZipProcessBackend backend(executable);
        if (format == "7z" && level == 5) request.compressionMemory = "1t";
        auto result = execute(backend, request); QVERIFY2(result.success, qPrintable(result.message + result.details)); request.operation = ArchiveOperation::Test; request.files.clear(); QVERIFY(execute(backend, request).success); request.operation = ArchiveOperation::List; result = execute(backend, request); QVERIFY(result.success); QCOMPARE(result.entries.size(), 1); QVERIFY2(result.entries.first().method.contains(level == 0 ? format == "zip" ? "Store" : "Copy" : format == "7z" ? "LZMA2" : "Deflate"), qPrintable(result.entries.first().method));
        request.operation = ArchiveOperation::Extract; request.outputDirectory = temp.filePath("out-" + format + '-' + QString::number(level)); request.overwriteMode = "overwrite"; QVERIFY(execute(backend, request).success); QFile output(request.outputDirectory + '/' + name); QVERIFY(output.open(QIODevice::ReadOnly)); QCOMPARE(QCryptographicHash::hash(output.readAll(), QCryptographicHash::Sha256), QCryptographicHash::hash(content, QCryptographicHash::Sha256));
    }
    void automaticEncryptedArchive() {
        QSettings().remove("Compression"); QFile file(temp.filePath("暗号 space.txt")); QVERIFY(file.open(QIODevice::WriteOnly)); QCOMPARE(file.write("encrypted automatic"), qint64(19)); file.close(); AddDialog dialog(temp.filePath("encrypted-auto.7z")); dialog.findChild<QLineEdit *>("addPassword")->setText("fixture secret"); dialog.findChild<QLineEdit *>("repeatPassword")->setText("fixture secret"); for (auto check : dialog.findChildren<QCheckBox *>()) if (check->text() == "Encrypt file &names") check->setChecked(true);
        auto request = dialog.options(); QVERIFY(request.methodAutomatic); QVERIFY(request.encryptNames); request.workingDirectory = temp.path(); request.files = {"暗号 space.txt"}; SevenZipProcessBackend backend(executable); auto result = execute(backend, request); QVERIFY2(result.success, qPrintable(result.message + result.details)); QVERIFY(!result.details.contains("fixture secret"));
        request.operation = ArchiveOperation::List; request.files.clear(); request.password = "wrong fixture"; result = execute(backend, request); QVERIFY(!result.success); QVERIFY(result.passwordRequired); QVERIFY(!result.details.contains("wrong fixture")); request.password = "fixture secret"; result = execute(backend, request); QVERIFY(result.success); QCOMPARE(result.entries.first().path, QString("暗号 space.txt")); request.operation = ArchiveOperation::Test; QVERIFY(execute(backend, request).success); request.operation = ArchiveOperation::Extract; request.outputDirectory = temp.filePath("encrypted-auto-out"); request.overwriteMode = "overwrite"; QVERIFY(execute(backend, request).success); QFile restored(request.outputDirectory + "/暗号 space.txt"); QVERIFY(restored.open(QIODevice::ReadOnly)); QCOMPARE(restored.readAll(), QByteArray("encrypted automatic"));
    }
    void insufficientMemoryAndPasswordValidation() {
        QSettings().remove("Compression"); AddDialog dialog(temp.filePath("validation.7z")); dialog.findChild<QComboBox *>("compressionDictionary")->setCurrentText("256 MB"); dialog.findChild<QComboBox *>("compressionMemory")->setCurrentText("256 MB"); int warnings = 0;
        QTimer::singleShot(0, &dialog, [&] { auto box = dialog.findChild<QMessageBox *>(); QVERIFY(box); QVERIFY(box->text().contains("Required:")); ++warnings; box->accept(); }); QTest::mouseClick(dialog.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok), Qt::LeftButton); QCOMPARE(warnings, 1); QCOMPARE(dialog.result(), int(QDialog::Rejected));
        dialog.findChild<QComboBox *>("archiveFormat")->setCurrentText("zip"); dialog.findChild<QLineEdit *>("addPassword")->setText(QString(100, 'a')); dialog.findChild<QLineEdit *>("repeatPassword")->setText(QString(100, 'a')); for (auto combo : dialog.findChildren<QComboBox *>()) if (combo->findText("ZipCrypto") >= 0) combo->setCurrentText("AES-256"); QTimer::singleShot(0, &dialog, [&] { auto box = dialog.findChild<QMessageBox *>(); QVERIFY(box); QVERIFY(box->text().contains("99")); ++warnings; box->accept(); }); QTest::mouseClick(dialog.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok), Qt::LeftButton); QCOMPARE(warnings, 2); QCOMPARE(dialog.result(), int(QDialog::Rejected));
    }
    void helpInventoryAndNavigation() {
        QCOMPARE(Help::pages().size(), 70); QFile file(":/help/manifest.json"); QVERIFY(file.open(QIODevice::ReadOnly)); const auto manifest = QJsonDocument::fromJson(file.readAll()).object(); const auto files = manifest.value("files").toObject(); QCOMPARE(files.size(), 80);
        for (auto it = files.begin(); it != files.end(); ++it) { QFile original(":/help/" + it.key()); QVERIFY(original.open(QIODevice::ReadOnly)); QCOMPARE(QString::fromLatin1(QCryptographicHash::hash(original.readAll(), QCryptographicHash::Sha256).toHex()), it.value().toString()); }
        HelpWindow window; auto browser = window.findChild<QTextBrowser *>("helpBrowser"); QVERIFY(browser); for (const auto &page : Help::pages()) { QVERIFY(window.navigate(QUrl("qrc:/help/" + page))); QCOMPARE(window.source().path(), "/help/" + page); QVERIFY2(!browser->toPlainText().trimmed().isEmpty(), qPrintable(page)); }
        auto contents = window.findChild<QTreeWidget *>("helpContents"); int count = 0; QTreeWidgetItemIterator entries(contents); while (*entries) { QVERIFY(window.navigate(QUrl("qrc:/help/" + (*entries)->data(0, Qt::UserRole).toString()))); ++count; ++entries; } QCOMPARE(count, 70); QCOMPARE(window.findChild<QTreeWidget *>("helpIndex")->topLevelItemCount(), 69);
        QVERIFY(window.navigate(QUrl("qrc:/help/fm/Menu.htm"))); QCOMPARE(window.source().path(), QString("/help/fm/menu.htm")); QVERIFY(window.navigate(QUrl("qrc:/help/fm/plugins/7-zip/add.htm"))); QVERIFY(window.navigate(QUrl("../../../../cmdline/switches/method.htm#lzma"))); QCOMPARE(window.source().path(), QString("/help/cmdline/switches/method.htm"));
        static const QRegularExpression links("href=[\"']([^\"']+)[\"']", QRegularExpression::CaseInsensitiveOption); for (const auto &page : Help::pages()) { auto matches = links.globalMatch(QString::fromLatin1(Help::resource(QUrl("qrc:/help/" + page)))); while (matches.hasNext()) { const auto url = QUrl(matches.next().captured(1)); if (Help::external(url)) continue; QVERIFY2(!Help::resolve(url, QUrl("qrc:/help/" + page)).isEmpty(), qPrintable(page + " -> " + url.toString())); } }
    }
    void helpResourceBoundary() {
        HelpWindow window; const auto source = window.source(); const QList<QUrl> unsafe{QUrl("file:///etc/passwd"), QUrl("https://example.com/"), QUrl("qrc:/help/../../../etc/passwd"), QUrl("qrc:/icons/FM.ico"), QUrl("qrc:/help/manifest.json"), QUrl("qrc:/help/start.htm?other=1"), QUrl("qrc://remote/help/start.htm"), QUrl("javascript:alert(1)")};
        for (const auto &url : unsafe) { QVERIFY(!window.navigate(url)); QVERIFY(Help::resource(url).isEmpty()); QCOMPARE(window.source(), source); }
        QVERIFY(Help::external(QUrl("http://www.7-zip.org/support.html"))); QVERIFY(!Help::external(QUrl("http://www.7-zip.org/other.html"))); QVERIFY(!Help::external(QUrl("http://www.7-zip.org:8080/"))); QSignalSpy external(&window, &HelpWindow::externalLinkRequested); auto browser = window.findChild<QTextBrowser *>("helpBrowser"); QVERIFY(QMetaObject::invokeMethod(browser, "anchorClicked", Q_ARG(QUrl, QUrl("http://www.7-zip.org/recover.html")))); QCOMPARE(external.size(), 1); QCOMPARE(window.source(), source);
    }
    void contextualHelpButtons() {
        AddDialog add(temp.filePath("help.7z")); QTest::mouseClick(add.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Help), Qt::LeftButton); QPointer<HelpWindow> shared = Help::currentWindow(); QVERIFY(shared); QCOMPARE(shared->source().path(), QString("/help/fm/plugins/7-zip/add.htm"));
        ExtractDialog extract(temp.filePath("out"), false); QTest::mouseClick(extract.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Help), Qt::LeftButton); QCOMPARE(Help::currentWindow(), shared.data()); QCOMPARE(shared->source().path(), QString("/help/fm/plugins/7-zip/extract.htm"));
        OptionsDialog options; auto tabs = options.findChild<QTabWidget *>("optionsTabs"); auto buttons = options.findChild<QDialogButtonBox *>("optionsButtons"); for (int n = 0; n < 6; ++n) { tabs->setCurrentIndex(n); QTest::mouseClick(buttons->button(QDialogButtonBox::Help), Qt::LeftButton); QCOMPARE(Help::currentWindow(), shared.data()); QCOMPARE(shared->source().path(), QString("/help/fm/options.htm")); QCOMPARE(shared->source().fragment(), QStringList({"system", "sevenZip", "folders", "editor", "settings", "language"})[n]); }
        MainWindow main(executable); QTRY_VERIFY_WITH_TIMEOUT(!main.operationBusy(), 10000); auto action = main.findChild<QAction *>("helpAction"); QVERIFY(action); action->trigger(); QCOMPARE(Help::currentWindow(), shared.data()); QCOMPARE(shared->source().path(), QString("/help/fm/index.htm"));
        shared->close(); QTRY_VERIFY(shared.isNull());
    }
    void fullTextSearch() {
        HelpSearch::Options exact; exact.similarWords = false;
        const QList<HelpSearch::Topic> topics{{"one.htm", "Compression", "compress archive zip file name"}, {"two.htm", "Extraction", "extract archive file names"}, {"three.htm", "Zip only", "zip alpha beta gamma"}};
        auto paths = [](const HelpSearch::Results &result) { QStringList values; for (const auto &hit : result.hits) values << hit.path; values.sort(); return values; };
        QCOMPARE(paths(HelpSearch::search("archive AND (compress OR extract)", topics, exact)), QStringList({"one.htm", "two.htm"}));
        QCOMPARE(paths(HelpSearch::search("archive NOT zip", topics, exact)), QStringList({"two.htm"}));
        QCOMPARE(paths(HelpSearch::search("\"file name\"", topics, exact)), QStringList({"one.htm"}));
        QCOMPARE(paths(HelpSearch::search("extr?ct arch*", topics, exact)), QStringList({"two.htm"}));
        QCOMPARE(paths(HelpSearch::search("zip NEAR file", topics, exact)), QStringList({"one.htm"}));
        QCOMPARE(paths(HelpSearch::search("(zip OR extract) file", topics, exact)), QStringList({"one.htm", "two.htm"}));
        exact.titlesOnly = true; QCOMPARE(paths(HelpSearch::search("zip", topics, exact)), QStringList({"three.htm"}));
        exact.titlesOnly = false; exact.withinPrevious = true; exact.previous = {"two.htm"}; QCOMPARE(paths(HelpSearch::search("archive", topics, exact)), QStringList({"two.htm"}));
        exact.withinPrevious = false; QVERIFY(HelpSearch::search("files", topics, exact).hits.isEmpty()); exact.similarWords = true; QCOMPARE(paths(HelpSearch::search("files", topics, exact)), QStringList({"one.htm", "two.htm"}));
        for (const auto &query : QStringList{"", "\"archive", "archive OR", "(archive", "archive)", "NOT archive", QString(4097, 'x'), QString(35, '(') + "archive" + QString(35, ')')}) QVERIFY2(!HelpSearch::search(query, topics, exact).error.isEmpty(), qPrintable(query));
        exact.similarWords = false; const auto all = Help::topics(); QCOMPARE(all.size(), 70); const auto found = HelpSearch::search("\"Encrypt file names\"", all, exact); QVERIFY(found.error.isEmpty()); QVERIFY(paths(found).contains("fm/plugins/7-zip/add.htm"));
    }
    void helpSearchUiAndPrint() {
        HelpWindow window; auto query = window.findChild<QLineEdit *>("helpSearchQuery"); auto list = window.findChild<QPushButton *>("helpListTopics"); auto results = window.findChild<QTreeWidget *>("helpSearchResults"); QVERIFY(query && list && results);
        window.findChild<QCheckBox *>("helpSearchSimilar")->setChecked(false); query->setText("\"Encrypt file names\""); QSignalSpy finished(&window, &HelpWindow::searchFinished);
        QTest::mouseClick(list, Qt::LeftButton); QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, 20000); QVERIFY(results->topLevelItemCount() > 0); QVERIFY(query->isEnabled());
        QTreeWidgetItem *add = nullptr; for (int n = 0; n < results->topLevelItemCount(); ++n) if (results->topLevelItem(n)->data(0, Qt::UserRole).toString() == "fm/plugins/7-zip/add.htm") add = results->topLevelItem(n); QVERIFY(add); results->setCurrentItem(add);
        QTest::mouseClick(window.findChild<QPushButton *>("helpDisplay"), Qt::LeftButton); QCOMPARE(window.source().path(), QString("/help/fm/plugins/7-zip/add.htm"));
        QPrinter printer(QPrinter::HighResolution); printer.setOutputFormat(QPrinter::PdfFormat); const auto output = temp.filePath("日本語 Help output.pdf"); printer.setOutputFileName(output); QVERIFY(window.print(&printer)); QFile pdf(output); QVERIFY(pdf.open(QIODevice::ReadOnly)); QVERIFY(pdf.readAll().startsWith("%PDF-")); QVERIFY(pdf.size() > 10000);
        QVERIFY(window.navigate(QUrl("qrc:/help/cmdline/index.htm"))); const auto pages = Help::printTopics(window.source(), true); QVERIFY(pages.size() > 10); QVERIFY(pages.contains("cmdline/commands/add.htm")); printer.setOutputFileName(temp.filePath("all-command-topics.pdf")); QVERIFY(window.print(&printer, true)); QVERIFY(QFileInfo(printer.outputFileName()).size() > 30000);
        QVERIFY(Help::printTopics(QUrl("file:///etc/passwd"), true).isEmpty()); QVERIFY(!window.print(nullptr));
    }
    void originalAboutActions() {
        AboutDialog dialog; QSignalSpy link(&dialog, &AboutDialog::homePageRequested); QTest::mouseClick(dialog.findChild<QPushButton *>("aboutHomePage"), Qt::LeftButton); QCOMPARE(link.size(), 1); QCOMPARE(link.first().first().toUrl(), QUrl("https://www.7-zip.org/"));
        QVERIFY(QMetaObject::invokeMethod(dialog.findChild<QShortcut *>(), "activated")); QPointer<HelpWindow> help = Help::currentWindow(); QVERIFY(help); QCOMPARE(help->source().path(), QString("/help/start.htm")); QTest::mouseClick(dialog.findChild<QPushButton *>("aboutOk"), Qt::LeftButton); QCOMPARE(dialog.result(), int(QDialog::Accepted)); help->close(); QTRY_VERIFY(help.isNull());
    }
    void helpWindowLifetimeAndHistory() {
        if (auto existing = Help::currentWindow()) { QPointer<HelpWindow> old = existing; old->close(); QTRY_VERIFY(old.isNull()); }
        QPointer<HelpWindow> shared;
        {
            QDialog caller; caller.setWindowModality(Qt::ApplicationModal); caller.show();
            Help::show(&caller, "start.htm"); shared = Help::currentWindow(); QVERIFY(shared); QVERIFY(!shared->parentWidget()); QVERIFY(shared->isVisible());
            QVERIFY(shared->windowHandle()); QCOMPARE(shared->windowHandle()->transientParent(), caller.windowHandle());
            Help::show(&caller, "fm/temp.htm"); QCOMPARE(Help::currentWindow(), shared.data());
            auto browser = shared->findChild<QTextBrowser *>("helpBrowser"); QVERIFY(browser->isBackwardAvailable()); browser->backward(); QCOMPARE(shared->source().path(), QString("/help/start.htm")); browser->forward(); QCOMPARE(shared->source().path(), QString("/help/fm/temp.htm"));
        }
        QVERIFY(shared); QVERIFY(shared->isVisible()); QVERIFY(!shared->windowHandle()->transientParent());
        Help::show(nullptr, "fm/options.htm#settings"); QCOMPARE(Help::currentWindow(), shared.data()); QCOMPARE(shared->source().fragment(), QString("settings"));
        shared->close(); QTRY_VERIFY(shared.isNull()); QVERIFY(!Help::currentWindow());
        Help::show(nullptr, "start.htm"); shared = Help::currentWindow(); QVERIFY(shared); shared->close(); QTRY_VERIFY(shared.isNull());
    }
    void helpHistoryAndOperators() {
        QSettings().remove("Help/SearchHistory"); HelpWindow window;
        auto history = window.findChild<QComboBox *>("helpSearchHistory"); auto query = window.findChild<QLineEdit *>("helpSearchQuery");
        auto menu = window.findChild<QMenu *>("helpSearchOperatorMenu"); auto operators = window.findChild<QToolButton *>("helpSearchOperators");
        QVERIFY(history && query && menu && operators); QCOMPARE(history->count(), 0); QVERIFY(query->text().isEmpty());
        QCOMPARE(menu->actions().size(), 4);
        const QStringList words{"AND", "OR", "NOT", "NEAR"};
        for (int n = 0; n < words.size(); ++n) { query->setText("archivefile"); query->setCursorPosition(7); menu->actions()[n]->trigger(); QCOMPARE(query->text(), "archive " + words[n] + " file"); }
        query->setText("archive wrong file"); query->setSelection(8, 5); menu->actions()[0]->trigger(); QCOMPARE(query->text(), QString("archive AND file"));
        query->setText("archive"); query->setCursorPosition(7); menu->actions()[1]->trigger(); QCOMPARE(query->text(), QString("archive OR "));
        auto list = window.findChild<QPushButton *>("helpListTopics"); QSignalSpy finished(&window, &HelpWindow::searchFinished);
        query->setText("archive AND file"); QTest::mouseClick(list, Qt::LeftButton); QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, 20000); QVERIFY(finished.last()[0].toInt() > 0);
        QCOMPARE(query->text(), QString("archive AND file")); QCOMPARE(history->itemText(0), query->text());
        query->setText("zip OR extract"); finished.clear(); QTest::keyClick(query, Qt::Key_Return); QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, 20000); QCOMPARE(history->count(), 2); QCOMPARE(history->itemText(0), QString("zip OR extract"));
        history->setCurrentIndex(1); QCOMPARE(query->text(), QString("archive AND file")); finished.clear(); QTest::mouseClick(list, Qt::LeftButton); QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, 20000); QCOMPARE(history->count(), 2); QCOMPARE(history->itemText(0), QString("archive AND file"));
        query->setText("archive OR"); finished.clear(); QTest::mouseClick(list, Qt::LeftButton); QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, 20000); QCOMPARE(finished.last()[0].toInt(), -1); QCOMPARE(history->count(), 2);
        query->setText(QString(4097, 'x')); finished.clear(); QTest::mouseClick(list, Qt::LeftButton); QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, 20000); QCOMPARE(finished.last()[0].toInt(), -1); QCOMPARE(history->count(), 2);
        HelpWindow reopened; auto restored = reopened.findChild<QComboBox *>("helpSearchHistory"); QCOMPARE(restored->count(), 2); QCOMPARE(restored->itemText(0), QString("archive AND file"));
        UiLanguage::set("ja"); UiLanguage::apply(&reopened); QCOMPARE(restored->itemText(0), QString("archive AND file")); QCOMPARE(reopened.findChild<QMenu *>("helpSearchOperatorMenu")->actions()[0]->text(), QString("AND")); UiLanguage::set("en");
    }
    void helpHistoryBounds() {
        QStringList saved{"", QString(4097, 'x'), "zip", "zip"}; for (int n = 0; n < 30; ++n) saved << "owned-query-" + QString::number(n);
        QSettings().setValue("Help/SearchHistory", saved); HelpWindow window;
        auto history = window.findChild<QComboBox *>("helpSearchHistory"); QCOMPARE(history->count(), 20); QCOMPARE(history->itemText(0), QString("zip"));
        auto query = window.findChild<QLineEdit *>("helpSearchQuery"); query->setText("freshmissingword"); QSignalSpy finished(&window, &HelpWindow::searchFinished);
        QTest::mouseClick(window.findChild<QPushButton *>("helpListTopics"), Qt::LeftButton); QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, 20000); QCOMPARE(finished.last()[0].toInt(), 0);
        QCOMPARE(history->count(), 20); QCOMPARE(history->itemText(0), QString("freshmissingword")); QCOMPARE(QSettings().value("Help/SearchHistory").toStringList().size(), 20);
    }
};
int main(int argc, char **argv) {
    QLocale::setDefault(QLocale(QLocale::English)); QApplication::setDesktopSettingsAware(false); const QByteArray plugins = qgetenv("PORT_TEST_PLUGIN_ROOT"); if (!plugins.isEmpty()) QCoreApplication::setLibraryPaths({QString::fromUtf8(plugins)}); QApplication app(argc, argv); QCoreApplication::setOrganizationName("7zip-mac-port-tests"); QCoreApplication::setApplicationName("compression-help"); if (argc < 2) return 2; executable = QString::fromLocal8Bit(argv[1]); --argc; for (int n = 1; n < argc; ++n) argv[n] = argv[n + 1]; CompressionHelpTests tests; return QTest::qExec(&tests, argc, argv); }
#include "compression-help.moc"
