#include <QApplication>
#include <QDate>
#include <QPushButton>
#include <QSpinBox>
#include <iostream>
#include "ui/RecommendationWidget.h"
#include "ui/RecipeDetailsWidget.h"
#include "ui/CookingConfirmationWidget.h"
namespace V = Shixu::Contracts::V1;
int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    RecommendationWidget recommendations;
    V::RecommendationPreferences emitted;
    int requests = 0, selected = 0;
    QObject::connect(&recommendations, &RecommendationWidget::recommendationRequestedV1,
        [&](const V::RecommendationPreferences& p) { emitted = p; ++requests; });
    QObject::connect(&recommendations, &RecommendationWidget::recipeSelected,
        [&](int id) { selected = id; });
    recommendations.requestRecommendations();
    if (requests != 0) return 1;
    recommendations.selectMoodV1(V::Mood::TreatMyself);
    recommendations.requestRecommendations();
    if (requests != 1 || emitted.mood != V::Mood::TreatMyself || emitted.maxPreparationMinutes != 75) return 2;
    V::RecommendationResult result;
    result.outcome = {V::OperationStatus::Succeeded, V::ErrorCode::None, {}};
    Shixu::RecommendationEngine::Recommendation card;
    card.recipeId = 12; card.dishName = QStringLiteral("番茄炒蛋");
    card.recipe.id = 12; card.recipe.name = "番茄炒蛋"; card.recipe.cookingTimeMinutes = 15;
    result.cards.push_back(card); result.hasMore = true;
    recommendations.setRecommendationResult(result);
    const auto details = recommendations.findChildren<QPushButton*>();
    for (auto* button : details) if (button->text() == QStringLiteral("查看菜谱详情")) button->click();
    if (selected != 12) return 3;
    for (auto* button : details) if (button->text() == QStringLiteral("换一批")) button->click();
    if (requests != 2 || emitted.excludedRecipeIds.size() != 1 || emitted.excludedRecipeIds.front() != 12) return 4;
    RecipeDetailsWidget recipe;
    int cookingId = 0;
    QObject::connect(&recipe, &RecipeDetailsWidget::cookingRequested, [&](int id) { cookingId = id; });
    recipe.setRecipe(card.recipe, card.nutrition, {});
    for (auto* button : recipe.findChildren<QPushButton*>()) button->click();
    if (cookingId != 12) return 5;
    CookingConfirmationWidget confirmation;
    int completions = 0;
    QObject::connect(&confirmation, &CookingConfirmationWidget::completionRequested,
        [&](const V::CookingCompletionRequest&, const QString&) { ++completions; });
    V::CookingCompletionPreview preview;
    preview.outcome = {V::OperationStatus::Succeeded, V::ErrorCode::None, {}};
    preview.confirmationToken = "opaque"; preview.snapshot.recipe = card.recipe;
    preview.snapshot.request.eventId = "5f778a96-91c7-4e42-a5ae-d7ad189d6105";
    preview.snapshot.request.recipeId = 12; preview.snapshot.request.servings = 1;
    V::CookingCompletionResult pending;
    pending.eventId = preview.snapshot.request.eventId;
    pending.stage = V::CompletionStage::Prepared;
    pending.outcome = {V::OperationStatus::NeedsConfirmation, V::ErrorCode::InventoryChanged, "Re-preview needed"};
    confirmation.setProgress(pending);
    confirmation.setBusy(false);
    QPushButton* retry = nullptr;
    QPushButton* confirm = nullptr;
    for (auto* button : confirmation.findChildren<QPushButton*>()) {
        if (button->text() == QStringLiteral("继续完成未结算步骤")) retry = button;
        if (button->text() == QStringLiteral("确认完成并记录")) confirm = button;
    }
    if (!retry || retry->isHidden() || !confirm || confirm->isEnabled()) return 6;
    confirmation.setPreview(preview);
    confirmation.setBusy(false);
    if (!retry->isHidden() || !confirm->isEnabled()) return 7;
    confirm->click();
    if (completions != 1) return 8;
    return 0;
}
