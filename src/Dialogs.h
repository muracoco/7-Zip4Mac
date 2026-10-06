#pragma once
#include "ArchiveBackend.h"
#include "CompressionMath.h"
#include "ProgressPresentation.h"
#include <QDialog>
#include <QComboBox>
#include <QLineEdit>
#include <QCheckBox>
#include <QLabel>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QElapsedTimer>
#include <QTimer>
#include <QFormLayout>
#include <QMap>
#include <QTreeWidget>
#include <QPointer>

class AddDialog : public QDialog {
    Q_OBJECT
public:
    explicit AddDialog(QString archive, QWidget *parent = nullptr);
    ArchiveRequest options() const;
    void accept() override;
private:
    void formatChanged();
    void methodChanged();
    void levelChanged();
    void rebuildMethods();
    void saveOptionsInMemory();
    void dictionaryChanged();
    void updateAutomatic();
    void updateOptionsSummary();
    CompressionMath::Input mathInput() const;
    QString selectedMethod() const;
    QLineEdit *archive, *password, *reenter, *volume, *parameters;
    QComboBox *format, *level, *method, *dictionary, *word, *solid, *threads, *update, *encryption;
    QCheckBox *encryptNames, *showPassword;
    QLabel *wordLabel, *compressMemory, *decompressMemory, *memoryLabel, *decompressMemoryLabel;
    bool updatingCompression = false;
    QString currentFormat;
    QMap<QString, QVariantMap> formatDrafts;
    QWidget *threadControls, *memoryControls;
    QComboBox *paths, *memory;
    QCheckBox *deleteAfter;
    ArchiveRequest advanced;
};
class ExtractDialog : public QDialog {
    Q_OBJECT
public:
    explicit ExtractDialog(QString destination, bool selected, QWidget *parent = nullptr);
    ArchiveRequest options() const;
    bool selectedOnly() const;
    void accept() override;
private:
    QLineEdit *destination, *password, *subfolder;
    QComboBox *paths, *overwrite, *selection;
    QCheckBox *eliminate, *nameEnabled, *showPassword;
};
class ProgressDialog : public QDialog {
    Q_OBJECT
public:
    explicit ProgressDialog(QString operation, QString target, QWidget *parent = nullptr);
    ~ProgressDialog() override;
    void update(int percent, quint64 processed, quint64 total, QString file, bool estimated = true);
    void updateDetails(const ArchiveProgress &progress);
    void append(QString text);
    void reportError(ArchiveOperationError error);
    void finish(const ArchiveResult &result);
    void finishForRetry();
    void finishFile(QString error);
    void setPauseAvailable(bool available);
    void setPaused(bool paused);
    void setBackground(bool background);
signals:
    void cancelRequested();
    void pauseRequested(bool paused);
    void backgroundRequested(bool background);
protected:
    void reject() override;
    bool event(QEvent *event) override;
private slots:
    void refreshPresentation();
private:
    struct Completion {
        QString error, ok, title;
        bool cancelled = false;
        std::optional<ChecksumPresentation> checksum;
    };
    void complete(Completion completion);
    void requestCancel();
    qint64 activeMilliseconds() const;
    QProgressBar *bar;
    QLabel *elapsed, *processedSize, *totalSize, *current, *errors, *remaining, *speed, *status;
    QLabel *files, *filesTotal, *packedSize, *ratio;
    QTreeWidget *messages;
    QPlainTextEdit *operationLog;
    QPushButton *cancelButton, *pauseButton, *backgroundButton;
    QElapsedTimer clock, pauseClock;
    QTimer timer;
    bool running = true;
    bool paused = false;
    qint64 pausedMilliseconds = 0;
    QString operationTitle;
    QString target;
    OfficialProgressState presentation;
    ArchiveProgress lastProgress;
    bool presentationReady = false, nativePresentation = false, background = false;
    bool confirmingCancel = false, cancelWasRequested = false;
    std::optional<Completion> deferredCompletion;
    QPointer<QWidget> titleWindow;
    QString appliedTitlePrefix;
    bool titleRestored = false;
    qsizetype messageRows = 0;
    quint64 messageColumns = 0;
    int errorCount = 0;
};
