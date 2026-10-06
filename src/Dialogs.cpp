#include "Dialogs.h"
#include "ResourceDialogs.h"
#include "CompressionOptionsText.h"
#include "ProgressText.h"
#include "ChecksumResultsDialog.h"
#include "MacProcessPriority.h"
#include <limits>
#include <cmath>
#include <QDialogButtonBox>
#include <QEvent>
#include <QApplication>
#include <QClipboard>
#include <QAction>
#include <QFileDialog>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QMessageBox>
#include <QThread>
#include <QStandardItemModel>
#include <QFileInfo>
#include <QDir>
#include "OptionsDialog.h"
#include "UiLanguage.h"
#include "upstream/UiResourceIds.h"
using namespace OfficialUi;
#include "ArchiveFormats.h"
#include "Help.h"
#include <QProcess>
#include <QRegularExpression>
#include <QSignalBlocker>
#include <QScopedValueRollback>

static QString unitOption(QString text) { text = text.toLower().remove(' '); text.replace("mb", "m"); text.replace("gb", "g"); text.replace("kb", "k"); text.replace("tb", "t"); return text; }
static constexpr int automaticRole = Qt::UserRole + 17;
static bool automatic(const QComboBox *control) { return control->currentData(automaticRole).toBool() && control->currentText() == control->itemText(control->currentIndex()); }
static void autoItem(QComboBox *control, QString value) { control->addItem("*  " + value); control->setItemData(0, true, automaticRole); }
static void restoreChoice(QComboBox *control, QString value, bool useAuto) { if (useAuto || value.isEmpty()) control->setCurrentIndex(0); else { auto index = control->findText(value); if (index > 0) control->setCurrentIndex(index); else control->setEditText(value); } }
static void compressionOptions(ArchiveRequest &value, QString format, QString method, QWidget *parent) {
    const auto caps = ArchiveFormats::find(format);
    QDialog dialog(parent); dialog.setWindowTitle("Options"); dialog.setObjectName("compressOptionsDialog"); dialog.resize(430, 420); auto layout = new QVBoxLayout(&dialog);
    auto links = new QGroupBox("NTFS", &dialog); auto linkLayout = new QVBoxLayout(links);
    auto symbolic = new QCheckBox("Store symbolic links"); symbolic->setObjectName("storeSymbolicLinks"); symbolic->setChecked(value.storeSymbolicLinks); symbolic->setVisible(caps.symbolicLinks); symbolic->setEnabled(caps.symbolicLinks); linkLayout->addWidget(symbolic);
    auto hard = new QCheckBox("Store hard links"); hard->setObjectName("storeHardLinks"); hard->setChecked(value.storeHardLinks); hard->setVisible(caps.hardLinks); hard->setEnabled(caps.hardLinks); linkLayout->addWidget(hard);
    layout->addWidget(links); links->setVisible(caps.symbolicLinks || caps.hardLinks);
    // CompressOptionsDialog.rc puts the handler/method label above the Time box.
    auto typeInfo = new QLabel(UiLanguage::text("Type") + ": " + format + (format == "tar" ? ": " + method : QString()), &dialog); typeInfo->setObjectName("compressionTimeInfo"); typeInfo->setProperty("uiLiteral", true); layout->addWidget(typeInfo);
    auto time = new QGroupBox("Time", &dialog); auto form = new QGridLayout(time); form->setColumnStretch(1, 1);
    auto precisionSet = new QCheckBox(":"); precisionSet->setObjectName("timestampPrecisionSet"); precisionSet->setAccessibleName(UiLanguage::text("Timestamp precision:"));
    auto precisionLabel = new QLabel("Timestamp precision:"); auto precision = new QComboBox; precision->setObjectName("timestampPrecision"); precision->setProperty("uiLiteral", true);
    auto precisionText = [](int p) {
        const auto ns = UiLanguage::resource(4091), sec = UiLanguage::resource(4090);
        if (p == 0) return "100 " + ns + " : Windows";
        if (p == 1) return "1 " + sec + " : Unix";
        if (p == 2) return "2 " + sec + " : DOS";
        if (p == 3) return "1 " + ns + " : Linux";
        if (p == 16) return "1 " + sec;
        if (p >= 16 && p <= 25) return QString::number(quint64(std::pow(10, 25 - p))) + ' ' + ns;
        return QString::number(p);
    };
    quint32 mask = caps.precisionMask; if (caps.defaultPrecision != 0) mask |= quint32(1) << caps.defaultPrecision;
    const int defaultPrecision = format == "gzip" ? 1 : caps.defaultPrecision;
    for (int p = 0; p <= 25; ++p) if ((mask >> p) & 1) precision->addItem(precisionText(p), p);
    const int selected = value.timestampPrecision < 0 ? defaultPrecision : value.timestampPrecision;
    if (precision->findData(selected) < 0 && selected > 2) precision->addItem(precisionText(selected), selected);
    precision->setCurrentIndex(qMax(0, precision->findData(selected))); precisionSet->setChecked(value.timestampPrecision >= 0);
    const bool precisionSpecified = precisionSet->isChecked() || precision->count() > 1;
    precisionSet->setEnabled(precisionSpecified); precisionSet->setVisible(precisionSpecified); precision->setVisible(precision->count() != 0); precisionLabel->setVisible(precision->count() != 0); precision->setEnabled(precisionSet->isChecked() && precision->count() > 1);
    form->addWidget(precisionSet, 0, 0); form->addWidget(precisionLabel, 0, 1); form->addWidget(precision, 0, 2);
    auto timeOption = [form](int row, QString name, QString label, int option, bool defaultValue) {
        auto specified = new QCheckBox(":"); specified->setObjectName(name + "Set"); specified->setAccessibleName("Specify " + label); specified->setChecked(option >= 0);
        auto check = new ResourceCheckBox(label); check->setObjectName(name); check->setChecked(option < 0 ? defaultValue : option != 0); check->setEnabled(specified->isChecked()); form->addWidget(specified, row, 0); form->addWidget(check, row, 1, 1, 2);
        QObject::connect(specified, &QCheckBox::toggled, check, [check, defaultValue](bool set) { if (!set) check->setChecked(defaultValue); check->setEnabled(set); }); return qMakePair(specified, check);
    };
    auto modified = timeOption(1, "storeModificationTime", "Store modification time", value.modificationTime, caps.defaultModificationTime);
    auto created = timeOption(2, "storeCreationTime", "Store creation time", value.creationTime, caps.defaultCreationTime);
    auto accessed = timeOption(3, "storeAccessTime", "Store last access time", value.accessTime, caps.defaultAccessTime);
    auto latest = timeOption(4, "latestArchiveTime", "Set archive time to latest file time", value.latestArchiveTimeSpecified || value.latestArchiveTime ? int(value.latestArchiveTime) : -1, false);
    auto preserve = new ResourceCheckBox("Do not change source files last access time"); preserve->setObjectName("preserveAccessTime"); preserve->setChecked(value.preserveAccessTime); form->addWidget(preserve, 5, 0, 1, 3); layout->addWidget(time);
    auto supported = [=] {
        const int effectivePrecision = precisionSet->isChecked() ? precision->currentData().toInt() : defaultPrecision;
        bool c = caps.creationTime, a = caps.accessTime;
        if (format == "tar") { c = false; a = method == "POSIX"; }
        if (format == "zip" && effectivePrecision != 0) { c = false; a = false; }
        auto set = [](auto pair, bool allowed) { pair.first->setVisible(allowed); pair.second->setVisible(allowed); pair.first->setEnabled(allowed); pair.second->setEnabled(allowed && pair.first->isChecked()); };
        set(modified, caps.modificationTime); set(created, c); set(accessed, a);
        if (caps.modificationTime && value.modificationTime < 0 && !caps.keepName) { modified.first->hide(); modified.second->setEnabled(false); }
    };
    QObject::connect(precisionSet, &QCheckBox::toggled, &dialog, [=](bool checked) { if (!checked) precision->setCurrentIndex(qMax(0, precision->findData(defaultPrecision))); precision->setEnabled(checked && precision->count() > 1); supported(); });
    QObject::connect(precision, &QComboBox::currentIndexChanged, &dialog, supported); supported();
    auto b = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Help); layout->addWidget(b); QObject::connect(b, &QDialogButtonBox::accepted, &dialog, &QDialog::accept); QObject::connect(b, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    QObject::connect(b, &QDialogButtonBox::helpRequested, &dialog, [&] { Help::show(&dialog, "fm/plugins/7-zip/add.htm#options"); });
    UiLanguage::bindTitle(&dialog, IDB_COMPRESS_OPTIONS); UiLanguage::bind(precisionLabel, IDT_COMPRESS_TIME_PREC); UiLanguage::bind(time, IDG_COMPRESS_TIME);
    UiLanguage::bind(symbolic, IDX_COMPRESS_NT_SYM_LINKS); UiLanguage::bind(hard, IDX_COMPRESS_NT_HARD_LINKS);
    UiLanguage::bind(modified.second, IDX_COMPRESS_MTIME); UiLanguage::bind(created.second, IDX_COMPRESS_CTIME); UiLanguage::bind(accessed.second, IDX_COMPRESS_ATIME); UiLanguage::bind(latest.second, IDX_COMPRESS_ZTIME); UiLanguage::bind(preserve, IDX_COMPRESS_PRESERVE_ATIME);
    auto geometry = new ResourceDialogLayout(&dialog, IDD_COMPRESS_OPTIONS);
    geometry->bind("IDG_COMPRESS_NTFS", links); geometry->bind("IDT_COMPRESS_TIME_INFO", typeInfo);
    geometry->bind("IDC_COMPRESS_TIME_PREC", precision); geometry->bind("IDX_COMPRESS_PREC_SET", precisionSet);
    geometry->bind("IDX_COMPRESS_MTIME_SET", modified.first); geometry->bind("IDX_COMPRESS_CTIME_SET", created.first); geometry->bind("IDX_COMPRESS_ATIME_SET", accessed.first); geometry->bind("IDX_COMPRESS_ZTIME_SET", latest.first);
    geometry->bindButtons(b); geometry->install();
    UiLanguage::apply(&dialog); if (dialog.exec() != QDialog::Accepted) return;
    auto option = [](auto pair) { return pair.first->isChecked() ? int(pair.second->isChecked()) : -1; };
    value.timestampPrecision = precisionSet->isEnabled() && precisionSet->isChecked() ? precision->currentData().toInt() : -1; value.modificationTime = option(modified); value.creationTime = option(created); value.accessTime = option(accessed);
    value.latestArchiveTimeSpecified = latest.first->isChecked(); value.latestArchiveTime = latest.first->isChecked() && latest.second->isChecked(); value.preserveAccessTime = preserve->isChecked(); if (symbolic->isEnabled()) value.storeSymbolicLinks = symbolic->isChecked(); if (hard->isEnabled()) value.storeHardLinks = hard->isChecked();
}

