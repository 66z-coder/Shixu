#pragma once

#include <QList>
#include <QWidget>

#include "model/FoodCandidate.h"

class QLabel;
class QTableWidget;

class InventoryWidget : public QWidget
{
    Q_OBJECT

public:
    explicit InventoryWidget(QWidget* parent = nullptr);
    void setItems(const QList<FoodCandidate>& items);

private:
    QLabel* m_summary;
    QTableWidget* m_table;
};
