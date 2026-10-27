#pragma once

#include <QDate>
#include <QList>
#include <QString>
#include <QWidget>

class QLabel;
class QGridLayout;
class QPaintEvent;
class RewardSystem;
class FridgeCanvas;

// UI snapshot; InventoryManager's FoodCandidate values will be mapped here at the integration boundary.
struct HomeFoodItem {
    QString name;
    double quantity = 0;
    QString unit;
    QDate expiryDate;
};

class HomeWidget : public QWidget
{
    Q_OBJECT

public:
    explicit HomeWidget(RewardSystem* rewards, QWidget* parent = nullptr);
    void setInventory(const QList<HomeFoodItem>& foods);

signals:
    void foodEntryRequested();
    void inventoryRequested();
    void recipesRequested();
    void checkInRequested();

private:
    void refresh();

protected:
    void paintEvent(QPaintEvent* event) override;

private:

    RewardSystem* m_rewards;
    FridgeCanvas* m_fridge;
    QLabel* m_inventoryCount;
    QLabel* m_expiringCount;
    QLabel* m_inventoryPreview;
    QLabel* m_expiringPreview;
    QLabel* m_checkInStatus;
    QGridLayout* m_foodGrid;
    QList<HomeFoodItem> m_foods;
    bool m_inventoryConnected = false;
};
