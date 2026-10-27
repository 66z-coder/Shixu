#pragma once

#include <QString>
#include <QStringList>
#include <QVector>
#include <QWidget>

class QLabel;
class QVBoxLayout;

// Presentation data kept separate from the recommendation algorithm's
// temporary recipe and inventory types. The integration layer will map the
// finalized recommendation result into this structure.
struct RecommendationViewItem
{
    QString recipeId;
    QString dishName;
    double totalScore = 0.0;
    double ingredientMatchScore = 0.0;
    double expiryUrgencyScore = 0.0;
    double preferenceMatchScore = 0.0;
    double nutritionScore = 0.0;
    double preparationTimeScore = 0.0;
    QStringList reasons;
};

class RecommendationWidget : public QWidget
{
public:
    explicit RecommendationWidget(QWidget* parent = nullptr);

    void setRecommendations(const QVector<RecommendationViewItem>& items);

private:
    QLabel* m_summary = nullptr;
    QWidget* m_emptyState = nullptr;
    QVBoxLayout* m_resultsLayout = nullptr;
};
