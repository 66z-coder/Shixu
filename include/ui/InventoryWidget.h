#pragma once

#include <QList>
#include <QWidget>
#include <functional>

#include "model/FoodCandidate.h"

class QLabel;
class QTableWidget;
class QComboBox;

class InventoryWidget : public QWidget
{
    Q_OBJECT

public:
    explicit InventoryWidget(QWidget* parent = nullptr);
    void setItems(const QList<FoodCandidate>& items);
    void setUpdateHandler(std::function<bool(FoodCandidate, QString*)> handler);

private:
    QLabel* m_summary;
    QTableWidget* m_table;
    QComboBox* m_category;
    QList<FoodCandidate> m_items;
    std::function<bool(FoodCandidate, QString*)> m_update;
    void renderItems();
    void editItem(int row);
};