static QComboBox *combo(QStringList items, QWidget *parent) {
    auto c = new QComboBox(parent); c->addItems(items); c->setMinimumWidth(130); return c;
}
static void disableChoice(QComboBox *c, int i, const QString &reason) {
    auto model = qobject_cast<QStandardItemModel *>(c->model());
    if (model && model->item(i)) { model->item(i)->setEnabled(false); model->item(i)->setToolTip(reason); }
}
static QDialogButtonBox *buttons(QDialog *dialog) {
    auto b = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Help, dialog);
    QObject::connect(b, &QDialogButtonBox::rejected, dialog, &QDialog::reject);
    QObject::connect(b, &QDialogButtonBox::helpRequested, dialog, [dialog] { Help::show(dialog, dialog->objectName() == "addDialog" ? "fm/plugins/7-zip/add.htm" : "fm/plugins/7-zip/extract.htm"); });
    return b;
}
AddDialog::AddDialog(QString archivePath, QWidget *parent) : QDialog(parent) {
    setWindowTitle("Add to Archive"); setObjectName("addDialog"); resize(750, 530);
    auto layout = new QVBoxLayout(this); layout->setSpacing(12);
    auto top = new QHBoxLayout;
    auto archiveLabel = new QLabel("&Archive:", this); top->addWidget(archiveLabel); auto savedHistory = QSettings().value("Compression/History").toStringList(); while (savedHistory.size() > 20) savedHistory.removeLast(); auto archiveHistory = combo(savedHistory, this); archiveHistory->setObjectName("archiveHistory"); archiveHistory->setEditable(true); archive = archiveHistory->lineEdit(); archive->setText(archivePath); archiveLabel->setBuddy(archiveHistory); archive->setObjectName("archivePath"); top->addWidget(archiveHistory);
    auto browse = new QPushButton("...", this); browse->setFixedWidth(30); top->addWidget(browse); layout->addLayout(top);
    connect(browse, &QPushButton::clicked, this, [this] {
        auto path = QFileDialog::getSaveFileName(this, "Browse", archive->text(), "Archives (*.7z *.zip)", nullptr, QFileDialog::DontConfirmOverwrite);
        if (!path.isEmpty()) archive->setText(path);
    });
    auto columns = new QHBoxLayout; auto left = new QFormLayout; left->setVerticalSpacing(6); auto right = new QVBoxLayout; columns->addLayout(left, 1); columns->addLayout(right, 1); columns->setSpacing(28);
    QStringList formats{"7z", "zip", "tar", "wim", "xz", "gzip", "bzip2", "Hash"}; formats.sort(Qt::CaseInsensitive);
    format = combo(formats, this); format->setProperty("uiLiteral", true); format->setObjectName("archiveFormat"); left->addRow("Archive &format:", format);
    level = combo({"0 - Store", "1 - Fastest", "2", "3 - Fast", "4", "5 - Normal", "6", "7 - Maximum", "8", "9 - Ultra"}, this); level->setObjectName("compressionLevel"); level->setCurrentIndex(5); left->addRow("Compression &level:", level);
    method = combo({}, this); method->setObjectName("compressionMethod"); left->addRow("Compression &method:", method);
    dictionary = combo({}, this); dictionary->setObjectName("compressionDictionary"); dictionary->setEditable(true); left->addRow("&Dictionary size:", dictionary);
    word = combo({}, this); word->setObjectName("compressionWord"); word->setEditable(true); wordLabel = new QLabel("&Word size:", this); wordLabel->setBuddy(word); left->addRow(wordLabel, word);
    solid = combo({}, this); solid->setObjectName("compressionSolid"); solid->setEditable(true); autoItem(solid, "—"); solid->addItem("Non-solid"); for (unsigned n = 20; n <= 36; ++n) solid->addItem(CompressionMath::sizeText(quint64(1) << n)); solid->addItem("Solid"); left->addRow("&Solid Block size:", solid);
    threads = combo({}, this); threads->setObjectName("compressionThreads"); threads->setEditable(true); threadControls = new QWidget(this); auto threadRow = new QHBoxLayout(threadControls); threadRow->setContentsMargins(0, 0, 0, 0); threadRow->addWidget(threads); threadRow->addWidget(new QLabel("/ " + QString::number(qMax(1, QThread::idealThreadCount())), this)); left->addRow("Number of CPU &threads:", threadControls);
    memory = combo({}, this); memory->setObjectName("compressionMemory"); memory->setEditable(true); autoItem(memory, "80%"); for (int n = 10; n <= 100; n += 10) memory->addItem(QString::number(n) + '%'); for (unsigned n = 54; n <= (20 + sizeof(size_t) * 3 - 1) * 2; ++n) memory->addItem(CompressionMath::sizeText(quint64(2 + (n & 1)) << (n / 2)));
    compressMemory = new QLabel(this); compressMemory->setObjectName("compressionMemoryRequired"); compressMemory->setProperty("uiLiteral", true); memoryControls = new QWidget(this); auto memoryRow = new QVBoxLayout(memoryControls); memoryRow->setContentsMargins(0, 0, 0, 0); memoryRow->addWidget(memory); memoryRow->addWidget(compressMemory); left->addRow("Memory usage for Compressing:", memoryControls); memoryLabel = qobject_cast<QLabel *>(left->labelForField(memoryControls));
    decompressMemory = new QLabel(this); decompressMemory->setObjectName("decompressionMemoryRequired"); decompressMemory->setProperty("uiLiteral", true); left->addRow("Memory usage for Decompressing:", decompressMemory); decompressMemoryLabel = qobject_cast<QLabel *>(left->labelForField(decompressMemory));
    auto volumes = combo({"", "10m", "100m", "650m", "700m", "4480m"}, this); volumes->setEditable(true); volume = volumes->lineEdit(); volume->setObjectName("splitVolumes"); volume->setPlaceholderText("e.g. 100m"); left->addRow("Split to &volumes, bytes:", volumes);
    parameters = new QLineEdit(this); parameters->setObjectName("compressionParameters"); parameters->setToolTip("Compression properties, for example: d=64m fb=64"); left->addRow("Parameters:", parameters);
    auto options = new QPushButton("Options", this); options->setObjectName("compressionOptions"); left->addRow(options); connect(options, &QPushButton::clicked, this, [this] { compressionOptions(advanced, format->currentText(), selectedMethod(), this); updateOptionsSummary(); });
    auto modes = new QFormLayout;
    update = combo({"Add and replace files", "Update and add files", "Freshen existing files", "Synchronize files"}, this); modes->addRow("&Update mode:", update);
    paths = combo({"Relative pathnames", "Full pathnames", "Absolute pathnames"}, this); paths->setObjectName("compressionPathMode"); modes->addRow("Path mode:", paths); right->addLayout(modes);
    auto optionsGroup = new QGroupBox("Options", this); auto optionLayout = new QVBoxLayout(optionsGroup);
    for (const auto &text : {"Create SF&X archive", "Compress shared files"}) {
        auto check = new QCheckBox(text, this); UiLanguage::bind(check, QString::fromLatin1(text).startsWith("Create") ? IDX_COMPRESS_SFX : IDX_COMPRESS_SHARED); check->setEnabled(false); check->setToolTip("Windows SFX / sharing-lock setting. macOS reads files without Windows sharing locks."); optionLayout->addWidget(check);
    }
    deleteAfter = new QCheckBox("Delete files after compression", this); deleteAfter->setObjectName("deleteAfterCompression"); deleteAfter->setToolTip("Move successfully archived source files to the macOS Trash, after final output installation."); optionLayout->addWidget(deleteAfter);
    right->addWidget(optionsGroup);
    auto group = new QGroupBox("Encryption", this); auto encryptionLayout = new QVBoxLayout(group);
    password = new QLineEdit(this); password->setObjectName("addPassword"); password->setEchoMode(QLineEdit::Password);
    reenter = new QLineEdit(this); reenter->setObjectName("repeatPassword"); reenter->setEchoMode(QLineEdit::Password);
    auto passwordLabel = new QLabel("Enter &password:", this); passwordLabel->setBuddy(password); encryptionLayout->addWidget(passwordLabel); encryptionLayout->addWidget(password);
    auto reenterLabel = new QLabel("Reenter password:", this); encryptionLayout->addWidget(reenterLabel); encryptionLayout->addWidget(reenter);
    showPassword = new QCheckBox("Show Password", this); encryptionLayout->addWidget(showPassword);
    connect(showPassword, &QCheckBox::toggled, this, [this, reenterLabel](bool checked) { password->setEchoMode(checked ? QLineEdit::Normal : QLineEdit::Password); reenter->setVisible(!checked); reenterLabel->setVisible(!checked); });
    encryption = combo({"AES-256"}, this); auto encryptionForm = new QFormLayout; encryptionForm->addRow("&Encryption method:", encryption); encryptionLayout->addLayout(encryptionForm);
    encryptNames = new QCheckBox("Encrypt file &names", this); encryptionLayout->addWidget(encryptNames); right->addWidget(group); right->addStretch();
    layout->addLayout(columns); auto b = buttons(this); layout->addWidget(b);
    connect(b, &QDialogButtonBox::accepted, this, [this] {
        if (archive->text().trimmed().isEmpty()) { QMessageBox::warning(this, "7-Zip", "Specify an archive name."); return; }
        bool threadsValid; int threadCount = threads->currentText().toInt(&threadsValid); const auto threadLimit = qMin<unsigned>(CompressionMath::calculate(mathInput()).maximumThreads, qMax(1, QThread::idealThreadCount()) * 2); if (threads->isEnabled() && !automatic(threads) && (!threadsValid || threadCount < 1 || unsigned(threadCount) > threadLimit)) { QMessageBox::warning(this, "7-Zip", "Specify a CPU thread count between 1 and " + QString::number(threadLimit) + '.'); return; }
        if (!showPassword->isChecked() && password->text() != reenter->text()) { QMessageBox::warning(this, "7-Zip", "Passwords do not match"); return; }
        if (format->currentText() == "zip") {
            for (auto c : password->text()) if (c.unicode() > 127) { QMessageBox::warning(this, "7-Zip", "Use only English letters, numbers and special characters (!, #, $, ...) for ZIP password."); return; }
            if (encryption->currentText() == "AES-256" && password->text().size() > 99) { QMessageBox::warning(this, "7-Zip", "ZIP AES passwords must not exceed 99 characters."); return; }
        }
        if (encryptNames->isChecked() && password->text().isEmpty()) { QMessageBox::warning(this, "7-Zip", "Enter a password to encrypt file names."); return; }
        const QString mem = automatic(memory) ? QString() : unitOption(memory->currentText());
        if (!mem.isEmpty() && !QRegularExpression("^(?:[1-9][0-9]?|100)%$|^[1-9][0-9]*[kmgt]$").match(mem).hasMatch()) { QMessageBox::warning(this, "7-Zip", "Specify a compression memory limit such as 80% or 4g."); return; }
        for (auto control : {dictionary, solid}) if (control->isEnabled() && !automatic(control) && control->currentText() != "Solid" && control->currentText() != "Non-solid" && (CompressionMath::parseSize(control->currentText()) == CompressionMath::Automatic || CompressionMath::parseSize(control->currentText()) == 0) && !(control == solid && QStringList{"off", "on"}.contains(control->currentData().toString()))) { QMessageBox::warning(this, "7-Zip", "Specify a valid dictionary or solid block size."); return; }
        const auto wordSizes = CompressionMath::words(selectedMethod(), format->currentText()); if (word->isEnabled() && !automatic(word) && (wordSizes.isEmpty() || word->currentText().toUInt() < wordSizes.first() || word->currentText().toUInt() > wordSizes.last())) { QMessageBox::warning(this, "7-Zip", "Specify a valid word size or order for the selected method."); return; }
        const auto input = mathInput(); const auto calculated = CompressionMath::calculate(input);
        if (memory->isEnabled() && calculated.compressMemory != CompressionMath::Automatic && calculated.compressMemory > input.memoryLimit) { QMessageBox::warning(this, "7-Zip", "The compression settings require more memory than the selected limit.\n\nRequired: " + CompressionMath::sizeText(calculated.compressMemory, true) + "\nLimit: " + CompressionMath::sizeText(input.memoryLimit, true)); return; }
        if (deleteAfter->isChecked() && QMessageBox::question(this, "7-Zip", "Move successfully archived source files to Trash after compression?", QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) return;
        accept();
    });
    connect(format, &QComboBox::currentIndexChanged, this, &AddDialog::formatChanged);
    connect(archiveHistory, &QComboBox::activated, this, [this, archiveHistory](int index) {
        QString extension = QFileInfo(archiveHistory->itemText(index)).suffix().toLower();
        if (extension == "gz") extension = "gzip"; if (extension == "bz2") extension = "bzip2"; if (extension == "sha1" || extension == "sha256") extension = "Hash";
        const auto selected = format->findText(extension); if (selected >= 0) format->setCurrentIndex(selected);
    });
    connect(method, &QComboBox::currentIndexChanged, this, &AddDialog::methodChanged);
    connect(level, &QComboBox::currentIndexChanged, this, &AddDialog::levelChanged);
    connect(dictionary, &QComboBox::currentTextChanged, this, &AddDialog::dictionaryChanged);
    for (auto control : {word, solid, threads, memory}) connect(control, &QComboBox::currentTextChanged, this, &AddDialog::updateAutomatic);
    QString initialFormat = QFileInfo(archivePath).suffix().toLower(); if (initialFormat == "gz") initialFormat = "gzip"; if (initialFormat == "bz2") initialFormat = "bzip2"; if (initialFormat == "sha256" || initialFormat == "sha1") initialFormat = "Hash"; if (format->findText(initialFormat) < 0) initialFormat = QSettings().value("Compression/LastFormat", "7z").toString(); { QSignalBlocker blocker(format); format->setCurrentIndex(qMax(0, format->findText(initialFormat))); }
    formatChanged();
    UiLanguage::bindTitle(this, IDD_COMPRESS); UiLanguage::bind(archiveLabel, IDT_COMPRESS_ARCHIVE);
    for (const auto &pair : {qMakePair(static_cast<QWidget *>(format), IDT_COMPRESS_FORMAT), qMakePair(static_cast<QWidget *>(level), IDT_COMPRESS_LEVEL), qMakePair(static_cast<QWidget *>(method), IDT_COMPRESS_METHOD), qMakePair(static_cast<QWidget *>(dictionary), IDT_COMPRESS_DICTIONARY), qMakePair(static_cast<QWidget *>(word), IDT_COMPRESS_ORDER), qMakePair(static_cast<QWidget *>(solid), IDT_COMPRESS_SOLID), qMakePair(threadControls, IDT_COMPRESS_THREADS), qMakePair(memoryControls, IDT_COMPRESS_MEMORY), qMakePair(static_cast<QWidget *>(decompressMemory), IDT_COMPRESS_MEMORY_DE), qMakePair(static_cast<QWidget *>(volumes), IDT_SPLIT_TO_VOLUMES), qMakePair(static_cast<QWidget *>(parameters), IDT_COMPRESS_PARAMETERS), qMakePair(static_cast<QWidget *>(update), IDT_COMPRESS_UPDATE_MODE), qMakePair(static_cast<QWidget *>(paths), IDT_COMPRESS_PATH_MODE), qMakePair(static_cast<QWidget *>(encryption), IDT_COMPRESS_ENCRYPTION_METHOD)}) UiLanguage::bindFormLabel(pair.first, pair.second);
    UiLanguage::bind(options, IDB_COMPRESS_OPTIONS); UiLanguage::bind(optionsGroup, IDG_COMPRESS_OPTIONS); UiLanguage::bind(group, IDG_COMPRESS_ENCRYPTION);
    UiLanguage::bind(deleteAfter, IDX_COMPRESS_DEL); UiLanguage::bind(passwordLabel, IDT_PASSWORD_ENTER); UiLanguage::bind(reenterLabel, IDT_PASSWORD_REENTER); UiLanguage::bind(showPassword, IDX_PASSWORD_SHOW); UiLanguage::bind(encryptNames, IDX_COMPRESS_ENCRYPT_FILE_NAMES);
    for (int n = 0; n < update->count(); ++n) update->setItemData(n, IDS_COMPRESS_UPDATE_MODE_ADD + unsigned(n), UiLanguage::ResourceIdRole);
    for (int n = 0; n < paths->count(); ++n) paths->setItemData(n, n == 0 ? 3414u : 3411u + unsigned(n == 2 ? 2 : 0), UiLanguage::ResourceIdRole);
    auto geometry = new ResourceDialogLayout(this, IDD_COMPRESS);
    for (const auto &pair : {qMakePair(QString("IDC_COMPRESS_ARCHIVE"), static_cast<QWidget *>(archiveHistory)), qMakePair(QString("IDB_COMPRESS_SET_ARCHIVE"), static_cast<QWidget *>(browse)), qMakePair(QString("IDC_COMPRESS_FORMAT"), static_cast<QWidget *>(format)), qMakePair(QString("IDC_COMPRESS_LEVEL"), static_cast<QWidget *>(level)), qMakePair(QString("IDC_COMPRESS_METHOD"), static_cast<QWidget *>(method)), qMakePair(QString("IDC_COMPRESS_DICTIONARY"), static_cast<QWidget *>(dictionary)), qMakePair(QString("IDC_COMPRESS_ORDER"), static_cast<QWidget *>(word)), qMakePair(QString("IDC_COMPRESS_SOLID"), static_cast<QWidget *>(solid)), qMakePair(QString("IDC_COMPRESS_THREADS"), static_cast<QWidget *>(threads)), qMakePair(QString("IDC_COMPRESS_MEM_USE"), static_cast<QWidget *>(memory)), qMakePair(QString("IDC_COMPRESS_VOLUME"), static_cast<QWidget *>(volumes)), qMakePair(QString("IDE_COMPRESS_PARAMETERS"), static_cast<QWidget *>(parameters)), qMakePair(QString("IDC_COMPRESS_UPDATE_MODE"), static_cast<QWidget *>(update)), qMakePair(QString("IDC_COMPRESS_PATH_MODE"), static_cast<QWidget *>(paths)), qMakePair(QString("IDC_COMPRESS_ENCRYPTION_METHOD"), static_cast<QWidget *>(encryption)), qMakePair(QString("IDE_COMPRESS_PASSWORD1"), static_cast<QWidget *>(password)), qMakePair(QString("IDE_COMPRESS_PASSWORD2"), static_cast<QWidget *>(reenter)), qMakePair(QString("IDT_COMPRESS_MEMORY_VALUE"), static_cast<QWidget *>(compressMemory)), qMakePair(QString("IDT_COMPRESS_MEMORY_DE_VALUE"), static_cast<QWidget *>(decompressMemory))}) geometry->bind(pair.first, pair.second);
    geometry->bind("IDT_COMPRESS_HARDWARE_THREADS", threadControls->findChild<QLabel *>());
    geometry->container(threadControls, {"IDC_COMPRESS_THREADS", "IDT_COMPRESS_HARDWARE_THREADS"}); geometry->container(memoryControls, {"IDC_COMPRESS_MEM_USE", "IDT_COMPRESS_MEMORY_VALUE"});
    auto folderLabel = new QLabel(QFileInfo(archive->text()).absolutePath(), this); folderLabel->setProperty("uiLiteral", true); folderLabel->setObjectName("archiveFolderLabel"); geometry->bind("IDT_COMPRESS_ARCHIVE_FOLDER", folderLabel);
    connect(archive, &QLineEdit::textChanged, folderLabel, [this, folderLabel] { folderLabel->setText(QFileInfo(archive->text()).absolutePath()); });
    auto optionsSummary = new QLabel(this); optionsSummary->setProperty("uiLiteral", true); optionsSummary->setObjectName("compressionOptionsSummary"); geometry->bind("IDT_COMPRESS_OPTIONS", optionsSummary);
    geometry->bindButtons(b); geometry->install();
    updateOptionsSummary();
    UiLanguage::apply(this);
}
void AddDialog::updateOptionsSummary() {
    if (auto label = findChild<QLabel *>("compressionOptionsSummary")) label->setText(compressionOptionsText(advanced, format->currentText()));
}
static quint64 numericChoice(const QComboBox *control) {
    if (automatic(control)) return CompressionMath::Automatic;
    if (control->currentText() == control->itemText(control->currentIndex()) && control->currentData().isValid()) return control->currentData().toULongLong();
    return CompressionMath::parseSize(control->currentText());
}
static void populateNumeric(QComboBox *control, const CompressionMath::NumericChoices &choices) {
    control->clear();
    for (const auto &choice : choices.items) {
        control->addItem(choice.text, QVariant::fromValue(choice.value));
        control->setItemData(control->count() - 1, choice.automatic, automaticRole);
    }
    control->setCurrentIndex(choices.selected);
    control->setEnabled(control->count() > 1);
}
static QString solidCommand(const QComboBox *control) {
    if (control->count() == 0 || automatic(control)) return {};
    if (control->currentText() == control->itemText(control->currentIndex())) {
        const auto value = control->currentData();
        if (value.toString() == "off" || value.toString() == "on") return value.toString();
        if (value.isValid()) return unitOption(CompressionMath::sizeText(value.toULongLong()));
    }
    if (control->currentText() == "Non-solid") return "off";
    if (control->currentText() == "Solid") return "on";
    return unitOption(control->currentText());
}
void AddDialog::saveOptionsInMemory() {
    if (currentFormat.isEmpty()) return;
    auto &state = formatDrafts[currentFormat];
    state["Level"] = level->currentData().toInt(); state["Method"] = selectedMethod();
    const QMap<QComboBox *, QString> names{{method, "Method"}, {dictionary, "Dictionary"}, {word, "Word"}, {solid, "Solid"}, {threads, "Threads"}, {memory, "Memory"}};
    for (auto control : names.keys()) state[names.value(control) + "Automatic"] = automatic(control) || control->count() == 0;
    state["DictionaryBytes"] = QString::number(numericChoice(dictionary)); state["DictionaryText"] = dictionary->currentText();
    state["Word"] = automatic(word) ? QString() : word->currentText(); state["SolidText"] = solid->currentText(); state["SolidCommand"] = solidCommand(solid);
    state["Threads"] = automatic(threads) ? 0 : threads->currentText().toInt(); state["Memory"] = automatic(memory) ? QString() : memory->currentText();
    state["Parameters"] = parameters->text(); state["PathMode"] = paths->currentIndex(); state["Update"] = update->currentIndex(); state["Volume"] = volume->text();
    state["Encryption"] = encryption->currentText(); state["EncryptNames"] = encryptNames->isChecked(); state["ShowPassword"] = showPassword->isChecked();
    state["SymbolicLinks"] = advanced.storeSymbolicLinks; state["HardLinks"] = advanced.storeHardLinks;
    state["Precision"] = advanced.timestampPrecision; state["MTime"] = advanced.modificationTime; state["CTime"] = advanced.creationTime; state["ATime"] = advanced.accessTime;
    state["Latest"] = advanced.latestArchiveTime; state["LatestSpecified"] = advanced.latestArchiveTimeSpecified; state["PreserveAccess"] = advanced.preserveAccessTime;
}
void AddDialog::formatChanged() {
    if (updatingCompression) return;
    saveOptionsInMemory();
    QScopedValueRollback<bool> updating(updatingCompression, true);
    currentFormat = format->currentText(); const QString fmt = currentFormat; const bool seven = fmt == "7z", encrypted = seven || fmt == "zip";
    if (!formatDrafts.contains(fmt)) {
        QSettings settings; settings.beginGroup("Compression/" + fmt); QVariantMap state;
        for (const auto &key : settings.childKeys()) state[key] = settings.value(key);
        formatDrafts[fmt] = state;
    }
    const auto &state = formatDrafts[fmt];
    level->clear();
    int selected = -1; const auto savedLevel = state.value("Level", 5).toUInt();
    for (int value : CompressionMath::levels(fmt)) {
        const auto id = CompressionMath::levelNameId(value); const auto prefix = QString::number(value) + (id ? " - " : "");
        level->addItem(prefix + (id ? UiLanguage::resource(id) : QString()), value); const int index = level->count() - 1;
        level->setItemData(index, id, UiLanguage::ResourceIdRole); level->setItemData(index, prefix, UiLanguage::PrefixRole);
        if (unsigned(value) <= savedLevel || selected < 0) selected = index;
    }
    level->setCurrentIndex(selected); level->setEnabled(level->count() > 1);
    rebuildMethods();
    encryption->clear(); if (encrypted) encryption->addItems(seven ? QStringList{"AES-256"} : QStringList{"ZipCrypto", "AES-256"});
    encryptNames->setEnabled(seven); if (!seven) encryptNames->setChecked(false);
    for (QWidget *field : QList<QWidget *>{password, reenter, encryption, showPassword}) field->setEnabled(encrypted); if (!encrypted) { password->clear(); reenter->clear(); }
    QString name = archive->text();
    if (QStringList{"7z", "zip", "tar", "wim", "xz", "gz", "bz2", "sha256", "sha1"}.contains(QFileInfo(name).suffix().toLower())) archive->setText(name.left(name.lastIndexOf('.')) + '.' + (fmt == "gzip" ? "gz" : fmt == "bzip2" ? "bz2" : fmt == "Hash" ? "sha256" : fmt));
    volume->parentWidget()->setEnabled(fmt != "Hash"); deleteAfter->setEnabled(fmt != "Hash"); if (fmt == "Hash") deleteAfter->setChecked(false); findChild<QPushButton *>("compressionOptions")->setEnabled(fmt != "Hash");
    parameters->setText(state.value("Parameters").toString()); paths->setCurrentIndex(qBound(0, state.value("PathMode", 0).toInt(), 2)); update->setCurrentIndex(qBound(0, state.value("Update", 0).toInt(), 3));
    advanced.timestampPrecision = state.value("Precision", -1).toInt(); advanced.modificationTime = state.value("MTime", -1).toInt(); advanced.creationTime = state.value("CTime", -1).toInt(); advanced.accessTime = state.value("ATime", -1).toInt(); advanced.latestArchiveTime = state.value("Latest", false).toBool(); advanced.preserveAccessTime = state.value("PreserveAccess", false).toBool();
    advanced.latestArchiveTimeSpecified = state.value("LatestSpecified", advanced.latestArchiveTime).toBool();
    advanced.storeSymbolicLinks = state.value("SymbolicLinks", true).toBool(); advanced.storeHardLinks = state.value("HardLinks", false).toBool();
    volume->setText(state.value("Volume").toString()); encryption->setCurrentIndex(qMax(0, encryption->findText(state.value("Encryption", seven ? "AES-256" : "ZipCrypto").toString()))); encryptNames->setChecked(seven && state.value("EncryptNames", false).toBool()); showPassword->setChecked(state.value("ShowPassword", false).toBool());
    const QString storedMemory = state.value("Memory").toString(); restoreChoice(memory, storedMemory, state.value("MemoryAutomatic", storedMemory.isEmpty() || storedMemory.startsWith("Automatic")).toBool());
    updatingCompression = false; methodChanged();
    updateOptionsSummary();
}
QString AddDialog::selectedMethod() const { return method->count() == 0 ? QString("Copy") : method->currentData().toString(); }
void AddDialog::rebuildMethods() {
    const auto names = CompressionMath::methods(currentFormat, level->currentData().toInt()); const auto &state = formatDrafts[currentFormat];
    method->clear();
    for (const auto &name : names) { if (method->count() == 0) { autoItem(method, name); method->setItemData(0, name); } else method->addItem(name, name); }
    if (!state.value("MethodAutomatic", !state.contains("Method")).toBool()) method->setCurrentIndex(qMax(0, method->findData(state.value("Method"))));
    method->setEnabled(method->count() > 1);
}
void AddDialog::levelChanged() {
    if (updatingCompression) return;
    {
        QScopedValueRollback<bool> updating(updatingCompression, true);
        auto &state = formatDrafts[currentFormat];
        for (const auto &key : {"MethodAutomatic", "DictionaryAutomatic", "WordAutomatic", "SolidAutomatic", "ThreadsAutomatic"}) state[key] = true;
        state["Level"] = level->currentData().toInt(); state["Method"] = QString();
        rebuildMethods();
    }
    methodChanged();
}
void AddDialog::methodChanged() {
    if (updatingCompression) return;
    {
        QScopedValueRollback<bool> updating(updatingCompression, true);
        const auto fmt = format->currentText(), name = selectedMethod(); const auto &state = formatDrafts[currentFormat];
        const bool matches = state.value("MethodAutomatic", !state.contains("Method")).toBool() ? automatic(method) : state.value("Method").toString().compare(name, Qt::CaseInsensitive) == 0;
        const auto input = mathInput();
        quint64 savedDictionary = CompressionMath::Automatic;
        if (matches && !state.value("DictionaryAutomatic", !state.contains("DictionaryText")).toBool()) savedDictionary = state.contains("DictionaryBytes") ? state.value("DictionaryBytes").toString().toULongLong() : CompressionMath::parseSize(state.value("DictionaryText").toString());
        populateNumeric(dictionary, CompressionMath::dictionaryChoices(input, savedDictionary));
        populateNumeric(word, CompressionMath::wordChoices(input, matches && !state.value("WordAutomatic", !state.contains("Word")).toBool() ? state.value("Word").toUInt() : quint32(-1)));
        wordLabel->setProperty("uiText", "&Word size:"); wordLabel->setText(UiLanguage::resource(4007));
        solid->clear();
        if (CompressionMath::hasSolidControl(fmt) && level->currentData().toInt() != 0) {
            autoItem(solid, "—"); if (fmt == "7z") { solid->addItem(UiLanguage::resource(4072), "off"); solid->setItemData(solid->count() - 1, 4072, UiLanguage::ResourceIdRole); }
            for (unsigned n = 20; n <= 36; ++n) solid->addItem(CompressionMath::sizeText(quint64(1) << n), QVariant::fromValue(quint64(1) << n)); solid->addItem(UiLanguage::resource(4073), "on"); solid->setItemData(solid->count() - 1, 4073, UiLanguage::ResourceIdRole);
            QString storedSolid = state.value("SolidText").toString(); const auto command = state.value("SolidCommand").toString();
            if (command == "off") storedSolid = UiLanguage::resource(4072); else if (command == "on") storedSolid = UiLanguage::resource(4073); else if (!command.isEmpty()) storedSolid = CompressionMath::sizeText(CompressionMath::parseSize(command));
            restoreChoice(solid, storedSolid, !matches || state.value("SolidAutomatic", !state.contains("SolidText")).toBool() || (fmt == "xz" && command == "off"));
        }
        solid->setEnabled(solid->count() > 1);
        threads->clear();
        if (QStringList{"7z", "zip", "xz", "bzip2"}.contains(fmt)) { autoItem(threads, "—"); for (unsigned n : CompressionMath::threadChoices(mathInput())) threads->addItem(QString::number(n)); }
        if (matches && !state.value("ThreadsAutomatic", state.value("Threads", 0).toInt() == 0).toBool()) { const int selected = threads->findText(state.value("Threads").toString()); if (selected >= 0) threads->setCurrentIndex(selected); }
        threads->setEnabled(threads->count() > 1);
        const bool memoryAllowed = CompressionMath::hasMemoryControl(fmt);
        memory->setEnabled(memoryAllowed); memoryControls->setVisible(memoryAllowed); memoryLabel->setVisible(memoryAllowed); decompressMemory->setVisible(memoryAllowed); decompressMemoryLabel->setVisible(memoryAllowed);
        if (fmt == "Hash" && QStringList{"sha1", "sha256"}.contains(QFileInfo(archive->text()).suffix().toLower())) archive->setText(archive->text().left(archive->text().lastIndexOf('.')) + (name == "SHA1" ? ".sha1" : ".sha256"));
    }
    updateAutomatic();
}
void AddDialog::dictionaryChanged() {
    if (updatingCompression) return;
    saveOptionsInMemory();
    if (!automatic(solid) && solidCommand(solid) != "off" && solidCommand(solid) != "on") {
        QSignalBlocker blocker(solid); solid->setCurrentIndex(0); formatDrafts[currentFormat]["SolidAutomatic"] = true;
    }
    updateAutomatic();
}
CompressionMath::Input AddDialog::mathInput() const {
    CompressionMath::Input input; input.format = format->currentText(); input.method = selectedMethod(); input.level = level->currentData().toInt(); input.cpus = qMax(1, QThread::idealThreadCount()); input.ram = CompressionMath::systemRam();
    input.dictionary = numericChoice(dictionary);
    const auto solidValue = solidCommand(solid); input.solid = solidValue.isEmpty() ? CompressionMath::Automatic : solidValue == "on" ? CompressionMath::Automatic - 1 : solidValue == "off" ? 0 : CompressionMath::parseSize(solidValue);
    input.threads = automatic(threads) ? 0 : threads->currentText().toInt(); const QString limitText = automatic(memory) ? "80%" : memory->currentText();
    const auto ram = qMax<quint64>(1 << 26, input.ram ? input.ram : quint64(sizeof(size_t)) << 29); input.memoryLimit = limitText.endsWith('%') ? ram / 100 * limitText.chopped(1).toUInt() : CompressionMath::parseSize(limitText); return input;
}
void AddDialog::updateAutomatic() {
    if (updatingCompression) return;
    QScopedValueRollback<bool> updating(updatingCompression, true); const auto calculated = CompressionMath::calculate(mathInput());
    auto refresh = [](QComboBox *control, QString text) { if (control->count() == 0) return; QSignalBlocker blocker(control); const auto manualText = automatic(control) ? QString() : control->currentText(); control->setItemText(0, "*  " + text); if (control->isEditable() && control->currentIndex() == 0 && !manualText.isEmpty()) control->setEditText(manualText); };
    refresh(dictionary, CompressionMath::sizeText(calculated.dictionary)); refresh(word, QString::number(calculated.word)); refresh(solid, CompressionMath::sizeText(calculated.solid)); refresh(threads, QString::number(calculated.threads));
    compressMemory->setText(CompressionMath::sizeText(calculated.compressMemory, true)); decompressMemory->setText(CompressionMath::sizeText(calculated.decompressMemory, true));
}
ArchiveRequest AddDialog::options() const {
    ArchiveRequest r; r.operation = ArchiveOperation::Add; r.archive = archive->text(); r.format = format->currentText(); r.level = level->currentData().toInt(); r.method = r.level == 0 && r.format != "tar" && r.format != "Hash" ? "Copy" : selectedMethod(); r.methodAutomatic = automatic(method) && r.format != "tar" && r.format != "Hash";
    r.dictionary = dictionary->isEnabled() && !automatic(dictionary) ? unitOption(CompressionMath::sizeText(numericChoice(dictionary))) : QString(); r.wordSize = word->isEnabled() && !automatic(word) ? word->currentText() : QString();
    r.solid = !solid->isEnabled() ? QString() : solidCommand(solid);
    r.threads = threads->isEnabled() && !automatic(threads) ? threads->currentText().toInt() : 0; r.updateMode = QStringList{"add", "update", "fresh", "sync"}.at(update->currentIndex());
    r.volume = r.format == "Hash" ? QString() : volume->text().trimmed(); r.password = password->isEnabled() ? password->text() : QString(); r.encryptNames = encryptNames->isEnabled() && encryptNames->isChecked(); r.encryptionMethod = encryption->currentText() == "ZipCrypto" ? "ZipCrypto" : "AES256";
    r.pathMode = QStringList{"relative", "full", "absolute"}.at(paths->currentIndex()); r.parameters = parameters->text(); r.compressionMemory = memory->isEnabled() && !automatic(memory) ? unitOption(memory->currentText()) : QString();
    r.timestampPrecision = advanced.timestampPrecision; r.modificationTime = advanced.modificationTime; r.creationTime = advanced.creationTime; r.accessTime = advanced.accessTime; r.latestArchiveTime = advanced.latestArchiveTime; r.preserveAccessTime = advanced.preserveAccessTime; r.storeSymbolicLinks = advanced.storeSymbolicLinks; r.storeHardLinks = advanced.storeHardLinks; r.deleteAfter = deleteAfter->isChecked();
    r.latestArchiveTimeSpecified = advanced.latestArchiveTimeSpecified;
    const auto caps = ArchiveFormats::find(r.format);
    if (!caps.modificationTime) r.modificationTime = -1;
    if (!caps.creationTime || r.format == "tar" || (r.format == "zip" && r.timestampPrecision > 0)) r.creationTime = -1;
    if ((!caps.accessTime && r.format != "tar") || (r.format == "tar" && r.method != "POSIX") || (r.format == "zip" && r.timestampPrecision > 0)) r.accessTime = -1;
    r.storeSymbolicLinks = caps.symbolicLinks && r.storeSymbolicLinks; r.storeHardLinks = caps.hardLinks && r.storeHardLinks;
    return r;
}
void AddDialog::accept() {
    saveOptionsInMemory(); QSettings settings;
    for (auto format = formatDrafts.cbegin(); format != formatDrafts.cend(); ++format) {
        settings.beginGroup("Compression/" + format.key());
        for (auto value = format.value().cbegin(); value != format.value().cend(); ++value) settings.setValue(value.key(), value.value());
        settings.endGroup();
    }
    settings.setValue("Compression/LastFormat", currentFormat);
    auto history = settings.value("Compression/History").toStringList(); history.removeAll(archive->text()); history.prepend(archive->text()); while (history.size() > 20) history.removeLast(); settings.setValue("Compression/History", history); QDialog::accept();
}
ExtractDialog::ExtractDialog(QString dest, bool selected, QWidget *parent) : QDialog(parent) {
    setWindowTitle("Extract"); setObjectName("extractDialog"); resize(630, 310);
    auto layout = new QVBoxLayout(this); auto destinationLabel = new QLabel("E&xtract to:", this); layout->addWidget(destinationLabel);
    QSettings s; auto row = new QHBoxLayout; auto history = combo(s.value("Extract/History").toStringList(), this); history->setEditable(true); destination = history->lineEdit(); destination->setText(QFileInfo(dest).absolutePath()); destinationLabel->setBuddy(history); destination->setObjectName("extractDestination"); row->addWidget(history); auto browse = new QPushButton("...", this); browse->setFixedWidth(30); row->addWidget(browse); layout->addLayout(row);
    auto nameRow = new QHBoxLayout; nameEnabled = new QCheckBox(this); nameEnabled->setObjectName("extractNameEnabled"); nameEnabled->setAccessibleName("Extract to named subfolder"); nameEnabled->setChecked(s.value("Extract/NameEnabled", true).toBool()); subfolder = new QLineEdit(QFileInfo(dest).fileName(), this); subfolder->setObjectName("extractSubfolder"); subfolder->setVisible(nameEnabled->isChecked()); nameRow->addWidget(nameEnabled); nameRow->addWidget(subfolder); nameRow->addStretch(); layout->addLayout(nameRow); connect(nameEnabled, &QCheckBox::toggled, subfolder, &QLineEdit::setVisible);
    connect(browse, &QPushButton::clicked, this, [this] { auto p = QFileDialog::getExistingDirectory(this, "Extract", destination->text()); if (!p.isEmpty()) destination->setText(p); });
    auto columns = new QHBoxLayout; auto left = new QFormLayout;
    paths = combo({"Full pathnames", "No pathnames", "Absolute pathnames"}, this); paths->setObjectName("extractPathMode"); paths->setCurrentIndex(qBound(0, s.value("Extract/PathMode", 0).toInt(), 2)); left->addRow("Path mode:", paths);
    eliminate = new QCheckBox("Eliminate duplication of root folder", this); eliminate->setObjectName("extractEliminateRoot"); eliminate->setChecked(FileManagerSettings::load().eliminateRoot); left->addRow(eliminate);
    connect(paths, &QComboBox::currentIndexChanged, this, [this] { eliminate->setEnabled(paths->currentIndex() == 0); });
    eliminate->setEnabled(paths->currentIndex() == 0);
    overwrite = combo({"Ask before overwrite", "Overwrite without prompt", "Skip existing files", "Auto rename", "Auto rename existing files"}, this); overwrite->setObjectName("extractOverwriteMode"); overwrite->setCurrentIndex(qBound(0, s.value("Extract/Overwrite", 0).toInt(), 4)); left->addRow("Overwrite mode:", overwrite);
    selection = combo({"All files", "Selected files"}, this); selection->setObjectName("extractFilesMode"); if (selected) selection->setCurrentIndex(1); else disableChoice(selection, 1, "No files are selected."); auto selectionLabel = new QLabel("Files:", this); selectionLabel->setBuddy(selection); left->addRow(selectionLabel, selection); columns->addLayout(left);
    auto right = new QVBoxLayout; auto group = new QGroupBox("Password", this); auto pw = new QVBoxLayout(group); password = new QLineEdit(this); password->setEchoMode(QLineEdit::Password); password->setObjectName("extractPassword"); pw->addWidget(password); showPassword = new QCheckBox("Show Password", this); pw->addWidget(showPassword); right->addWidget(group);
    connect(showPassword, &QCheckBox::toggled, this, [this](bool checked) { password->setEchoMode(checked ? QLineEdit::Normal : QLineEdit::Password); }); showPassword->setChecked(s.value("Extract/ShowPassword", false).toBool());
    auto security = new QCheckBox("Restore file security", this); security->setEnabled(false); right->addWidget(security); right->addStretch(); columns->addLayout(right); layout->addLayout(columns);
    auto b = buttons(this); layout->addWidget(b); connect(b, &QDialogButtonBox::accepted, this, [this] {
        if (destination->text().trimmed().isEmpty()) return;
        if (nameEnabled->isChecked() && (!SevenZipProcessBackend::safeArchivePath(subfolder->text()) || subfolder->text().contains('/'))) { QMessageBox::warning(this, "7-Zip", "Specify a valid subfolder name."); return; }
        if (paths->currentIndex() == 2 && QMessageBox::question(this, "7-Zip", "Absolute pathnames can restore files outside the extraction folder, to paths stored in the archive. Continue?", QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) return;
        accept();
    });
    UiLanguage::bindTitle(this, IDD_EXTRACT); UiLanguage::bind(destinationLabel, IDT_EXTRACT_EXTRACT_TO); UiLanguage::bindFormLabel(paths, IDT_EXTRACT_PATH_MODE); UiLanguage::bindFormLabel(overwrite, IDT_EXTRACT_OVERWRITE_MODE);
    UiLanguage::bind(group, IDG_PASSWORD); UiLanguage::bind(eliminate, IDX_EXTRACT_ELIM_DUP); UiLanguage::bind(security, IDX_EXTRACT_NT_SECUR); UiLanguage::bind(showPassword, IDX_PASSWORD_SHOW);
    for (int n = 0; n < paths->count(); ++n) paths->setItemData(n, 3411u + unsigned(n), UiLanguage::ResourceIdRole);
    for (int n = 0; n < overwrite->count(); ++n) overwrite->setItemData(n, 3421u + unsigned(n), UiLanguage::ResourceIdRole);
    auto geometry = new ResourceDialogLayout(this, IDD_EXTRACT);
    geometry->bind("IDC_EXTRACT_PATH", history); geometry->bind("IDB_EXTRACT_SET_PATH", browse); geometry->bind("IDX_EXTRACT_NAME_ENABLE", nameEnabled); geometry->bind("IDE_EXTRACT_NAME", subfolder); geometry->bind("IDC_EXTRACT_PATH_MODE", paths); geometry->bind("IDC_EXTRACT_OVERWRITE_MODE", overwrite); geometry->bind("IDE_EXTRACT_PASSWORD", password);
    geometry->extra(selectionLabel, {8, 146, 60, 8}); geometry->extra(selection, {72, 144, 96, 14}); geometry->bindButtons(b); geometry->install();
    UiLanguage::apply(this);
}
ArchiveRequest ExtractDialog::options() const {
    ArchiveRequest r; r.operation = ArchiveOperation::Extract; r.outputDirectory = nameEnabled->isChecked() ? QDir(destination->text()).filePath(subfolder->text()) : destination->text(); r.pathMode = QStringList{"full", "flat", "absolute"}.at(paths->currentIndex());
    r.overwriteMode = QStringList{"ask", "overwrite", "skip", "rename", "renameExisting"}.at(overwrite->currentIndex()); r.password = password->text(); r.eliminateRoot = eliminate->isChecked() && paths->currentIndex() == 0; return r;
}
bool ExtractDialog::selectedOnly() const { return selection->currentIndex() == 1; }
void ExtractDialog::accept() {
    QSettings().setValue("Options/EliminateRoot", eliminate->isChecked());
    QSettings s; s.setValue("Extract/PathMode", paths->currentIndex()); s.setValue("Extract/Overwrite", overwrite->currentIndex()); s.setValue("Extract/NameEnabled", nameEnabled->isChecked()); s.setValue("Extract/ShowPassword", showPassword->isChecked()); auto history = s.value("Extract/History").toStringList(); history.removeAll(destination->text()); history.prepend(destination->text()); while (history.size() > 32) history.removeLast(); s.setValue("Extract/History", history); QDialog::accept();
}

ProgressDialog::ProgressDialog(QString operation, QString targetPath, QWidget *parent) : QDialog(parent), target(targetPath) {
    titleWindow = parent ? parent->window() : nullptr;
    setObjectName("progressDialog"); operationTitle = operation; setProperty("uiManagedTitle", true); setWindowTitle(operation); resize(620, 370); setWindowModality(Qt::WindowModal);
    connect(this, &QDialog::finished, this, [this] {
        titleRestored = true;
        if (titleWindow && !appliedTitlePrefix.isEmpty()) {
            auto text = titleWindow->windowTitle();
            if (text.startsWith(appliedTitlePrefix)) text.remove(0, appliedTitlePrefix.size());
            titleWindow->setWindowTitle(text); appliedTitlePrefix.clear();
        }
    });
    auto layout = new QVBoxLayout(this); status = new QLabel(this); status->setObjectName("progressStatus"); status->setProperty("uiLiteral", true); status->setTextFormat(Qt::PlainText); layout->addWidget(status);
    auto row = new QHBoxLayout; auto left = new QFormLayout; auto right = new QFormLayout;
    elapsed = new QLabel(officialProgressTime(0), this); errors = new QLabel("0", this); totalSize = new QLabel(this); processedSize = new QLabel(this);
    remaining = new QLabel(this); speed = new QLabel(this); errors->setObjectName("progressErrors");
    filesTotal = new QLabel(this); filesTotal->setObjectName("progressFilesTotal"); filesTotal->setProperty("uiLiteral", true);
    files = new QLabel("0", this); packedSize = new QLabel(this); ratio = new QLabel(this); files->setObjectName("progressFiles"); packedSize->setObjectName("progressPacked"); ratio->setObjectName("progressRatio"); processedSize->setObjectName("progressProcessed"); totalSize->setObjectName("progressTotal"); speed->setObjectName("progressSpeed"); remaining->setObjectName("progressRemaining");
    left->addRow("Elapsed time:", elapsed); left->addRow("Remaining time:", remaining); left->addRow("Files:", files); left->addRow("Errors:", errors);
    right->addRow("Total size:", totalSize); right->addRow("Speed:", speed); right->addRow("Processed:", processedSize); right->addRow("Compressed size:", packedSize); right->addRow("Compression ratio:", ratio); row->addLayout(left); row->addLayout(right); layout->addLayout(row);
    current = new QLabel(this); current->setObjectName("progressFileName"); current->setTextFormat(Qt::PlainText); layout->addWidget(current);
    bar = new QProgressBar(this); bar->setTextVisible(false); bar->setRange(0, 100); bar->setValue(0); layout->addWidget(bar);
    messages = new QTreeWidget(this); messages->setObjectName("progressMessages"); messages->setColumnCount(2); messages->setHeaderHidden(true); messages->setRootIsDecorated(false); messages->setSelectionMode(QAbstractItemView::ExtendedSelection); messages->setAllColumnsShowFocus(true); layout->addWidget(messages);
    auto copy = new QAction(UiLanguage::text("Copy"), messages); copy->setObjectName("copyProgressMessages"); copy->setShortcut(QKeySequence("Ctrl+C")); copy->setShortcutContext(Qt::WidgetWithChildrenShortcut); messages->addAction(copy); messages->setContextMenuPolicy(Qt::ActionsContextMenu);
    connect(copy, &QAction::triggered, this, [this] { QVector<unsigned> selected; for (auto item : messages->selectedItems()) selected.append(unsigned(messages->indexOfTopLevelItem(item))); QApplication::clipboard()->setText(presentation.copyMessages(selected)); });
    // Console diagnostics are retained separately from the original numbered
    // error list, including fallback failures without a native error packet.
    operationLog = new QPlainTextEdit(this); operationLog->setObjectName("progressOperationLog"); operationLog->setReadOnly(true); operationLog->hide();
    auto actions = new QHBoxLayout; actions->addStretch();
    backgroundButton = new QPushButton("Background", this); backgroundButton->setObjectName("backgroundOperation"); actions->addWidget(backgroundButton);
    connect(backgroundButton, &QPushButton::clicked, this, [this] { setBackground(!background); });
    pauseButton = new QPushButton("Pause", this); pauseButton->setObjectName("pauseOperation"); pauseButton->setEnabled(false); actions->addWidget(pauseButton); connect(pauseButton, &QPushButton::clicked, this, [this] { emit pauseRequested(!paused); });
    cancelButton = new QPushButton("Cancel", this); cancelButton->setObjectName("cancelOperation"); actions->addWidget(cancelButton); connect(cancelButton, &QPushButton::clicked, this, &ProgressDialog::requestCancel); layout->addLayout(actions);
    clock.start(); timer.setInterval(1000); connect(&timer, &QTimer::timeout, this, &ProgressDialog::refreshPresentation); timer.start();
    for (const auto &pair : {qMakePair(elapsed, IDT_PROGRESS_ELAPSED), qMakePair(remaining, IDT_PROGRESS_REMAINING), qMakePair(files, IDT_PROGRESS_FILES), qMakePair(errors, IDT_PROGRESS_ERRORS), qMakePair(totalSize, IDT_PROGRESS_TOTAL), qMakePair(speed, IDT_PROGRESS_SPEED), qMakePair(processedSize, IDT_PROGRESS_PROCESSED), qMakePair(packedSize, IDT_PROGRESS_PACKED), qMakePair(ratio, IDT_PROGRESS_RATIO)}) { UiLanguage::bindFormLabel(pair.first, pair.second); pair.first->setProperty("uiLiteral", true); }
    current->setProperty("uiLiteral", true); UiLanguage::bind(backgroundButton, IDB_PROGRESS_BACKGROUND); UiLanguage::bind(pauseButton, IDB_PAUSE); UiLanguage::bind(cancelButton, 402);
    auto geometry = new ResourceDialogLayout(this, 97);
    for (const auto &pair : {qMakePair(QString("IDT_PROGRESS_ELAPSED_VAL"), elapsed), qMakePair(QString("IDT_PROGRESS_REMAINING_VAL"), remaining), qMakePair(QString("IDT_PROGRESS_FILES_VAL"), files), qMakePair(QString("IDT_PROGRESS_FILES_TOTAL"), filesTotal), qMakePair(QString("IDT_PROGRESS_ERRORS_VAL"), errors), qMakePair(QString("IDT_PROGRESS_TOTAL_VAL"), totalSize), qMakePair(QString("IDT_PROGRESS_SPEED_VAL"), speed), qMakePair(QString("IDT_PROGRESS_PROCESSED_VAL"), processedSize), qMakePair(QString("IDT_PROGRESS_PACKED_VAL"), packedSize), qMakePair(QString("IDT_PROGRESS_RATIO_VAL"), ratio), qMakePair(QString("IDT_PROGRESS_STATUS"), status), qMakePair(QString("IDT_PROGRESS_FILE_NAME"), current)}) geometry->bind(pair.first, pair.second);
    geometry->bind("IDC_PROGRESS1", bar); geometry->bind("IDL_PROGRESS_MESSAGES", messages); geometry->bind("IDCANCEL", cancelButton); geometry->install();
    presentationReady = true; lastProgress.titleFileName = target; refreshPresentation(); UiLanguage::apply(this);
}
ProgressDialog::~ProgressDialog() { MacProcessPriority::release(this); }
void ProgressDialog::refreshPresentation() {
    if (!presentationReady) return;
    QString title = UiLanguage::text(operationTitle);
    if (operationTitle == "Add" || operationTitle == "Compressing") title = UiLanguage::resource(3301);
    else if (operationTitle == "Extract" || operationTitle == "Extracting") title = UiLanguage::resource(3300);
    else if (operationTitle == "Test" || operationTitle == "Testing") title = UiLanguage::resource(3302);
    presentation.setLanguage(title, UiLanguage::resource(IDB_PAUSE), UiLanguage::resource(IDS_CONTINUE), UiLanguage::resource(IDS_PROGRESS_PAUSED), UiLanguage::resource(IDB_PROGRESS_BACKGROUND), UiLanguage::resource(IDS_PROGRESS_FOREGROUND));
    presentation.setFileNameCapacity(property("progressFileNameCapacity").toUInt());
    if (running) { auto snapshot = lastProgress; snapshot.status = officialProgressStatus(snapshot.status); presentation.update(snapshot, quint64(qMax(qint64(0), activeMilliseconds()))); }
    const auto &view = presentation.view();
    for (const auto &pair : {qMakePair(120u, elapsed), qMakePair(121u, remaining), qMakePair(111u, files), qMakePair(112u, filesTotal), qMakePair(126u, errors), qMakePair(122u, totalSize), qMakePair(123u, speed), qMakePair(124u, processedSize), qMakePair(110u, packedSize), qMakePair(125u, ratio), qMakePair(103u, status)}) pair.second->setText(view.text.value(pair.first));
    current->setText(view.text.value(102));
    UiLanguage::bind(pauseButton, paused ? IDS_CONTINUE : IDB_PAUSE); UiLanguage::bind(backgroundButton, background ? IDS_PROGRESS_FOREGROUND : IDB_PROGRESS_BACKGROUND);
    pauseButton->setText(view.text.value(IDB_PAUSE)); backgroundButton->setText(view.text.value(IDB_PROGRESS_BACKGROUND)); setWindowTitle(view.title);
    if (titleWindow && !titleRestored) {
        auto base = titleWindow->windowTitle(); if (!appliedTitlePrefix.isEmpty() && base.startsWith(appliedTitlePrefix)) base.remove(0, appliedTitlePrefix.size());
        appliedTitlePrefix = view.parentTitlePrefix; titleWindow->setWindowTitle(appliedTitlePrefix + base);
    }
    bar->setRange(0, view.barMaximum); bar->setValue(view.barPosition);
    if (!running) {
        UiLanguage::bind(cancelButton, IDS_CLOSE); cancelButton->setText(UiLanguage::resource(IDS_CLOSE));
        cancelButton->setEnabled(true); cancelButton->setDefault(view.cancelDefault);
        pauseButton->setVisible(!view.hidePause); backgroundButton->setVisible(!view.hideBackground);
    }
    for (qsizetype n = messageRows; n < view.messages.size(); ++n) new QTreeWidgetItem(messages, {view.messages[n].first, view.messages[n].second});
    messageRows = view.messages.size();
    if (messageColumns != view.columnRevision) { messages->resizeColumnToContents(0); messages->resizeColumnToContents(1); messageColumns = view.columnRevision; }
    errors->setVisible(view.errorsVisible); messages->setVisible(view.errorsVisible);
    for (auto label : findChildren<QLabel *>()) if (label->property("resourceControlId").toUInt() == IDT_PROGRESS_ERRORS) label->setVisible(view.errorsVisible);
}
void ProgressDialog::update(int, quint64 processed, quint64 total, QString file, bool estimated) {
    if (nativePresentation && estimated) return;
    lastProgress = {}; lastProgress.completed = processed; if (total) lastProgress.total = total;
    lastProgress.current = file; lastProgress.titleFileName = target; refreshPresentation();
}
void ProgressDialog::updateDetails(const ArchiveProgress &progress) {
    nativePresentation = true; lastProgress = progress; if (lastProgress.titleFileName.isEmpty()) lastProgress.titleFileName = target; refreshPresentation();
}
void ProgressDialog::append(QString text) {
    text.replace('\r', '\n'); text.replace(QChar('\b'), QChar(' '));
    operationLog->moveCursor(QTextCursor::End); operationLog->insertPlainText(text);
}
void ProgressDialog::reportError(ArchiveOperationError error) {
    const auto text = officialProgressError(error); if (text.isEmpty()) return;
    presentation.addError(text, quint64(qMax(qint64(0), activeMilliseconds()))); ++errorCount; refreshPresentation();
}
void ProgressDialog::finish(const ArchiveResult &result) {
    if (!running) return;
    if (!result.success) append("\n" + result.message + "\n7-Zip exit code: " + QString::number(result.exitCode) + "\n" + result.details);
    Completion completion; completion.cancelled = result.cancelled;
    if (!result.cancelled) {
        // Original Sync.Messages retains individual file failures. Only a
        // fatal operation failure belongs in FinalMessage.ErrorMessage.
        if (!result.success && errorCount == 0)
            completion.error = result.message + "\nTarget: " + result.target + "\n7-Zip exit code: " + QString::number(result.exitCode) + (result.details.isEmpty() ? QString() : '\n' + result.details);
        if (result.success && result.operation == ArchiveOperation::Test) {
            completion.title = UiLanguage::resource(3302);
            if (result.testInside && result.completion.hash) completion.ok = officialInsideTestResult(*result.completion.hash);
            else if (result.completion.decompression)
                completion.ok = result.testInside ? officialInsideTestResult(*result.completion.decompression, lastProgress.current) : officialTestResult(*result.completion.decompression);
            else completion.ok = UiLanguage::resource(3001); // Unenhanced engine compatibility.
        }
        if (result.completion.hash && (result.operation == ArchiveOperation::Hash || result.operation == ArchiveOperation::HashArchive))
            completion.checksum = officialHashResults(*result.completion.hash, result.completion.decompression);
    }
    complete(std::move(completion));
}
void ProgressDialog::finishForRetry() {
    // End only this worker attempt without a final failure notice or a Cancel
    // question. The caller retains the real result and starts its password
    // retry; the archive operation itself is not reported as cancelled.
    Completion completion; completion.cancelled = true; complete(std::move(completion));
}
void ProgressDialog::complete(Completion completion) {
    if (!running) return;
    if (confirmingCancel) { deferredCompletion = std::move(completion); return; }
    if (background) setBackground(false);
    setPaused(false); refreshPresentation();
    running = false; timer.stop(); pauseButton->setEnabled(false);
    presentation.finish(completion.error, completion.ok, completion.title, completion.cancelled, quint64(qMax(qint64(0), activeMilliseconds())));
    refreshPresentation();
    // ProcessWasFinished_GuiVirt presents the original property list before
    // the progress dialog handles FinalMessage and its numbered error list.
    if (completion.checksum) { ChecksumResultsDialog dialog(*completion.checksum, this); dialog.exec(); }
    const auto view = presentation.view();
    for (const auto &notice : view.finalNotices) {
        QMessageBox message(notice.error ? QMessageBox::Critical : QMessageBox::Information, notice.title, notice.text, QMessageBox::Ok, this);
        message.setObjectName("progressFinalMessage"); message.setTextFormat(Qt::PlainText);
        message.setProperty("uiLiteral", true); UiLanguage::apply(&message); message.exec();
    }
    if (view.ended) accept();
    else { show(); raise(); }
}
qint64 ProgressDialog::activeMilliseconds() const { return clock.elapsed() - pausedMilliseconds - (paused ? pauseClock.elapsed() : 0); }
void ProgressDialog::setPauseAvailable(bool available) { pauseButton->setEnabled(running && available); }
void ProgressDialog::setPaused(bool value) {
    if (paused == value) return;
    if (value) { presentation.tick(quint64(qMax(qint64(0), activeMilliseconds())), true); pauseClock.start(); } else pausedMilliseconds += pauseClock.elapsed();
    paused = value; presentation.setPaused(value); refreshPresentation();
}
void ProgressDialog::setBackground(bool value) {
    if (background == value) return;
    const auto error = MacProcessPriority::lease(this, value);
    if (!error.isEmpty()) { reportError({-1, false, {}, {}, error}); return; }
    background = value; presentation.setBackground(value); refreshPresentation(); emit backgroundRequested(value);
}
void ProgressDialog::requestCancel() {
    if (!running) { accept(); return; }
    if (confirmingCancel || cancelWasRequested) return;
    const bool wasPaused = paused;
    if (!wasPaused && pauseButton->isEnabled()) emit pauseRequested(true);
    confirmingCancel = true;
    QMessageBox question(QMessageBox::Question, UiLanguage::text(operationTitle), UiLanguage::resource(IDS_PROGRESS_ASK_CANCEL), QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel, this);
    question.setObjectName("progressCancelConfirmation"); question.setTextFormat(Qt::PlainText); question.setDefaultButton(QMessageBox::Yes); question.setEscapeButton(QMessageBox::Cancel); UiLanguage::apply(&question);
    const auto answer = question.exec(); confirmingCancel = false;
    if (!wasPaused && paused) emit pauseRequested(false);
    if (deferredCompletion) { const auto result = *deferredCompletion; deferredCompletion.reset(); complete(result); return; }
    if (answer != QMessageBox::Yes) return;
    cancelWasRequested = true; cancelButton->setEnabled(false); emit cancelRequested();
}
void ProgressDialog::finishFile(QString error) {
    Completion completion; completion.cancelled = error.contains("cancelled", Qt::CaseInsensitive);
    if (!completion.cancelled && errorCount == 0) completion.error = error;
    if (!error.isEmpty()) append(error);
    complete(std::move(completion));
}
bool ProgressDialog::event(QEvent *event) {
    if (presentationReady && event->type() == QEvent::DynamicPropertyChange) {
        auto change = static_cast<QDynamicPropertyChangeEvent *>(event);
        if (change->propertyName() == "progressFileNameCapacity") refreshPresentation();
    }
    return QDialog::event(event);
}
void ProgressDialog::reject() { if (running) requestCancel(); else QDialog::reject(); }
