// SPDX-License-Identifier: LGPL-3.0-or-later
#include "MainWindow.h"
#include "AddressCombo.h"
#include "ComboDialog.h"
#include "FileListItem.h"
#include "PanelSort.h"
#include "PortStyle.h"
#include "progress-dialog-driver.h"
#include <QApplication>
#include <QFile>
#include <QFileInfo>
#include <QHeaderView>
#include <QDir>
#include <QScopeGuard>
#include <QSettings>
#include <QSignalSpy>
#include <QTest>
#include <sys/stat.h>
#include <unistd.h>

static QString executable;
static bool write(QString path, QByteArray bytes = "owned 日本語 payload") { QDir().mkpath(QFileInfo(path).absolutePath()); QFile file(path); return file.open(QIODevice::WriteOnly) && file.write(bytes)==bytes.size(); }
static QByteArray read(QString path) { QFile file(path); return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray(); }
template<class T> static T *owned(MainWindow &window, const QString &name) {
    for (auto widget : window.findChildren<T *>(name)) { auto parent=widget->parentWidget(); while(parent&&!qobject_cast<MainWindow *>(parent)) parent=parent->parentWidget(); if(parent==&window) return widget; } return nullptr;
}
static FileList *list(MainWindow &window) { return owned<FileList>(window,"fileList"); }
static AddressCombo *combo(MainWindow &window) { return owned<AddressCombo>(window,"pathCombo"); }
static QTreeWidgetItem *find(MainWindow &window, QString name) { auto files=list(window); for(int i=0;i<files->topLevelItemCount();++i) if(panelItemName(files->topLevelItem(i))==name) return files->topLevelItem(i); return nullptr; }
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
static void activate(MainWindow &window) { QApplication::setActiveWindow(window.window()); }
QT_WARNING_POP
static void focus(MainWindow &window, QTreeWidgetItem *row) { activate(window); list(window)->setCurrentItem(row); (owned<IconFileList>(window,"iconFileList")->isVisible()?static_cast<QWidget *>(owned<IconFileList>(window,"iconFileList")):list(window))->setFocus(); }

