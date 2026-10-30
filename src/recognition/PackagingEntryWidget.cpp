#include "ui/WheelGuard.h"
#include "recognition/PackagingEntryWidget.h"
#include "recognition/PackagingDateParser.h"
#include "model/FoodCategory.h"
#include "recognition/FoodCategorySuggestion.h"
#include <QCalendarWidget>
#include <QDialog>
#include <QDialogButtonBox>
#include <QSignalBlocker>
#include <QLineEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QPlainTextEdit>
#include <QCheckBox>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QFileDialog>
#include <QImageReader>
#include <QMessageBox>
#include <QPixmap>
#include <QListWidget>
#include <QScrollArea>
#include <QPainter>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QFileInfo>
#include <QUrl>

namespace {
// Paint the original image into the current preview area without imposing its width.
class PackagingPreview final : public QLabel
{
public:
    explicit PackagingPreview(QWidget* parent) : QLabel(parent) {
        setAlignment(Qt::AlignCenter);
        setMinimumWidth(0);
        setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        setFixedHeight(144);
        setText("选择文件或拖入一张本地图片");
    }
    void showImage(const QImage& image) {
        original_ = QPixmap::fromImage(image);
        setText(original_.isNull() ? QStringLiteral("选择文件或拖入一张本地图片") : QString());
        update();
    }
protected:
    void paintEvent(QPaintEvent* event) override {
        QLabel::paintEvent(event);
        if (original_.isNull()) return;
        const QRect area = contentsRect().adjusted(12, 12, -12, -12);
        const auto scaled = original_.scaled(area.size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
        QPainter painter(this);
        painter.drawPixmap(area.center() - QPoint(scaled.width()/2, scaled.height()/2), scaled);
    }
private:
    QPixmap original_;
};

// Optional numeric fields keep zero as "unknown", but show a real placeholder.
// Unlike specialValueText, the placeholder is not part of the editable number.
template<class Base, class Number>
class OptionalNumberSpinBox : public Base
{
public:
    explicit OptionalNumberSpinBox(QWidget* parent, const QString& hint) : Base(parent)
    {
        this->lineEdit()->setPlaceholderText(hint);
        QObject::connect(this->lineEdit(), &QLineEdit::textEdited, this,
                         [this](const QString& text) {
            if (text.trimmed().isEmpty()) this->setValue(0);
        });
    }
protected:
    QString textFromValue(Number value) const override
    {
        return value == 0 ? QString() : Base::textFromValue(value);
    }
    Number valueFromText(const QString& text) const override
    {
        return text.trimmed().isEmpty() ? Number(0) : Base::valueFromText(text);
    }
    QValidator::State validate(QString& text, int& position) const override
    {
        return text.trimmed().isEmpty() ? QValidator::Acceptable : Base::validate(text, position);
    }
};
using OptionalIntSpinBox = OptionalNumberSpinBox<QSpinBox, int>;
using OptionalDoubleSpinBox = OptionalNumberSpinBox<QDoubleSpinBox, double>;

bool isCountUnit(const QString& unit)
{
    return QStringList{"个", "盒", "袋", "瓶", "包", "罐", "根", "颗",
                       "枚", "只", "支", "把", "份"}.contains(unit);
}
QString weightText(double grams)
{
    return grams > 0 ? QString("总重量：%1 g").arg(grams, 0, 'f', 1)
                     : QStringLiteral("总重量：未知");
}
}


PackagingEntryWidget::PackagingEntryWidget(QWidget* parent) : QWidget(parent)
{
    Shixu::installWheelGuard();
    setWindowTitle("食序 - 包装日期识别与食材录入");
    resize(1100, 800);
    setObjectName("packagingEntry");
    setAttribute(Qt::WA_StyledBackground, true);
    setStyleSheet(QStringLiteral(R"(
        QWidget#packagingEntry { background: #EDF7F9; color: #24465B; font-size: 14px; }
        QWidget#entryEditor, QScrollArea#entryScroll { background: transparent; border: 0; }
        QFrame[entryCard="true"] { background: #FFFFFF; border: 1px solid #E1EDF0; border-radius: 14px; }
        QLabel { color: #38586C; background: transparent; border: 0; }
        QLabel[entryHeading="true"] { color: #244E6A; font-size: 18px; font-weight: 600; }
        QLabel[entryHint="true"] { color: #627E90; font-size: 12px; }
        QLabel#packagePreview { background: #EFF7FA; border: 1px dashed #C8DEE8; border-radius: 10px; color: #7893A0; }
        QLabel#entryStatus, QLabel#entrySummary { background: #EFF7FA; border-radius: 8px; padding: 10px; }
        QListWidget { background: #FFFFFF; color: #38586C; border: 1px solid #E1EDF0; border-radius: 10px; padding: 4px; outline: 0; }
        QListWidget::item { padding: 12px 8px; border-bottom: 1px solid #EDF3F6; }
        QListWidget::item:selected { background: #DCEEF8; color: #17416B; border-radius: 6px; }
        QListWidget::item:hover { background: #EFF7FA; }
        QListWidget::item:selected:hover { background:#DCEEF8; }
        QCheckBox { color: #38586C; spacing: 8px; }
    )"));
    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(16);
    auto* scroll = new QScrollArea(this);
    scroll->setObjectName("entryScroll");
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto* editor = new QWidget(scroll);
    editor->setObjectName("entryEditor");
    auto* sections = new QVBoxLayout(editor);
    sections->setContentsMargins(0, 0, 4, 0);
    sections->setSpacing(16);
    auto makeCard = [](QWidget* parent) {
        auto* card = new QFrame(parent);
        card->setProperty("entryCard", true);
        return card;
    };
    auto heading = [](const QString& text, QWidget* parent) {
        auto* label = new QLabel(text, parent);
        label->setProperty("entryHeading", true);
        label->setWordWrap(true);
        return label;
    };
    auto* recognitionCard = makeCard(editor);
    recognitionCard->setObjectName("packageDropArea");
    auto* layout = new QVBoxLayout(recognitionCard);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(12);
    layout->addWidget(heading("包装日期识别", recognitionCard));
    sections->addWidget(recognitionCard);
    scroll->setWidget(editor);
    root->addWidget(scroll, 3);
    auto* batchCard = makeCard(this);
    batchCard->setMinimumWidth(220);
    auto* batch = new QVBoxLayout(batchCard);
    batch->setContentsMargins(20, 20, 20, 20);
    batch->setSpacing(12);
    batch->addWidget(heading("待提交食材", batchCard));
    auto* batchHint = new QLabel("逐项添加后统一提交\n双击列表中的食材可修改", batchCard);
    batchHint->setProperty("entryHint", true);
    batchHint->setWordWrap(true);
    batch->addWidget(batchHint);
    list_ = new QListWidget(this); list_->setObjectName("pendingFoods");
    list_->setMinimumWidth(0); list_->setWordWrap(true);
    list_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    batch->addWidget(list_, 1);
    edit_ = new QPushButton("修改选中食材", this); edit_->setObjectName("editFood");
    remove_ = new QPushButton("删除选中食材", this); remove_->setObjectName("deleteFood"); remove_->setProperty("danger", true);
    submit_ = new QPushButton("确认提交全部食材", this); submit_->setObjectName("submitCandidate"); submit_->setProperty("primary", true);
    batch->addWidget(edit_); batch->addWidget(remove_); batch->addWidget(submit_);
    root->addWidget(batchCard, 2);
    auto* intro = new QLabel("可导入或拖入一张本地包装图片，点击识别后核对日期；也可直接手动录入。", this);
    intro->setWordWrap(true);
    intro->setProperty("entryHint", true);
    layout->addWidget(intro);
    auto* buttons = new QHBoxLayout();
    choose_ = new QPushButton("导入包装图片", this);
    choose_->setObjectName("choosePackageImage");
    recognize_ = new QPushButton("识别包装文字", this);
    cancel_ = new QPushButton("取消识别", this);
    recognize_->setEnabled(false);
    cancel_->setEnabled(false);
    layout->addWidget(choose_);
    buttons->setSpacing(12);
    buttons->addWidget(recognize_); buttons->addWidget(cancel_);
    layout->addLayout(buttons);
    preview_ = new PackagingPreview(this);
    preview_->setObjectName("packagePreview");
    layout->addWidget(preview_);
    raw_ = new QPlainTextEdit(this);
    raw_->setObjectName("packageText");
    raw_->setPlaceholderText("例如：生产日期：2026-10-20\n保质期：30天\n或：有效期至：2026-11-19");
    raw_->setMaximumHeight(110);
    layout->addWidget(raw_);
    parse_ = new QPushButton("解析日期信息", this);
    layout->addWidget(parse_);
    status_ = new QLabel("请填写食材信息，并选择日期依据。", this);
    status_->setObjectName("entryStatus");
    status_->setTextFormat(Qt::PlainText);
    status_->setWordWrap(true);
    layout->addWidget(status_);
    auto* foodCard = makeCard(editor);
    layout = new QVBoxLayout(foodCard);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(12);
    layout->addWidget(heading("食材信息", foodCard));
    sections->addWidget(foodCard);
    sections->addStretch();
    auto* form = new QFormLayout();
    form_ = form;
    form->setSpacing(12);
    form->setRowWrapPolicy(QFormLayout::WrapAllRows);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    name_ = new QLineEdit(this); name_->setObjectName("foodName");
    category_ = new QComboBox(this); category_->setObjectName("foodCategory");
    category_->addItems(foodCategories()); category_->setCurrentText(QStringLiteral("其他"));
    quantity_ = new QDoubleSpinBox(this); quantity_->setObjectName("foodQuantity");
    quantity_->setDecimals(1); quantity_->setRange(.1, 9999); quantity_->setValue(1);
    unit_ = new QComboBox(this); unit_->setObjectName("foodUnit"); unit_->setEditable(true);
    unit_->addItems({"", "个", "盒", "袋", "瓶", "包", "kg", "g", "斤", "ml", "L"});
    unit_->setCurrentIndex(0);
    mode_ = new QComboBox(this); mode_->setObjectName("dateMode");
    mode_->addItems({"请选择日期依据", "已知到期日期（识别或手动填写）", "生产日期＋保质期", "到期日期未知（手动录入）", "从今天起保存"});
    expiry_ = new QLineEdit(this); expiry_->setObjectName("expiryDate");
    production_ = new QLineEdit(this); production_->setObjectName("productionDate");
    expiry_->setPlaceholderText("请选择到期日期"); production_->setPlaceholderText("请选择生产日期");
    auto dateRow = [this](QLineEdit *field) {
        auto *row = new QWidget(this); auto *box = new QHBoxLayout(row);
        box->setContentsMargins(0, 0, 0, 0); box->addWidget(field, 1);
        field->setReadOnly(true);
        auto *choose = new QPushButton(QStringLiteral("选择日期"), row); box->addWidget(choose);
        connect(choose, &QPushButton::clicked, this, [this, field] {
            QDialog dialog(this); dialog.setWindowTitle(QStringLiteral("选择日期"));
            auto *layout = new QVBoxLayout(&dialog); auto *calendar = new QCalendarWidget(&dialog);
            const QDate current = QDate::fromString(field->text(), Qt::ISODate);
            calendar->setSelectedDate(current.isValid() ? current : QDate::currentDate());
            layout->addWidget(calendar);
            auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
            layout->addWidget(buttons);
            connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
            connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
            if (dialog.exec() == QDialog::Accepted) field->setText(calendar->selectedDate().toString(Qt::ISODate));
        });
        return row;
    };
    expiryRow_ = dateRow(expiry_); productionRow_ = dateRow(production_);
    period_ = new OptionalIntSpinBox(this, "请输入保质期"); period_->setObjectName("shelfLife");
    period_->setRange(0, 10000);
    periodUnit_ = new QComboBox(this); periodUnit_->setObjectName("shelfLifeUnit");
    periodUnit_->addItems({"天", "月", "年"});
    form->addRow("食材名称", name_); form->addRow("分类（自动建议，可修改）", category_); form->addRow("数量", quantity_); form->addRow("单位", unit_);
    unitWeight_ = new OptionalDoubleSpinBox(this, "可选，未填写表示重量未知"); unitWeight_->setObjectName("gramsPerUnit");
    unitWeight_->setDecimals(1); unitWeight_->setRange(0, 1000000);
    unitWeightLabel_ = new QLabel(this);
    form->addRow(unitWeightLabel_, unitWeight_);
    totalWeight_ = new QLabel(this); totalWeight_->setObjectName("totalWeight");
    totalWeight_->setWordWrap(true);
    form->addRow("总重量", totalWeight_);
    form->addRow("日期依据", mode_); form->addRow("明确到期日", expiryRow_);
    form->addRow("生产日期", productionRow_); form->addRow("保质期数值", period_);
    form->addRow("保质期单位", periodUnit_);
    storageDays_ = new OptionalIntSpinBox(this, "请输入天数，例如 3"); storageDays_->setObjectName("storageDays");
    storageDays_->setRange(0, 10000);
    storageDaysLabel_ = new QLabel("预计保存天数（天）", this);
    form->addRow(storageDaysLabel_, storageDays_);
    layout->addLayout(form);
    computed_ = new QLabel(this); computed_->setObjectName("entrySummary"); computed_->setWordWrap(true); computed_->setTextFormat(Qt::PlainText);
    layout->addWidget(computed_);
    confirmed_ = new QCheckBox("已核对当前食材信息和日期\n（如未知，已明确选择未知）", this);
    confirmed_->setObjectName("confirmed"); layout->addWidget(confirmed_);
    save_ = new QPushButton("添加食材到列表", this); save_->setObjectName("saveFood"); save_->setProperty("primary", true);
    reset_ = new QPushButton("清空表单 / 取消修改", this); reset_->setObjectName("resetForm");
    layout->addWidget(save_); layout->addWidget(reset_);
    QList<QWidget*> dropTargets=recognitionCard->findChildren<QWidget*>();dropTargets.append(recognitionCard);
    for(auto* target:dropTargets){target->setAcceptDrops(true);target->installEventFilter(this);}
    for (auto* button : findChildren<QPushButton*>()) {
        button->setMinimumHeight(36);
        button->setCursor(Qt::PointingHandCursor);
    }
    for (auto* field : findChildren<QLineEdit*>()) {
        if (!qobject_cast<QAbstractSpinBox*>(field->parentWidget()) && !qobject_cast<QComboBox*>(field->parentWidget()))
            field->setMinimumHeight(36);
    }
    for (auto* field : findChildren<QAbstractSpinBox*>()) field->setMinimumHeight(36);
    for (auto* field : findChildren<QComboBox*>()) {
        field->setMinimumHeight(36);
        field->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
        field->setMinimumContentsLength(8);
        field->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }
    connect(choose_, &QPushButton::clicked, this, &PackagingEntryWidget::chooseImage);
    connect(parse_, &QPushButton::clicked, this, &PackagingEntryWidget::parseText);
    connect(cancel_, &QPushButton::clicked, &ocr_, &ReceiptOcrService::cancel);
    connect(recognize_, &QPushButton::clicked, this, [this] {
        confirmed_->setChecked(false);
        choose_->setEnabled(false); recognize_->setEnabled(false); cancel_->setEnabled(true); parse_->setEnabled(false);
        raw_->setReadOnly(true);
        status_->setText("正在本地识别包装文字……首次需加载模型，后续图片会更快。");
        ocr_.start(imagePath_);
        refresh();
    });
    connect(&ocr_, &ReceiptOcrService::completed, this, [this](const ReceiptOcrResult& result) {
        choose_->setEnabled(true); recognize_->setEnabled(!imagePath_.isEmpty()); cancel_->setEnabled(false); parse_->setEnabled(true);
        raw_->setReadOnly(false);
        if (!result.success) { status_->setText(result.errorMessage); refresh(); return; }
        QStringList lines;
        for (const auto& line : result.lines) lines << line.text;
        raw_->setPlainText(lines.join('\n'));
        parseText();
    });
    auto changed = [this] { confirmed_->setChecked(false); refresh(); };
    for (auto* edit : {name_, production_, expiry_}) connect(edit, &QLineEdit::textChanged, this, changed);
    connect(raw_, &QPlainTextEdit::textChanged, this, [this] {
        // 原文改变后清除上一份解析结果，避免新文字配上旧日期。
        production_->clear(); expiry_->clear(); period_->setValue(0);
        mode_->setCurrentIndex(0);
        confirmed_->setChecked(false); refresh();
    });
    connect(quantity_, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, changed);
    connect(unitWeight_, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, changed);
    // 换商品或单位后清除旧单件重量，避免把一个的重量误用于一盒。
    connect(unit_, &QComboBox::currentTextChanged, this, [this] { unitWeight_->setValue(0); });
    connect(name_, &QLineEdit::textChanged, this, [this] {
        unitWeight_->setValue(0);
        if (!categoryManual_) {
            QSignalBlocker blocker(category_);
            category_->setCurrentText(suggestFoodCategory(normalizer_.normalizeFoodName(name_->text())));
        }
        refresh();
    });
    connect(category_, QOverload<int>::of(&QComboBox::activated), this, [this](int) { categoryManual_ = true; });
    connect(category_, &QComboBox::currentTextChanged, this, changed);
    connect(period_, QOverload<int>::of(&QSpinBox::valueChanged), this, changed);
    connect(storageDays_, QOverload<int>::of(&QSpinBox::valueChanged), this, changed);
    connect(mode_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
        if (index == 4 && !storageStart_.isValid()) storageStart_ = QDate::currentDate();
        refresh();
    });
    for (auto* combo : {unit_, mode_, periodUnit_})
        connect(combo, &QComboBox::currentTextChanged, this, changed);
    connect(confirmed_, &QCheckBox::toggled, this, &PackagingEntryWidget::refresh);
    connect(save_, &QPushButton::clicked, this, [this] {
        FoodCandidate candidate;
        const QString error = validate(candidate);
        if (!error.isEmpty() || !confirmed_->isChecked() || ocr_.isBusy()) return;
        Entry entry{candidate, raw_->toPlainText(), production_->text(), expiry_->text(), imagePath_,
                    mode_->currentIndex(), period_->value(), periodUnit_->currentIndex(), storageDays_->value(), storageStart_};
        QString text = QString("%1 × %2 %3 | 到期：%4").arg(candidate.name).arg(candidate.quantity)
            .arg(candidate.unit, candidate.expiryDate.isValid() ? candidate.expiryDate.toString(Qt::ISODate) : QStringLiteral("未知"));
        text += " | " + candidate.category + " | " + weightText(candidate.totalWeightGrams);
        if (editingRow_ >= 0) {
            entries_[editingRow_] = entry;
            list_->item(editingRow_)->setText(text);
        } else {
            entries_.push_back(entry); list_->addItem(text);
        }
        clearForm();
        status_->setText("已保存到待提交列表，可继续录入下一件食材。");
    });
    connect(reset_, &QPushButton::clicked, this, &PackagingEntryWidget::clearForm);
    connect(edit_, &QPushButton::clicked, this, &PackagingEntryWidget::editSelected);
    connect(list_, &QListWidget::itemDoubleClicked, this, [this] { editSelected(); });
    connect(remove_, &QPushButton::clicked, this, [this] {
        const int row = list_->currentRow();
        if (ocr_.isBusy() || row < 0) return;
        entries_.erase(entries_.begin() + row);
        delete list_->takeItem(row);
        if (row == editingRow_) clearForm();
        else if (editingRow_ > row) --editingRow_;
        refresh();
    });
    connect(submit_, &QPushButton::clicked, this, [this] {
        if (!submit_->isEnabled()) return;
        std::vector<FoodCandidate> foods;
        for (const auto& entry : entries_) foods.push_back(entry.food);
        submitting_ = true;
        status_->setText("正在保存食材……");
        setEnabled(false);
        refresh();
        emit foodsConfirmed(foods);
    });
    connect(list_, &QListWidget::currentRowChanged, this, &PackagingEntryWidget::refresh);
    refresh();
}

void PackagingEntryWidget::completeSubmission(bool success, const QString& message)
{
    if (!submitting_) return;
    const int count = static_cast<int>(entries_.size());
    if (success) { entries_.clear(); list_->clear(); }
    submitting_ = false;
    setEnabled(true);
    refresh();
    status_->setText(message.isEmpty()
        ? (success ? QString("已成功保存 %1 条食材，可继续录入。").arg(count)
                   : QStringLiteral("保存失败，待提交内容已保留，请重试。"))
        : message);
}

void PackagingEntryWidget::chooseImage()
{
    const QString path = QFileDialog::getOpenFileName(this, "选择包装日期照片", imagePath_, "图片 (*.png *.jpg *.jpeg *.bmp *.webp *.PNG *.JPG *.JPEG)");
    if (path.isEmpty()) return;
    loadImageFile(path);
}

bool PackagingEntryWidget::loadImageFile(const QString& path)
{
    if(submitting_||ocr_.isBusy()||!isEnabled()){status_->setText("正在识别或提交，请完成后再替换图片。");return false;}
    if(!QFileInfo(path).isFile()){status_->setText("请导入一张本地图片文件，不能导入文件夹或链接。");return false;}
    QImageReader reader(path); reader.setAutoTransform(true);
    const auto image = reader.read();
    if (image.isNull()) { status_->setText("图片无法读取，请重新选择；当前内容已保留。"); return false; }
    imagePath_ = path;
    static_cast<PackagingPreview*>(preview_)->showImage(image);
    raw_->clear(); production_->clear(); expiry_->clear(); period_->setValue(0); mode_->setCurrentIndex(0);
    confirmed_->setChecked(false); recognize_->setEnabled(true);
    status_->setText(QStringLiteral("已导入：%1。请识别包装文字。").arg(QFileInfo(path).fileName()));
    refresh();
    return true;
}

bool PackagingEntryWidget::eventFilter(QObject* watched,QEvent* event)
{
    if(event->type()==QEvent::DragEnter||event->type()==QEvent::DragMove||event->type()==QEvent::Drop){
        auto* drop=static_cast<QDropEvent*>(event);
        const auto* mime=drop->mimeData();
        if(!mime->hasUrls())return QWidget::eventFilter(watched,event);
        if(submitting_||ocr_.isBusy()||!isEnabled()){
            status_->setText("正在识别或提交，请完成后再替换图片。");drop->ignore();return true;
        }
        const auto urls=mime->urls();
        if(urls.size()!=1||!urls.front().isLocalFile()||!QFileInfo(urls.front().toLocalFile()).isFile()){
            status_->setText("请一次拖入一张本地图片；不支持多文件、目录或远程链接。");drop->ignore();return true;
        }
        QImageReader reader(urls.front().toLocalFile());
        if(!reader.canRead()){
            status_->setText("图片无法读取；当前内容已保留。");drop->ignore();return true;
        }
        if(event->type()!=QEvent::Drop||loadImageFile(urls.front().toLocalFile()))drop->acceptProposedAction();
        else drop->ignore();
        return true;
    }
    return QWidget::eventFilter(watched,event);
}

void PackagingEntryWidget::parseText()
{
    const auto result = PackagingDateParser().parse(raw_->toPlainText());
    production_->clear(); expiry_->clear(); period_->setValue(0); mode_->setCurrentIndex(0);
    if (!result.requiresManualResolution) {
        if (result.productionDate.isValid()) production_->setText(result.productionDate.toString(Qt::ISODate));
        if (result.expiryDate.isValid()) expiry_->setText(result.expiryDate.toString(Qt::ISODate));
        period_->setValue(result.shelfLife); periodUnit_->setCurrentIndex(static_cast<int>(result.shelfLifeUnit));
        if (result.expiryDate.isValid()) mode_->setCurrentIndex(1);
        else if (result.productionDate.isValid() || result.shelfLife > 0) mode_->setCurrentIndex(2);
    }
    status_->setText(result.messages.isEmpty() ? "已解析标签日期，请核对字段后确认" : result.messages.join('\n'));
    confirmed_->setChecked(false);
    refresh();
}

QString PackagingEntryWidget::validate(FoodCandidate& candidate) const
{
    candidate.name = normalizer_.normalizeFoodName(name_->text());
    candidate.category = category_->currentText();
    candidate.quantity = quantity_->value();
    candidate.unit = normalizer_.normalizeUnit(unit_->currentText());
    if (candidate.unit == "g") candidate.totalWeightGrams = candidate.quantity;
    else if (candidate.unit == "kg") candidate.totalWeightGrams = candidate.quantity * 1000.0;
    else if (candidate.unit == "斤") candidate.totalWeightGrams = candidate.quantity * 500.0;
    else if (isCountUnit(candidate.unit)) {
        candidate.gramsPerUnit = unitWeight_->value();
        candidate.totalWeightGrams = candidate.quantity * candidate.gramsPerUnit;
    }
    if (candidate.name.isEmpty()) return "请填写食材名称";
    if (!normalizer_.isSupportedUnit(unit_->currentText())) return "请选择或输入映射表支持的单位（如 公斤/kg、毫升/ml、盒）";
    if (mode_->currentIndex() == 0) return "请选择日期依据";
    const QDate production = PackagingDateParser::parseDate(production_->text());
    if (mode_->currentIndex() == 1) {
        candidate.expiryDate = PackagingDateParser::parseDate(expiry_->text());
        if (!candidate.expiryDate.isValid()) return "请补充有效的完整到期日期（四位年份）";
    } else if (mode_->currentIndex() == 2) {
        if (!production.isValid()) return "请补充有效的完整生产日期";
        if (period_->value() <= 0) return "请补充明确的保质期数值";
        candidate.expiryDate = PackagingDateParser::calculateExpiry(production, period_->value(),
            static_cast<ShelfLifeUnit>(periodUnit_->currentIndex()), false);
        if (!candidate.expiryDate.isValid()) return "无法计算有效到期日，请检查输入";
    }
    if (mode_->currentIndex() == 4) {
        if (storageDays_->value() <= 0) return "请填写预计保存天数";
        if (!storageStart_.isValid()) return "请选择从今天起保存";
        candidate.expiryDate = storageStart_.addDays(storageDays_->value());
        if (!candidate.expiryDate.isValid()) return "无法计算到期日，请检查输入";
    }
    return {};
}

void PackagingEntryWidget::refresh()
{
    const bool calculated = mode_->currentIndex() == 2;
    const bool fromToday = mode_->currentIndex() == 4;
    storageDays_->setVisible(fromToday); storageDaysLabel_->setVisible(fromToday);
    form_->setRowVisible(expiryRow_, mode_->currentIndex() == 1);
    form_->setRowVisible(productionRow_, calculated);
    form_->setRowVisible(period_, calculated); form_->setRowVisible(periodUnit_, calculated);
    const QString normalizedUnit = normalizer_.normalizeUnit(unit_->currentText());
    const bool countUnit = isCountUnit(normalizedUnit);
    unitWeight_->setVisible(countUnit); unitWeightLabel_->setVisible(countUnit);
    unitWeightLabel_->setText(QString("每%1重量（g）").arg(normalizedUnit));
    FoodCandidate candidate;
    const QString error = validate(candidate);
    totalWeight_->setText(candidate.totalWeightGrams > 0
        ? QString("%1 g").arg(candidate.totalWeightGrams, 0, 'f', 1)
        : (normalizedUnit == "ml" || normalizedUnit == "L"
           ? QStringLiteral("未知（体积不能直接换算重量）")
           : QStringLiteral("未知，可补充重量或直接用 g/kg 录入")));
    QString summary = error;
    if (error.isEmpty()) {
        summary = QString("输出预览：%1 × %2 %3；到期日期 %4").arg(candidate.name).arg(candidate.quantity)
            .arg(candidate.unit, candidate.expiryDate.isValid() ? candidate.expiryDate.toString(Qt::ISODate) : QStringLiteral("未知"));
        if (fromToday) summary += QString("\n按起始日期 %1＋%2 天计算（用户预计保存时间）。").arg(storageStart_.toString(Qt::ISODate)).arg(storageDays_->value());
        if (calculated) summary += "\n按生产日期＋保质期计算；月/年按日历计算，目标月无对应日期时取月末。";
        if (candidate.expiryDate.isValid() && candidate.expiryDate < QDate::currentDate()) summary += "\n该日期早于今天，请核对。";
    }
    computed_->setText(summary);
    save_->setText(editingRow_ >= 0 ? "保存修改" : "添加食材到列表");
    save_->setEnabled(error.isEmpty() && confirmed_->isChecked() && !ocr_.isBusy());
    reset_->setEnabled(!ocr_.isBusy());
    edit_->setEnabled(list_->currentRow() >= 0 && !ocr_.isBusy());
    remove_->setEnabled(list_->currentRow() >= 0 && !ocr_.isBusy());
    const bool unfinished = !name_->text().isEmpty() || !raw_->toPlainText().isEmpty()
        || !imagePath_.isEmpty() || mode_->currentIndex() != 0 || !production_->text().isEmpty()
        || !expiry_->text().isEmpty() || period_->value() != 0 || !unit_->currentText().isEmpty()
        || quantity_->value() != 1;
    submit_->setEnabled(!submitting_ && !entries_.empty() && editingRow_ < 0 && !unfinished && !ocr_.isBusy());
}

void PackagingEntryWidget::clearForm()
{
    editingRow_ = -1;
    storageStart_ = QDate(); storageDays_->setValue(0);
    raw_->clear(); name_->clear(); production_->clear(); expiry_->clear();
    categoryManual_ = false; category_->setCurrentText(QStringLiteral("其他"));
    period_->setValue(0); periodUnit_->setCurrentIndex(0); mode_->setCurrentIndex(0);
    unitWeight_->setValue(0);
    unit_->setCurrentIndex(0); quantity_->setValue(1); confirmed_->setChecked(false);
    imagePath_.clear(); static_cast<PackagingPreview*>(preview_)->showImage(QImage());
    recognize_->setEnabled(false); list_->clearSelection(); list_->setCurrentRow(-1);
    status_->setText("可直接手动录入下一件，或导入新的包装图片。");
    refresh();
}

void PackagingEntryWidget::editSelected()
{
    const int row = list_->currentRow();
    if (row < 0 || ocr_.isBusy()) return;
    // 不静默覆盖正在填写的另一条数据。
    if (!name_->text().isEmpty() || !raw_->toPlainText().isEmpty() || mode_->currentIndex() != 0
        || !imagePath_.isEmpty() || !unit_->currentText().isEmpty() || quantity_->value() != 1
        || !production_->text().isEmpty() || !expiry_->text().isEmpty() || period_->value() != 0) {
        status_->setText("请先保存当前内容，或点击“清空表单 / 取消修改”，再打开另一条。");
        return;
    }
    const auto entry = entries_[row];
    storageStart_ = entry.storageStart; storageDays_->setValue(entry.storageDays);
    raw_->setPlainText(entry.raw);
    name_->setText(entry.food.name); quantity_->setValue(entry.food.quantity); unit_->setCurrentText(entry.food.unit);
    category_->setCurrentText(entry.food.category); categoryManual_ = true;
    unitWeight_->setValue(entry.food.gramsPerUnit);
    production_->setText(entry.production); expiry_->setText(entry.expiry);
    period_->setValue(entry.period); periodUnit_->setCurrentIndex(entry.periodUnit); mode_->setCurrentIndex(entry.mode);
    imagePath_ = entry.imagePath;
    if (!imagePath_.isEmpty()) {
        QImageReader reader(imagePath_); reader.setAutoTransform(true);
        static_cast<PackagingPreview*>(preview_)->showImage(reader.read());
    }
    recognize_->setEnabled(!imagePath_.isEmpty());
    editingRow_ = row; confirmed_->setChecked(false);
    status_->setText("正在修改选中食材，核对后点击保存修改。");
    refresh();
}
