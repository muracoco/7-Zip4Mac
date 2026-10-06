// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include <QDialog>
#include <QElapsedTimer>
#include <QProcess>
#include <QTimer>
#include <QStringConverter>
#include "ProgressChannel.h"
class QComboBox;
class QLabel;
class QPlainTextEdit;
class QPushButton;
struct BenchmarkResult {
    quint64 compressSpeed = 0, compressUsage = 0, compressRatingUsage = 0, compressRating = 0;
    quint64 decompressSpeed = 0, decompressUsage = 0, decompressRatingUsage = 0, decompressRating = 0;
};
struct BenchmarkValues {
    bool defined = false;
    quint64 speed = 0, usage = 0, ratingUsage = 0, rating = 0, size = 0;
    QStringList display; // Size, CPU Usage, Speed, Rating / Usage, Rating.
};
struct BenchmarkSnapshot {
    BenchmarkValues encCurrent, decCurrent, encResult, decResult, total;
    int completed = 0;
    bool finished = false;
    QString log;
    QJsonObject system;
};
class BenchmarkRunner : public QObject {
    Q_OBJECT
public:
    explicit BenchmarkRunner(QString executable, QObject *parent = nullptr);
    ~BenchmarkRunner() override;
    void start(int dictionaryLog, int threads, int passes);
    void startDictionary(quint64 dictionary, int threads, int passes);
    void stop();
    bool busy() const { return running; }
    bool stopped() const { return stopping; }
    static quint64 memoryUsage(quint64 dictionary, int threads);
signals:
    void output(QString text);
    void result(BenchmarkResult result, int completedPasses);
    void snapshot(BenchmarkSnapshot snapshot);
    void finished(QString error);
private:
    void launch();
    void consume();
    void consumeSnapshot(const QJsonObject &object);
    QProcess process;
    ProgressChannel channel{&process, this};
    QString executable, buffer;
    quint64 dictionaryBytes = quint64(1) << 25;
    int threads = 1, passes = 1, completed = 0;
    bool stopping = false, sawResult = false, running = false;
    quint64 generation = 0;
    QStringDecoder decoder{QStringDecoder::Utf8};
};
Q_DECLARE_METATYPE(BenchmarkResult)
Q_DECLARE_METATYPE(BenchmarkSnapshot)
class BenchmarkDialog : public QDialog {
    Q_OBJECT
public:
    explicit BenchmarkDialog(QString executable, QWidget *parent = nullptr);
protected:
    void reject() override;
private:
    void restart();
    void launch();
    BenchmarkRunner runner;
    QComboBox *dictionary, *threads, *passes;
    QLabel *memory, *elapsed, *passCount, *totalRating, *totalUsage, *totalRatingUsage, *status;
    QList<QLabel *> currentValues, averageValues;
    QList<QLabel *> currentSizes, averageSizes;
    QPlainTextEdit *log;
    QPushButton *stopButton;
    QTimer timer;
    QElapsedTimer clock;
    bool pendingRestart = false, closing = false;
};
