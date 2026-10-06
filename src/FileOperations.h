#pragma once
#include <QObject>
#include <QFutureWatcher>
#include "OperationControl.h"
#include "Overwrite.h"
#include "FileInstall.h"
#include <memory>
#include <functional>
#include <sys/stat.h>
struct CombinePlan {
    QStringList volumes;
    QString outputName;
    quint64 totalSize = 0;
    QString error;
};
enum class LinkType { Hard, SymbolicFile, SymbolicDirectory };
struct SymbolicLinkSnapshot {
    QString path, folder, error;
    QByteArray name, target;
    struct stat directory{}, link{};
    bool symbolic = false;
};
enum class LinkEditPhase { Prepared, Validated, Exchanged, Rollback };
using LinkEditObserver = std::function<void(LinkEditPhase, const QString &)>;
class FileOperations : public QObject {
    Q_OBJECT
public:
    explicit FileOperations(QObject *parent = nullptr);
    ~FileOperations() override;
    void transfer(QStringList sources, QString destination, bool move);
    void trash(QStringList sources);
    void rename(QString source, QString destination);
    void create(QString folder, QString name, bool directory);
    static QString creationPath(const QString &folder, const QString &name);
    void trashSnapshots(QList<FileSnapshot> sources);
    void split(QString source, QString destination, QList<quint64> sizes);
    void combine(QString firstVolume, QString destination);
    static QList<quint64> parseVolumeSizes(QString text, QString *error = nullptr);
    static quint64 volumeCount(quint64 size, const QList<quint64> &sizes);
    static CombinePlan inspectVolumes(QString firstVolume);
    static QString createLink(QString from, QString target, LinkType type);
    static SymbolicLinkSnapshot inspectSymbolicLink(QString path);
    // Optional synchronous phase observer supports diagnostic fault injection.
    // Production callers leave it empty; there is no environment-based hook.
    static QString editSymbolicLink(const SymbolicLinkSnapshot &, QString target, LinkType type, const LinkEditObserver &observer = {});
    void cancel();
    bool resolveOverwrite(quint64 id, OverwriteAnswer answer);
    bool setPaused(bool paused);
    bool busy() const { return active; }
signals:
    void transferDestinationPrepared(QString destination);
    void finished(QString error);
    void trashed(QString source, QString trashPath);
    void progress(QString path);
    void byteProgress(quint64 processed, quint64 total, QString path);
    void pausedChanged(bool paused);
    void pauseAvailabilityChanged(bool available);
    void overwriteRequested(OverwriteConflict conflict);
private:
    QFutureWatcher<QString> watcher;
    bool active = false;
    std::shared_ptr<OperationControl> stop;
    OverwriteBroker overwriteBroker;
};
