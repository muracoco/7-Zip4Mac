// SPDX-License-Identifier: LGPL-3.0-or-later
#include "ArchiveBackend.h"
#include "ArchiveProperties.h"
#include <QStandardPaths>
#include "ExtractionSession.h"
#include "ExtractionInstaller.h"
#include <QCoreApplication>
#include <QDirIterator>
#include <QFile>
#include <QProcess>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <QRegularExpression>
#include <fcntl.h>
#include <unistd.h>
#include <csignal>

static QString executable;
static bool write(const QString &path, const QByteArray &bytes) {
    QDir().mkpath(QFileInfo(path).absolutePath()); QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size();
}
static QByteArray read(const QString &path) { QFile file(path); return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray(); }
static struct stat stamp(const QString &path) { struct stat value{}; ::lstat(QFile::encodeName(path).constData(), &value); return value; }
static QByteArray linkTarget(const QString &path) { QByteArray value(65536, 0); const auto size = ::readlink(QFile::encodeName(path).constData(), value.data(), value.size()); return size < 0 ? QByteArray() : value.left(size); }
static QString console(const QStringList &arguments, const QString &working = {}) {
    QProcess process; process.setWorkingDirectory(working); process.start(executable, arguments);
    if (!process.waitForStarted(10000) || !process.waitForFinished(30000)) return "Official console timed out: " + process.errorString();
    return process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0 ? QString() : QString::fromUtf8(process.readAllStandardOutput() + process.readAllStandardError());
}
static ArchiveResult run(SevenZipProcessBackend &backend, const ArchiveRequest &request) {
    QSignalSpy done(&backend, &ArchiveBackend::finished); backend.start(request);
    if (done.isEmpty() && !done.wait(30000)) { backend.cancel(); return {}; }
    return qvariant_cast<ArchiveResult>(done.last().first());
}
static QString orderedFixture(const QString &archive, const QString &format, bool flat, bool links = false, int variant = 0) {
    QProcess process;
    process.start("python3", {"-c", R"PY(
import io, os, pathlib, stat, subprocess, sys, tarfile, tempfile, warnings, zipfile
path, fmt, flat, links = sys.argv[1], sys.argv[2], sys.argv[3]=='1', sys.argv[4]=='1'
name='が space.txt' if sys.argv[5]=='2' else '日本語 space.txt'
rows=[(f'{chr(97+i)}/{name}' if flat else f'folder/{name}', f'payload {i}'.encode(), '') for i in range(3)]
if sys.argv[5]=='1': rows=[(f'folder/{name}.txt',f'payload {i}'.encode(),'') for i,name in enumerate(['Case','case','CASE'])]
if links:
    rows=[('target.txt',b'first bytes',''),('alias.txt',b'','target.txt'),
          ('target.txt',b'second bytes',''),('later.txt',b'','target.txt'),('chain.txt',b'','alias.txt')]
if fmt=='7z':
    with tempfile.TemporaryDirectory(prefix='7zip-ordered-source-') as folder:
        for i,(_,data,_) in enumerate(rows):
            item=pathlib.Path(folder)/f'{i}.txt'; item.write_bytes(data); item.chmod(0o640); os.utime(item,(1600000000+i*2,1600000000+i*2))
        subprocess.run([sys.argv[6],'a','-t7z','-mtc=on','-mta=on','--',path,'0.txt','1.txt','2.txt'],cwd=folder,stdout=subprocess.PIPE,stderr=subprocess.PIPE,check=True)
        names=sum(([f'{i}.txt',row[0]] for i,row in enumerate(rows)),[])
        subprocess.run([sys.argv[6],'rn','--',path]+names,cwd=folder,stdout=subprocess.PIPE,stderr=subprocess.PIPE,check=True)
elif fmt=='zip':
    warnings.simplefilter('ignore', UserWarning)
    with zipfile.ZipFile(path,'w') as z:
        for i,(name,data,link) in enumerate(rows):
            item=zipfile.ZipInfo(name,(2020,9,13,12,26,2+i*2)); item.create_system=3
            item.external_attr=(stat.S_IFREG|0o640)<<16
            z.writestr(item,data)
else:
    with tarfile.open(path,'w',format=tarfile.USTAR_FORMAT if links else tarfile.PAX_FORMAT) as t:
        for i,(name,data,link) in enumerate(rows):
            item=tarfile.TarInfo(name); item.mode=0o640; item.mtime=1600000000+i*2
            if link: item.type=tarfile.LNKTYPE; item.linkname=link
            else: item.size=len(data)
            t.addfile(item, None if link else io.BytesIO(data))
)PY", archive, format, flat ? "1" : "0", links ? "1" : "0", QString::number(variant), executable});
    if (!process.waitForStarted(10000) || !process.waitForFinished(30000) || process.exitCode() != 0) return QString::fromUtf8(process.readAllStandardError()) + process.errorString();
    return {};
}
static QMap<QString, struct stat> filesIn(const QString &root) {
    QMap<QString, struct stat> result;
    QDirIterator items(root, QDir::Files | QDir::Hidden | QDir::System, QDirIterator::Subdirectories);
    while (items.hasNext()) { const auto path = items.next(); result[QDir(root).relativeFilePath(path)] = stamp(path); }
    return result;
}
static QMap<qint64, timespec> archiveCreationTimes(const QString &archive) {
    // Independent original-console property output, not the port's metadata
    // decoder. Windows SetFileTime keeps these CTime/MTime fields independent.
    QProcess process; auto environment=QProcessEnvironment::systemEnvironment(); environment.insert("TZ","UTC"); process.setProcessEnvironment(environment);
    process.start(executable,{"l","-slt","-ba","--",archive});
    if(!process.waitForStarted(10000) || !process.waitForFinished(30000) || process.exitCode()!=0) return {};
    QMap<qint64,timespec> result;
    for(const auto &block:QString::fromUtf8(process.readAllStandardOutput()).split("\n\n")) {
        const auto c=QRegularExpression("(?:^|\\n)Created = ([^\\n]+)").match(block), m=QRegularExpression("(?:^|\\n)Modified = ([^\\n]+)").match(block);
        if(!c.hasMatch() || !m.hasMatch()) continue;
        const auto created=QDateTime::fromString(QString(c.captured(1).left(19)).replace(' ','T')+'Z',Qt::ISODate);
        const auto modified=QDateTime::fromString(QString(m.captured(1).left(19)).replace(' ','T')+'Z',Qt::ISODate);
        bool ok=true; const auto fraction=c.captured(1).mid(20).leftJustified(9,'0').left(9).toLongLong(&ok);
        if(!ok || !created.isValid() || !modified.isValid()) return {};
        result[modified.toSecsSinceEpoch()]={time_t(created.toSecsSinceEpoch()),long(fraction)};
    }
    return result;
}
class ExtractionMetadataTests : public QObject {
    Q_OBJECT
private slots:
    void interruptedStreams_data() {
        QTest::addColumn<QString>("format"); QTest::addColumn<QString>("mode");
        for (const auto &format : {QString("7z"), QString("zip")})
            for (const auto &mode : {QString("new"), QString("overwrite"), QString("rename"), QString("renameExisting")})
                QTest::newRow(qPrintable(format + '-' + mode)) << format << mode;
    }
    void interruptedStreams() {
        QFETCH(QString, format); QFETCH(QString, mode);
        QTemporaryDir temp(QFileInfo(QDir::tempPath()).canonicalFilePath() + "/7zip-interrupted-XXXXXX");
        const auto source = temp.filePath("source"), archive = temp.filePath("large." + format);
        const QString name = "日本語 space.bin";
        const QByteArray bytes(64 * 1024 * 1024, 'q'), previous("previous user file");
        QVERIFY(write(source + '/' + name, bytes));
        const auto error = console({"a", "-t" + format, "-mx=1", "--", archive, name}, source);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        const auto baseline = temp.filePath("original"), output = temp.filePath("public"), stage = temp.filePath("private"), records = temp.filePath("records");
        for (const auto &path : {baseline, output, stage, records}) QVERIFY(QDir().mkpath(path));
        if (mode != "new") { QVERIFY(write(baseline + '/' + name, previous)); QVERIFY(write(output + '/' + name, previous)); }
        const auto actualName = mode == "rename" ? "日本語 space_1.bin" : name;
        const auto flag = mode == "rename" ? "-aou" : mode == "renameExisting" ? "-aot" : "-aoa";
        // Untouched official console supplies the process-death behavior.
        QProcess original;
        auto stopOriginal = qScopeGuard([&] { if (original.state() != QProcess::NotRunning) { original.kill(); original.waitForFinished(5000); } });
        QTimer pollOriginal; pollOriginal.setInterval(1);
        connect(&pollOriginal, &QTimer::timeout, this, [&] {
            if (stamp(baseline + '/' + actualName).st_size >= 65536) { pollOriginal.stop(); original.kill(); }
        });
        QSignalSpy originalDone(&original, &QProcess::finished);
        original.start(executable, {"x", flag, "-o" + baseline, "--", archive});
        QVERIFY(original.waitForStarted(10000)); original.closeWriteChannel(); pollOriginal.start();
        QVERIFY(!originalDone.isEmpty() || originalDone.wait(10000)); pollOriginal.stop();
        QCOMPARE(original.exitStatus(), QProcess::CrashExit);
        const auto baselineBytes = read(baseline + '/' + actualName);
        QVERIFY(baselineBytes.size() > 0); QVERIFY(baselineBytes.size() < bytes.size()); QCOMPARE(baselineBytes, bytes.first(baselineBytes.size()));

        SevenZipProcessBackend reader(executable); ArchiveRequest listing; listing.operation = ArchiveOperation::List; listing.archive = archive;
        const auto listed = run(reader, listing); QVERIFY(listed.success);
        OverwriteBroker broker; ExtractionSession session(stage, output, records, listed.entries, broker);
        QProcess process;
        auto stopProcess = qScopeGuard([&] { if (process.state() != QProcess::NotRunning) { process.kill(); process.waitForFinished(5000); } });
        auto environment = QProcessEnvironment::systemEnvironment();
        environment.insert("SEVENZIP_PORT_OUTPUT_STATE_RPC", "1"); environment.insert("SEVENZIP_PORT_OVERWRITE_RPC", "1");
        environment.insert("SEVENZIP_PORT_EXTRACTION_RPC", "1"); environment.insert("SEVENZIP_PORT_EXTRACTION_PATH", records + "/outputs.jsonl");
        process.setProcessEnvironment(environment); ProgressChannel channel(&process); QVERIFY(channel.prepare(true));
        bool opened = false; int published = 0;
        connect(&channel, &ProgressChannel::fileStateRequest, this, [&](NativeFileStateRequest request) { QVERIFY(channel.replyFileState(request, session.lookup(request))); });
        connect(&channel, &ProgressChannel::extractionRequest, this, [&](NativeExtractionRequest request) {
            const auto code = session.apply(request); QVERIFY2(code == 0, qPrintable(session.error()));
            opened |= request.action == "begin"; published += request.action == "publish";
            QVERIFY(channel.replyExtraction(request, code));
        });
        QTimer poll; poll.setInterval(1);
        connect(&poll, &QTimer::timeout, this, [&] { if (opened && stamp(stage + '/' + actualName).st_size >= 65536) { poll.stop(); process.kill(); } });
        QSignalSpy done(&process, &QProcess::finished);
        process.start(QFileInfo(executable).absolutePath() + "/7zz-progress", {"x", flag, "-o" + stage, "--", archive});
        QVERIFY(process.waitForStarted(10000)); process.closeWriteChannel(); poll.start();
        QVERIFY(!done.isEmpty() || done.wait(10000)); poll.stop();
        QCOMPARE(process.exitStatus(), QProcess::CrashExit); QVERIFY(opened); QCOMPARE(published, 0);
        const auto stagedBytes = read(stage + '/' + actualName);
        QVERIFY(stagedBytes.size() > 0); QVERIFY(stagedBytes.size() < bytes.size());
        const auto closeError = session.close(); QVERIFY2(closeError.isEmpty(), qPrintable(closeError)); QVERIFY(!session.retainsStaging());
        QCOMPARE(read(output + '/' + actualName), stagedBytes); QCOMPARE(stagedBytes, bytes.first(stagedBytes.size()));
        const auto actual = filesIn(output), expected = filesIn(baseline); QCOMPARE(actual.keys(), expected.keys());
        if (mode == "rename") QCOMPARE(read(output + '/' + name), previous);
        if (mode == "renameExisting") QCOMPARE(read(output + "/日本語 space_1.bin"), previous);
        QCOMPARE(session.close(), closeError);
        listing.operation = ArchiveOperation::Test; QVERIFY(run(reader, listing).success);
    }
    void interruptedRecoveryConflict_data() {
        QTest::addColumn<bool>("changedSource");
        QTest::newRow("public-output-changed") << false;
        QTest::newRow("private-stream-replaced") << true;
    }
    void interruptedRecoveryConflict() {
        QFETCH(bool, changedSource);
        QTemporaryDir temp(QFileInfo(QDir::tempPath()).canonicalFilePath() + "/7zip-recovery-XXXXXX");
        const auto stage = temp.filePath("private"), output = temp.filePath("public"), records = temp.filePath("records"), name = QString("file.txt");
        for (const auto &folder : {stage, output, records}) QVERIFY(QDir().mkpath(folder));
        ArchiveEntry entry; entry.path = name; entry.archiveIndex = 0; OverwriteBroker broker;
        ExtractionSession session(stage, output, records, {entry}, broker);
        const auto target = output + '/' + name, stream = stage + '/' + name;
        QVERIFY(write(target, "approved previous"));
        const auto approved = session.lookup({0, "1", stream, false}); QVERIFY(approved.exists);
        NativeExtractionRequest mutation{0, "1", "mutation", {{"operation", "delete"}, {"path", stream}}};
        QCOMPARE(session.apply(mutation), 0); QVERIFY(write(stream, "interrupted bytes")); const auto opened = stamp(stream);
        NativeExtractionRequest begin{0, "2", "begin", {{"index", 0}, {"path", stream}, {"device", QString::number(quint64(opened.st_dev))}, {"inode", QString::number(quint64(opened.st_ino))}}};
        QCOMPARE(session.apply(begin), 0);
        if (changedSource) { QVERIFY(QFile::rename(stream, stage + "/saved-stream")); QVERIFY(write(stream, "other private file")); }
        else QVERIFY(write(target, "other actor's public bytes"));
        const auto error = session.close(); QVERIFY(!error.isEmpty()); QVERIFY(session.retainsStaging()); QVERIFY(error.contains(stage));
        if (changedSource) { QCOMPARE(read(target), QByteArray("approved previous")); QCOMPARE(read(stage + "/saved-stream"), QByteArray("interrupted bytes")); }
        else { QCOMPARE(read(target), QByteArray("other actor's public bytes")); QCOMPARE(read(stream), QByteArray("interrupted bytes"));
            const auto retained = QDir(output).entryList({".7zip-displaced-*"}, QDir::Dirs | QDir::Hidden); QCOMPARE(retained.size(), 1);
            QCOMPARE(read(output + '/' + retained.first() + "/previous"), QByteArray("approved previous"));
        }
        QCOMPARE(session.close(), error);
    }
    void backendShutdownRetainsOpenedOutput_data() {
        QTest::addColumn<QString>("format");
        QTest::newRow("7z") << QString("7z"); QTest::newRow("zip") << QString("zip");
    }
    void backendShutdownRetainsOpenedOutput() {
        QFETCH(QString, format);
        QTemporaryDir temp(QFileInfo(QDir::tempPath()).canonicalFilePath() + "/7zip-shutdown-XXXXXX");
        const auto source = temp.filePath("source"), archive = temp.filePath("large." + format), output = temp.filePath("public");
        const QString name = "日本語 space.bin"; const QByteArray bytes(32 * 1024 * 1024, 's');
        QVERIFY(write(source + '/' + name, bytes)); QVERIFY(write(output + '/' + name, "previous user bytes"));
        const auto error = console({"a", "-t" + format, "-mx=1", "--", archive, name}, source); QVERIFY2(error.isEmpty(), qPrintable(error));
        auto backend = std::make_unique<SevenZipProcessBackend>(executable); bool closing = false, closed = false;
        connect(backend.get(), &ArchiveBackend::extractionCheckpoint, this, [&](const QString &phase, const QString &) {
            if (phase != "begin" || closing) return;
            closing = true;
            QTimer::singleShot(0, this, [&] { backend.reset(); closed = true; });
        });
        ArchiveRequest request; request.operation = ArchiveOperation::Extract; request.archive = archive; request.outputDirectory = output; request.overwriteMode = "overwrite";
        backend->start(request); QTRY_VERIFY_WITH_TIMEOUT(closed, 10000);
        QVERIFY(QFileInfo::exists(output + '/' + name));
        const auto actual = read(output + '/' + name); QVERIFY(actual.size() <= bytes.size()); QCOMPARE(actual, bytes.first(actual.size()));
        QVERIFY(QDir(output).entryList({".7zip-displaced-*", ".7zip-extract-*"}, QDir::Dirs | QDir::Hidden).isEmpty());
        SevenZipProcessBackend reader(executable); request.operation = ArchiveOperation::Test; QVERIFY(run(reader, request).success);
    }
    void nativeOrderedPublication_data() {
        QTest::addColumn<QString>("format"); QTest::addColumn<QString>("mode");
        for (const auto &format : {"7z", "zip"}) for (const auto &mode : {"-aoa", "-aos", "-aou", "-aot"})
            QTest::newRow(qPrintable(QString(format)+'-'+mode)) << QString(format) << QString(mode);
    }
    void nativeOrderedPublication() {
        QFETCH(QString, format); QFETCH(QString, mode); QTemporaryDir temp(QFileInfo(QDir::tempPath()).canonicalFilePath()+"/7zip-stream-XXXXXX");
        const auto archive=temp.filePath("duplicates."+format), stage=temp.filePath("private"), output=temp.filePath("public"), baseline=temp.filePath("original"), records=temp.filePath("records");
        QVERIFY(QDir().mkpath(stage)); QVERIFY(QDir().mkpath(records)); const auto error=orderedFixture(archive,format,false); QVERIFY2(error.isEmpty(),qPrintable(error));
        for(const auto &root:{output,baseline}) QVERIFY(write(root+"/folder/日本語 space.txt","protected original"));
        QProcess original; original.start(executable,{"x",mode,"-o"+baseline,"--",archive}); QVERIFY(original.waitForFinished(10000)); QCOMPARE(original.exitCode(),0);
        SevenZipProcessBackend reader(executable); ArchiveRequest listing; listing.operation=ArchiveOperation::List; listing.archive=archive; const auto listed=run(reader,listing); QVERIFY2(listed.success,qPrintable(listed.message+listed.details));
        OverwriteBroker broker; ExtractionSession session(stage,output,records,listed.entries,broker);
        QProcess process; auto cleanup=qScopeGuard([&] { if(process.state()!=QProcess::NotRunning) { process.kill(); process.waitForFinished(5000); } });
        auto environment=QProcessEnvironment::systemEnvironment(); environment.insert("SEVENZIP_PORT_OUTPUT_STATE_RPC","1"); environment.insert("SEVENZIP_PORT_EXTRACTION_RPC","1"); environment.insert("SEVENZIP_PORT_EXTRACTION_PATH",records+"/outputs.jsonl"); process.setProcessEnvironment(environment);
        ProgressChannel channel(&process); QVERIFY(channel.prepare(true)); int publications=0,mutations=0,finishes=0,begins=0; QByteArray previous="protected original";
        connect(&channel,&ProgressChannel::fileStateRequest,this,[&](NativeFileStateRequest request) {
            // The preceding completed stream is already public at the next
            // original CheckExistFile, rather than reconstructed at process exit.
            if(!request.probe && publications && mode=="-aoa") QCOMPARE(read(output+"/folder/日本語 space.txt"),previous);
            QVERIFY(channel.replyFileState(request,session.lookup(request)));
        });
        connect(&channel,&ProgressChannel::extractionRequest,this,[&](NativeExtractionRequest request) {
            const auto code=session.apply(request); QVERIFY2(code==0,qPrintable(session.error()));
            if(request.action=="publish") { ++publications; if(!request.item.value("snapshot").toString().isEmpty() && mode=="-aoa") previous=read(output+"/folder/日本語 space.txt"); }
            else if(request.action=="mutation") ++mutations; else if(request.action=="finish") ++finishes; else if(request.action=="begin") ++begins;
            QVERIFY(channel.replyExtraction(request,code)); QVERIFY(!channel.replyExtraction(request,code));
        });
        QSignalSpy done(&process,&QProcess::finished); process.start(QFileInfo(executable).absolutePath()+"/7zz-progress",{"x",mode,"-o"+stage,"--",archive});
        QVERIFY(process.waitForStarted(10000)); process.closeWriteChannel(); QVERIFY(!done.isEmpty()||done.wait(15000));
        QCOMPARE(process.exitStatus(),QProcess::NormalExit); QCOMPARE(process.exitCode(),0); QCOMPARE(finishes,1);
        QCOMPARE(begins, mode == "-aos" ? 0 : 3);
        if(mode=="-aoa"||mode=="-aot") QCOMPARE(mutations,3); else QCOMPARE(mutations,0);
        const auto expected=filesIn(baseline), actual=filesIn(output); QCOMPARE(actual.keys(),expected.keys());
        for(auto file=expected.cbegin();file!=expected.cend();++file) QCOMPARE(read(output+'/'+file.key()),read(baseline+'/'+file.key()));
    }
    void nativeOrderedAsk_data() { orderedCollisionAsk_data(); }
    void nativeOrderedAsk() {
        QFETCH(QByteArray, letters); QFETCH(QList<OverwriteAnswer>, answers); QTemporaryDir temp(QFileInfo(QDir::tempPath()).canonicalFilePath()+"/7zip-stream-XXXXXX");
        const auto archive=temp.filePath("duplicates.zip"), stage=temp.filePath("private"), output=temp.filePath("public"), baseline=temp.filePath("original"), records=temp.filePath("records");
        QVERIFY(QDir().mkpath(stage)); QVERIFY(QDir().mkpath(records)); const auto error=orderedFixture(archive,"zip",false); QVERIFY2(error.isEmpty(),qPrintable(error));
        for(const auto &root:{output,baseline}) QVERIFY(write(root+"/folder/日本語 space.txt","protected original"));
        QProcess original; original.start(executable,{"x","-o"+baseline,"--",archive}); QVERIFY(original.waitForStarted(10000)); original.write(letters); original.closeWriteChannel(); QVERIFY(original.waitForFinished(10000));
        SevenZipProcessBackend reader(executable); ArchiveRequest listing; listing.operation=ArchiveOperation::List; listing.archive=archive; const auto listed=run(reader,listing); QVERIFY(listed.success);
        OverwriteBroker broker; ExtractionSession session(stage,output,records,listed.entries,broker);
        QProcess process; auto cleanup=qScopeGuard([&] { if(process.state()!=QProcess::NotRunning) { process.kill(); process.waitForFinished(5000); } });
        auto environment=QProcessEnvironment::systemEnvironment(); environment.insert("SEVENZIP_PORT_OUTPUT_STATE_RPC","1"); environment.insert("SEVENZIP_PORT_OVERWRITE_RPC","1"); environment.insert("SEVENZIP_PORT_EXTRACTION_RPC","1"); environment.insert("SEVENZIP_PORT_EXTRACTION_PATH",records+"/outputs.jsonl"); process.setProcessEnvironment(environment);
        ProgressChannel channel(&process); QVERIFY(channel.prepare(true)); int prompts=0;
        connect(&channel,&ProgressChannel::fileStateRequest,this,[&](NativeFileStateRequest request) { QVERIFY(channel.replyFileState(request,session.lookup(request))); });
        connect(&channel,&ProgressChannel::extractionRequest,this,[&](NativeExtractionRequest request) { const auto code=session.apply(request); QVERIFY2(code==0,qPrintable(session.error())); QVERIFY(channel.replyExtraction(request,code)); });
        connect(&channel,&ProgressChannel::overwriteRequest,this,[&](NativeOverwriteRequest request) { QVERIFY(prompts<answers.size()); QVERIFY(channel.replyOverwrite(request,answers[prompts++])); });
        QSignalSpy done(&process,&QProcess::finished); process.start(QFileInfo(executable).absolutePath()+"/7zz-progress",{"x","-o"+stage,"--",archive});
        QVERIFY(process.waitForStarted(10000)); process.closeWriteChannel(); QVERIFY(!done.isEmpty()||done.wait(10000));
        QCOMPARE(process.exitStatus(),QProcess::NormalExit); QCOMPARE(process.exitCode(),original.exitCode()); QCOMPARE(prompts,answers.size());
        const auto expected=filesIn(baseline), actual=filesIn(output); QCOMPARE(actual.keys(),expected.keys());
        for(auto file=expected.cbegin();file!=expected.cend();++file) QCOMPARE(read(output+'/'+file.key()),read(baseline+'/'+file.key()));
    }
    void nativeOrderedApprovalChanged() {
        QTemporaryDir temp(QFileInfo(QDir::tempPath()).canonicalFilePath()+"/7zip-stream-XXXXXX");
        const auto archive=temp.filePath("one.zip"), stage=temp.filePath("private"), output=temp.filePath("public"), records=temp.filePath("records"), path=output+"/file.txt";
        QVERIFY(QDir().mkpath(stage)); QVERIFY(QDir().mkpath(records)); QVERIFY(write(path,"approved old"));
        QProcess fixture; fixture.start(QStandardPaths::findExecutable("python3"),{"-c","import sys,zipfile\nwith zipfile.ZipFile(sys.argv[1],'w') as z:z.writestr('file.txt',b'incoming bytes')",archive}); QVERIFY(fixture.waitForFinished(10000)); QCOMPARE(fixture.exitCode(),0);
        SevenZipProcessBackend reader(executable); ArchiveRequest listing; listing.operation=ArchiveOperation::List; listing.archive=archive; const auto listed=run(reader,listing); QVERIFY(listed.success);
        OverwriteBroker broker; ExtractionSession session(stage,output,records,listed.entries,broker);
        QProcess process; auto cleanup=qScopeGuard([&] { if(process.state()!=QProcess::NotRunning) { process.kill(); process.waitForFinished(5000); } });
        auto environment=QProcessEnvironment::systemEnvironment(); environment.insert("SEVENZIP_PORT_OUTPUT_STATE_RPC","1"); environment.insert("SEVENZIP_PORT_OVERWRITE_RPC","1"); environment.insert("SEVENZIP_PORT_EXTRACTION_RPC","1"); environment.insert("SEVENZIP_PORT_EXTRACTION_PATH",records+"/outputs.jsonl"); process.setProcessEnvironment(environment);
        ProgressChannel channel(&process); QVERIFY(channel.prepare(true)); int prompts=0,refused=0,published=0;
        connect(&channel,&ProgressChannel::fileStateRequest,this,[&](NativeFileStateRequest request) { QVERIFY(channel.replyFileState(request,session.lookup(request))); });
        connect(&channel,&ProgressChannel::overwriteRequest,this,[&](NativeOverwriteRequest request) {
            ++prompts; QVERIFY(QFile::rename(path,output+"/saved-old.txt")); QVERIFY(write(path,"unapproved other actor")); QVERIFY(channel.replyOverwrite(request,OverwriteAnswer::Yes));
        });
        connect(&channel,&ProgressChannel::extractionRequest,this,[&](NativeExtractionRequest request) {
            const auto code=session.apply(request); if(request.action=="mutation") { QCOMPARE(code,ESTALE); ++refused; } else { QCOMPARE(code,0); if(request.action=="publish") ++published; }
            QVERIFY(channel.replyExtraction(request,code));
        });
        QSignalSpy done(&process,&QProcess::finished); process.start(QFileInfo(executable).absolutePath()+"/7zz-progress",{"x","-o"+stage,"--",archive});
        QVERIFY(process.waitForStarted(10000)); process.closeWriteChannel(); QVERIFY(!done.isEmpty()||done.wait(10000));
        QCOMPARE(process.exitStatus(),QProcess::NormalExit); QCOMPARE(process.exitCode(),2); QCOMPARE(prompts,1); QCOMPARE(refused,1); QCOMPARE(published,0);
        QCOMPARE(read(path),QByteArray("unapproved other actor")); QCOMPARE(read(output+"/saved-old.txt"),QByteArray("approved old")); QVERIFY(!QFileInfo::exists(stage+"/file.txt")); QVERIFY(session.error().contains("changed"));
    }
    void nativeOutputStateSkip_data() {
        QTest::addColumn<QString>("kind");
        for (const auto &kind : {"file", "directory", "symlink"}) QTest::newRow(kind) << QString(kind);
    }
    void nativeOutputStateSkip() {
        QFETCH(QString, kind); QTemporaryDir temp(QFileInfo(QDir::tempPath()).canonicalFilePath() + "/7zip-state-XXXXXX");
        const auto archive = temp.filePath("crc.zip"), stage = temp.filePath("private"), output = temp.filePath("public");
        QVERIFY(QDir().mkpath(stage)); QVERIFY(QDir().mkpath(output));
        QProcess fixture; fixture.start(QStandardPaths::findExecutable("python3"), {"-c",
            "import sys,zipfile,struct\np=sys.argv[1]\nwith zipfile.ZipFile(p,'w') as z:z.writestr('日本語 space.txt',b'incoming bytes')\nb=bytearray(open(p,'rb').read());i=b.index(b'PK\\x01\\x02');struct.pack_into('<I',b,i+16,0x12345678);struct.pack_into('<I',b,14,0x12345678);open(p,'wb').write(b)", archive});
        QVERIFY(fixture.waitForStarted(10000)); QVERIFY(fixture.waitForFinished(10000)); QCOMPARE(fixture.exitCode(), 0);
        const auto protectedPath = output + "/日本語 space.txt";
        if (kind == "directory") QVERIFY(write(protectedPath + "/child.txt", "protected directory"));
        else if (kind == "symlink") { QVERIFY(write(output + "/target.txt", "protected target")); QVERIFY(::symlink("target.txt", QFile::encodeName(protectedPath).constData()) == 0); }
        else QVERIFY(write(protectedPath, "protected file"));
        QProcess test; test.start(executable, {"t", "--", archive}); QVERIFY(test.waitForFinished(10000)); QCOMPARE(test.exitCode(), 2);
        QProcess process; auto cleanup = qScopeGuard([&] { if (process.state() != QProcess::NotRunning) { process.kill(); process.waitForFinished(5000); } });
        auto environment = QProcessEnvironment::systemEnvironment(); environment.insert("SEVENZIP_PORT_OUTPUT_STATE_RPC", "1"); process.setProcessEnvironment(environment);
        ProgressChannel channel(&process); QVERIFY(channel.prepare(true)); int queries = 0;
        NativeFileStateRequest last;
        connect(&channel, &ProgressChannel::fileStateRequest, this, [&](NativeFileStateRequest request) {
            QCOMPARE(request.path, stage + "/日本語 space.txt"); const auto file = FileInstall::inspect(protectedPath);
            QVERIFY2(file.error.isEmpty(), qPrintable(file.error)); QVERIFY(file.exists); last = request; ++queries;
            QVERIFY(channel.replyFileState(request, file)); QVERIFY(!channel.replyFileState(request, file));
        });
        QSignalSpy done(&process, &QProcess::finished); process.start(QFileInfo(executable).absolutePath() + "/7zz-progress", {"x", "-aos", "-o" + stage, "--", archive});
        QVERIFY(process.waitForStarted(10000)); process.closeWriteChannel(); QVERIFY(!done.isEmpty() || done.wait(10000));
        QCOMPARE(process.exitStatus(), QProcess::NormalExit); QCOMPARE(process.exitCode(), 0); QCOMPARE(queries, 1);
        QVERIFY(!QFileInfo::exists(stage + "/日本語 space.txt")); QVERIFY(!channel.replyFileState(last, {}));
        if (kind == "directory") QCOMPARE(read(protectedPath + "/child.txt"), QByteArray("protected directory"));
        else if (kind == "symlink") { QVERIFY(S_ISLNK(stamp(protectedPath).st_mode)); QCOMPARE(read(output + "/target.txt"), QByteArray("protected target")); }
        else QCOMPARE(read(protectedPath), QByteArray("protected file"));
    }
    void nativeOutputStateRename_data() {
        QTest::addColumn<QString>("name"); QTest::addColumn<QString>("renamed");
        QTest::newRow("extension") << QString("日本語 space.tar.gz") << QString("日本語 space.tar_2.gz");
        QTest::newRow("dotfile") << QString(".hidden") << QString(".hidden_2");
    }
    void nativeOutputStateRename() {
        QFETCH(QString, name); QFETCH(QString, renamed); QTemporaryDir temp(QFileInfo(QDir::tempPath()).canonicalFilePath() + "/7zip-state-XXXXXX");
        const auto archive = temp.filePath("one.zip"), stage = temp.filePath("private"), output = temp.filePath("public"), baseline = temp.filePath("original");
        QVERIFY(QDir().mkpath(stage)); QProcess fixture; fixture.start(QStandardPaths::findExecutable("python3"), {"-c", "import sys,zipfile\nwith zipfile.ZipFile(sys.argv[1],'w') as z:z.writestr(sys.argv[2],b'incoming bytes')", archive, name});
        QVERIFY(fixture.waitForFinished(10000)); QCOMPARE(fixture.exitCode(), 0);
        auto first = renamed; first.replace("_2", "_1");
        for (const auto &root : {output, baseline}) { QVERIFY(write(root + '/' + name, "protected original")); QVERIFY(write(root + '/' + first, "protected suffix")); }
        QProcess original; original.start(executable, {"x", "-aou", "-o" + baseline, "--", archive}); QVERIFY(original.waitForFinished(10000)); QCOMPARE(original.exitCode(), 0);
        QCOMPARE(read(baseline + '/' + renamed), QByteArray("incoming bytes"));
        QProcess process; auto cleanup = qScopeGuard([&] { if (process.state() != QProcess::NotRunning) { process.kill(); process.waitForFinished(5000); } });
        auto environment = QProcessEnvironment::systemEnvironment(); environment.insert("SEVENZIP_PORT_OUTPUT_STATE_RPC", "1"); process.setProcessEnvironment(environment);
        ProgressChannel channel(&process); QVERIFY(channel.prepare(true)); int queries = 0;
        connect(&channel, &ProgressChannel::fileStateRequest, this, [&](NativeFileStateRequest request) {
            const auto relative = QDir(stage).relativeFilePath(request.path); QVERIFY(SevenZipProcessBackend::safeArchivePath(relative));
            const auto file = FileInstall::inspect(output + '/' + relative); QVERIFY2(file.error.isEmpty(), qPrintable(file.error));
            ++queries; QVERIFY(channel.replyFileState(request, file));
        });
        QSignalSpy done(&process, &QProcess::finished); process.start(QFileInfo(executable).absolutePath() + "/7zz-progress", {"x", "-aou", "-o" + stage, "--", archive});
        QVERIFY(process.waitForStarted(10000)); process.closeWriteChannel(); QVERIFY(!done.isEmpty() || done.wait(10000));
        QCOMPARE(process.exitStatus(), QProcess::NormalExit); QCOMPARE(process.exitCode(), 0); QVERIFY(queries >= 31);
        QCOMPARE(filesIn(stage).keys(), QStringList{renamed}); QCOMPARE(read(stage + '/' + renamed), read(baseline + '/' + renamed));
        QCOMPARE(read(output + '/' + name), QByteArray("protected original")); QCOMPARE(read(output + '/' + first), QByteArray("protected suffix"));
    }
    void nativeOutputStateUnsafeParent() {
        QTemporaryDir temp(QFileInfo(QDir::tempPath()).canonicalFilePath() + "/7zip-state-XXXXXX");
        const auto archive=temp.filePath("one.zip"), stage=temp.filePath("private"), output=temp.filePath("public"), outside=temp.filePath("outside");
        QVERIFY(QDir().mkpath(stage)); QVERIFY(QDir().mkpath(output)); QVERIFY(write(outside + "/file.txt", "protected outside"));
        QVERIFY(::symlink(QFile::encodeName(outside).constData(), QFile::encodeName(output + "/folder").constData()) == 0);
        QProcess fixture; fixture.start(QStandardPaths::findExecutable("python3"), {"-c", "import sys,zipfile\nwith zipfile.ZipFile(sys.argv[1],'w') as z:z.writestr('folder/file.txt',b'incoming bytes')", archive}); QVERIFY(fixture.waitForFinished(10000)); QCOMPARE(fixture.exitCode(), 0);
        QProcess process; auto cleanup=qScopeGuard([&] { if(process.state()!=QProcess::NotRunning) { process.kill(); process.waitForFinished(5000); } });
        auto environment=QProcessEnvironment::systemEnvironment(); environment.insert("SEVENZIP_PORT_OUTPUT_STATE_RPC","1"); process.setProcessEnvironment(environment);
        ProgressChannel channel(&process); QVERIFY(channel.prepare(true)); int queries=0;
        connect(&channel,&ProgressChannel::fileStateRequest,this,[&](NativeFileStateRequest request) {
            const auto file=FileInstall::inspect(output+'/'+QDir(stage).relativeFilePath(request.path)); QVERIFY(!file.error.isEmpty()); QVERIFY(file.errorCode!=0);
            ++queries; QVERIFY(channel.replyFileState(request,file));
        });
        QSignalSpy done(&process,&QProcess::finished); process.start(QFileInfo(executable).absolutePath()+"/7zz-progress",{"x","-aoa","-o"+stage,"--",archive});
        QVERIFY(process.waitForStarted(10000)); process.closeWriteChannel(); QVERIFY(!done.isEmpty()||done.wait(10000));
        QCOMPARE(process.exitStatus(),QProcess::NormalExit); QCOMPARE(process.exitCode(),2); QCOMPARE(queries,1);
        QVERIFY(process.readAllStandardError().contains("Cannot inspect extraction destination")); QVERIFY(!QFileInfo::exists(stage+"/folder/file.txt")); QCOMPARE(read(outside+"/file.txt"),QByteArray("protected outside"));
        const auto missing=FileInstall::inspect(output+"/absent/child.txt"); QVERIFY(missing.error.isEmpty()); QVERIFY(!missing.exists); QVERIFY(!QFileInfo::exists(output+"/absent"));
    }
    void nativeOutputStatePeerLoss_data() {
        QTest::addColumn<bool>("cancel"); QTest::newRow("peer-closed") << false; QTest::newRow("cancel") << true;
    }
    void nativeOutputStatePeerLoss() {
        QFETCH(bool, cancel); QTemporaryDir temp(QFileInfo(QDir::tempPath()).canonicalFilePath()+"/7zip-state-XXXXXX");
        const auto archive=temp.filePath("one.zip"), stage=temp.filePath("private"); QVERIFY(QDir().mkpath(stage));
        QProcess fixture; fixture.start(QStandardPaths::findExecutable("python3"),{"-c","import sys,zipfile\nwith zipfile.ZipFile(sys.argv[1],'w') as z:z.writestr('file.txt',b'incoming bytes')",archive}); QVERIFY(fixture.waitForFinished(10000)); QCOMPARE(fixture.exitCode(),0);
        QProcess process; auto cleanup=qScopeGuard([&] { if(process.state()!=QProcess::NotRunning) { process.kill(); process.waitForFinished(5000); } });
        auto environment=QProcessEnvironment::systemEnvironment(); environment.insert("SEVENZIP_PORT_OUTPUT_STATE_RPC","1"); process.setProcessEnvironment(environment);
        ProgressChannel channel(&process); QVERIFY(channel.prepare(true)); int queries=0;
        connect(&channel,&ProgressChannel::fileStateRequest,this,[&](NativeFileStateRequest) {
            ++queries; if(cancel) process.terminate(); else channel.prepare(false);
        });
        QSignalSpy done(&process,&QProcess::finished); process.start(QFileInfo(executable).absolutePath()+"/7zz-progress",{"x","-aoa","-o"+stage,"--",archive});
        QVERIFY(process.waitForStarted(10000)); process.closeWriteChannel(); QVERIFY(!done.isEmpty()||done.wait(5000));
        QCOMPARE(process.exitStatus(),QProcess::NormalExit); QCOMPARE(process.exitCode(),255); QCOMPARE(queries,1); QVERIFY(!QFileInfo::exists(stage+"/file.txt"));
    }
    void nativeOutputStateOriginalMetadata() {
        QTemporaryDir temp(QFileInfo(QDir::tempPath()).canonicalFilePath()+"/7zip-state-XXXXXX");
        const auto archive=temp.filePath("one.zip"), stage=temp.filePath("private"), output=temp.filePath("public"), path=output+"/日本語 space.txt";
        QVERIFY(QDir().mkpath(stage)); QVERIFY(QDir().mkpath(output)); QFile existing(path); QVERIFY(existing.open(QIODevice::ReadWrite));
        constexpr qint64 size=(qint64(5)<<30)+123; QVERIFY(existing.resize(size)); const timespec times[2]{{1600000000,123456700},{1600000000,987654300}}; QVERIFY(::futimens(existing.handle(),times)==0); existing.close();
        QProcess fixture; fixture.start(QStandardPaths::findExecutable("python3"),{"-c","import sys,zipfile\nwith zipfile.ZipFile(sys.argv[1],'w') as z:z.writestr('日本語 space.txt',b'incoming bytes')",archive}); QVERIFY(fixture.waitForFinished(10000)); QCOMPARE(fixture.exitCode(),0);
        QProcess process; auto cleanup=qScopeGuard([&] { if(process.state()!=QProcess::NotRunning) { process.kill(); process.waitForFinished(5000); } });
        auto environment=QProcessEnvironment::systemEnvironment(); environment.insert("SEVENZIP_PORT_OUTPUT_STATE_RPC","1"); environment.insert("SEVENZIP_PORT_OVERWRITE_RPC","1"); process.setProcessEnvironment(environment);
        ProgressChannel channel(&process); QVERIFY(channel.prepare(true)); int queries=0,prompts=0;
        connect(&channel,&ProgressChannel::fileStateRequest,this,[&](NativeFileStateRequest request) { ++queries; QVERIFY(channel.replyFileState(request,FileInstall::inspect(path))); });
        connect(&channel,&ProgressChannel::overwriteRequest,this,[&](NativeOverwriteRequest request) {
            ++prompts; QCOMPARE(request.conflict.existing.size,quint64(size)); QVERIFY(!request.conflict.existing.directory);
            QCOMPARE(request.conflict.existing.fileTime.value_or(0),quint64(1600000000+11644473600LL)*10000000+9876543);
            QCOMPARE(request.conflict.incoming.size,quint64(14)); QVERIFY(channel.replyOverwrite(request,OverwriteAnswer::No));
        });
        QSignalSpy done(&process,&QProcess::finished); process.start(QFileInfo(executable).absolutePath()+"/7zz-progress",{"x","-o"+stage,"--",archive});
        QVERIFY(process.waitForStarted(10000)); process.closeWriteChannel(); QVERIFY(!done.isEmpty()||done.wait(10000));
        QCOMPARE(process.exitCode(),0); QCOMPARE(queries,1); QCOMPARE(prompts,1); QCOMPARE(QFileInfo(path).size(),size); QVERIFY(!QFileInfo::exists(stage+"/日本語 space.txt"));
    }
    void incrementalInstallationSession() {
        QTemporaryDir temp(QFileInfo(QDir::tempPath()).canonicalFilePath() + "/7zip-session-XXXXXX"); const auto stage = temp.filePath("stage"), output = temp.filePath("output"), name = QString("日本語 space.txt");
        QVERIFY(QDir().mkpath(stage)); QVERIFY(write(output + '/' + name, "existing")); OverwriteBroker broker;
        auto control = std::make_shared<OperationControl>(); ExtractionInstaller installer(stage, output, false, {}, {}, "renameExisting", broker, control);
        for (int index = 0; index < 3; ++index) {
            const auto payload = QByteArray("payload ") + QByteArray::number(index); const auto path = temp.filePath(QString("cache-%1").arg(index)); QVERIFY(write(path, payload));
            const auto source = FileInstall::capture(path, true); QVERIFY(source.error.isEmpty());
            ExtractMetadata info; info.created = timespec{1600000000 + index, 0};
            const auto group = QString::number(quint64(source.stamp.st_dev)) + ':' + QString::number(quint64(source.stamp.st_ino));
            const auto error = installer.install(name, source, info, group); QVERIFY2(error.isEmpty(), qPrintable(error));
            QCOMPARE(read(output + '/' + name), payload); QVERIFY(!QFileInfo::exists(path));
            const auto backup = output + "/日本語 space_" + QString::number(index + 1) + ".txt";
            QCOMPARE(read(backup), index == 0 ? QByteArray("existing") : QByteArray("payload ") + QByteArray::number(index - 1));
        }
        QVERIFY(installer.finish().isEmpty()); QVERIFY(installer.finish().isEmpty());
        QCOMPARE(stamp(output + '/' + name).st_birthtimespec.tv_sec, time_t(1600000002));
        QCOMPARE(stamp(output + "/日本語 space_2.txt").st_birthtimespec.tv_sec, time_t(1600000000));
        QCOMPARE(stamp(output + "/日本語 space_3.txt").st_birthtimespec.tv_sec, time_t(1600000001));
        const auto another = temp.filePath("after-finish"); QVERIFY(write(another, "remain private"));
        QVERIFY(installer.install("late.txt", FileInstall::capture(another, true), {}, "1:1").contains("finalized")); QVERIFY(!QFileInfo::exists(output + "/late.txt"));
    }
    void approvedOutputChangedBeforePublication() {
        QTemporaryDir temp(QFileInfo(QDir::tempPath()).canonicalFilePath() + "/7zip-session-XXXXXX"); const auto stage = temp.filePath("stage"), output = temp.filePath("output"), payload = temp.filePath("cache");
        QVERIFY(QDir().mkpath(stage)); QVERIFY(write(output + "/target.txt", "approved old output")); QVERIFY(write(payload, "new archive bytes"));
        const auto approved = FileInstall::capture(output + "/target.txt"), source = FileInstall::capture(payload, true);
        QVERIFY(approved.error.isEmpty()); QVERIFY(approved.exists); QVERIFY(source.error.isEmpty());
        QVERIFY(QFile::rename(approved.path, output + "/old-saved.txt")); QVERIFY(write(approved.path, "replacement by another actor"));
        OverwriteBroker broker; auto control = std::make_shared<OperationControl>(); ExtractionInstaller installer(stage, output, false, {}, {}, "ask", broker, control);
        const auto error = installer.install("target.txt", source, {}, "1:1", approved, "overwrite"); QVERIFY(!error.isEmpty());
        QCOMPARE(read(approved.path), QByteArray("replacement by another actor")); QCOMPARE(read(payload), QByteArray("new archive bytes"));
        QVERIFY(installer.finish(error).contains(error));
    }
    void failedOutputs_data() {
        QTest::addColumn<QString>("format"); QTest::addColumn<QString>("failure"); QTest::addColumn<bool>("native"); QTest::addColumn<QString>("mode"); QTest::addColumn<QString>("option");
        for (const auto &format : {"zip", "7z"}) for (const auto &failure : {"crc", "unsupported"}) for (bool native : {false, true}) {
            if (QString(format) == "7z" && QString(failure) == "unsupported") continue;
            QTest::newRow(qPrintable(QString("%1-%2-%3-overwrite").arg(format).arg(failure).arg(native ? "native" : "legacy"))) << QString(format) << QString(failure) << native << QString("overwrite") << QString("-aoa");
            QTest::newRow(qPrintable(QString("%1-%2-%3-rename-existing").arg(format).arg(failure).arg(native ? "native" : "legacy"))) << QString(format) << QString(failure) << native << QString("renameExisting") << QString("-aot");
        }
    }
    void failedOutputs() {
        QFETCH(QString, format); QFETCH(QString, failure); QFETCH(bool, native); QFETCH(QString, mode); QFETCH(QString, option);
        QTemporaryDir temp; const auto archive = temp.filePath("partial." + format), baseline = temp.filePath("official"), output = temp.filePath("application");
        QProcess fixture; fixture.start("python3", {"-c", R"PY(
import os, pathlib, struct, subprocess, sys, tempfile, zipfile
path=pathlib.Path(sys.argv[1]); names=['folder/ASCII.txt','folder/日本語 space.txt','folder/after.txt']
if sys.argv[3]=='7z':
  with tempfile.TemporaryDirectory(dir=path.parent) as root:
    for i,name in enumerate(names):
      p=pathlib.Path(root)/name; p.parent.mkdir(parents=True,exist_ok=True); p.write_bytes(('payload '+str(i)).encode()); p.chmod(0o640); os.utime(p,(1600000000+i*2,1600000000+i*2))
    (pathlib.Path(root)/'empty').mkdir()
    subprocess.run([sys.argv[4],'a','-mx0','-ms=off','--',str(path),'.'],cwd=root,check=True,stdout=subprocess.DEVNULL)
  data=bytearray(path.read_bytes()); data[data.index(b'payload 1')]^=1; path.write_bytes(data); sys.exit(0)
with zipfile.ZipFile(path,'w',compression=zipfile.ZIP_STORED) as z:
  for i,name in enumerate(names):
    info=zipfile.ZipInfo(name,(2024,5,6,7,8,10)); info.create_system=3; info.external_attr=0o100640<<16
    z.writestr(info,('payload '+str(i)).encode())
  z.writestr('empty/',b'')
with zipfile.ZipFile(path) as z: bad=z.getinfo(names[1]); offset=bad.header_offset
data=bytearray(path.read_bytes()); n,x=struct.unpack_from('<HH',data,offset+26)
if sys.argv[2]=='crc': data[offset+30+n+x]^=1
else:
  struct.pack_into('<H',data,offset+8,255)
  central=data.index(b'PK\x01\x02')
  while True:
    n,x,c=struct.unpack_from('<HHH',data,central+28)
    if struct.unpack_from('<I',data,central+42)[0]==offset:
      struct.pack_into('<H',data,central+10,255); break
    central+=46+n+x+c
path.write_bytes(data)
)PY", archive, failure, format, executable});
        QVERIFY(fixture.waitForStarted(10000)); QVERIFY(fixture.waitForFinished(30000)); QCOMPARE(fixture.exitCode(), 0);
        for (const auto &root : {baseline, output}) QVERIFY(write(root + "/folder/日本語 space.txt", "existing output"));
        QProcess original; original.start(executable, {"x", option, "-o" + baseline, "--", archive});
        QVERIFY(original.waitForStarted(10000)); QVERIFY(original.waitForFinished(30000)); QCOMPARE(original.exitStatus(), QProcess::NormalExit); QCOMPARE(original.exitCode(), 2);
        const auto diagnostic = QString::fromUtf8(original.readAllStandardError()); QVERIFY(diagnostic.contains(failure == "crc" ? "CRC Failed" : "Unsupported Method"));
        SevenZipProcessBackend backend(executable); ArchiveRequest request; request.archive = archive;
        const auto listing = run(backend, request); QVERIFY2(listing.success, qPrintable(listing.message + listing.details));
        request.operation = ArchiveOperation::Extract; request.outputDirectory = output; request.overwriteMode = mode;
        if (native) {
            request.selectionDirectory = 0; request.selectionFlat = true; request.selectionSnapshot = listing.metadata.sourceSnapshot;
            request.selectionType = listing.archiveType; request.selectionEntryCount = listing.entries.size();
            for (const auto &directory : listing.metadata.directories) for (const auto &row : directory.children)
                if (row.archiveIndex >= 0) request.selection.append({row.identity, row.archiveIndex, row.name});
        }
        const auto result = run(backend, request);
        QVERIFY(!result.success); QVERIFY(!result.cancelled); QCOMPARE(result.exitCode, 2); QCOMPARE(result.operation, ArchiveOperation::Extract); QCOMPARE(result.target, archive);
        QVERIFY(result.details.contains(failure == "crc" ? "CRC Failed" : "Unsupported Method"));
        const auto expected = filesIn(baseline), actual = filesIn(output); QCOMPARE(actual.keys(), expected.keys());
        for (auto item = expected.cbegin(); item != expected.cend(); ++item) {
            const auto path = item.key(); QCOMPARE(read(output + '/' + path), read(baseline + '/' + path));
            const auto before = stamp(baseline + '/' + path), after = stamp(output + '/' + path);
            QCOMPARE(after.st_mode & 07777, before.st_mode & 07777); QCOMPARE(after.st_mtimespec.tv_sec, before.st_mtimespec.tv_sec);
        }
        QVERIFY(QFileInfo(output + "/empty").isDir());
        const auto originalDirectory = stamp(baseline + "/empty"), actualDirectory = stamp(output + "/empty");
        QCOMPARE(actualDirectory.st_mode & 07777, originalDirectory.st_mode & 07777); QCOMPARE(actualDirectory.st_mtimespec.tv_sec, originalDirectory.st_mtimespec.tv_sec);
        request.operation = ArchiveOperation::Test; const auto reused = run(backend, request); QVERIFY(!reused.success); QCOMPARE(reused.exitCode, 2);
        request.archive = temp.filePath("good.zip"); QVERIFY(console({"a", "--", request.archive, "ASCII.txt"}, baseline + "/folder").isEmpty());
        request.selection.clear(); request.selectionDirectory = -1; const auto recovered = run(backend, request); QVERIFY2(recovered.success, qPrintable(recovered.message + recovered.details));
    }
    void failedPasswordOutputs_data() {
        QTest::addColumn<QString>("format"); QTest::addColumn<QString>("encryption");
        QTest::newRow("7z-data") << QString("7z") << QString();
        QTest::newRow("zip-aes") << QString("zip") << QString("AES256");
        QTest::newRow("zip-crypto") << QString("zip") << QString("ZipCrypto");
    }
    void failedPasswordOutputs() {
        QFETCH(QString, format); QFETCH(QString, encryption); QTemporaryDir temp;
        const auto source = temp.filePath("source"), archive = temp.filePath("password." + format), baseline = temp.filePath("official"), output = temp.filePath("application");
        QVERIFY(write(source + "/ASCII.txt", QByteArray(32768, 'a'))); QVERIFY(write(source + "/日本語 space.txt", QByteArray(32768, 'b')));
        QProcess creator; creator.setWorkingDirectory(source);
        QStringList arguments{"a", "-t" + format, "-p"}; if (!encryption.isEmpty()) arguments << "-mem=" + encryption;
        arguments << "--" << archive << "."; creator.start(executable, arguments); QVERIFY(creator.waitForStarted(10000));
        creator.write("fixture-password\nfixture-password\n"); creator.closeWriteChannel(); QVERIFY(creator.waitForFinished(30000)); QCOMPARE(creator.exitCode(), 0);
        QProcess original; original.start(executable, {"x", "-aoa", "-o" + baseline, "--", archive});
        QVERIFY(original.waitForStarted(10000)); original.write("wrong-password\n"); original.closeWriteChannel(); QVERIFY(original.waitForFinished(30000)); QCOMPARE(original.exitCode(), 2);
        SevenZipProcessBackend backend(executable); ArchiveRequest request; request.operation = ArchiveOperation::Extract;
        request.archive = archive; request.outputDirectory = output; request.overwriteMode = "overwrite"; request.password = "wrong-password";
        auto result = run(backend, request); QVERIFY(!result.success); QCOMPARE(result.exitCode, 2); QVERIFY(result.passwordRequired);
        QVERIFY(!result.details.contains(request.password)); QVERIFY(!result.message.contains(request.password));
        const auto expected = filesIn(baseline), actual = filesIn(output); QCOMPARE(actual.keys(), expected.keys());
        for (auto item = expected.cbegin(); item != expected.cend(); ++item) QCOMPARE(read(output + '/' + item.key()), read(baseline + '/' + item.key()));
        request.password = "fixture-password"; result = run(backend, request); QVERIFY2(result.success, qPrintable(result.message + result.details));
        QCOMPARE(read(output + "/ASCII.txt"), read(source + "/ASCII.txt")); QCOMPARE(read(output + "/日本語 space.txt"), read(source + "/日本語 space.txt"));
    }
    void failedInstallationRestoresFolders() {
        QTemporaryDir temp; const auto source = temp.filePath("source"), archive = temp.filePath("folders.7z"), baseline = temp.filePath("official"), output = temp.filePath("application");
        QVERIFY(write(source + "/folder/first.txt", "first payload")); QVERIFY(write(source + "/folder/second.txt", "second payload"));
        const timespec times[2]{{1600000010, 0}, {1600000012, 0}};
        QVERIFY(::utimensat(AT_FDCWD, QFile::encodeName(source + "/folder").constData(), times, 0) == 0);
        QVERIFY(::chmod(QFile::encodeName(source + "/folder").constData(), 0740) == 0);
        auto error = console({"a", "--", archive, "."}, source); QVERIFY2(error.isEmpty(), qPrintable(error));
        for (const auto &root : {baseline, output}) QVERIFY(write(root + "/folder/second.txt/keep.txt", "protected child"));
        QProcess original; original.start(executable, {"x", "-aoa", "-o" + baseline, "--", archive});
        QVERIFY(original.waitForStarted(10000)); QVERIFY(original.waitForFinished(30000)); QCOMPARE(original.exitCode(), 2);
        SevenZipProcessBackend backend(executable); ArchiveRequest request; request.operation = ArchiveOperation::Extract;
        request.archive = archive; request.outputDirectory = output; request.overwriteMode = "overwrite";
        const auto result = run(backend, request); QVERIFY(!result.success); QCOMPARE(result.exitCode, original.exitCode()); QVERIFY(!result.message.isEmpty());
        QCOMPARE(read(output + "/folder/first.txt"), read(baseline + "/folder/first.txt"));
        QCOMPARE(read(output + "/folder/second.txt/keep.txt"), QByteArray("protected child"));
        const auto before = stamp(baseline + "/folder"), after = stamp(output + "/folder");
        QCOMPARE(after.st_mode & 07777, before.st_mode & 07777); QCOMPARE(after.st_mtimespec.tv_sec, before.st_mtimespec.tv_sec);
    }
    void orderedCollisions_data() {
        QTest::addColumn<QString>("format"); QTest::addColumn<QString>("mode"); QTest::addColumn<QString>("option");
        QTest::addColumn<bool>("flat"); QTest::addColumn<bool>("native"); QTest::addColumn<bool>("existing"); QTest::addColumn<int>("variant");
        const QStringList modes{"overwrite", "skip", "rename", "renameExisting"}, options{"-aoa", "-aos", "-aou", "-aot"};
        for (const auto &format : {"zip", "tar", "7z"}) for (bool flat : {false,true}) for (int variant=0;variant<(flat?1:3);++variant) for (bool native : {false,true}) for (bool existing : {false,true}) for (int mode=0;mode<4;++mode)
            QTest::newRow(qPrintable(QString("%1-%2-%3-%4-%5-%6").arg(format).arg(flat?"flat":"full").arg(native?"native":"legacy").arg(existing?"existing":"empty").arg(modes[mode]).arg(variant))) << QString(format) << modes[mode] << options[mode] << flat << native << existing << variant;
    }
    void orderedCollisions() {
        QFETCH(QString,format); QFETCH(QString,mode); QFETCH(QString,option); QFETCH(bool,flat); QFETCH(bool,native); QFETCH(bool,existing); QFETCH(int,variant);
        QTemporaryDir temp; const auto archive=temp.filePath("duplicates."+format), baseline=temp.filePath("official"), output=temp.filePath("application");
        auto error=orderedFixture(archive,format,flat,false,variant); QVERIFY2(error.isEmpty(),qPrintable(error));
        const auto name=variant==1?QString("Case.txt"):variant==2?QString("が space.txt"):QString("日本語 space.txt");
        if (existing) for (const auto &root : {baseline,output}) { QVERIFY(write(root+(flat?"/":"/folder/")+name,"existing original")); QVERIFY(write(root+(flat?"/":"/folder/")+QFileInfo(name).completeBaseName()+"_1.txt","existing auto-name")); }
        error=console({flat?"e":"x",option,"-o"+baseline,"--",archive}); QVERIFY2(error.isEmpty(),qPrintable(error));
        SevenZipProcessBackend backend(executable); ArchiveRequest request; request.archive=archive; const auto listing=run(backend,request); QVERIFY(listing.success); QCOMPARE(listing.entries.size(),3);
        request.operation=ArchiveOperation::Extract; request.outputDirectory=output; request.pathMode=flat?"flat":"full"; request.overwriteMode=mode;
        if(native) { request.selectionDirectory=0; request.selectionFlat=true; request.selectionSnapshot=listing.metadata.sourceSnapshot; request.selectionType=listing.archiveType; request.selectionEntryCount=listing.entries.size(); for(const auto &dir:listing.metadata.directories) for(const auto &row:dir.children) if(row.archiveIndex>=0) request.selection.append({row.identity,row.archiveIndex,row.name}); QCOMPARE(request.selection.size(),3); }
        const auto result=run(backend,request); QVERIFY2(result.success,qPrintable(result.message+result.details));
        const auto expected=filesIn(baseline), actual=filesIn(output); QCOMPARE(actual.keys(),expected.keys());
        const auto creation= format=="7z" ? archiveCreationTimes(archive) : QMap<qint64,timespec>(); if(format=="7z") QCOMPARE(creation.size(),3);
        for(auto item=expected.cbegin();item!=expected.cend();++item) { QCOMPARE(read(output+'/'+item.key()),read(baseline+'/'+item.key())); QCOMPARE(actual[item.key()].st_mode&07777,item->st_mode&07777); QCOMPARE(actual[item.key()].st_mtimespec.tv_sec,item->st_mtimespec.tv_sec);
            if(format=="7z" && read(baseline+'/'+item.key()).startsWith("payload ")) {
                QVERIFY(creation.contains(item->st_mtimespec.tv_sec)); const auto time=creation.value(item->st_mtimespec.tv_sec);
                QCOMPARE(actual[item.key()].st_birthtimespec.tv_sec,time.tv_sec); QCOMPARE(actual[item.key()].st_birthtimespec.tv_nsec/100,time.tv_nsec/100);
                QCOMPARE(item->st_birthtimespec.tv_sec,qMin(time.tv_sec,item->st_mtimespec.tv_sec));
            }
        }
    }
    void orderedCollisionAsk_data() {
        QTest::addColumn<QByteArray>("letters"); QTest::addColumn<QList<OverwriteAnswer>>("answers");
        QTest::newRow("yes") << QByteArray("y\ny\ny\n") << QList<OverwriteAnswer>{OverwriteAnswer::Yes,OverwriteAnswer::Yes,OverwriteAnswer::Yes};
        QTest::newRow("no") << QByteArray("n\nn\nn\n") << QList<OverwriteAnswer>{OverwriteAnswer::No,OverwriteAnswer::No,OverwriteAnswer::No};
        QTest::newRow("yes-all") << QByteArray("a\n") << QList<OverwriteAnswer>{OverwriteAnswer::YesToAll};
        QTest::newRow("no-all") << QByteArray("s\n") << QList<OverwriteAnswer>{OverwriteAnswer::NoToAll};
        QTest::newRow("auto-rename") << QByteArray("u\n") << QList<OverwriteAnswer>{OverwriteAnswer::AutoRename};
        QTest::newRow("cancel") << QByteArray("q\n") << QList<OverwriteAnswer>{OverwriteAnswer::Cancel};
        QTest::newRow("no-yes-no") << QByteArray("n\ny\nn\n") << QList<OverwriteAnswer>{OverwriteAnswer::No,OverwriteAnswer::Yes,OverwriteAnswer::No};
    }
    void orderedCollisionAsk() {
        QFETCH(QByteArray,letters); QFETCH(QList<OverwriteAnswer>,answers); QTemporaryDir temp;
        const auto archive=temp.filePath("duplicates.zip"),baseline=temp.filePath("official"),output=temp.filePath("application");
        const auto error=orderedFixture(archive,"zip",false); QVERIFY2(error.isEmpty(),qPrintable(error));
        for(const auto &root:{baseline,output}) QVERIFY(write(root+"/folder/日本語 space.txt","existing original"));
        QProcess process; process.start(executable,{"x","-o"+baseline,"--",archive}); QVERIFY(process.waitForStarted(10000)); process.write(letters); process.closeWriteChannel(); QVERIFY(process.waitForFinished(30000));
        const bool cancel=answers.contains(OverwriteAnswer::Cancel); QCOMPARE(process.exitCode(),cancel?255:0);
        SevenZipProcessBackend backend(executable); int conflicts=0;
        connect(&backend,&ArchiveBackend::overwriteRequested,this,[&](OverwriteConflict conflict){
            QVERIFY(conflicts<answers.size()); QVERIFY(conflict.incoming.sizeDefined); QCOMPARE(conflict.incoming.size,quint64(9));
            QVERIFY(backend.resolveOverwrite(conflict.id,answers[conflicts++]));
        });
        ArchiveRequest request; request.operation=ArchiveOperation::Extract; request.archive=archive; request.outputDirectory=output; request.overwriteMode="ask";
        const auto result=run(backend,request); QCOMPARE(result.cancelled,cancel); QCOMPARE(result.success,!cancel); QCOMPARE(conflicts,answers.size());
        const auto expected=filesIn(baseline),actual=filesIn(output); QCOMPARE(actual.keys(),expected.keys());
        for(auto item=expected.cbegin();item!=expected.cend();++item) QCOMPARE(read(output+'/'+item.key()),read(baseline+'/'+item.key()));
        request.operation=ArchiveOperation::Test; const auto reused=run(backend,request); QVERIFY2(reused.success,qPrintable(reused.message+reused.details));
    }
    void nativeOverwriteChannel_data() { orderedCollisionAsk_data(); }
    void nativeOverwriteChannel() {
        QFETCH(QByteArray, letters); QFETCH(QList<OverwriteAnswer>, answers); QTemporaryDir temp;
        const auto archive = temp.filePath("duplicates.zip"), baseline = temp.filePath("official"), output = temp.filePath("native-channel");
        const auto error = orderedFixture(archive, "zip", false); QVERIFY2(error.isEmpty(), qPrintable(error));
        for (const auto &root : {baseline, output}) QVERIFY(write(root + "/folder/日本語 space.txt", "existing original"));
        QProcess original; original.start(executable, {"x", "-o" + baseline, "--", archive});
        QVERIFY(original.waitForStarted(10000)); original.write(letters); original.closeWriteChannel(); QVERIFY(original.waitForFinished(30000));
        QProcess process; auto environment = QProcessEnvironment::systemEnvironment(); environment.insert("SEVENZIP_PORT_OVERWRITE_RPC", "1"); process.setProcessEnvironment(environment);
        ProgressChannel channel(&process); QVERIFY(channel.prepare(true)); int prompts = 0; NativeOverwriteRequest lastRequest;
        connect(&channel, &ProgressChannel::overwriteRequest, this, [&](NativeOverwriteRequest request) {
            QVERIFY(prompts < answers.size()); QCOMPARE(request.conflict.existing.path, output + "/folder/日本語 space.txt");
            QCOMPARE(request.conflict.incoming.path, QString("folder/日本語 space.txt")); QVERIFY(request.conflict.incoming.sizeDefined);
            QCOMPARE(request.conflict.incoming.size, quint64(9)); QVERIFY(request.conflict.incoming.modified.isValid());
            lastRequest = request; QVERIFY(channel.replyOverwrite(request, answers[prompts++]));
            QVERIFY(!channel.replyOverwrite(request, OverwriteAnswer::Yes)); // One-shot reply.
        });
        QSignalSpy done(&process, &QProcess::finished); process.start(QFileInfo(executable).absolutePath() + "/7zz-progress", {"x", "-o" + output, "--", archive});
        QVERIFY(process.waitForStarted(10000)); process.closeWriteChannel(); QVERIFY(!done.isEmpty() || done.wait(30000));
        QCOMPARE(process.exitStatus(), QProcess::NormalExit); QCOMPARE(process.exitCode(), original.exitCode()); QCOMPARE(prompts, answers.size());
        QVERIFY(!channel.replyOverwrite(lastRequest, OverwriteAnswer::Yes));
        const auto expected = filesIn(baseline), actual = filesIn(output); QCOMPARE(actual.keys(), expected.keys());
        for (auto item = expected.cbegin(); item != expected.cend(); ++item) QCOMPARE(read(output + '/' + item.key()), read(baseline + '/' + item.key()));
    }
    void nativeOverwritePeerLoss_data() {
        QTest::addColumn<bool>("signalCancel"); QTest::newRow("peer-disconnect") << false; QTest::newRow("signal-cancel") << true;
    }
    void nativeOverwritePeerLoss() {
        QFETCH(bool, signalCancel);
        QTemporaryDir temp; const auto archive = temp.filePath("duplicates.zip"), output = temp.filePath("native-channel");
        QVERIFY(orderedFixture(archive, "zip", false).isEmpty()); QVERIFY(write(output + "/folder/日本語 space.txt", "protected"));
        QProcess process; auto environment = QProcessEnvironment::systemEnvironment(); environment.insert("SEVENZIP_PORT_OVERWRITE_RPC", "1"); process.setProcessEnvironment(environment);
        auto channel = std::make_unique<ProgressChannel>(&process); QVERIFY(channel->prepare(true)); bool requested = false;
        connect(channel.get(), &ProgressChannel::overwriteRequest, this, [&](NativeOverwriteRequest) {
            requested = true; if (signalCancel) process.terminate(); else QTimer::singleShot(0, this, [&] { channel.reset(); });
        });
        QSignalSpy done(&process, &QProcess::finished); process.start(QFileInfo(executable).absolutePath() + "/7zz-progress", {"x", "-o" + output, "--", archive});
        QVERIFY(process.waitForStarted(10000)); process.closeWriteChannel(); QVERIFY(!done.isEmpty() || done.wait(5000));
        QVERIFY(requested); QCOMPARE(process.exitStatus(), QProcess::NormalExit); QCOMPARE(process.exitCode(), 255);
        QCOMPARE(read(output + "/folder/日本語 space.txt"), QByteArray("protected"));
    }
    void nativeOverwriteDelayedAndStaleReplies() {
        QTemporaryDir temp; const auto archive = temp.filePath("duplicates.zip"), output = temp.filePath("native-channel");
        QVERIFY(orderedFixture(archive, "zip", false).isEmpty());
        QProcess process; auto environment = QProcessEnvironment::systemEnvironment(); environment.insert("SEVENZIP_PORT_OVERWRITE_RPC", "1"); process.setProcessEnvironment(environment);
        ProgressChannel channel(&process); NativeOverwriteRequest previous; int prompts = 0, turn = 0;
        connect(&channel, &ProgressChannel::overwriteRequest, this, [&](NativeOverwriteRequest request) {
            QCOMPARE(++prompts, 1); if (turn) { QCOMPARE(previous.id, request.id); QVERIFY(previous.generation != request.generation); QVERIFY(!channel.replyOverwrite(previous, OverwriteAnswer::Yes)); }
            QTimer::singleShot(750, this, [&, request] { QVERIFY(channel.replyOverwrite(request, OverwriteAnswer::YesToAll)); previous = request; });
        });
        for (turn = 0; turn < 2; ++turn) {
            prompts = 0; QVERIFY(write(output + "/folder/日本語 space.txt", "protected")); QVERIFY(channel.prepare(true));
            QSignalSpy done(&process, &QProcess::finished); process.start(QFileInfo(executable).absolutePath() + "/7zz-progress", {"x", "-o" + output, "--", archive});
            QVERIFY(process.waitForStarted(10000)); process.closeWriteChannel(); QVERIFY(!done.isEmpty() || done.wait(5000));
            QCOMPARE(process.exitCode(), 0); QCOMPARE(prompts, 1); QVERIFY(!channel.replyOverwrite(previous, OverwriteAnswer::Yes));
            QCOMPARE(read(output + "/folder/日本語 space.txt"), QByteArray("payload 2"));
        }
    }
    void nativeOverwriteSkipAvoidsDecoder() {
        QTemporaryDir temp; const auto archive = temp.filePath("duplicates.zip"), output = temp.filePath("native-channel");
        QVERIFY(orderedFixture(archive, "zip", false).isEmpty()); QFile zip(archive); QVERIFY(zip.open(QIODevice::ReadOnly)); auto bytes = zip.readAll(); zip.close();
        const auto offset = bytes.indexOf("payload 0"); QVERIFY(offset >= 0); bytes[offset] ^= 1; QVERIFY(write(archive, bytes));
        QProcess verify; verify.start(executable, {"t", "--", archive}); QVERIFY(verify.waitForStarted(10000)); QVERIFY(verify.waitForFinished(30000)); QCOMPARE(verify.exitCode(), 2);
        QVERIFY(write(output + "/folder/日本語 space.txt", "protected"));
        QProcess process; auto environment = QProcessEnvironment::systemEnvironment(); environment.insert("SEVENZIP_PORT_OVERWRITE_RPC", "1"); process.setProcessEnvironment(environment);
        ProgressChannel channel(&process); QVERIFY(channel.prepare(true)); int prompts = 0;
        connect(&channel, &ProgressChannel::overwriteRequest, this, [&](NativeOverwriteRequest request) { ++prompts; QVERIFY(channel.replyOverwrite(request, OverwriteAnswer::NoToAll)); });
        QSignalSpy done(&process, &QProcess::finished); process.start(QFileInfo(executable).absolutePath() + "/7zz-progress", {"x", "-o" + output, "--", archive});
        QVERIFY(process.waitForStarted(10000)); process.closeWriteChannel(); QVERIFY(!done.isEmpty() || done.wait(5000));
        QCOMPARE(process.exitCode(), 0); QCOMPARE(prompts, 1); QCOMPARE(read(output + "/folder/日本語 space.txt"), QByteArray("protected"));
        QVERIFY(!QString::fromUtf8(process.readAllStandardError()).contains("CRC Failed"));
    }
    void orderedHardLinkChain_data() { explicitHardLinkOverwrite_data(); }
    void orderedHardLinkChain() {
        QFETCH(QString,mode); QFETCH(QString,option); QTemporaryDir temp; const auto archive=temp.filePath("chain.tar"),baseline=temp.filePath("official"),output=temp.filePath("application");
        auto error=orderedFixture(archive,"tar",false,true); QVERIFY2(error.isEmpty(),qPrintable(error));
        for(const auto &root:{baseline,output}) { QVERIFY(write(root+"/target.txt","existing target")); QVERIFY(write(root+"/alias.txt","existing alias")); }
        error=console({"x",option,"-o"+baseline,"--",archive}); QVERIFY2(error.isEmpty(),qPrintable(error));
        SevenZipProcessBackend backend(executable); ArchiveRequest request; request.operation=ArchiveOperation::Extract; request.archive=archive; request.outputDirectory=output; request.overwriteMode=mode;
        const auto result=run(backend,request); QVERIFY2(result.success,qPrintable(result.message+result.details));
        const auto expected=filesIn(baseline),actual=filesIn(output); QCOMPARE(actual.keys(),expected.keys());
        for(auto item=expected.cbegin();item!=expected.cend();++item) {
            QCOMPARE(read(output+'/'+item.key()),read(baseline+'/'+item.key())); QCOMPARE(actual[item.key()].st_mode&07777,item->st_mode&07777); QCOMPARE(actual[item.key()].st_mtimespec.tv_sec,item->st_mtimespec.tv_sec);
            for(auto other=expected.cbegin();other!=expected.cend();++other) QCOMPARE(actual[item.key()].st_ino==actual[other.key()].st_ino,item->st_ino==other->st_ino);
        }
    }
    void zipOmittedTimes() {
        QTemporaryDir temp; QVERIFY(temp.isValid());
        const auto source = temp.filePath("source"), output = temp.filePath("output"), archive = temp.filePath("omitted.zip");
        const QString name = "日本語 space.txt";
        QVERIFY(write(source + '/' + name, "ZIP with omitted creation/access times"));
        QVERIFY(QDir().mkpath(source + "/empty"));
        QVERIFY(::symlink(QFile::encodeName(name).constData(), QFile::encodeName(source + "/link").constData()) == 0);
        const timespec times[2]{{1600000000, 0}, {1600000000, 0}};
        for (const auto &leaf : {name, QString("empty"), QString("link")})
            QVERIFY(::utimensat(AT_FDCWD, QFile::encodeName(source + '/' + leaf).constData(), times, AT_SYMLINK_NOFOLLOW) == 0);
        const auto original = stamp(source + '/' + name);
        const auto created = console({"a", "-tzip", "-mx=0", "-snl", "-mtc=off", "-mta=off", "--", archive, "."}, source);
        QVERIFY2(created.isEmpty(), qPrintable(created));
        QCOMPARE(stamp(source + '/' + name).st_birthtimespec.tv_sec, original.st_birthtimespec.tv_sec);
        QCOMPARE(stamp(source + '/' + name).st_mtimespec.tv_sec, original.st_mtimespec.tv_sec);
        SevenZipProcessBackend backend(executable); ArchiveRequest request;
        request.operation = ArchiveOperation::Extract; request.archive = archive; request.outputDirectory = output; request.overwriteMode = "overwrite";
        const auto result = run(backend, request); QVERIFY2(result.success, qPrintable(result.message + result.details));
        for (const auto &leaf : {name, QString("empty"), QString("link")}) {
            const auto restored = stamp(output + '/' + leaf);
            QVERIFY2(restored.st_birthtimespec.tv_sec >= 1600000000, qPrintable(leaf));
            QVERIFY2(restored.st_atimespec.tv_sec >= 1600000000, qPrintable(leaf));
            QCOMPARE(restored.st_mtimespec.tv_sec, time_t(1600000000));
        }
        QCOMPARE(read(output + '/' + name), QByteArray("ZIP with omitted creation/access times"));
        QCOMPARE(linkTarget(output + "/link"), QFile::encodeName(name));
    }
    void metadataAndLinks_data() {
        QTest::addColumn<QString>("format"); for (const auto &format : {"7z", "zip", "tar", "wim"}) QTest::newRow(format) << QString(format);
    }
    void metadataAndLinks() {
        QFETCH(QString, format); QTemporaryDir temp; QVERIFY(temp.isValid());
        const auto source = temp.filePath("source"), baseline = temp.filePath("official"), output = temp.filePath("application");
        QVERIFY(write(source + "/payload.txt", "original engine metadata fixture"));
        QVERIFY(write(source + "/folder/日本語 space.txt", "nested content")); QVERIFY(QDir().mkpath(source + "/empty"));
        QVERIFY(::link(QFile::encodeName(source + "/payload.txt").constData(), QFile::encodeName(source + "/z-hard.txt").constData()) == 0);
        QVERIFY(::symlink("payload.txt", QFile::encodeName(source + "/relative-link").constData()) == 0);
        QVERIFY(::symlink("folder", QFile::encodeName(source + "/directory-link").constData()) == 0);
        QVERIFY(::symlink("missing", QFile::encodeName(source + "/dangling-link").constData()) == 0);
        QVERIFY(::symlink("/payload.txt", QFile::encodeName(source + "/absolute-link").constData()) == 0);
        const timespec times[2]{{1600000000, 123456700}, {1600000300, 765432100}};
        QDirIterator sourceItems(source, QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System, QDirIterator::Subdirectories);
        while (sourceItems.hasNext()) {
            const auto path = sourceItems.next(); const auto info = stamp(path);
            if (!S_ISLNK(info.st_mode)) QVERIFY(::chmod(QFile::encodeName(path).constData(), S_ISDIR(info.st_mode) ? 0750 : 0640) == 0);
            QVERIFY(::utimensat(AT_FDCWD, QFile::encodeName(path).constData(), times, AT_SYMLINK_NOFOLLOW) == 0);
        }
        const auto archive = temp.filePath("archive." + format);
        QStringList arguments{"a", "-t" + format, "-snl", "-snh"};
        if (format == "7z" || format == "zip") arguments << "-mtc=on" << "-mta=on";
        arguments << "--" << archive << ".";
        const auto created = console(arguments, source); QVERIFY2(created.isEmpty(), qPrintable(created));
        const auto extracted = console({"x", "-aoa", "-o" + baseline, "--", archive}); QVERIFY2(extracted.isEmpty(), qPrintable(extracted));
        SevenZipProcessBackend backend(executable); ArchiveRequest request; request.operation = ArchiveOperation::List; request.archive = archive;
        const auto listing = run(backend, request); QVERIFY2(listing.success, qPrintable(listing.message + listing.details));
        request.operation = ArchiveOperation::Extract; request.outputDirectory = output; request.pathMode = "full"; request.overwriteMode = "overwrite";
        const auto result = run(backend, request); QVERIFY2(result.success, qPrintable(result.message + result.details));
        QMap<QString, QPair<dev_t, ino_t>> groups; int count = 0;
        QDirIterator expectedItems(baseline, QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System, QDirIterator::Subdirectories);
        while (expectedItems.hasNext()) {
            const auto path = expectedItems.next(), relative = QDir(baseline).relativeFilePath(path), actualPath = output + '/' + relative;
            const auto expected = stamp(path), actual = stamp(actualPath); QVERIFY2(actual.st_ino, qPrintable(relative)); ++count;
            QCOMPARE(actual.st_mode & (S_IFMT | 07777), expected.st_mode & (S_IFMT | 07777));
            QCOMPARE(actual.st_mtimespec.tv_sec, expected.st_mtimespec.tv_sec); QCOMPARE(actual.st_mtimespec.tv_nsec, expected.st_mtimespec.tv_nsec);
            if (S_ISLNK(expected.st_mode)) {
                auto target = linkTarget(path); const auto prefix = QFile::encodeName(baseline + '/');
                if (target.startsWith(prefix)) target = QFile::encodeName(QFileInfo(output).canonicalFilePath() + '/') + target.mid(prefix.size());
                QCOMPARE(linkTarget(actualPath), target);
            } else if (S_ISREG(expected.st_mode)) {
                QCOMPARE(read(actualPath), read(path));
                if (expected.st_nlink > 1) {
                    const auto group = QString::number(expected.st_dev) + ':' + QString::number(expected.st_ino);
                    if (groups.contains(group)) { QCOMPARE(actual.st_dev, groups[group].first); QCOMPARE(actual.st_ino, groups[group].second); }
                    else groups[group] = {actual.st_dev, actual.st_ino};
                }
            }
            for (const auto &entry : listing.entries) if (entry.path == relative) for (const auto &property : entry.orderedProperties) if (property.name == "Created" && property.type == 64 && !property.value.isEmpty()) {
                bool ok; const auto ticks = property.fileTime.toULongLong(&ok); QVERIFY(ok);
                QCOMPARE(actual.st_birthtimespec.tv_sec, time_t(ticks / 10000000ULL) - 11644473600LL);
                QCOMPARE(actual.st_birthtimespec.tv_nsec / 100, long(ticks % 10000000ULL));
            }
        }
        QVERIFY(count >= 8);
    }
    void explicitHardLinkOverwrite_data() {
        QTest::addColumn<QString>("mode"); QTest::addColumn<QString>("option");
        QTest::newRow("overwrite") << QString("overwrite") << QString("-aoa");
        QTest::newRow("skip") << QString("skip") << QString("-aos");
        QTest::newRow("auto-rename") << QString("rename") << QString("-aou");
        QTest::newRow("rename-existing") << QString("renameExisting") << QString("-aot");
    }
    void explicitHardLinkOverwrite() {
        QFETCH(QString, mode); QFETCH(QString, option); QTemporaryDir temp;
        const auto source = temp.filePath("source"), baseline = temp.filePath("official"), output = temp.filePath("application");
        QVERIFY(write(source + "/data.txt", "new payload")); QVERIFY(::link(QFile::encodeName(source + "/data.txt").constData(), QFile::encodeName(source + "/z-alias.txt").constData()) == 0);
        const auto archive = temp.filePath("archive.tar"); auto error = console({"a", "-ttar", "-snh", "--", archive, "."}, source); QVERIFY2(error.isEmpty(), qPrintable(error));
        QVERIFY(write(baseline + "/data.txt", "old payload")); QVERIFY(write(output + "/data.txt", "old payload"));
        error = console({"x", option, "-o" + baseline, "--", archive}); QVERIFY2(error.isEmpty(), qPrintable(error));
        SevenZipProcessBackend backend(executable); ArchiveRequest request; request.operation = ArchiveOperation::Extract; request.archive = archive;
        request.outputDirectory = output; request.pathMode = "full"; request.overwriteMode = mode;
        const auto result = run(backend, request); QVERIFY2(result.success, qPrintable(result.message + result.details));
        for (const auto &name : {"data.txt", "data_1.txt", "z-alias.txt"}) { QCOMPARE(QFileInfo::exists(output + '/' + name), QFileInfo::exists(baseline + '/' + name)); QCOMPARE(read(output + '/' + name), read(baseline + '/' + name)); }
        const auto data = stamp(output + "/data.txt"), alias = stamp(output + "/z-alias.txt"); QCOMPARE(alias.st_ino, data.st_ino); QCOMPARE(alias.st_dev, data.st_dev);
    }
    void selectedExistingHardLink_data() {
        QTest::addColumn<QString>("mode"); QTest::addColumn<QString>("option"); QTest::addColumn<bool>("flat"); QTest::addColumn<bool>("native"); QTest::addColumn<bool>("absolute");
        for (const int pathMode : {0, 1, 2}) {
            const bool flat = pathMode == 1, absolute = pathMode == 2; const QString label = absolute ? "absolute-" : flat ? "flat-" : "full-";
            for (const auto &pair : {qMakePair("overwrite", "-aoa"), qMakePair("skip", "-aos"), qMakePair("rename", "-aou"), qMakePair("renameExisting", "-aot")})
                QTest::newRow(qPrintable(label + pair.first)) << QString(pair.first) << QString(pair.second) << flat << true << absolute;
            QTest::newRow(qPrintable(label + "legacy")) << QString("overwrite") << QString("-aoa") << flat << false << absolute;
        }
    }
    void selectedExistingHardLink() {
        QFETCH(QString, mode); QFETCH(QString, option); QFETCH(bool, flat); QFETCH(bool, native); QFETCH(bool, absolute); QTemporaryDir temp;
        const auto source = temp.filePath("source"), baseline = temp.filePath("official"), output = temp.filePath("application");
        const QString target = QString::fromUtf8("folder/a 日本語 source.txt"), alias = "folder/z-alias.txt", secondAlias = "folder/zz-alias.txt", extractedAlias = flat ? QString("z-alias.txt") : alias, extractedSecond = flat ? QString("zz-alias.txt") : secondAlias;
        QVERIFY(write(source + '/' + target, "archived target bytes")); QVERIFY(::link(QFile::encodeName(source + '/' + target).constData(), QFile::encodeName(source + '/' + alias).constData()) == 0);
        QVERIFY(::link(QFile::encodeName(source + '/' + target).constData(), QFile::encodeName(source + '/' + secondAlias).constData()) == 0);
        QVERIFY(::chmod(QFile::encodeName(source + '/' + target).constData(), 0640) == 0);
        const auto archive = temp.filePath("archive.tar"); auto error = console({"a", "-ttar", "-snh", "--", archive, "."}, source); QVERIFY2(error.isEmpty(), qPrintable(error));
        for (const auto &root : {baseline, output}) { QVERIFY(write(root + '/' + target, "existing destination bytes")); QVERIFY(::chmod(QFile::encodeName(root + '/' + target).constData(), 0400) == 0); QVERIFY(write(root + '/' + extractedAlias, "previous alias bytes")); QVERIFY(write(root + '/' + extractedSecond, "previous second alias")); }
        error = console({flat ? "e" : "x", option, "-o" + baseline, "--", archive, alias, secondAlias}); QVERIFY2(error.isEmpty(), qPrintable(error));
        SevenZipProcessBackend backend(executable); ArchiveRequest request; request.archive = archive;
        const auto snapshot = run(backend, request); QVERIFY(snapshot.success);
        bool explicitLink = false; for (const auto &entry : snapshot.entries) if (entry.path == alias) explicitLink = entry.properties.value("Hard Link") == target; QVERIFY(explicitLink);
        request.operation = ArchiveOperation::Extract; request.files = {alias, secondAlias}; request.outputDirectory = output; request.pathMode = absolute ? "absolute" : flat ? "flat" : "full"; request.overwriteMode = mode;
        if (native) {
            request.selectionDirectory = 0; request.selectionFlat = true; request.selectionSnapshot = snapshot.metadata.sourceSnapshot; request.selectionType = snapshot.archiveType; request.selectionEntryCount = snapshot.entries.size();
            for (const auto &dir : snapshot.metadata.directories) for (const auto &row : dir.children) if (row.path == alias || row.path == secondAlias) request.selection.append({row.identity, row.archiveIndex, row.name});
            QCOMPARE(request.selection.size(), 2);
        }
        const auto result = run(backend, request); QVERIFY2(result.success, qPrintable(result.message + result.details));
        const QStringList names{target, extractedAlias, flat ? "z-alias_1.txt" : "folder/z-alias_1.txt", extractedSecond, flat ? "zz-alias_1.txt" : "folder/zz-alias_1.txt"};
        for (const auto &name : names) { QCOMPARE(QFileInfo::exists(output + '/' + name), QFileInfo::exists(baseline + '/' + name)); QCOMPARE(read(output + '/' + name), read(baseline + '/' + name)); }
        const auto actualAlias = mode == "rename" ? names[2] : extractedAlias;
        if (mode != "skip") { QCOMPARE(stamp(output + '/' + actualAlias).st_ino, stamp(output + '/' + target).st_ino); QCOMPARE(stamp(output + '/' + actualAlias).st_dev, stamp(output + '/' + target).st_dev); }
        if (mode != "skip") QCOMPARE(stamp(output + '/' + (mode == "rename" ? names.last() : extractedSecond)).st_ino, stamp(output + '/' + target).st_ino);
        QCOMPARE(read(output + '/' + target), QByteArray("existing destination bytes"));
        QCOMPARE(stamp(output + '/' + target).st_mode & 07777, stamp(baseline + '/' + target).st_mode & 07777);
        if (mode != "skip") QCOMPARE(stamp(output + '/' + target).st_mtimespec.tv_sec, stamp(baseline + '/' + target).st_mtimespec.tv_sec);
    }
    void fileDirectoryConflicts_data() {
        QTest::addColumn<QString>("kind"); QTest::addColumn<QString>("mode"); QTest::addColumn<QString>("option");
        for (const auto &kind : {"empty-directory", "nonempty-directory", "incoming-directory", "readonly-file"})
            for (const auto &pair : {qMakePair("overwrite", "-aoa"), qMakePair("skip", "-aos"), qMakePair("rename", "-aou"), qMakePair("renameExisting", "-aot")})
                QTest::newRow(qPrintable(QString(kind) + '-' + pair.first)) << QString(kind) << QString(pair.first) << QString(pair.second);
    }
    void fileDirectoryConflicts() {
        QFETCH(QString, kind); QFETCH(QString, mode); QFETCH(QString, option); QTemporaryDir temp;
        const auto source = temp.filePath("source"), archive = temp.filePath("conflict.7z"), baseline = temp.filePath("official"), output = temp.filePath("application");
        QVERIFY(write(source + (kind == "incoming-directory" ? "/shared/child.txt" : "/shared"), "incoming payload"));
        const auto error = console({"a", "-t7z", "--", archive, "shared"}, source); QVERIFY2(error.isEmpty(), qPrintable(error));
        for (const auto &root : {baseline, output}) {
            QVERIFY(QDir().mkpath(root));
            if (kind == "empty-directory") QVERIFY(QDir().mkpath(root + "/shared"));
            else if (kind == "nonempty-directory") QVERIFY(write(root + "/shared/keep.txt", "protected child"));
            else { QVERIFY(write(root + "/shared", "previous payload")); if (kind == "readonly-file") QVERIFY(::chmod(QFile::encodeName(root + "/shared").constData(), 0400) == 0); }
        }
        QProcess original; original.start(executable, {"x", option, "-o" + baseline, "--", archive});
        QVERIFY(original.waitForStarted(10000)); QVERIFY(original.waitForFinished(10000)); QCOMPARE(original.exitStatus(), QProcess::NormalExit);
        SevenZipProcessBackend backend(executable); ArchiveRequest request; request.operation = ArchiveOperation::Extract; request.archive = archive; request.outputDirectory = output; request.overwriteMode = mode;
        const auto result = run(backend, request); QCOMPARE(result.success, original.exitCode() == 0); QCOMPARE(result.exitCode, original.exitCode());
        auto tree = [](const QString &root) {
            QMap<QString, mode_t> rows; QDirIterator iterator(root, QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System, QDirIterator::Subdirectories);
            while (iterator.hasNext()) { const auto path = iterator.next(); rows[QDir(root).relativeFilePath(path)] = stamp(path).st_mode & S_IFMT; } return rows;
        };
        QCOMPARE(tree(output), tree(baseline));
        for (const auto &name : filesIn(baseline).keys()) QCOMPARE(read(output + '/' + name), read(baseline + '/' + name));
        if (kind == "nonempty-directory" && mode != "renameExisting") QCOMPARE(read(output + "/shared/keep.txt"), QByteArray("protected child"));
    }
    void hardLinkPathMapping_data() {
        QTest::addColumn<int>("kind");
        QTest::newRow("legacy-eliminate-root") << 0;
        QTest::newRow("native-eliminate-root") << 1;
        QTest::newRow("native-current-folder") << 2;
        QTest::newRow("native-current-folder-eliminate-root") << 3;
    }
    void hardLinkPathMapping() {
        QFETCH(int, kind); QTemporaryDir temp;
        const bool current = kind >= 2, eliminate = kind != 2;
        const QString base = current ? "outer/" : "", prefix = eliminate ? "payload/" : "";
        const QString target = base + prefix + "folder/a-data.txt", alias = base + prefix + "folder/z-alias.txt";
        const auto source = temp.filePath("source"), archive = temp.filePath("full.tar"), adjusted = temp.filePath("adjusted.tar");
        QVERIFY(write(source + '/' + target, "archived reference bytes"));
        QVERIFY(::link(QFile::encodeName(source + '/' + target).constData(), QFile::encodeName(source + '/' + alias).constData()) == 0);
        auto error = console({"a", "-ttar", "-snh", "--", archive, "."}, source); QVERIFY2(error.isEmpty(), qPrintable(error));
        error = console({"a", "-ttar", "-snh", "--", adjusted, "."}, current ? source + "/outer" : source); QVERIFY2(error.isEmpty(), qPrintable(error));
        const auto output = temp.filePath(eliminate ? "application/payload" : "application/current");
        const auto baseline = temp.filePath(eliminate ? "official/payload" : "official/current");
        for (const auto &root : {output, baseline}) QVERIFY(write(root + "/folder/a-data.txt", "existing public reference"));
        QStringList args{"x", "-aoa", "-o" + baseline}; if (eliminate) args << "-spe";
        args << "--" << adjusted << alias.mid(base.size()); error = console(args); QVERIFY2(error.isEmpty(), qPrintable(error));
        SevenZipProcessBackend backend(executable); ArchiveRequest request; request.archive = archive;
        const auto snapshot = run(backend, request); QVERIFY2(snapshot.success, qPrintable(snapshot.message));
        request.operation = ArchiveOperation::Extract; request.files = {alias}; request.outputDirectory = output; request.eliminateRoot = eliminate; request.pathMode = "full"; request.overwriteMode = "overwrite";
        if (kind != 0) {
            ArchiveFolderIndex index(snapshot.entries, snapshot.metadata); const auto directory = index.findDirectory(base); QVERIFY(directory >= 0);
            request.selectionDirectory = directory; request.selectionFlat = true; request.selectionSnapshot = snapshot.metadata.sourceSnapshot; request.selectionType = snapshot.archiveType; request.selectionEntryCount = snapshot.entries.size();
            for (const auto &row : index.directoryRows(directory, true)) if (row.path == alias) request.selection.append({row.identity, row.archiveIndex, row.name});
            QCOMPARE(request.selection.size(), 1);
        }
        const auto result = run(backend, request); QVERIFY2(result.success, qPrintable(result.message + result.details));
        QCOMPARE(filesIn(output).keys(), filesIn(baseline).keys());
        for (const auto &name : {"folder/a-data.txt", "folder/z-alias.txt"}) QCOMPARE(read(output + '/' + name), read(baseline + '/' + name));
        QCOMPARE(read(output + "/folder/a-data.txt"), QByteArray("existing public reference"));
        QCOMPARE(stamp(output + "/folder/a-data.txt").st_ino, stamp(output + "/folder/z-alias.txt").st_ino);
        QVERIFY(!QFileInfo::exists(output + "/payload")); QVERIFY(!QFileInfo::exists(output + "/outer"));
    }
    void selectedRar5HardLink() {
        QTemporaryDir temp; const auto archive = temp.filePath("genuine.rar"), baseline = temp.filePath("official"), output = temp.filePath("application");
        const auto fixture = QFINDTESTDATA("fixtures/test_read_format_rar5_hardlink.rar.uu"); QVERIFY(!fixture.isEmpty());
        QProcess decode; decode.start("python3", {"-c", "import pathlib,binascii,sys; lines=pathlib.Path(sys.argv[1]).read_bytes().splitlines(); start=next(i for i,l in enumerate(lines) if l.startswith(b'begin ')); end=next(i for i in range(start+1,len(lines)) if lines[i]==b'end'); pathlib.Path(sys.argv[2]).write_bytes(b''.join(binascii.a2b_uu(l) for l in lines[start+1:end]))", fixture, archive});
        QVERIFY(decode.waitForFinished(10000)); QCOMPARE(decode.exitCode(), 0);
        for (const auto &root : {baseline, output}) QVERIFY(write(root + "/file.txt", "existing RAR5 reference"));
        const auto error = console({"x", "-aoa", "-o" + baseline, "--", archive, "hardlink.txt"}); QVERIFY2(error.isEmpty(), qPrintable(error));
        SevenZipProcessBackend backend(executable); ArchiveRequest request; request.archive = archive; const auto snapshot = run(backend, request); QVERIFY(snapshot.success); QCOMPARE(snapshot.archiveType.toLower(), QString("rar5"));
        request.operation = ArchiveOperation::Extract; request.files = {"hardlink.txt"}; request.outputDirectory = output; request.overwriteMode = "overwrite"; request.selectionDirectory = 0;
        request.selectionSnapshot = snapshot.metadata.sourceSnapshot; request.selectionType = snapshot.archiveType; request.selectionEntryCount = snapshot.entries.size();
        for (const auto &row : snapshot.metadata.directories[0].children) if (row.name == "hardlink.txt") request.selection.append({row.identity, row.archiveIndex, row.name});
        QCOMPARE(request.selection.size(), 1); bool explicitLink = false; for (const auto &entry : snapshot.entries) if (entry.path == "hardlink.txt") explicitLink = entry.properties.value("Hard Link") == "file.txt"; QVERIFY(explicitLink);
        const auto result = run(backend, request); QVERIFY2(result.success, qPrintable(result.message + result.details));
        QCOMPARE(read(output + "/file.txt"), QByteArray("existing RAR5 reference")); QCOMPARE(read(output + "/hardlink.txt"), read(baseline + "/hardlink.txt"));
        QCOMPARE(stamp(output + "/hardlink.txt").st_ino, stamp(output + "/file.txt").st_ino);
        QCOMPARE(stamp(output + "/file.txt").st_mode & 07777, stamp(baseline + "/file.txt").st_mode & 07777);
    }
    void existingHardLinkTargetFailures_data() {
        QTest::addColumn<QString>("failure"); for (const auto &failure : {"missing", "leaf-link", "parent-link", "changed"}) QTest::newRow(failure) << QString(failure);
    }
    void existingHardLinkTargetFailures() {
        QFETCH(QString, failure); QTemporaryDir temp;
        const auto source = temp.filePath("source"), output = temp.filePath("application"), protectedRoot = temp.filePath("protected");
        const QString target = "folder/a-data.txt", alias = "folder/z-alias.txt";
        QVERIFY(write(source + '/' + target, "archived source bytes")); QVERIFY(::link(QFile::encodeName(source + '/' + target).constData(), QFile::encodeName(source + '/' + alias).constData()) == 0);
        const auto archive = temp.filePath("archive.tar"); const auto error = console({"a", "-ttar", "-snh", "--", archive, "."}, source); QVERIFY2(error.isEmpty(), qPrintable(error));
        QVERIFY(write(protectedRoot + "/a-data.txt", "protected outside bytes")); QVERIFY(QDir().mkpath(output));
        if (failure == "parent-link") QVERIFY(::symlink(QFile::encodeName(protectedRoot).constData(), QFile::encodeName(output + "/folder").constData()) == 0);
        QVERIFY(write(output + '/' + alias, "previous alias retained"));
        if (failure == "leaf-link") QVERIFY(::symlink(QFile::encodeName(protectedRoot + "/a-data.txt").constData(), QFile::encodeName(output + '/' + target).constData()) == 0);
        if (failure == "changed") QVERIFY(write(output + '/' + target, "existing destination bytes"));
        SevenZipProcessBackend backend(executable); bool changed = false; QStringList checkpoints;
        if (failure == "changed") connect(&backend, &ArchiveBackend::extractionCheckpoint, &backend, [&](const QString &phase, const QString &path) {
            checkpoints << phase + ":" + path;
            if (!changed && phase == "seed" && QFileInfo(path).canonicalFilePath() == QFileInfo(output + '/' + target).canonicalFilePath()) { changed = write(output + '/' + target, QByteArray(26, 'x')); }
        });
        ArchiveRequest request; request.operation = ArchiveOperation::Extract; request.archive = archive; request.files = {alias}; request.outputDirectory = output; request.overwriteMode = "overwrite";
        auto result = run(backend, request);
        if (failure == "changed") QVERIFY2(changed, qPrintable(checkpoints.join('\n')));
        QVERIFY2(!result.success, qPrintable(result.message + result.details));
        if (failure == "changed") { QVERIFY(changed); QVERIFY2(result.message.contains("changed"), qPrintable(result.message)); QCOMPARE(read(output + '/' + target), QByteArray(26, 'x')); }
        else QCOMPARE(result.exitCode, 2);
        QCOMPARE(read(output + '/' + alias), QByteArray("previous alias retained")); QCOMPARE(read(protectedRoot + "/a-data.txt"), QByteArray("protected outside bytes"));
        request.operation = ArchiveOperation::Test; result = run(backend, request); QVERIFY2(result.success, qPrintable(result.message + result.details));
    }
    void existingLeafSymlink_data() {
        QTest::addColumn<QString>("mode"); QTest::newRow("replace") << QString("overwrite"); QTest::newRow("skip") << QString("skip");
    }
    void existingLeafSymlink() {
        QFETCH(QString, mode); QTemporaryDir temp; const auto source = temp.filePath("source"), output = temp.filePath("application"), protectedFile = temp.filePath("outside.txt");
        QVERIFY(write(source + "/payload.txt", "new archived payload")); QVERIFY(write(protectedFile, "protected existing user data")); QVERIFY(QDir().mkpath(output));
        QVERIFY(::symlink(QFile::encodeName(protectedFile).constData(), QFile::encodeName(output + "/payload.txt").constData()) == 0);
        const auto archive = temp.filePath("archive.7z"); const auto error = console({"a", "--", archive, "."}, source); QVERIFY2(error.isEmpty(), qPrintable(error));
        SevenZipProcessBackend backend(executable); ArchiveRequest request; request.operation = ArchiveOperation::Extract; request.archive = archive; request.outputDirectory = output; request.overwriteMode = mode;
        const auto result = run(backend, request); QVERIFY2(result.success, qPrintable(result.message + result.details));
        QCOMPARE(read(protectedFile), QByteArray("protected existing user data")); QCOMPARE(bool(S_ISLNK(stamp(output + "/payload.txt").st_mode)), mode == "skip");
        QCOMPARE(read(output + "/payload.txt"), mode == "skip" ? QByteArray("protected existing user data") : QByteArray("new archived payload"));
    }
    void unsafeLinkAndOutputParent() {
        QTemporaryDir temp; const auto source = temp.filePath("source"), output = temp.filePath("application"); QVERIFY(QDir().mkpath(source));
        QVERIFY(::symlink("../outside", QFile::encodeName(source + "/unsafe-link").constData()) == 0);
        const auto unsafe = temp.filePath("unsafe.7z"); auto error = console({"a", "-snl", "--", unsafe, "."}, source); QVERIFY2(error.isEmpty(), qPrintable(error));
        SevenZipProcessBackend backend(executable); ArchiveRequest request; request.operation = ArchiveOperation::Extract; request.archive = unsafe; request.outputDirectory = output; request.overwriteMode = "overwrite";
        auto result = run(backend, request); QVERIFY(!result.success); QCOMPARE(result.exitCode, 2); QVERIFY(result.message.contains("refused")); QVERIFY(!stamp(output + "/unsafe-link").st_ino);
        const auto safeSource = temp.filePath("safe-source"), protectedDirectory = temp.filePath("protected"); QVERIFY(write(safeSource + "/folder/payload.txt", "new archived payload")); QVERIFY(write(protectedDirectory + "/payload.txt", "protected data"));
        QVERIFY(::symlink(QFile::encodeName(protectedDirectory).constData(), QFile::encodeName(output + "/folder").constData()) == 0);
        const auto safe = temp.filePath("safe.7z"); error = console({"a", "--", safe, "."}, safeSource); QVERIFY2(error.isEmpty(), qPrintable(error));
        request.archive = safe; result = run(backend, request); QVERIFY(!result.success); QVERIFY(result.message.contains("refused")); QCOMPARE(read(protectedDirectory + "/payload.txt"), QByteArray("protected data"));
        request.operation = ArchiveOperation::Test; result = run(backend, request); QVERIFY2(result.success, qPrintable(result.message + result.details));
    }
};
int main(int argc, char **argv) { QCoreApplication app(argc, argv); if (argc < 2) return 2; executable = QString::fromLocal8Bit(argv[1]); ExtractionMetadataTests tests; --argc; ++argv; return QTest::qExec(&tests, argc, argv); }
#include "extract-metadata.moc"