class AddressWorkflowTests : public QObject {
    Q_OBJECT
    QTemporaryDir temporary;
    ProgressDialogDriver results;
    void settings(QString path, bool two=false, int mode=3) { QSettings().clear(); QSettings().setValue("View/LastPath",path); QSettings().setValue("View/Panel2Path",path); QSettings().setValue("View/AutoRefresh",false); QSettings().setValue("View/TwoPanels",two); QSettings().setValue("View/Mode",mode); QSettings().setValue("View/Panel2Mode",mode); QSettings().setValue("Options/AlternativeSelection",true); QSettings().setValue("Options/ShowDots",true); QSettings().setValue("Options/Language","en"); }
    ArchiveResult execute(ArchiveRequest request) { SevenZipProcessBackend backend(executable); QSignalSpy done(&backend,&ArchiveBackend::finished); backend.start(request); if(done.isEmpty()&&!done.wait(30000)) return {}; return qvariant_cast<ArchiveResult>(done.last()[0]); }
private slots:
    void initTestCase() { QVERIFY(temporary.isValid()); QSettings::setDefaultFormat(QSettings::IniFormat); QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,temporary.filePath("preferences")); }
    void init() { results.clear(); }
    void creationPaths_data() { QTest::addColumn<bool>("directory"); QTest::addColumn<QString>("kind"); for(bool dir:{false,true}) for(const auto &kind:{"absolute","parent","dot","literal"}) QTest::newRow(qPrintable(QString("%1-%2").arg(dir?"folder":"file",kind)))<<dir<<QString(kind); }
    void creationPaths() {
        QFETCH(bool,directory); QFETCH(QString,kind); const auto root=temporary.filePath(QString("create-%1-%2").arg(directory).arg(kind)), base=root+"/browsed"; QVERIFY(QDir().mkpath(base)); QVERIFY(QDir().mkpath(root+"/outside"));
        QString name, output; if(kind=="absolute") { name=root+"/outside/日本語 space"; output=name; } else if(kind=="parent") { name="../outside/日本語 parent"; output=root+"/outside/日本語 parent"; } else if(kind=="dot") { name="../browsed/./日本語 dot"; output=base+"/日本語 dot"; } else { name="literal\\backslash. "; output=base+'/'+name; }
        if(directory) { name+="/nested/empty"; output+="/nested/empty"; }
        settings(base); MainWindow window(executable); window.show(); QTRY_VERIFY(!window.operationBusy());
        bool seen=false; QTimer::singleShot(10,&window,[&]{auto dialog=window.findChild<ComboDialog *>(); QVERIFY(dialog); seen=true; dialog->setTextValue(name); dialog->accept();}); window.findChild<QAction *>(directory?"folderAction":"newfileAction")->trigger(); QVERIFY(seen); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(),10000);
        QVERIFY2(QFileInfo::exists(output),qPrintable(output)); QCOMPARE(QFileInfo(output).isDir(),directory); if(!directory) QCOMPARE(read(output),QByteArray()); QCOMPARE(window.currentDirectory(),base); QVERIFY(results.notices.isEmpty()); window.close();
    }
    void creationFailures_data() { QTest::addColumn<QString>("scenario"); for(const auto &s:{"existing-file","existing-directory","missing-parent","symlink-parent","readonly-parent","trailing-file-slash","embedded-null","existing-link"}) QTest::newRow(s)<<QString(s); }
    void creationFailures() {
        QFETCH(QString,scenario); const auto root=temporary.filePath("failure-"+scenario); QVERIFY(write(root+"/keep.txt","must survive")); QString name="new.txt"; bool directory=false; auto restore=qScopeGuard([&]{::chmod(QFile::encodeName(root).constData(),0755);});
        if(scenario=="existing-file") name="keep.txt";
        else if(scenario=="existing-directory") {name="present"; directory=true; QVERIFY(QDir().mkpath(root+'/'+name));}
        else if(scenario=="missing-parent") name="absent/new.txt";
        else if(scenario=="symlink-parent") {QVERIFY(QDir().mkpath(root+"/real")); QVERIFY(::symlink("real",QFile::encodeName(root+"/alias").constData())==0); name="alias/new.txt";}
        else if(scenario=="readonly-parent") QVERIFY(::chmod(QFile::encodeName(root).constData(),0555)==0);
        else if(scenario=="trailing-file-slash") name="new.txt/";
        else if(scenario=="embedded-null") name=QString("bad")+QChar::Null+QString("name");
        else {name="link"; QVERIFY(::symlink("keep.txt",QFile::encodeName(root+'/'+name).constData())==0);}
        FileOperations operation; QSignalSpy done(&operation,&FileOperations::finished); operation.create(root,name,directory); QTRY_COMPARE(done.size(),1); QVERIFY(!done.first()[0].toString().isEmpty()); QCOMPARE(read(root+"/keep.txt"),QByteArray("must survive")); QVERIFY(!QFileInfo::exists(root+"/new.txt")); QVERIFY(!QFileInfo::exists(root+"/real/new.txt")); QVERIFY(!QFileInfo::exists(root+"/absent"));
    }
    void addressEnterEscapeAndTab() {
        const auto root=temporary.filePath("address-edit"); QVERIFY(write(root+"/sub/payload.txt")); settings(root); MainWindow window(executable); window.show(); QTRY_VERIFY(!window.operationBusy()); activate(window); auto path=combo(window)->lineEdit(); path->setFocus(); path->setText("sub"); QTest::keyClick(path,Qt::Key_Return); QTRY_VERIFY(!window.operationBusy()&&window.currentDirectory()==root+"/sub"); QCOMPARE(window.focusWidget(),static_cast<QWidget *>(list(window))); QVERIFY(find(window,"payload.txt"));
        path->setFocus(); path->setText("cancel this edit"); QTest::keyClick(path,Qt::Key_Escape); QCOMPARE(path->text(),root+"/sub"); QCOMPARE(window.focusWidget(),static_cast<QWidget *>(list(window)));
        path->setFocus(); path->setText("not submitted"); QTest::keyClick(path,Qt::Key_Tab); QCOMPARE(window.currentDirectory(),root+"/sub"); QCOMPARE(window.focusWidget(),static_cast<QWidget *>(list(window))); QTest::keyClick(list(window),Qt::Key_Backspace); QTRY_VERIFY(!window.operationBusy()&&window.currentDirectory()==root); window.close();
    }
    void dropdownAncestorSelection() {
        const auto root=temporary.filePath("dropdown 日本語"); QVERIFY(write(root+"/sub/space folder/payload.txt")); settings(root+"/sub/space folder"); MainWindow window(executable); window.show(); QTRY_VERIFY(!window.operationBusy()); activate(window); auto paths=combo(window); paths->showPopup(); QVERIFY(paths->view()->isVisible()); QCOMPARE(paths->itemData(0).toString(),QString("/")); QCOMPARE(paths->itemData(0,Qt::UserRole+1).toUInt(),0U);
        const int parent=paths->findData(root+"/"); QVERIFY(parent>=0); QCOMPARE(paths->itemText(parent),QString("dropdown 日本語")); QCOMPARE(paths->itemText(parent+1),QString("sub")); QCOMPARE(paths->itemText(parent+2),QString("space folder")); QVERIFY(paths->findText("Documents")>parent+2); QVERIFY(paths->findText("Computer")>paths->findText("Documents"));
        paths->view()->setCurrentIndex(paths->model()->index(parent,0)); QTest::keyClick(paths->view(),Qt::Key_Return); QTRY_VERIFY(!window.operationBusy()&&window.currentDirectory()==root); QCOMPARE(paths->lineEdit()->text(),root); QCOMPARE(window.focusWidget(),static_cast<QWidget *>(list(window))); QVERIFY(find(window,"sub")); window.close();
    }
    void nestedDropdownSelection() {
        const auto root=temporary.filePath("nested-dropdown"); QVERIFY(write(root+"/input/folder/payload.txt")); ArchiveRequest request; request.operation=ArchiveOperation::Add; request.archive=root+"/inner.zip"; request.workingDirectory=root+"/input"; request.files={"folder"}; request.format="zip"; request.useArchiveDefaults=true; QVERIFY(execute(request).success);
        request.archive=root+"/outer.7z"; request.workingDirectory=root; request.files={"inner.zip"}; request.format="7z"; request.level=0; QVERIFY(execute(request).success);
        settings(root); MainWindow window(executable); window.show(); window.openPath(request.archive+"/inner.zip/folder"); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy()&&find(window,"payload.txt"),20000); activate(window); auto paths=combo(window); paths->showPopup(); QVERIFY(paths->findData(request.archive+"/inner.zip/folder/")>=0); QVERIFY(paths->findData(request.archive+"/inner.zip/")>=0); const int outer=paths->findData(request.archive+"/"); QVERIFY(outer>=0);
        paths->view()->setCurrentIndex(paths->model()->index(outer,0)); QTest::keyClick(paths->view(),Qt::Key_Return); QTRY_VERIFY(!window.operationBusy()&&window.currentArchivePath()==request.archive); QVERIFY(find(window,"inner.zip")); QCOMPARE(paths->lineEdit()->text(),request.archive+"/"); window.close();
    }
    void panelAndAddressKeys_data() { QTest::addColumn<int>("mode"); for(int mode:{0,3}) QTest::newRow(qPrintable(QString::number(mode)))<<mode; }
    void panelAndAddressKeys() {
        QFETCH(int,mode); const auto root=temporary.filePath("panel-tab-"+QString::number(mode)); QVERIFY(write(root+"/payload.txt")); settings(root,true,mode); MainWindow window(executable); window.show(); QTRY_VERIFY(!window.operationBusy()); auto other=window.findChild<MainWindow *>("secondPanel"); QVERIFY(other); focus(window,find(window,"payload.txt")); auto left=mode==3?static_cast<QWidget *>(list(window)):owned<IconFileList>(window,"iconFileList"); auto right=mode==3?static_cast<QWidget *>(list(*other)):owned<IconFileList>(*other,"iconFileList");
        QTest::keyClick(left,Qt::Key_Tab); QCOMPARE(window.focusWidget(),right); QTest::keyClick(right,Qt::Key_Backtab,Qt::ShiftModifier); QCOMPARE(window.focusWidget(),left);
        QTest::keyClick(left,Qt::Key_F2,Qt::AltModifier); QVERIFY(combo(*other)->view()->isVisible()); QTest::keyClick(combo(*other)->lineEdit(),Qt::Key_F1,Qt::AltModifier); QVERIFY(!combo(*other)->view()->isVisible()); QVERIFY(combo(window)->view()->isVisible()); QTest::keyClick(combo(window)->lineEdit(),Qt::Key_Escape); QVERIFY(!combo(window)->view()->isVisible()); QCOMPARE(window.focusWidget(),left);
        combo(*other)->lineEdit()->setFocus(); QTest::keyClick(combo(*other)->lineEdit(),Qt::Key_Tab); QCOMPARE(window.focusWidget(),right); QTest::keyClick(right,Qt::Key_W,Qt::ControlModifier); QTRY_VERIFY(!window.isVisible());
    }
    void originalSortAndSelectionDispatch() {
        const auto root=temporary.filePath("full-dispatch"); QVERIFY(write(root+"/a.txt")); QVERIFY(write(root+"/b.TXT")); QVERIFY(write(root+"/c.bin")); settings(root); MainWindow window(executable); window.show(); QTRY_VERIFY(!window.operationBusy()); auto files=list(window); focus(window,find(window,"a.txt"));
        const auto initial=files->header()->sortIndicatorOrder(); QTest::keyClick(files,Qt::Key_F3,Qt::ControlModifier|Qt::ShiftModifier); QTRY_VERIFY(!window.operationBusy()); QCOMPARE(files->property("sortProperty").toUInt(),unsigned(OfficialSort::kpidName)); QVERIFY(files->header()->sortIndicatorOrder()!=initial); QTest::keyClick(files,Qt::Key_F5,Qt::ControlModifier|Qt::AltModifier); QTRY_VERIFY(!window.operationBusy()); QCOMPARE(files->property("sortProperty").toUInt(),unsigned(OfficialSort::kpidMTime));
        QTest::keyClick(files,Qt::Key_A,Qt::ControlModifier); QCOMPARE(files->markedItems().size(),3); QTest::keyClick(files,Qt::Key_Minus,Qt::ShiftModifier); QCOMPARE(files->markedItems().size(),0); focus(window,find(window,"a.txt")); QTest::keyClick(files,Qt::Key_Plus,Qt::AltModifier|Qt::ShiftModifier); QCOMPARE(files->markedItems().size(),2); QTest::keyClick(files,Qt::Key_Asterisk,Qt::ControlModifier); QCOMPARE(files->markedItems().size(),1); QCOMPARE(panelItemName(files->markedItems().first()),QString("c.bin"));
        bool shown=false; QTimer::singleShot(10,&window,[&]{auto input=window.findChild<ComboDialog *>(); QVERIFY(input); shown=true; input->setTextValue("*.bin"); input->accept();}); QTest::keyClick(files,Qt::Key_Minus,Qt::ControlModifier); QVERIFY(shown); QCOMPARE(files->markedItems().size(),0); const auto directory=window.currentDirectory(); QTest::keyClick(files,Qt::Key_PageDown,Qt::ControlModifier); QCOMPARE(window.currentDirectory(),directory); QVERIFY(!window.operationBusy()); window.close();
    }
    void headerToggleAndClose() {
        const auto root=temporary.filePath("header-close"); QVERIFY(write(root+"/payload.txt")); settings(root); MainWindow window(executable); window.show(); QTRY_VERIFY(!window.operationBusy()); activate(window); auto path=combo(window)->lineEdit(); path->setFocus(); QTest::keyClick(path,Qt::Key_F9); QTRY_VERIFY(!window.operationBusy()); QVERIFY(window.findChild<MainWindow *>("secondPanel")); QTest::keyClick(path,Qt::Key_W,Qt::ControlModifier); QTRY_VERIFY(!window.isVisible());
    }
    void cleanupTestCase() { QSettings().clear(); }
};
int main(int argc,char **argv) {
    if(argc<2) return 1; executable=QString::fromLocal8Bit(argv[1]); if(!qEnvironmentVariable("PORT_TEST_PLUGIN_ROOT").isEmpty()) QCoreApplication::setLibraryPaths({qEnvironmentVariable("PORT_TEST_PLUGIN_ROOT")});
    QCoreApplication::setAttribute(Qt::AA_MacDontSwapCtrlAndMeta); QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs); QApplication app(argc,argv); applyPortAppearance(app); app.setApplicationName("7zip-address-workflow-test"); app.setOrganizationName("7zip-address-workflow-test"); app.setQuitOnLastWindowClosed(false);
    AddressWorkflowTests tests; QList<char *> args{argv[0]}; for(int i=2;i<argc;++i) args<<argv[i]; args<<nullptr; return QTest::qExec(&tests,args.size()-1,args.data());
}
#include "address-workflow.moc"
