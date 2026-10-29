#pragma once

#include <QString>
#include <QStringList>
#include <QVector>
#include <QWidget>
#include <vector>

#include "recommendation/RecommendationEngine.h"
#include "contracts/RecommendationContract.h"

class QButtonGroup;
class QLabel;
class QSpinBox;
class QPushButton;
class QVBoxLayout;

struct RecommendationViewItem
{
    int recipeId = 0;
    QString dishName;
    int cookingTimeMinutes = 0;
    QString moodName;
    int effectiveMaxPreparationMinutes = 0;
    double totalScore = 0.0;
    double ingredientMatchScore = 0.0;
    double expiryUrgencyScore = 0.0;
    double moodMatchScore = 0.0;
    double nutritionScore = 0.0;
    bool nutritionScoreAvailable = false;
    double preparationTimeScore = 0.0;
    int preparationTimeReferenceMinutes = 0;
    QStringList missingIngredients;
    QStringList reasons;
};

class RecommendationWidget : public QWidget
{
    Q_OBJECT

public:
    explicit RecommendationWidget(QWidget* parent = nullptr);

    void setWarnings(const QStringList& warnings);
    void setRecommendations(const QVector<RecommendationViewItem>& items);
    void selectMood(Shixu::RecommendationEngine::Mood mood);
    void requestRecommendations();
    void selectMoodV1(Shixu::Contracts::V1::Mood mood);
    void setRecommendationResult(const Shixu::Contracts::V1::RecommendationResult& result);
    void resetExcludedRecipeIds();

signals:
    void moodSelected(Shixu::RecommendationEngine::Mood mood);
    void moodSelectedV1(Shixu::Contracts::V1::Mood mood);
    void recommendationRequestedV1(const Shixu::Contracts::V1::RecommendationPreferences& preferences);
    void recipeSelected(int recipeId);
    void recommendationRequested(Shixu::RecommendationEngine::Mood mood,
                                 int maxPreparationMinutes);

private:
    void sendV1Request();
    void updateQuickButtons();
    QButtonGroup* m_moodButtons = nullptr;
    Shixu::Contracts::V1::Mood m_moodV1 = Shixu::Contracts::V1::Mood::NotSelected;
    Shixu::Contracts::V1::InventoryPolicy m_policy = Shixu::Contracts::V1::InventoryPolicy::PreferInventory;
    Shixu::Contracts::V1::Refinement m_refinement = Shixu::Contracts::V1::Refinement::None;
    std::vector<int> m_excludedIds;
    std::vector<int> m_visibleIds;
    QVBoxLayout* m_quickLayout = nullptr;
    QPushButton* m_swapButton = nullptr;
    QLabel* m_policyHint = nullptr;
    QSpinBox* m_customLimit = nullptr;
    QLabel* m_selectionHint = nullptr;
    QLabel* m_warning = nullptr;
    QLabel* m_summary = nullptr;
    QLabel* m_emptyTitle = nullptr;
    QLabel* m_emptyDescription = nullptr;
    QWidget* m_emptyState = nullptr;
    QVBoxLayout* m_resultsLayout = nullptr;
};
