#include "ui/InventoryWidget.h"
#include "ui/AppTheme.h"
#include "model/FoodCategory.h"
#include "core/AppClock.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDateEdit>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QStyledItemDelegate>
#include <QStyle>
#include <QTableWidget>
#include <QVBoxLayout>
#include <cmath>

namespace {
QString displayNumber(double value) {
    if (value > 0 && value < 0.0001) return QStringLiteral("<0.0001");
    QString result = QString::number(value, 'f', 4);
    if (result.contains('.')) {
        while (result.endsWith('0')) result.chop(1);
        if (result.endsWith('.')) result.chop(1);
    }
    return result;
}

QString displayQuantity(const FoodCandidate &food) {
    const QString unit = food.unit.trimmed().normalized(QString::NormalizationForm_KC).toLower();
    const bool counted = QStringList{QStringLiteral("个"), QStringLiteral("盒"), QStringLiteral("袋"),
        QStringLiteral("瓶"), QStringLiteral("包"), QStringLiteral("罐"), QStringLiteral("根"),
        QStringLiteral("颗"), QStringLiteral("枚"), QStringLiteral("只"), QStringLiteral("支"),
        QStringLiteral("把"), QStringLiteral("份")}.contains(unit);
    const QString raw = QStringLiteral("%1 %2").arg(displayNumber(food.quantity), food.unit);
    if (!counted) return raw;
    if (food.netContentPerUnit > 0 && (food.netContentUnit == "ml" || food.netContentUnit == "L"))
        return QStringLiteral("%1 · 共 %2 %3").arg(raw, displayNumber(food.quantity * food.netContentPerUnit), food.netContentUnit);
    if (!std::isfinite(food.gramsPerUnit) || food.gramsPerUnit <= 0)
        return raw + QStringLiteral(" · 重量未知");
    const double whole = std::floor(food.quantity);
    const double fraction = food.quantity - whole;
    if (fraction < 1e-9) return raw;
    const QString remainder = displayNumber(fraction * food.gramsPerUnit);
    if (whole < 1) return QStringLiteral("约 %1 克").arg(remainder);
    return QStringLiteral("约 %1 %2当量 + %3 克")
        .arg(displayNumber(whole), food.unit, remainder);
}
// Windows' native item style can paint a dark hover even with a light palette.
// Remove that native hover state and give the item an explicit light background.
class InventoryItemDelegate final : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override {
        QStyleOptionViewItem adjusted(option);
        initStyleOption(&adjusted, index);
        if (adjusted.state.testFlag(QStyle::State_MouseOver)) {
            adjusted.state &= ~QStyle::State_MouseOver;
            if (!adjusted.state.testFlag(QStyle::State_Selected))
                adjusted.backgroundBrush = QColor(QStringLiteral("#EDF7FC"));
        }
        QStyledItemDelegate::paint(painter, adjusted, index);
    }
};
} // namespace

