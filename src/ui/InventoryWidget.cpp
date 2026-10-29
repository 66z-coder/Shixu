#include "ui/InventoryWidget.h"

#include <QDate>
#include <QHeaderView>
#include <QLabel>
#include <QTableWidget>
#include <QVBoxLayout>

InventoryWidget::InventoryWidget(QWidget *parent) : QWidget(parent) {
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(16);

    m_summary = new QLabel(this);
    m_summary->setObjectName(QStringLiteral("inventorySummary"));
    layout->addWidget(m_summary);

    m_table = new QTableWidget(0, 4, this);
    m_table->setHorizontalHeaderLabels(
        {QStringLiteral("食材"), QStringLiteral("数量"), QStringLiteral("到期日期"), QStringLiteral("状态")});
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setAlternatingRowColors(true);
    m_table->verticalHeader()->hide();
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    layout->addWidget(m_table, 1);

    setStyleSheet(QStringLiteral(R"(
        QLabel#inventorySummary { color: #405347; font-size: 16px; font-weight: 600; }
        QTableWidget { background: #FFFFFF; alternate-background-color: #F3F8F6;
            color: #28485D; gridline-color: #DCE8EF; border: 1px solid #DDE7E5; border-radius: 8px; }
        QHeaderView::section { background: #E4F0ED; color: #294A4B;
            border: 0; border-bottom: 1px solid #CAD9D8; padding: 10px; font-weight: 700; }
    )"));
    setItems({});
}

void InventoryWidget::setItems(const QList<FoodCandidate> &items) {
    m_summary->setText(items.isEmpty() ? QStringLiteral("冰箱里还没有食材。录入后会在这里显示。")
                                       : QStringLiteral("当前共有 %1 项食材").arg(items.size()));
    m_table->setRowCount(static_cast<int>(items.size()));
    const QDate today = QDate::currentDate();
    for (int row = 0; row < items.size(); ++row) {
        const FoodCandidate &food = items[row];
        const QString quantity = QStringLiteral("%1 %2").arg(food.quantity).arg(food.unit);
        QString status = QStringLiteral("未设置日期");
        if (food.expiryDate.isValid()) {
            const int days = today.daysTo(food.expiryDate);
            status = days < 0    ? QStringLiteral("已过期")
                     : days <= 3 ? QStringLiteral("临期")
                                 : QStringLiteral("正常");
        }
        const QStringList columns = {food.name, quantity,
                                     food.expiryDate.isValid()
                                         ? food.expiryDate.toString(QStringLiteral("yyyy-MM-dd"))
                                         : QStringLiteral("—"),
                                     status};
        for (int column = 0; column < columns.size(); ++column) {
            auto *cell = new QTableWidgetItem(columns[column]);
            cell->setFlags(cell->flags() & ~Qt::ItemIsEditable);
            m_table->setItem(row, column, cell);
        }
    }
}
