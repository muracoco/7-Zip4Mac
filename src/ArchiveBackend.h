#pragma once
#include "CompletionData.h"
#include "Overwrite.h"
#include "ProgressChannel.h"
#include <QObject>
#include <QProcess>
#include <QProcessEnvironment>
#include <QTemporaryDir>
#include <QStringDecoder>
#include <QMap>
#include <QFutureWatcher>
#include "OperationControl.h"
#include <memory>
#include <sys/stat.h>
class ExtractionSession;
struct ExtractionTaskReply {
    enum Kind { State, Publication, Overwrite, Close } kind = State;
    NativeFileStateRequest state;
    NativeExtractionRequest publication;
    NativeOverwriteRequest overwrite;
    FileSnapshot file;
    int code = 0;
    OverwriteAnswer answer = OverwriteAnswer::Cancel;
    QString error;
    bool retainStaging = false;
};

enum class ArchiveOperation { List, Add, Extract, Test, Delete, Rename, Hash, HashFile, HashArchive, CreateFolder, ReplaceFile, Comment };
struct ArchiveProperty {
    QString name, value;
    ArchiveProperty() = default;
    ArchiveProperty(QString name, QString value) : name(std::move(name)), value(std::move(value)) {}
    quint32 id = 0;
    quint16 type = 0;
    bool native = false, raw = false;
    QString number, listValue, fileTime, timeFraction;
    QByteArray sortData;
    QList<quint16> timePrecision;
    quint32 rawSize = 0, rawType = 0;
    qint64 retrievalError = 0;
};
using ArchivePropertyList = QList<ArchiveProperty>;
struct ArchivePropertyDefinition { quint32 id = 0; quint16 type = 0; QString name; bool raw = false; };
struct ArchiveFolderTotals {
    quint64 size = 0, packed = 0, folders = 0, files = 0;
    quint32 crc = 0;
    bool crcDefined = true;
    qint64 archiveIndex = -1;
};
struct ArchiveRowIdentity { int directory = -1, item = -1; };
struct ArchiveRow {
    ArchiveRowIdentity identity;
    qint64 archiveIndex = -1;
    int childDirectory = -1, alternateDirectory = -1;
    QString name, path, copyName;
    ArchivePropertyList properties, flatProperties;
};
struct ArchiveDirectory {
    int id = -1, parent = -1, alternateDirectory = -1;
    QString path;
    bool alternateStreams = false;
    ArchiveFolderTotals totals;
    ArchivePropertyList properties;
    QList<ArchiveRow> children;
};
struct ArchiveMetadata {
    bool native = false;
    QString sourceSnapshot;
    bool selectionResolved = false;
    QList<quint32> selectedIndices;
    QString selectedPath;
    QList<ArchivePropertyDefinition> schema, rawSchema;
    QMap<QString, ArchiveFolderTotals> folders;
    QList<ArchiveDirectory> directories;
    ArchivePropertyList failedProperties;
};
struct ArchiveLayer {
    ArchivePropertyList properties;
    // Properties of the main subfile which opens the following inner layer.
    ArchivePropertyList childProperties;
};
struct ArchiveEntry {
    QString path, modified, attributes, crc, method, extension;
    quint64 size = 0, packed = 0;
    bool directory = false, encrypted = false, link = false, auxiliary = false, generated = false;
    QMap<QString, QString> properties;
    ArchivePropertyList orderedProperties;
    qint64 archiveIndex = -1;
    qint64 parentIndex = -1;
    quint32 parentType = 0;
};
struct ArchiveRequest {
    ArchiveOperation operation = ArchiveOperation::List;
    QString archive, workingDirectory, outputDirectory, password, temporaryDirectory;
    QStringList files;
    // Native Agent row identity is authoritative when provided. Paths remain
    // display/legacy fields and never select same-name siblings in this mode.
    struct SelectionItem { ArchiveRowIdentity identity; qint64 archiveIndex = -1; QString name; };
    QList<SelectionItem> selection;
    QString selectionSnapshot, selectionType;
    QString selectionBasePath;
    bool selectionAlternateStreams = false;
    int selectionDirectory = -1, selectionEntryCount = 0;
    bool selectionFlat = false, selectionPreservePaths = false;
    QString format = "7z", method = "LZMA2", dictionary = "32m", wordSize = "32", solid = "4g";
    int level = 5, threads = 2, memoryLimitGB = 0;
    bool methodAutomatic = false;
    QString updateMode = "add", pathMode = "relative", overwriteMode = "ask", volume;
    QString encryptionMethod = "AES256";
    bool encryptNames = false;
    bool eliminateRoot = false;
    QString parameters, compressionMemory;
    int timestampPrecision = -1, modificationTime = -1, creationTime = -1, accessTime = -1;
    bool latestArchiveTime = false, preserveAccessTime = false, storeSymbolicLinks = true, storeHardLinks = false;
    bool latestArchiveTimeSpecified = false;
    bool deleteAfter = false;
    // File Manager CopyFrom: handler defaults and logical archive folder.
    // Unlike AddDialog, no saved compression properties are applied.
    bool useArchiveDefaults = false;
    QString archivePrefix;
    QString hashMethod = "SHA256";
    bool hashTest = false;
    quint64 totalSizeHint = 0;
    QString replacementSource;
    QString comment;
    // Official Open As type, separate from the writable compression format.
    // Empty: normal; '*': one level; '#'/ '#:e': parser; explicit handler names.
    QString readMode;
};
struct ArchiveResult {
    ArchiveOperation operation = ArchiveOperation::List;
    QString target, message, details;
    int exitCode = -1;
    bool success = false, cancelled = false;
    QList<ArchiveEntry> entries;
    QString archiveType;
    QMap<QString, QString> properties;
    bool passwordRequired = false;
    bool testInside = false;
    CompletionData completion;
    QList<ArchiveLayer> layers; // Console order: outer to inner.
    ArchiveMetadata metadata;
    // Guarded mutation mapping, old handler index -> new handler index.
    QString previousSnapshot;
    QList<qint64> itemIndexMap;
    QList<int> directoryIndexMap;
    QList<QList<ArchiveRowIdentity>> rowIdentityMap;
};
Q_DECLARE_METATYPE(ArchiveResult)