InventoryWidget::InventoryWidget(QWidget *parent) : QWidget(parent) {
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(16);
    m_summary = new QLabel(this);
    m_summary->setObjectName("inventorySummary");
    m_summary->setWordWrap(true);
    layout->addWidget(m_summary);
    auto *filter = new QHBoxLayout;
    filter->addWidget(new QLabel(QStringLiteral("分类"), this));
    m_category = new QComboBox(this);
    m_category->setObjectName("inventoryCategoryFilter");
    m_category->addItem(QStringLiteral("全部"));
    m_category->addItems(foodCategories());
    filter->addWidget(m_category);
    filter->addStretch();
    layout->addLayout(filter);
    m_table = new QTableWidget(0, 5, this);
    m_table->setObjectName("inventoryTable");
    m_table->setHorizontalHeaderLabels({QStringLiteral("食材"), QStringLiteral("分类"),
                                        QStringLiteral("数量"), QStringLiteral("到期日期"),
                                        QStringLiteral("状态")});
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setMouseTracking(true);
    m_table->setItemDelegate(new InventoryItemDelegate(m_table));
    m_table->setAlternatingRowColors(true);
    m_table->verticalHeader()->hide();
    m_table->verticalHeader()->setDefaultSectionSize(46);
    m_table->setShowGrid(false);
    m_table->setWordWrap(true);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    for (int i = 1; i < 5; ++i)
        m_table->horizontalHeader()->setSectionResizeMode(i, QHeaderView::ResizeToContents);
    layout->addWidget(m_table, 1);
    auto *edit = new QPushButton(QStringLiteral("编辑选中食材"), this);
    edit->setObjectName("inventoryEdit");
    edit->setEnabled(false);
    layout->addWidget(edit, 0, Qt::AlignLeft);
    connect(m_table, &QTableWidget::itemSelectionChanged, this, [this, edit] {
        edit->setEnabled(!m_table->selectedItems().isEmpty());
    });
    connect(edit, &QPushButton::clicked, this, [this] { editItem(m_table->currentRow()); });
    connect(m_table, &QTableWidget::cellDoubleClicked, this, [this](int row, int) { editItem(row); });
    connect(m_category, &QComboBox::currentIndexChanged, this, [this] { renderItems(); });
    setStyleSheet(QStringLiteral(R"(
        QLabel#inventorySummary { color:#244E6A; font-size:18px; font-weight:600; }
        QTableWidget { background:white; alternate-background-color:#F3F8FC; color:#28485D; gridline-color:#DCE8EF; border:1px solid #DCE8EF; border-radius:14px; outline:0; }
        QTableWidget::item:hover { background:#EDF7FC; color:#244E6A; }
        QTableWidget::item:selected { background:#DCEEF8; color:#244E6A; }
        QTableWidget::item:selected:hover { background:#D5EAF6; color:#244E6A; }
        QHeaderView::section { background:#E8F3FA; color:#315970; border:0; padding:10px; font-weight:600; }
    )"));
    setItems({});
}
void InventoryWidget::setUpdateHandler(std::function<bool(FoodCandidate, QString*)> handler) { m_update = std::move(handler); }
void InventoryWidget::setItems(const QList<FoodCandidate> &items) { m_items = items; renderItems(); }
void InventoryWidget::renderItems() {
    m_table->setRowCount(0);
    const QString selected = m_category->currentText();
    for (const auto &food : m_items) {
        if (m_category->currentIndex() && food.category != selected) continue;
        const int row = m_table->rowCount(); m_table->insertRow(row);
        QString status = QStringLiteral("未设置日期");
        if (food.expiryDate.isValid()) {
            const int days = Shixu::appNow().date().daysTo(food.expiryDate);
            status = days < 0 ? QStringLiteral("已过期") : days <= 3 ? QStringLiteral("临期") : QStringLiteral("正常");
        }
        const QStringList columns{food.name, food.category, displayQuantity(food),
            food.expiryDate.isValid() ? food.expiryDate.toString(Qt::ISODate) : QStringLiteral("—"), status};
        for (int column = 0; column < columns.size(); ++column) {
            auto *cell = new QTableWidgetItem(columns[column]);
            cell->setData(Qt::UserRole, food.batchId);
            cell->setToolTip(columns[column]);
            if (column >= 3) cell->setTextAlignment(Qt::AlignCenter);
            if (column == 4 && food.expiryDate.isValid()) {
                const int days = Shixu::appNow().date().daysTo(food.expiryDate);
                cell->setForeground(QColor(days < 0 ? "#A34B3D" : days <= 3 ? "#9A6629" : "#38755C"));
            }
            m_table->setItem(row, column, cell);
        }
    }
    m_table->resizeRowsToContents();
    m_summary->setText(m_items.isEmpty() ? QStringLiteral("冰箱里还没有食材。录入后会在这里显示。")
        : QStringLiteral("当前显示 %1 / %2 项食材").arg(m_table->rowCount()).arg(m_items.size()));
}
void InventoryWidget::editItem(int row) {
    if (row < 0 || !m_table->item(row, 0) || !m_update) return;
    const qint64 id = m_table->item(row, 0)->data(Qt::UserRole).toLongLong();
    FoodCandidate food;
    bool found = false;
    for (const auto &item : m_items) if (item.batchId == id) { food = item; found = true; break; }
    if (!found) return;
    QDialog dialog(this); dialog.setWindowTitle(QStringLiteral("编辑食材")); dialog.setObjectName("inventoryEditDialog");
    dialog.setMinimumWidth(420);
    dialog.setStyleSheet(QStringLiteral(R"(
        QDialog#inventoryEditDialog { background:#EDF7F9; color:#244E6A; }
        QLabel#inventoryEditTitle { color:#244E6A; font-size:20px; font-weight:700; }
        QLabel#inventoryEditHint { color:#617B8C; font-size:12px; }
        QLabel#inventoryEditError { color:#A23D30; }
    )"));
    auto *outer = new QVBoxLayout(&dialog); outer->setContentsMargins(24, 22, 24, 22); outer->setSpacing(14);
    auto *title = new QLabel(QStringLiteral("编辑食材"), &dialog); title->setObjectName("inventoryEditTitle"); outer->addWidget(title);
    auto *description = new QLabel(QStringLiteral("修改当前批次的信息，保存后同步更新库存。"), &dialog);
    description->setObjectName("inventoryEditHint"); description->setWordWrap(true); outer->addWidget(description);
    auto *form = new QFormLayout; form->setHorizontalSpacing(18); form->setVerticalSpacing(12);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    auto *name = new QLineEdit(food.name, &dialog); name->setObjectName("inventoryEditName");
    auto *category = new QComboBox(&dialog); category->setObjectName("inventoryEditCategory");
    category->addItems(foodCategories()); category->setCurrentText(food.category);
    auto *quantity = new QDoubleSpinBox(&dialog); quantity->setObjectName("inventoryEditQuantity");
    quantity->setDecimals(4); quantity->setRange(0.0001, 1000000000); quantity->setValue(food.quantity);
    auto *unit = new QLineEdit(food.unit, &dialog); unit->setObjectName("inventoryEditUnit");
    auto *weight = new QDoubleSpinBox(&dialog); weight->setObjectName("inventoryEditWeight");
    weight->setDecimals(4); weight->setRange(0, 1000000000); weight->setValue(food.netContentPerUnit > 0 ? food.netContentPerUnit : food.gramsPerUnit);
    const double displayedQuantity = quantity->value();
    const double displayedWeight = weight->value();
    weight->setSpecialValueText(QStringLiteral("未知"));
    auto* contentUnit = new QComboBox(&dialog); contentUnit->setObjectName("inventoryNetContentUnit");
    contentUnit->addItems({"g", "kg", "ml", "L"}); contentUnit->setCurrentText(food.netContentUnit);
    auto* contentRow = new QHBoxLayout; contentRow->addWidget(weight, 3); contentRow->addWidget(contentUnit, 1);
    auto *hasDate = new QCheckBox(QStringLiteral("设置到期日期"), &dialog); hasDate->setChecked(food.expiryDate.isValid());
    auto *date = new QDateEdit(food.expiryDate.isValid() ? food.expiryDate : Shixu::appNow().date(), &dialog);
    date->setCalendarPopup(true); date->setDisplayFormat("yyyy-MM-dd"); date->setEnabled(hasDate->isChecked());
    connect(hasDate, &QCheckBox::toggled, date, &QWidget::setEnabled);
    form->addRow(QStringLiteral("名称"), name); form->addRow(QStringLiteral("分类"), category);
    form->addRow(QStringLiteral("数量"), quantity); form->addRow(QStringLiteral("单位"), unit);
    form->addRow(QStringLiteral("单件净含量（计数单位）"), contentRow); form->addRow(hasDate, date); outer->addLayout(form);
    auto *hint = new QLabel(QStringLiteral("计数单位可填写单件净含量；体积按 ml/L 保存，不直接换算克重。重量单位按数量自动计算。"), &dialog);
    hint->setObjectName("inventoryEditHint"); hint->setWordWrap(true); outer->addWidget(hint);
    auto *errorLabel = new QLabel(&dialog); errorLabel->setObjectName("inventoryEditError");
    errorLabel->setWordWrap(true); errorLabel->hide(); outer->addWidget(errorLabel);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dialog);
    buttons->button(QDialogButtonBox::Save)->setText(QStringLiteral("保存修改"));
    buttons->button(QDialogButtonBox::Save)->setObjectName("inventorySave");
    Shixu::markPrimary(buttons->button(QDialogButtonBox::Save));
    buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));
    outer->addWidget(buttons); connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, [&] {
        FoodCandidate updated = food;
        updated.name = name->text(); updated.category = category->currentText();
        if (quantity->value() != displayedQuantity) updated.quantity = quantity->value();
        updated.unit = unit->text();
        if (weight->value() != displayedWeight || contentUnit->currentText() != food.netContentUnit) {
            updated.netContentPerUnit = weight->value(); updated.netContentUnit = contentUnit->currentText();
            updated.gramsPerUnit = updated.netContentUnit == "g" ? weight->value()
                : updated.netContentUnit == "kg" ? weight->value() * 1000 : 0;
        }
        updated.expiryDate = hasDate->isChecked() ? date->date() : QDate();
        QString error;
        if (m_update(updated, &error)) dialog.accept();
        else { errorLabel->setText(error.isEmpty() ? QStringLiteral("保存失败，请检查输入后重试。") : error); errorLabel->show(); }
    });
    dialog.exec();
}
