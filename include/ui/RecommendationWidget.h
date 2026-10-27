#pragma once

#include <QString>
#include <QStringList>
#include <QVector>
#include <QWidget>

#include "recommendation/RecommendationEngine.h"

class QButtonGroup;
class QLabel;
class QSpinBox;
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

signals:
    void moodSelected(Shixu::RecommendationEngine::Mood mood);
    void recommendationRequested(Shixu::RecommendationEngine::Mood mood,
                                 int maxPreparationMinutes);

private:
    QButtonGroup* m_moodButtons = nullptr;
    QSpinBox* m_customLimit = nullptr;
    QLabel* m_selectionHint = nullptr;
    QLabel* m_warning = nullptr;
    QLabel* m_summary = nullptr;
    QLabel* m_emptyTitle = nullptr;
    QLabel* m_emptyDescription = nullptr;
    QWidget* m_emptyState = nullptr;
    QVBoxLayout* m_resultsLayout = nullptr;
};