class ArchiveBackend : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    virtual void start(const ArchiveRequest &) = 0;
    virtual void cancel() = 0;
    virtual bool busy() const = 0;
    virtual bool canPause() const = 0;
    virtual bool setPaused(bool paused) = 0;
signals:
    void progress(int percent, quint64 processed, quint64 total, QString current);
    void progressDetails(ArchiveProgress progress);
    void operationError(ArchiveOperationError error);
    void output(QString text);
    void finished(ArchiveResult result);
    void overwriteRequested(OverwriteConflict conflict);
    void extractionCheckpoint(QString phase, QString target);
    void mutationCheckpoint(QString phase, QString target);
    void pausedChanged(bool paused);
    void pauseAvailabilityChanged(bool available);
};

class SevenZipProcessBackend final : public ArchiveBackend {
    Q_OBJECT
public:
    QStringList temporaryPaths() const;
    explicit SevenZipProcessBackend(QString executable, QObject *parent = nullptr);
    ~SevenZipProcessBackend() override;
    void start(const ArchiveRequest &) override;
    void cancel() override;
    bool busy() const override { return active; }
    bool canPause() const override;
    bool setPaused(bool paused) override;
    bool setBackground(bool background);
    bool resolveOverwrite(quint64 id, OverwriteAnswer answer);
    static bool safeArchivePath(const QString &path);
    static QList<ArchiveEntry> parseListing(const QString &, QString *error = nullptr);
    static QList<ArchiveLayer> parseArchiveLayers(const QString &header);
private:
    void launch(QStringList arguments, QString working = {});
    void launchCommentHelper(QStringList arguments);
    bool applyZipComments(QList<ArchiveEntry> &items);
    void readOutput();
    void processFinished(int, QProcess::ExitStatus);
    void complete(bool success, int exitCode, QString message = {});
    void beginExtract();
    void finishListing();
    bool prepareNativeSelection(QProcessEnvironment &environment);
    bool readNativeListing(QList<ArchiveEntry> &items);
    void commitExtract();
    void finishExtract(QString error = {});
    void extractionWorkerFinished();
    void closeExtractionSession();
    void verifySources(QList<ArchiveEntry> items);
    void beginArchiveMutation();
    void mutationWorkerFinished();
    void mutationProcessFinished();
    bool mutationListing(QList<ArchiveEntry> &items);
    void commitArchiveMutation();
    void validateArchiveMutation();
    void verifyNextReplacement();
    enum class MutationPhase { None, Copy, BeforeList, Update, AfterList, VerifyReplacement, VerifyReplacementDigest, VerifyEmptyStream, Commit };
    MutationPhase mutationPhase = MutationPhase::None;
    bool mutationInstalled = false;
    QString mutationCandidate, mutationInput;
    struct stat mutationOriginal{}, replacementOriginal{};
    QByteArray mutationDigest, replacementDigest;
    quint64 replacementSize = 0, mutationPrefixSize = 0;
    QList<ArchiveEntry> mutationOriginalEntries;
    ArchiveMetadata mutationOriginalMetadata;
    QList<quint32> mutationSelectedIndices;
    QString mutationSelectedPath;
    std::unique_ptr<QTemporaryDir> extractMetadataStaging;
    QList<qint64> mutationItemIndexMap;
    QList<quint32> mutationVerificationCandidates;
    qsizetype mutationVerificationCursor = 0;
    ArchiveRequest mutationVerificationSelection;
    QString program;
    QProcess process;
    bool backgroundMode = false;
    ProgressChannel progressChannel;
    bool operationHadNativeError = false;
    ArchiveRequest request;
    QString textOutput, textErrors, currentFile;
    QString passwordOutputTail, passwordErrorTail;
    bool passwordDiagnostic = false;
    QString forcedReadFormat, lastProcessWorking;
    QStringList lastProcessArguments;
    QString archiveType;
    QMap<QString, QString> archiveProperties;
    QList<ArchiveLayer> archiveLayers;
    ArchiveMetadata archiveMetadata;
    std::unique_ptr<QTemporaryDir> completionStaging;
    std::unique_ptr<QTemporaryDir> metadataStaging;
    std::unique_ptr<QTemporaryDir> selectionStaging;
    std::unique_ptr<QTemporaryDir> transferInputStaging;
    bool resolvingNames = false;
    bool commentHelperActive = false, readingZipComments = false, zipCommentsRead = false;
    QStringDecoder stdoutDecoder{QStringDecoder::Utf8}, stderrDecoder{QStringDecoder::Utf8};
    std::unique_ptr<QTemporaryDir> staging;
    QList<ArchiveEntry> entries;
    QString extractRootPrefix;
    int extractExitCode = 0;
    QString extractResultMessage;
    QMap<QString, QString> extractTargets, extractHardTargets;
    std::shared_ptr<ExtractionSession> extractionSession;
    QFutureWatcher<ExtractionTaskReply> extractionWatcher;
    std::optional<std::pair<int, QProcess::ExitStatus>> extractionExit;
    struct AccessStamp { QString path; qint64 seconds; long nanos; };
    QList<AccessStamp> sourceAccessTimes;
    bool checkingSources = false;
    bool active = false, cancelled = false, preflight = false, creating = false;
    bool paused = false;
    quint64 totalSize = 0;
    QFutureWatcher<QString> mergeWatcher;
    std::shared_ptr<OperationControl> stopFlag;
    OverwriteBroker overwriteBroker;
};
QString operationName(ArchiveOperation);
