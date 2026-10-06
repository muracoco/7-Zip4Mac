// SPDX-License-Identifier: LGPL-3.0-or-later
#include "Benchmark.h"
#include "UiLanguage.h"
#include "Help.h"
#include <QComboBox>
#include <QFileInfo>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QSettings>
#include <QThread>
#include <QVBoxLayout>
#include <sys/sysctl.h>
#include <cstddef>
#include <cstdint>
namespace {
using UInt64 = uint64_t; using UInt32 = uint32_t;
#include "upstream/BenchmarkMemory.inc"
bool number(const QJsonObject &object, const char *key, quint64 &value) {
    const auto data = object.value(key);
    static const QRegularExpression decimal("^[0-9]{1,20}$");
    bool valid = false; value = data.toString().toULongLong(&valid);
    return data.isString() && decimal.match(data.toString()).hasMatch() && valid;
}
bool values(const QJsonValue &json, BenchmarkValues &value) {
    if (!json.isObject()) return false;
    const auto object = json.toObject();
    if (!object.value("defined").isBool()) return false;
    value.defined = object.value("defined").toBool();
    if (!value.defined) return true;
    if (!number(object, "speed", value.speed) || !number(object, "usage", value.usage) ||
        !number(object, "rating", value.rating) || !number(object, "rpu", value.ratingUsage) ||
        !number(object, "size", value.size)) return false;
    for (const char *key : {"sizeText", "usageText", "speedText", "rpuText", "ratingText"}) {
        if (!object.value(key).isString()) return false;
        value.display << object.value(key).toString();
    }
    return true;
}
QString sizeText(quint64 bytes) {
    const unsigned shift = bytes >= (quint64(1) << 31) ? 30 : bytes >= (1 << 21) ? 20 : 10;
    return QString::number(bytes >> shift) + (shift == 30 ? " GB" : shift == 20 ? " MB" : " KB");
}
quint64 systemRam() { quint64 ram = 0; size_t size = sizeof(ram); return ::sysctlbyname("hw.memsize", &ram, &size, nullptr, 0) == 0 ? ram : 0; }
}
BenchmarkRunner::BenchmarkRunner(QString program, QObject *parent) : QObject(parent), executable(program) {
    qRegisterMetaType<BenchmarkResult>(); qRegisterMetaType<BenchmarkSnapshot>();
    process.setProcessChannelMode(QProcess::MergedChannels);
    connect(&process, &QProcess::readyReadStandardOutput, this, &BenchmarkRunner::consume);
    connect(&channel, &ProgressChannel::benchmarkSnapshot, this, &BenchmarkRunner::consumeSnapshot);
    connect(&process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) { running = false; emit finished("Cannot start benchmark: " + process.errorString()); }
    });
    connect(&process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this, [this](int code, QProcess::ExitStatus exit) {
        const auto finishedGeneration = generation; consume();
        if (generation != finishedGeneration) return;
        running = false;
        if (stopping) { emit finished({}); return; }
        if (exit != QProcess::NormalExit || code || !sawResult || completed != passes) {
            emit finished("Benchmark failed. 7-Zip exit code: " + QString::number(code) +
                (!sawResult || completed != passes ? "\nIncomplete benchmark results. See the log." : QString()) +
                (buffer.isEmpty() ? QString() : "\n" + buffer.right(8192)));
            return;
        }
        emit finished({});
    });
}
BenchmarkRunner::~BenchmarkRunner() {
    disconnect(this, nullptr, nullptr, nullptr); disconnect(&process, nullptr, this, nullptr); disconnect(&channel, nullptr, this, nullptr);
    if (process.state() != QProcess::NotRunning) { process.terminate(); if (!process.waitForFinished(1000)) { process.kill(); process.waitForFinished(1000); } }
}
void BenchmarkRunner::start(int log, int count, int iterations) {
    if (log < 18 || log > 32) { if (!busy()) emit finished("Incorrect benchmark parameters."); return; }
    startDictionary(quint64(1) << log, count, iterations);
}
void BenchmarkRunner::startDictionary(quint64 bytes, int count, int iterations) {
    if (busy()) return;
    if (bytes < (quint64(1) << 18) || bytes > (quint64(1) << 32) || (bytes & 1023) || count < 1 || count > (1 << 14) || iterations < 1) {
        emit finished("Incorrect benchmark parameters."); return;
    }
    ++generation; dictionaryBytes = bytes; threads = count; passes = iterations;
    completed = 0; stopping = false; running = true; launch();
}
void BenchmarkRunner::launch() {
    buffer.clear(); decoder.resetState(); sawResult = false;
    auto environment = QProcessEnvironment::systemEnvironment();
    for (const char *key : {"SEVENZIP_PORT_METADATA_PATH", "SEVENZIP_PORT_COPY_FROM", "SEVENZIP_PORT_ADD_PREFIX", "SEVENZIP_PORT_BENCHMARK_DICT"}) environment.remove(key);
    environment.insert("SEVENZIP_PORT_BENCHMARK_DICT", QString::number(dictionaryBytes));
    process.setProcessEnvironment(environment);
    if (!channel.prepare(true)) { running = false; emit finished("Cannot create benchmark progress channel."); return; }
    const auto helper = QFileInfo(executable).absolutePath() + "/7zz-progress";
    // Bundled GUI benchmarks need the native callback adapter, including for
    // 3/2 dictionaries. A console sweep would silently round the dictionary.
    process.start(QFileInfo(executable).isExecutable() ? helper : executable,
        {"b", QString::number(passes), "-md" + QString::number(dictionaryBytes >> 10) + "k", "-mmt=" + QString::number(threads), "-sccUTF-8"});
}
void BenchmarkRunner::stop() {
    if (!running || stopping) return;
    ++generation; stopping = true;
    if (process.state() != QProcess::NotRunning) {
        process.terminate(); const auto pid = process.processId(); const auto stoppedGeneration = generation;
        QTimer::singleShot(1500, this, [this, pid, stoppedGeneration] {
            if (generation == stoppedGeneration && process.processId() == pid && process.state() != QProcess::NotRunning) process.kill();
        });
    } else { running = false; emit finished({}); }
}
void BenchmarkRunner::consume() {
    const QString chunk = decoder(process.readAllStandardOutput()); if (chunk.isEmpty()) return;
    buffer += chunk; if (buffer.size() > 65536) buffer.remove(0, buffer.size() - 65536);
    emit output(chunk);
}
void BenchmarkRunner::consumeSnapshot(const QJsonObject &object) {
    if (!running || stopping) return;
    BenchmarkSnapshot value; quint64 count = 0;
    if (!number(object, "completed", count) || count > quint64(passes) || count < quint64(completed) ||
        !object.value("finished").isBool() || !object.value("log").isString() ||
        !values(object.value("encCurrent"), value.encCurrent) || !values(object.value("decCurrent"), value.decCurrent) ||
        !values(object.value("encResult"), value.encResult) || !values(object.value("decResult"), value.decResult) ||
        !values(object.value("total"), value.total)) return;
    value.completed = int(count); value.finished = object.value("finished").toBool(); value.log = object.value("log").toString();
    if (object.contains("system")) {
        if (!object.value("system").isObject()) return;
        value.system = object.value("system").toObject();
        for (const char *key : {"cpu", "features", "first", "second", "hardware", "version"}) if (!value.system.value(key).isString()) return;
    }
    if (value.finished && value.completed != passes) return;
    if (count && (!value.encResult.defined || !value.decResult.defined)) return;
    const bool newPass = value.completed > completed;
    completed = value.completed; sawResult = sawResult || newPass;
    emit snapshot(value);
    if (newPass) {
        BenchmarkResult resultValue;
        resultValue.compressSpeed = value.encCurrent.speed; resultValue.compressUsage = value.encCurrent.usage;
        resultValue.compressRatingUsage = value.encCurrent.ratingUsage; resultValue.compressRating = value.encCurrent.rating;
        resultValue.decompressSpeed = value.decCurrent.speed; resultValue.decompressUsage = value.decCurrent.usage;
        resultValue.decompressRatingUsage = value.decCurrent.ratingUsage; resultValue.decompressRating = value.decCurrent.rating;
        emit result(resultValue, completed);
    }
}
quint64 BenchmarkRunner::memoryUsage(quint64 dict, int threads) {
    return GetBenchMemoryUsage(UInt32(qMax(1, threads)), 5, dict, false);
}
BenchmarkDialog::BenchmarkDialog(QString executable, QWidget *parent) : QDialog(parent), runner(executable, this) {
    setObjectName("benchmarkDialog"); setWindowTitle("Benchmark"); resize(980, 480);
    auto outer = new QVBoxLayout(this); auto body = new QHBoxLayout; auto main = new QVBoxLayout;
    auto controls = new QGridLayout;
    dictionary = new QComboBox(this); dictionary->setObjectName("benchmarkDictionary");
    const QSettings settings; const int hardware = qMax(1, QThread::idealThreadCount());
    int selectedThreads = settings.value("Benchmark/Threads", hardware).toInt(); selectedThreads &= ~1; selectedThreads = qBound(1, selectedThreads, 1 << 14);
    threads = new QComboBox(this); threads->setObjectName("benchmarkThreads");
    for (int n = 1; n <= hardware * 2; n += n == 1 ? 1 : 2) {
        threads->addItem(QString::number(n), n);
        const int next = n + (n < 2 ? 1 : 2);
        if (n <= selectedThreads && (selectedThreads < next || next > hardware * 2) && n != selectedThreads) threads->addItem(QString::number(selectedThreads), selectedThreads);
    }
    threads->setCurrentIndex(qMax(0, threads->findData(selectedThreads)));
    const auto ram = systemRam();
    const auto maximum = quint64(1) << (22 + sizeof(size_t) / 4 * 5);
    quint64 selectedDictionary = settings.value("Benchmark/DictionaryBytes", quint64(1) << qBound(18, settings.value("Benchmark/DictionaryLog", 25).toInt(), 32)).toULongLong();
    if (!settings.contains("Benchmark/DictionaryBytes") && !settings.contains("Benchmark/DictionaryLog") && ram) {
        while (selectedDictionary > (1 << 18) && BenchmarkRunner::memoryUsage(selectedDictionary, selectedThreads) > ram / 16 * 15) selectedDictionary >>= 1;
    }
    selectedDictionary = qBound<quint64>(1 << 18, selectedDictionary, maximum);
    int dictionaryIndex = 0;
    for (unsigned i = 34; i <= 62; ++i) {
        const auto bytes = quint64(2 + (i & 1)) << (i / 2);
        dictionary->addItem(sizeText(bytes), QVariant::fromValue(bytes));
        if (bytes <= selectedDictionary) dictionaryIndex = dictionary->count() - 1;
        if (bytes >= maximum) break;
    }
    dictionary->setCurrentIndex(dictionaryIndex);
    auto dictLabel = new QLabel("&Dictionary size:", this); dictLabel->setBuddy(dictionary);
    auto threadsLabel = new QLabel("&Number of CPU threads:", this); threadsLabel->setBuddy(threads);
    memory = new QLabel(this); memory->setObjectName("benchmarkMemory");
    controls->addWidget(dictLabel, 0, 0); controls->addWidget(dictionary, 0, 1); controls->addWidget(new QLabel("Memory usage:", this), 0, 2); controls->addWidget(memory, 1, 2);
    auto hardwareLabel = new QLabel("/ " + QString::number(hardware), this); hardwareLabel->setObjectName("benchmarkHardware"); hardwareLabel->setProperty("uiLiteral", true);
    controls->addWidget(threadsLabel, 2, 0); controls->addWidget(threads, 2, 1); controls->addWidget(hardwareLabel, 2, 2);
    auto restartButton = new QPushButton("&Restart", this); restartButton->setObjectName("benchmarkRestart");
    stopButton = new QPushButton("&Stop", this); stopButton->setObjectName("benchmarkStop");
    controls->addWidget(restartButton, 0, 3); controls->addWidget(stopButton, 1, 3); main->addLayout(controls);
    for (int side = 0; side < 2; ++side) {
        auto group = new QGroupBox(side ? "Decompressing" : "Compressing", this); auto table = new QGridLayout(group);
        const QStringList names{"Size", "CPU Usage", "Speed", "Rating / Usage", "Rating"};
        for (int n = 0; n < names.size(); ++n) { auto label = new QLabel(names[n], group); label->setAlignment(Qt::AlignRight); table->addWidget(label, 0, n + 1); table->setColumnMinimumWidth(n + 1, n == 3 ? 95 : 76); }
        table->addWidget(new QLabel("Current", group), 1, 0); table->addWidget(new QLabel("Resulting", group), 2, 0);
        auto currentSize = new QLabel(group), resultSize = new QLabel(group);
        currentSize->setObjectName("benchmarkCurrentSize" + QString::number(side)); resultSize->setObjectName("benchmarkResultSize" + QString::number(side));
        currentSizes << currentSize; averageSizes << resultSize; table->addWidget(currentSize, 1, 1); table->addWidget(resultSize, 2, 1);
        for (int n = 0; n < 4; ++n) {
            auto current = new QLabel(group), average = new QLabel(group);
            current->setObjectName("benchmarkCurrent" + QString::number(side * 4 + n)); average->setObjectName("benchmarkResult" + QString::number(side * 4 + n));
            current->setAlignment(Qt::AlignRight); average->setAlignment(Qt::AlignRight); currentValues << current; averageValues << average;
            const int column = n == 0 ? 3 : n == 1 ? 2 : n + 2;
            table->addWidget(current, 1, column); table->addWidget(average, 2, column);
        }
        currentSize->setAlignment(Qt::AlignRight); resultSize->setAlignment(Qt::AlignRight); main->addWidget(group);
    }
    auto totalsForm = new QFormLayout;
    elapsed = new QLabel("0 s", this); elapsed->setObjectName("benchmarkElapsed");
    totalRating = new QLabel(this); totalRating->setObjectName("benchmarkTotalRating");
    totalUsage = new QLabel(this); totalUsage->setObjectName("benchmarkTotalUsage");
    totalRatingUsage = new QLabel(this); totalRatingUsage->setObjectName("benchmarkTotalRatingUsage");
    passCount = new QLabel("0 /", this); passes = new QComboBox(this); passes->setObjectName("benchmarkPasses");
    const int selectedPasses = qMax(1, settings.value("Benchmark/Passes", 10).toInt());
    for (int n : {1, 2, 5, 10, 100, 1000, 10000, 100000, 1000000, 10000000}) {
        passes->addItem(QString::number(n), n);
        const int next = n < 2 ? 2 : n < 5 ? 5 : n < 10 ? 10 : n * 10;
        if (n <= selectedPasses && (selectedPasses < next || n == 10000000) && n != selectedPasses) passes->addItem(QString::number(selectedPasses), selectedPasses);
    }
    passes->setCurrentIndex(qMax(0, passes->findData(selectedPasses)));
    auto passRow = new QHBoxLayout; passRow->addWidget(passCount); passRow->addWidget(passes); passRow->addStretch();
    auto totalsGroup = new QGroupBox("Total Rating", this); auto totalsRow = new QHBoxLayout(totalsGroup); totalsRow->addWidget(totalUsage); totalsRow->addWidget(totalRatingUsage); totalsRow->addWidget(totalRating);
    totalsForm->addRow("Elapsed time:", elapsed); totalsForm->addRow("Passes:", passRow);
    auto summary = new QHBoxLayout; summary->addLayout(totalsForm); summary->addWidget(totalsGroup); main->addLayout(summary);
    QMap<QString, QLabel *> systemLabels;
    for (const char *key : {"cpu", "features", "first", "second", "version"}) {
        auto label = new QLabel(this); label->setObjectName("benchmarkSystem" + QString::fromLatin1(key)); label->setProperty("uiLiteral", true); label->setTextFormat(Qt::PlainText); label->setWordWrap(true);
        systemLabels.insert(QString::fromLatin1(key), label); main->addWidget(label);
    }
    status = new QLabel(this); status->setObjectName("benchmarkStatus"); main->addWidget(status); main->addStretch();
    log = new QPlainTextEdit(this); log->setObjectName("benchmarkLog"); log->setReadOnly(true); log->setMinimumWidth(245); log->setMaximumBlockCount(1200);
    body->addLayout(main, 3); body->addWidget(log, 1); outer->addLayout(body);
    auto buttons = new QHBoxLayout; buttons->addStretch();
    auto help = new QPushButton("&Help", this); help->setObjectName("benchmarkHelp");
    auto cancel = new QPushButton("Cancel", this); cancel->setObjectName("benchmarkCancel");
    buttons->addWidget(help); buttons->addWidget(cancel); outer->addLayout(buttons);
    connect(help, &QPushButton::clicked, this, [this] { Help::show(this, "fm/benchmark.htm"); });
    connect(restartButton, &QPushButton::clicked, this, &BenchmarkDialog::restart);
    connect(stopButton, &QPushButton::clicked, this, [this] { pendingRestart = false; runner.stop(); stopButton->setEnabled(false); if (!runner.busy()) { timer.stop(); status->setText("Stopped"); } });
    connect(cancel, &QPushButton::clicked, this, &BenchmarkDialog::reject);
    connect(&runner, &BenchmarkRunner::output, log, [this](QString text) { if (log->toPlainText().isEmpty()) log->insertPlainText(text); });
    connect(&runner, &BenchmarkRunner::snapshot, this, [this, hardwareLabel, systemLabels](const BenchmarkSnapshot &value) {
        if (!value.system.isEmpty()) {
            hardwareLabel->setText(value.system.value("hardware").toString());
            for (auto it = systemLabels.cbegin(); it != systemLabels.cend(); ++it) it.value()->setText(value.system.value(it.key()).toString());
            if (value.system.value("first") == value.system.value("second")) systemLabels.value("second")->clear();
        }
        passCount->setText(QString::number(value.completed) + " /");
        auto show = [this](const BenchmarkValues &data, int side, bool resulting) {
            if (!data.defined) return;
            (resulting ? averageSizes : currentSizes)[side]->setText(data.display[0]);
            auto &labels = resulting ? averageValues : currentValues;
            const int fields[]{2, 1, 3, 4};
            for (int n = 0; n < 4; ++n) labels[side * 4 + n]->setText(data.display[fields[n]]);
        };
        show(value.encCurrent, 0, false); show(value.decCurrent, 1, false); show(value.encResult, 0, true); show(value.decResult, 1, true);
        if (value.finished && value.total.defined) { totalUsage->setText(value.total.display[1]); totalRatingUsage->setText(value.total.display[3]); totalRating->setText(value.total.display[4]); }
        if (!value.log.isEmpty() && log->toPlainText() != value.log) log->setPlainText(value.log);
    });
    connect(&runner, &BenchmarkRunner::finished, this, [this](QString error) {
        timer.stop(); stopButton->setEnabled(false);
        const auto milliseconds = clock.elapsed(); elapsed->setText(QString::number(milliseconds / 1000) + '.' + QString::number(milliseconds % 1000).rightJustified(3, '0') + " s");
        if (closing) { QDialog::reject(); return; }
        if (pendingRestart) { pendingRestart = false; launch(); return; }
        status->setText(error.isEmpty() ? (runner.stopped() ? "Stopped" : "Finished") : error);
    });
    timer.setInterval(1000); connect(&timer, &QTimer::timeout, this, [this] { elapsed->setText(QString::number(clock.elapsed() / 1000) + " s"); });
    for (auto combo : {dictionary, threads, passes}) connect(combo, &QComboBox::currentIndexChanged, this, &BenchmarkDialog::restart);
    UiLanguage::apply(this); QTimer::singleShot(0, this, &BenchmarkDialog::launch);
}
void BenchmarkDialog::restart() { if (runner.busy()) { pendingRestart = true; runner.stop(); } else launch(); }
void BenchmarkDialog::launch() {
    if (closing || runner.busy()) return;
    const auto bytes = dictionary->currentData().toULongLong(); const int count = threads->currentData().toInt();
    const auto required = BenchmarkRunner::memoryUsage(bytes, count), ram = systemRam();
    memory->setText(QString::number((required + (1 << 20) - 1) >> 20) + " MB" + (ram ? " / " + QString::number((ram + (1 << 20) - 1) >> 20) + " MB" : QString()));
    if (ram && required > ram / 16 * 15) { status->setText("Not enough memory for this dictionary and thread count."); stopButton->setEnabled(false); return; }
    QSettings settings; settings.setValue("Benchmark/DictionaryBytes", bytes); settings.setValue("Benchmark/Threads", count); settings.setValue("Benchmark/Passes", passes->currentData());
    passCount->setText("0 /");
    for (auto value : currentValues + averageValues + currentSizes + averageSizes) value->clear();
    totalRating->clear(); totalUsage->clear(); totalRatingUsage->clear(); log->clear(); status->setText("Running");
    clock.start(); elapsed->setText("0 s"); timer.start(); stopButton->setEnabled(true); runner.startDictionary(bytes, count, passes->currentData().toInt());
}
void BenchmarkDialog::reject() { pendingRestart = false; closing = true; if (runner.busy()) runner.stop(); else QDialog::reject(); }
