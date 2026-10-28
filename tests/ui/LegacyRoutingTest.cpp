#include <QApplication>
#include <QPushButton>
#include <QTemporaryDir>
#include "reward/RewardSystem.h"
#include "ui/HomeWidget.h"
#include "ui/RecommendationWidget.h"
namespace V = Shixu::Contracts::V1;
namespace Old = Shixu::RecommendationEngine;
int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    QTemporaryDir dir;
    if (!dir.isValid()) return 1;
    RewardSystem reward(nullptr, dir.filePath(QStringLiteral("reward.ini")));
    HomeWidget home(&reward);
    RecommendationWidget recommendations;
    int oldHomeSelection = 0, oldHomeRequest = 0, newHomeRequest = 0;
    int oldRecommendationRequest = 0, newRecommendationRequest = 0;
    QObject::connect(&home, &HomeWidget::moodSelected, &recommendations,
                     &RecommendationWidget::selectMood);
    QObject::connect(&home, &HomeWidget::moodSelected, [&] (Old::Mood) { ++oldHomeSelection; });
    QObject::connect(&home, &HomeWidget::moodRecommendationRequested,
                     [&] (Old::Mood mood) {
        ++oldHomeRequest;
        recommendations.selectMood(mood);
        recommendations.requestRecommendations();
    });
    QObject::connect(&recommendations, &RecommendationWidget::recommendationRequested,
                     [&] (Old::Mood, int) { ++oldRecommendationRequest; });
    auto moods = home.findChildren<QPushButton*>(QStringLiteral("homeMoodChoice"));
    if (moods.size() != 5) return 2;
    moods.front()->click();
    if (oldHomeSelection != 1) return 3;
    auto action = home.findChild<QPushButton*>(QStringLiteral("homeMoodAction"));
    if (!action) return 4;
    action->click();
    if (oldHomeRequest != 1 || oldRecommendationRequest != 1) return 5;
    QObject::connect(&home, &HomeWidget::moodRecommendationRequestedV1,
                     [&] (V::Mood) { ++newHomeRequest; });
    action->click();
    if (newHomeRequest != 1 || oldHomeRequest != 1) return 6;
    QObject::connect(&recommendations, &RecommendationWidget::recommendationRequestedV1,
                     [&] (const V::RecommendationPreferences&) { ++newRecommendationRequest; });
    recommendations.requestRecommendations();
    if (newRecommendationRequest != 1 || oldRecommendationRequest != 1) return 7;
    return 0;
}
