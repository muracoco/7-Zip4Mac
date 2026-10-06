#include "progress-dialog-driver.h"
// SPDX-License-Identifier: LGPL-3.0-or-later
#include <QLocale>
#include "LanguageDocument.h"
#include "UiLanguage.h"
#include "Dialogs.h"
#include "OptionsDialog.h"
#include "MainWindow.h"
#include "PortStyle.h"
#include <QToolButton>
#include <QApplication>
#include <QBuffer>
#include <QFile>
#include <QDir>
#include <QTemporaryDir>
#include <QTest>
#include <QTabWidget>
#include <QGroupBox>
#include <QAction>
#include <QDialogButtonBox>
#include <QMenuBar>
#include <QProcess>
static QString executable;
static LanguageDocument load(QString path) { QFile file(path); if (!file.open(QIODevice::ReadOnly)) return {}; return readOfficialLanguage(file); }
static QByteArray fixture() { return ";!@Lang2@!UTF-8!\n0\n7-Zip\nEnglish\nEnglish\n"; }
class LanguageSettingsTests : public QObject {
    Q_OBJECT
    QTemporaryDir temporary;
    ProgressDialogDriver resultDialogs;
private slots:
    void initTestCase() {
        QVERIFY(temporary.isValid()); QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, temporary.filePath("preferences"));
    }
    void init() { resultDialogs.clear(); QSettings().clear(); QSettings().setValue("View/LastPath", temporary.path()); UiLanguage::set("en"); }
    void cleanup() { UiLanguage::set("en"); }
    void officialFilesAndInformation() {
        const auto choices = UiLanguage::available(); QCOMPARE(choices.size(), 93); QCOMPARE(choices.first().first, QString("en"));
        const auto english = load(":/languages/en.ttt"); QVERIFY(english.valid); QVERIFY(english.entries.size() > 400); QVERIFY(UiLanguage::invalidFiles().isEmpty());
        for (int n = 1; n < choices.size(); ++n) {
            const auto code = choices[n].first; const auto doc = load(UiLanguage::directory() + '/' + code + ".txt");
            QVERIFY2(doc.valid, qPrintable(code)); QCOMPARE(doc.entries.value(0), QString("7-Zip")); QVERIFY(!doc.comments.isEmpty());
            const auto info = UiLanguage::information(code); QVERIFY2(info.startsWith(code + " : "), qPrintable(info)); QVERIFY(info.contains('%'));
            if (n > 1) QVERIFY(QString::compare(choices[n - 1].second, choices[n].second, Qt::CaseInsensitive) <= 0);
        }
        QVERIFY(UiLanguage::information("en").startsWith("- : ")); QVERIFY(UiLanguage::information("not-a-language").isEmpty());
    }
    void parserBoundaryAndEscapes() {
        auto bytes = fixture() + QByteArray("400\nOK\\nnext\\tcolumn\\\\slash\\q\n");
        const auto doc = parseOfficialLanguage(bytes); QVERIFY(doc.valid); QCOMPARE(doc.entries.value(400), QString("OK\nnext\tcolumn\\slash\\q"));
        QVERIFY(parseOfficialLanguage(QByteArray::fromHex("efbbbf") + bytes).valid);
        bytes.replace("\n", "\r\n"); QCOMPARE(parseOfficialLanguage(bytes).entries, doc.entries);
        QVERIFY(!parseOfficialLanguage("invalid").valid); QVERIFY(!parseOfficialLanguage(fixture() + "400\ntrailing\\").valid);
        QVERIFY(!parseOfficialLanguage(fixture() + QByteArray("400\n") + QByteArray::fromHex("ff")).valid);
        QVERIFY(!parseOfficialLanguage(fixture() + "400\nOK\n399\nwrong order\n").valid);
        QVERIFY(!parseOfficialLanguage(fixture() + "1073741825\nToo high\n").valid);
        const auto spaced = parseOfficialLanguage(fixture() + "400 \nvalue\n"); QVERIFY(spaced.valid); QCOMPARE(spaced.entries.value(3), QString("400 "));
        const auto blank = parseOfficialLanguage(fixture() + "400\n\n;comment\nvalue\n"); QVERIFY(blank.valid); QCOMPARE(blank.entries.value(402), QString("value"));
        const auto terminated = parseOfficialLanguage(fixture() + QByteArray(1, '\0') + QByteArray::fromHex("ff")); QVERIFY(terminated.valid);
        QVERIFY(!parseOfficialLanguage(fixture() + QByteArray((1 << 20), 'x')).valid);
        QCOMPARE(parseOfficialLanguage(fixture() + "400\n日本語😀\n").entries.value(400), QString("日本語😀"));
    }
    void missingExtraAndTranslatorInformation() {
        const auto english = parseOfficialLanguage(fixture() + "400\nOriginal missing\n401\nOK\n");
        const auto localized = parseOfficialLanguage(fixture() + "; Translator: Test author\n401\nD'accord\n402\nExtra value\n");
        QVERIFY(english.valid && localized.valid);
        const auto info = officialLanguageInformation("test", localized, english);
        QVERIFY(info.contains("Translator: Test author")); QVERIFY(info.contains("Missing lines: 1")); QVERIFY(info.contains("400 : Original missing"));
        QVERIFY(info.contains("Extra lines: 1")); QVERIFY(info.contains("402 : Extra value"));
    }
    void allLanguageControlResources() {
        AddDialog add(temporary.filePath("Cancel.7z")); ExtractDialog extract(temporary.filePath("Cancel"), true);
        OptionsDialog options; ProgressDialog progress("Extracting", "Cancel");
        ArchiveProgress snapshot; snapshot.mode = "extract"; snapshot.current = "Cancel"; progress.updateDetails(snapshot);
        for (const auto &choice : UiLanguage::available()) {
            UiLanguage::set(choice.first);
            for (auto dialog : QList<QWidget *>{&add, &extract, &options, &progress}) {
                UiLanguage::apply(dialog);
                const auto titleId = dialog->property("uiTitleId").toUInt(); if (titleId) QCOMPARE(dialog->windowTitle(), UiLanguage::resource(titleId));
                for (auto control : dialog->findChildren<QWidget *>()) {
                    if (control->property("uiLiteral").toBool()) continue;
                    const auto id = control->property("uiResourceId").toUInt(); if (!id) continue;
                    auto expected = UiLanguage::resource(id); if (control->property("uiAppendColon").toBool() && !expected.endsWith(':')) expected += ':';
                    if (auto label = qobject_cast<QLabel *>(control)) QCOMPARE(label->text(), expected);
                    else if (auto button = qobject_cast<QAbstractButton *>(control)) QCOMPARE(button->text(), expected);
                    else if (auto group = qobject_cast<QGroupBox *>(control)) QCOMPARE(group->title(), expected);
                }
            }
            QCOMPARE(add.findChild<QLineEdit *>("archivePath")->text(), temporary.filePath("Cancel.7z"));
            QCOMPARE(progress.findChild<QLabel *>("progressFileName")->text(), QString("\nCancel"));
            const auto tabs = options.findChild<QTabWidget *>("optionsTabs");
            QCOMPARE(tabs->count(), 6);
            QCOMPARE(tabs->tabText(1), QString("7-Zip"));
            for (int n = 0; n < tabs->count(); ++n) QVERIFY2(!tabs->tabText(n).isEmpty(), qPrintable(choice.first));
        }
    }
    void optionsApplyCancelAndInfo() {
        OptionsDialog options; auto language = options.findChild<QComboBox *>("uiLanguage"); auto info = options.findChild<QPlainTextEdit *>("languageInfo");
        QVERIFY(language && info); language->setCurrentIndex(language->findData("ja")); QCOMPARE(info->toPlainText(), UiLanguage::information("ja")); QCOMPARE(FileManagerSettings::load().language, QString("en"));
        options.findChild<QCheckBox *>("showDots")->setChecked(true);
        options.findChild<QDialogButtonBox *>("optionsButtons")->button(QDialogButtonBox::Apply)->click();
        QCOMPARE(FileManagerSettings::load().language, QString("ja")); QVERIFY(FileManagerSettings::load().showDots); QCOMPARE(options.windowTitle(), UiLanguage::resource(2100));
        auto tabs = options.findChild<QTabWidget *>(); QCOMPARE(tabs->tabText(4), UiLanguage::resource(2500));
        language->setCurrentIndex(language->findData("fr")); options.findChild<QCheckBox *>("showDots")->setChecked(false); options.reject();
        QCOMPARE(FileManagerSettings::load().language, QString("ja")); QVERIFY(FileManagerSettings::load().showDots);
    }
    void progressStatesAndManualValues() {
        AddDialog add(temporary.filePath("manual.7z")); auto dictionary = add.findChild<QComboBox *>("compressionDictionary"); dictionary->setEditText("13 MB");
        ProgressDialog progress("Testing", "Cancel"); progress.setPauseAvailable(true); progress.setPaused(true); UiLanguage::set("ja"); UiLanguage::apply(&progress); UiLanguage::apply(&add);
        QCOMPARE(dictionary->currentText(), QString("13 MB")); QCOMPARE(add.options().dictionary, QString("13m"));
        QCOMPARE(progress.findChild<QPushButton *>("pauseOperation")->text(), UiLanguage::resource(411));
        progress.setPaused(false); QCOMPARE(progress.findChild<QPushButton *>("pauseOperation")->text(), UiLanguage::resource(446));
        ArchiveResult result; result.success = false; result.exitCode = 2; result.message = "Cancel"; progress.finish(result);
        UiLanguage::set("fr"); UiLanguage::apply(&progress); QCOMPARE(progress.findChild<QPushButton *>("cancelOperation")->text(), UiLanguage::resource(408));
        QCOMPARE(resultDialogs.notices.size(), 1); QVERIFY(resultDialogs.notices.first().error); QVERIFY(resultDialogs.notices.first().text.startsWith("Cancel\n")); QVERIFY(resultDialogs.notices.first().text.contains("7-Zip exit code: 2")); QVERIFY(!progress.isVisible());
    }
    void menuToolbarAndPropertyIdentity() {
        MainWindow window(executable); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000);
        for (const auto code : {"ja", "fr", "ar", "en"}) {
            UiLanguage::set(code); UiLanguage::apply(&window);
            QCOMPARE(window.findChild<QAction *>("optionsAction")->text(), UiLanguage::resource(900));
            QCOMPARE(window.findChild<QAction *>("sort1Action")->text(), UiLanguage::resource(1020));
            QCOMPARE(window.menuBar()->actions().first()->text(), UiLanguage::resource(500));
            QCOMPARE(UiLanguage::propertyName(7, "Size"), UiLanguage::resource(1007));
            auto copy = window.findChild<QToolButton *>("copyButton"); QVERIFY(copy); auto text = UiLanguage::resource(7203); text.remove('&'); QCOMPARE(copy->text(), text);
        }
    }
    void editableBundleLanguageDirectory() {
        const auto bundle = temporary.filePath("owned-language-bundle/Contents"); QVERIFY(QDir().mkpath(bundle + "/MacOS")); QVERIFY(QDir().mkpath(bundle + "/Resources/Lang"));
        const auto binary = bundle + "/MacOS/language-witness"; QVERIFY(QFile::copy(QCoreApplication::applicationFilePath(), binary));
        QVERIFY(QFile::setPermissions(binary, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner));
        QFile localized(bundle + "/Resources/Lang/ja.txt"); QVERIFY(localized.open(QIODevice::WriteOnly));
        localized.write(fixture() + "2100\nEdited bundle translation\n"); localized.close();
        QFile invalid(bundle + "/Resources/Lang/invalid.txt"); QVERIFY(invalid.open(QIODevice::WriteOnly)); invalid.write("Invalid Lang2 file"); invalid.close();
        QProcess process; process.start(binary, {executable, "--language-directory-witness"}); QVERIFY(process.waitForFinished(15000));
        QCOMPARE(process.exitStatus(), QProcess::NormalExit); QVERIFY2(process.exitCode() == 0, process.readAllStandardError().constData());
    }
};
int main(int argc, char **argv) {
    QLocale::setDefault(QLocale(QLocale::English));
    if (argc < 2) return 2; executable = argv[1]; auto plugins = qEnvironmentVariable("PORT_TEST_PLUGIN_ROOT"); if (!plugins.isEmpty()) QCoreApplication::setLibraryPaths({plugins});
    QCoreApplication::setAttribute(Qt::AA_MacDontSwapCtrlAndMeta); QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    QApplication app(argc, argv); applyPortAppearance(app); app.setOrganizationName("SevenZipMacPortTests"); app.setApplicationName("LanguageSettings"); app.setQuitOnLastWindowClosed(false);
    if (argc == 3 && QByteArray(argv[2]) == "--language-directory-witness") {
        UiLanguage::set("ja");
        return UiLanguage::directory().endsWith("/Contents/Resources/Lang") && UiLanguage::resource(2100) == "Edited bundle translation"
            && UiLanguage::available().size() == 2 && UiLanguage::invalidFiles() == QStringList{"invalid.txt"} ? 0 : 1;
    }
    LanguageSettingsTests tests; QList<char *> args{argv[0]}; for (int i = 2; i < argc; ++i) args << argv[i]; args << nullptr; return QTest::qExec(&tests, args.size() - 1, args.data());
}
#include "language-settings.moc"
