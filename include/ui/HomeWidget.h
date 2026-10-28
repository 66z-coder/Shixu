#pragma once

#include <QDate>
#include <QList>
#include <QString>
#include <QWidget>

#include "recommendation/RecommendationEngine.h"
#include "contracts/CoreContracts.h"

class QButtonGroup;
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
    void selectMood(Shixu::RecommendationEngine::Mood mood);
    void selectMoodV1(Shixu::Contracts::V1::Mood mood);

signals:
    void foodEntryRequested();
    void inventoryRequested();
    void recipesRequested();
    void checkInRequested();
    void moodRecommendationRequested(Shixu::RecommendationEngine::Mood mood);
    void moodSelected(Shixu::RecommendationEngine::Mood mood);
    void moodSelectedV1(Shixu::Contracts::V1::Mood mood);
    void moodRecommendationRequestedV1(Shixu::Contracts::V1::Mood mood);

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
    QButtonGroup* m_moodButtons;
    QLabel* m_moodHint;
    QGridLayout* m_foodGrid;
    QList<HomeFoodItem> m_foods;
    bool m_inventoryConnected = false;
};
