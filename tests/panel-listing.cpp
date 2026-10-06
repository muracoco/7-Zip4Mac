// SPDX-License-Identifier: LGPL-3.0-or-later
#include "MainWindow.h"
#include "FileListItem.h"
#include "PanelSelection.h"
#include "PortStyle.h"
#include "TimeText.h"
#include <QAction>
#include <QApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QSettings>
#include "input-driver.h"
#include <QClipboard>
#include <QSignalSpy>
#include <QTest>
#include <unistd.h>

using namespace OfficialSort;
static QString executable;
static bool write(QString path, QByteArray bytes) { QDir().mkpath(QFileInfo(path).absolutePath()); QFile file(path); return file.open(QIODevice::WriteOnly) && file.write(bytes)==bytes.size(); }
static FileList *tree(MainWindow &window) { return window.findChild<FileList *>("fileList"); }
class ListingTests : public QObject {
    Q_OBJECT
    QTemporaryDir temporary;
    QString root, many;
    ArchiveResult execute(SevenZipProcessBackend &backend, ArchiveRequest request) {
        QSignalSpy done(&backend,&ArchiveBackend::finished); backend.start(request);
        if(done.isEmpty() && !done.wait(40000)) qFatal("Owned listing fixture operation timed out");
        return qvariant_cast<ArchiveResult>(done.last().first());
    }
    void populate(FileList &list, int count) {
        list.setSortingEnabled(false); list.setColumnCount(2); list.setHeaderLabels({"Name","Size"});
        list.headerItem()->setData(0,ColumnPropertyRole,kpidName); list.headerItem()->setData(1,ColumnPropertyRole,kpidSize);
        for(int row=0;row<count;++row) {
            auto item=new FileItem(&list,{QString("file%1").arg(count-row)}); item->setData(0,Qt::UserRole+104,row);
            ArchiveProperty size; size.id=kpidSize; size.type=21; size.number=QString::number(9007199254740993ULL+row);
            item->setSortProperties({size},row);
        }
    }
private slots:
    void initTestCase() {
        QVERIFY(temporary.isValid()); QSettings::setDefaultFormat(QSettings::IniFormat); QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,temporary.filePath("preferences"));
        root=temporary.filePath("source 日本語"); many=temporary.filePath("many entries"); QVERIFY(write(root+"/ASCII.txt","1234567")); QVERIFY(QDir().mkpath(many));
        QVERIFY(write(root+"/日本語     tail ","unicode")); QVERIFY(write(root+"/RLO\u202e.txt","direction"));
        for(int row=0;row<15000;++row) QVERIFY(::link(QFile::encodeName(root+"/ASCII.txt").constData(),QFile::encodeName(many+'/'+QString("日本語 file%1.txt").arg(row)).constData())==0);
        SevenZipProcessBackend backend(executable);
        for(const auto &format:{QString("7z"),QString("zip")}) {
            ArchiveRequest request; request.operation=ArchiveOperation::Add; request.archive=temporary.filePath("many."+format); request.workingDirectory=many; request.files={"."}; request.format=format; request.level=0; request.method=format=="zip"?"Copy":"LZMA2";
            const auto result=execute(backend,request); QVERIFY2(result.success,qPrintable(result.message+result.details));
        }
        qInfo()<<"Owned listing fixtures:"<<temporary.path();
    }
    void init() {
        QSettings().clear(); QSettings().setValue("View/LastPath",root); QSettings().setValue("View/AutoRefresh",false);
        FileManagerSettings settings; settings.realIcons=false; settings.alternativeSelection=true; QVERIFY(settings.save());
    }
    void originalDisplayAndDeferredProperties() {
        QCOMPARE(officialPanelName(""),QString("_")); QCOMPARE(officialPanelName("name\u202e.txt"),QString("name_.txt"));
        QCOMPARE(officialPanelName("日本語     name"),QString("日本語 ... name")); QCOMPARE(officialPanelName("name "),QString("name \u009c"));
        QCOMPARE(officialPanelName("😀  name"),QString("😀  name"));
        QCOMPARE(officialPanelSize(18446744073709551615ULL),QString("18 446 744 073 709 551 615"));
        ArchiveProperty size; size.id=kpidSize; size.type=21; size.number="9007199254740993";
        ArchiveProperty comment; comment.id=kpidComment; comment.type=8; comment.value="one\r\ntwo";
        ArchiveProperty date; date.id=kpidMTime; date.type=64; date.fileTime="134354736001234567"; date.timeFraction="123456789";
        const auto columns=std::make_shared<const QList<ArchivePropertyDefinition>>(QList<ArchivePropertyDefinition>{{kpidName,8,"Name"},{kpidSize,21,"Size"},{kpidMTime,64,"Modified"},{kpidComment,8,"Comment"}});
        FileItem item; item.setText(0,"original"); item.setDeferredProperties({size,date,comment},columns,9,true);
        QCOMPARE(item.text(1),QString("9 007 199 254 740 993")); QCOMPARE(item.text(3),QString("one  two")); QVERIFY(item.text(2).endsWith(".123456789Z"));
        ArchiveProperty prefix; prefix.id=kpidPrefix; prefix.value="raw\r\nprefix"; QCOMPARE(panelPropertyText(prefix,9,true),prefix.value);
        item.setText(1,"explicit F3 result"); QCOMPARE(item.text(1),QString("explicit F3 result"));
    }
    void displayedNameKeepsRealFilesystemIdentity() {
        MainWindow window(executable); window.show(); QTRY_VERIFY(!window.operationBusy()); auto list=tree(window); QVERIFY(list);
        bool found=false; for(int row=0;row<list->topLevelItemCount();++row) {
            auto item=dynamic_cast<FileItem *>(list->topLevelItem(row)); QVERIFY(item);
            const auto path=item->data(0,Qt::UserRole).toString();
            if(path.endsWith("日本語     tail ")) { QCOMPARE(item->text(0),QString("日本語 ... tail \u009c")); QVERIFY(QFileInfo::exists(path)); QCOMPARE(item->sortInfo.name,std::wstring(L"日本語     tail ")); found=true; }
        } QVERIFY(found);
    }
    void asynchronousSortKeepsExactValuesMarksFocusAndUnsorted() {
        FileList list; populate(list,40000); auto marked=list.topLevelItem(200),focused=list.topLevelItem(31000);
        marked->setData(0,PanelMarkRole,true); focused->setSelected(true); list.setCurrentItem(focused,0,QItemSelectionModel::NoUpdate);
        QSignalSpy state(&list,&FileList::sortingStateChanged); int ticks=0; QTimer heartbeat; heartbeat.setInterval(1); connect(&heartbeat,&QTimer::timeout,&list,[&]{++ticks;}); heartbeat.start();
        list.sortItems(0,Qt::AscendingOrder); QVERIFY(list.sortingBusy()); QTRY_VERIFY_WITH_TIMEOUT(!list.sortingBusy(),15000); QVERIFY(ticks>0);
        QCOMPARE(list.topLevelItem(0)->text(0),QString("file1")); QCOMPARE(list.topLevelItem(39999)->text(0),QString("file40000")); QCOMPARE(list.currentItem(),focused); QVERIFY(focused->isSelected()); QVERIFY(marked->data(0,PanelMarkRole).toBool()); QCOMPARE(state.size(),2);
        list.sortItems(1,Qt::DescendingOrder); QTRY_VERIFY(!list.sortingBusy()); QCOMPARE(list.topLevelItem(0)->text(0),QString("file1"));
        list.setProperty("virtualSortProperty",kpidNoProperty); list.sortItems(0,Qt::AscendingOrder); QTRY_VERIFY(!list.sortingBusy()); QCOMPARE(list.topLevelItem(0)->text(0),QString("file40000"));
        list.sortItems(0,Qt::DescendingOrder); QTRY_VERIFY(!list.sortingBusy()); QCOMPARE(list.topLevelItem(0)->text(0),QString("file1"));
    }
    void supersededSortClearAndDestruction() {
        FileList list; populate(list,40000); list.sortItems(0,Qt::AscendingOrder); QVERIFY(list.sortingBusy()); list.sortItems(0,Qt::DescendingOrder);
        QTRY_VERIFY(!list.sortingBusy()); QCOMPARE(list.topLevelItem(0)->text(0),QString("file40000"));
        list.sortItems(0,Qt::AscendingOrder); list.clear(); new FileItem(&list,{"new view"}); QTest::qWait(100); QCOMPARE(list.topLevelItemCount(),1); QCOMPARE(list.topLevelItem(0)->text(0),QString("new view")); QVERIFY(!list.sortingBusy());
        auto abandoned=new FileList; populate(*abandoned,15000); abandoned->sortItems(0,Qt::AscendingOrder); QElapsedTimer clock; clock.start(); delete abandoned; QVERIFY(clock.elapsed()<500); QTest::qWait(50);
    }
    void nativeIconsBothKindsAndNavigation() {
        FileList list; populate(list,70); QList<PanelIconRequest> requests;
        for(int row=0;row<70;++row) requests.append({row,row%2?QString("archive.txt"):root+"/ASCII.txt",false,bool(row%2)});
        list.loadNativeIcons(requests); QVERIFY(list.nativeIconsBusy()); list.sortItems(0,Qt::AscendingOrder);
        QTRY_VERIFY_WITH_TIMEOUT(!list.nativeIconsBusy(),15000); for(int row=0;row<70;++row) QVERIFY(!list.topLevelItem(row)->icon(0).isNull());
        list.loadNativeIcons(requests); list.clear(); new FileItem(&list,{"new view"}); QTest::qWait(100); QVERIFY(list.topLevelItem(0)->icon(0).isNull()); QVERIFY(!list.nativeIconsBusy());
        auto abandoned=new FileList; populate(*abandoned,70); abandoned->loadNativeIcons(requests); delete abandoned; QTest::qWait(50);
        auto settings=FileManagerSettings::load(); settings.realIcons=true; QVERIFY(settings.save()); MainWindow window(executable); window.show(); QTRY_VERIFY(!window.operationBusy()); auto real=tree(window);
        QTRY_VERIFY(!real->nativeIconsBusy()); QVERIFY(!real->topLevelItem(0)->icon(0).isNull());
    }

    void archiveRawNameRename_data() { QTest::addColumn<QString>("format"); QTest::newRow("7z")<<"7z"; QTest::newRow("zip")<<"zip"; }
    void archiveRawNameRename() {
        QFETCH(QString,format); const auto source=temporary.filePath("odd-name-"+format), name=QString("日本語     tail ");
        QVERIFY(write(source+"/parent/"+name,"owned content")); SevenZipProcessBackend backend(executable);
        ArchiveRequest request; request.operation=ArchiveOperation::Add; request.archive=temporary.filePath("odd-name."+format); request.format=format; request.method=format=="zip"?"Deflate":"LZMA2"; request.workingDirectory=source; request.files={"parent"};
        const auto created=execute(backend,request); QVERIFY2(created.success,qPrintable(created.message+created.details));
        MainWindow window(executable); window.show(); window.openPath(request.archive+"/parent/"); QTRY_VERIFY(!window.operationBusy()); auto list=tree(window); QCOMPARE(list->topLevelItemCount(),1);
        auto item=list->topLevelItem(0); QCOMPARE(panelItemName(item),name); QCOMPARE(item->text(0),officialPanelName(name));
        list->setCurrentItem(item,0,QItemSelectionModel::NoUpdate); list->setItemSelected(item,true);
        QKeyEvent copy(QEvent::KeyPress,Qt::Key_C,Qt::ControlModifier); QApplication::sendEvent(list,&copy); QCOMPARE(QApplication::clipboard()->text(),name);
        bool prompted=false;
        QTimer::singleShot(10,&window,[&]{auto input=testInput(&window); QVERIFY(input); QCOMPARE(input->textValue(),name); input->setTextValue("renamed 日本語.txt"); prompted=true; input->accept();});
        window.findChild<QAction *>("renameAction")->trigger(); QTRY_VERIFY(prompted); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(),10000);
        QCOMPARE(list->topLevelItemCount(),1); QCOMPARE(list->topLevelItem(0)->data(0,Qt::UserRole).toString(),QString("parent/renamed 日本語.txt"));
        ArchiveRequest verify; verify.operation=ArchiveOperation::Test; verify.archive=request.archive; QVERIFY(execute(backend,verify).success);
    }
    void actualLargeArchive_data() { QTest::addColumn<QString>("format"); QTest::newRow("7z")<<"7z"; QTest::newRow("zip")<<"zip"; }
    void actualLargeArchive() {
        QFETCH(QString,format); MainWindow window(executable); window.show(); QTRY_VERIFY(!window.operationBusy()); auto list=tree(window);
        QElapsedTimer clock; clock.start(); qint64 last=0,maximumGap=0; int ticks=0; QTimer heartbeat; heartbeat.setInterval(5);
        connect(&heartbeat,&QTimer::timeout,&window,[&]{auto now=clock.elapsed();maximumGap=qMax(maximumGap,now-last);last=now;++ticks;}); heartbeat.start();
        const auto archive=temporary.filePath("many."+format); window.openPath(archive); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(),40000);
        maximumGap=qMax(maximumGap,clock.elapsed()-last); QCOMPARE(window.currentArchivePath(),archive); QCOMPARE(list->topLevelItemCount(),15000); QVERIFY(ticks>5);
        QCOMPARE(list->topLevelItem(0)->text(0),QString("日本語 file0.txt")); QCOMPARE(list->topLevelItem(14999)->text(0),QString("日本語 file14999.txt"));
        QCOMPARE(list->topLevelItem(0)->text(propertyColumn(list,kpidSize)),QString("7"));
        auto marked=list->topLevelItem(100); list->setItemSelected(marked,true); list->setCurrentItem(marked,0,QItemSelectionModel::NoUpdate);
        window.findChild<QAction *>("sort3Action")->trigger(); QTRY_VERIFY(!window.operationBusy()); QVERIFY(marked->data(0,PanelMarkRole).toBool()); QCOMPARE(list->currentItem(),marked);
        qInfo()<<format<<"15,000 archive rows elapsed ms"<<clock.elapsed()<<"maximum event-loop gap ms"<<maximumGap;
        window.openPath(root); QTRY_VERIFY(!window.operationBusy()); QCOMPARE(window.currentDirectory(),root); QCOMPARE(list->topLevelItemCount(),3);
    }
};
int main(int argc,char **argv) {
    if(argc<2) return 1; executable=QString::fromLocal8Bit(argv[1]);
    if(!qEnvironmentVariable("PORT_TEST_PLUGIN_ROOT").isEmpty()) QCoreApplication::setLibraryPaths({qEnvironmentVariable("PORT_TEST_PLUGIN_ROOT")});
    QCoreApplication::setAttribute(Qt::AA_MacDontSwapCtrlAndMeta); QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    QApplication app(argc,argv); applyPortAppearance(app); app.setOrganizationName("SevenZipMacPortTests"); app.setApplicationName("PanelListing"); app.setQuitOnLastWindowClosed(false);
    ListingTests tests; QList<char *> args{argv[0]}; for(int n=2;n<argc;++n) args<<argv[n]; args<<nullptr; return QTest::qExec(&tests,args.size()-1,args.data());
}
#include "panel-listing.moc"
