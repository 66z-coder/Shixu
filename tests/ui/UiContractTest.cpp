#include "ui/CookingConfirmationWidget.h"
#include "ui/RecipeDetailsWidget.h"
#include "ui/RecommendationWidget.h"
#include <QApplication>
#include <QDate>
#include <QComboBox>
#include <QDialog>
#include <QFrame>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QProgressBar>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTimer>
#include <iostream>
namespace V = Shixu::Contracts::V1;
int main(int argc, char **argv) {
    QApplication app(argc, argv);
    RecommendationWidget recommendations;
    V::RecommendationPreferences emitted;
    int requests = 0, selected = 0;
    QObject::connect(&recommendations, &RecommendationWidget::recommendationRequestedV1,
                     [&](const V::RecommendationPreferences &p) {
                         emitted = p;
                         ++requests;
                     });
    QObject::connect(&recommendations, &RecommendationWidget::recipeStepsSelected,
                     [&](int id) { selected = id; });
    recommendations.requestRecommendations();
    if (requests != 0)
        return 1;
    recommendations.selectMoodV1(V::Mood::TreatMyself);
    recommendations.requestRecommendations();
    if (requests != 1 || emitted.mood != V::Mood::TreatMyself || emitted.maxPreparationMinutes != 75)
        return 2;
    V::RecommendationResult result;
    result.outcome = {V::OperationStatus::Succeeded, V::ErrorCode::None, {}};
    Shixu::RecommendationEngine::Recommendation card;
    card.recipeId = 12;
    card.dishName = QStringLiteral("番茄炒蛋");
    card.recipe.id = 12;
    card.recipe.name = "番茄炒蛋";
    card.recipe.cookingTimeMinutes = 15;
    card.recipe.tags = {V::Tags::Quick, V::Tags::Light};
    card.effectiveMaxPreparationMinutes = 30;
    card.availability.push_back({"番茄", 100, 50, 50, false, false, V::AvailabilityState::Insufficient});
    card.totalScore = 95.8;
    card.reasons = {QStringLiteral("食材齐全"), QStringLiteral("预计 15 分钟")};
    result.cards.push_back(card);
    result.hasMore = true;
    recommendations.setRecommendationResult(result);
    recommendations.findChild<QPushButton *>("recommendationExplanation")->click();
    auto *reasonPanel = recommendations.findChild<QFrame *>("recommendationReasonPanel");
    auto *score = reasonPanel->findChild<QLabel *>("reasonScore");
    if (reasonPanel->isHidden() || !score ||
        !score->text().contains(QStringLiteral("95.8")) ||
        !reasonPanel->findChild<QLabel *>("reasonStock")->text().contains(QStringLiteral("番茄")) ||
        !reasonPanel->findChild<QLabel *>("reasonTime")->text().contains(QStringLiteral("15 分钟")) ||
        !reasonPanel->findChild<QLabel *>("reasonText")->text().contains(QStringLiteral("快手"))) return 13;
    {
        // Each alternative must disclose its own evidence without requesting a new
        // batch, navigating, or affecting the other cards' expansion state.
        RecommendationWidget evidenceCards;
        auto threeCards = result;
        auto alternative = card;
        alternative.role = V::CardRole::AlternativeOne;
        alternative.recipeId = 13;
        alternative.totalScore = 82.4;
        alternative.reasons = {QStringLiteral("备选一：实际标签匹配"), QStringLiteral("库存数量待确认")};
        alternative.availability = {{"鸡蛋", 100, 0, 0, false, false, V::AvailabilityState::Unknown}};
        threeCards.cards.push_back(alternative);
        alternative.role = V::CardRole::AlternativeTwo;
        alternative.recipeId = 14;
        alternative.totalScore = 76.3;
        alternative.reasons = {QStringLiteral("备选二：主要食材不同")};
        alternative.availability = {{"虾", 100, 100, 0, false, false, V::AvailabilityState::Enough}};
        threeCards.cards.push_back(alternative);
        int unexpectedActions = 0;
        QObject::connect(&evidenceCards, &RecommendationWidget::recommendationRequestedV1,
                         [&] { ++unexpectedActions; });
        QObject::connect(&evidenceCards, &RecommendationWidget::recipeStepsSelected,
                         [&] { ++unexpectedActions; });
        evidenceCards.setRecommendationResult(threeCards);
        auto panels = evidenceCards.findChildren<QFrame *>("recommendationReasonPanel");
        auto toggles = evidenceCards.findChildren<QPushButton *>("recommendationExplanation");
        if (panels.size() != 3 || toggles.size() != 3) return 20;
        for (int i = 0; i < 3; ++i) {
            if (!panels[i]->isHidden()) return 21;
            toggles[i]->click();
            if (panels[i]->isHidden() ||
                panels[i]->findChild<QLabel *>("reasonScore")->text() != QString::number(threeCards.cards[i].totalScore, 'f', 1)) return 22;
            for (int j = 0; j < 3; ++j)
                if (i != j && !panels[j]->isHidden()) return 23;
            toggles[i]->click();
            if (!panels[i]->isHidden()) return 24;
        }
        if (!panels[1]->findChild<QLabel *>("reasonHighlight")->text().contains(QStringLiteral("备选一")) ||
            !panels[1]->findChild<QLabel *>("reasonStock")->text().contains(QStringLiteral("数量待确认")) ||
            !panels[2]->findChild<QLabel *>("reasonHighlight")->text().contains(QStringLiteral("主要食材不同")) ||
            panels[2]->findChild<QLabel *>("reasonStock")->property("needsAttention").toBool() ||
            unexpectedActions != 0) return 25;
    }
    const auto details = recommendations.findChildren<QPushButton *>();
    for (auto *button : details)
        if (button->objectName() == QStringLiteral("viewRecommendedRecipe"))
            button->click();
    if (selected != 12)
        return 3;
    for (auto *button : details)
        if (button->text() == QStringLiteral("换一批"))
            button->click();
    if (requests != 2 || emitted.excludedRecipeIds.size() != 1 || emitted.excludedRecipeIds.front() != 12)
        return 4;
    RecipeDetailsWidget recipe;
    int cookingId = 0;
    QObject::connect(&recipe, &RecipeDetailsWidget::cookingRequested, [&](int id) { cookingId = id; });
    recipe.setRecipe(card.recipe, {184, 15.1, 10.8, 9.2}, {});
    if (recipe.findChild<QLabel *>("recipeCalories")->text() != QStringLiteral("184") ||
        recipe.findChild<QLabel *>("recipeMacroValue0")->text() != QStringLiteral("15.1 克") ||
        recipe.findChild<QLabel *>("recipeMacroValue1")->text() != QStringLiteral("10.8 克") ||
        recipe.findChild<QLabel *>("recipeMacroValue2")->text() != QStringLiteral("9.2 克") ||
        recipe.findChild<QProgressBar *>("recipeMacroBar0")->value() != qRound(1000.0 * 15.1 * 4 / (15.1 * 4 + 10.8 * 4 + 9.2 * 9)))
        return 17;
    recipe.findChild<QPushButton *>("recipeComplete")->click();
    if (cookingId != 12)
        return 5;
    recipe.clearRecipe();
    if (!recipe.findChild<QLabel *>("recipeCalories")->text().isEmpty() ||
        recipe.findChild<QProgressBar *>("recipeMacroBar0")->value() != 0 ||
        recipe.findChild<QPushButton *>("recipeComplete")->isEnabled()) return 18;
    recipe.setRecipe(card.recipe, {}, {});
    if (recipe.findChild<QProgressBar *>("recipeMacroBar0")->value() != 0) return 19;
    CookingConfirmationWidget confirmation;
    int completions = 0;
    QObject::connect(&confirmation, &CookingConfirmationWidget::completionRequested,
                     [&](const V::CookingCompletionRequest &, const QString &) { ++completions; });
    V::CookingCompletionPreview preview;
    preview.outcome = {V::OperationStatus::Succeeded, V::ErrorCode::None, {}};
    preview.confirmationToken = "opaque";
    preview.snapshot.recipe = card.recipe;
    preview.snapshot.request.eventId = "5f778a96-91c7-4e42-a5ae-d7ad189d6105";
    preview.snapshot.request.recipeId = 12;
    preview.snapshot.request.servings = 1;
    V::CookingCompletionResult pending;
    pending.eventId = preview.snapshot.request.eventId;
    pending.stage = V::CompletionStage::Prepared;
    pending.outcome = {V::OperationStatus::NeedsConfirmation, V::ErrorCode::InventoryChanged,
                       "Re-preview needed"};
    confirmation.setProgress(pending);
    confirmation.setBusy(false);
    QPushButton *retry = nullptr;
    QPushButton *confirm = nullptr;
    for (auto *button : confirmation.findChildren<QPushButton *>()) {
        if (button->text() == QStringLiteral("继续完成未结算步骤"))
            retry = button;
        if (button->text() == QStringLiteral("确认完成并记录"))
            confirm = button;
    }
    if (!retry || retry->isHidden() || !confirm || confirm->isEnabled())
        return 6;
    confirmation.setPreview(preview);
    confirmation.setBusy(false);
    if (!retry->isHidden() || !confirm->isEnabled())
        return 7;
    confirm->click();
    if (completions != 1)
        return 8;
    V::CookingCompletionResult completed;
    completed.stage = V::CompletionStage::Completed;
    completed.outcome = {V::OperationStatus::Succeeded, V::ErrorCode::None, {}};
    confirmation.setProgress(completed);
    auto *close = confirmation.findChild<QPushButton *>("confirmClose");
    auto *title = confirmation.findChild<QLabel *>("confirmTitle");
    if (!confirm->isHidden() || !close || close->text() != QStringLiteral("完成") ||
        !title || !title->text().contains(QStringLiteral("做饭完成"))) return 14;
    recommendations.selectMoodV1(V::Mood::Tired);
    if(recommendations.findChild<QLabel*>("moodHeading")->text()!=QStringLiteral("今天很累"))return 12;
    recommendations.requestRecommendations();
    const auto quickButtons = recommendations.findChildren<QPushButton *>("quickAdjustment");
    if (quickButtons.size() != 3) return 15;
    const int beforeQuick = requests;
    quickButtons[2]->click();
    if (requests != beforeQuick + 1 || emitted.refinements != std::vector<V::Refinement>{V::Refinement::MoreSpecial}) return 16;
    quickButtons[2]->click();
    if (!emitted.refinements.empty()) return 9;
    quickButtons[1]->click();
    if (emitted.maxPreparationMinutes != 20 || !quickButtons[1]->isChecked()) return 10;
    quickButtons[1]->click();
    if (emitted.maxPreparationMinutes != 10 || !quickButtons[1]->isChecked()) return 37;
    recommendations.findChild<QPushButton *>("resetRecommendationAdjustments")->click();
    if (!emitted.refinements.empty() || emitted.maxPreparationMinutes != 30 || quickButtons[1]->isChecked()) return 11;
    RecommendationWidget flow;
    V::RecommendationPreferences latest;
    int flowRequests = 0;
    QObject::connect(&flow, &RecommendationWidget::recommendationRequestedV1,
                     [&](const V::RecommendationPreferences &p) { latest = p; ++flowRequests; });
    auto *stages = flow.findChild<QStackedWidget *>("recommendationStages");
    flow.selectMoodV1(V::Mood::TreatMyself);
    if (!stages || stages->currentIndex() != 0 || flowRequests != 0) return 20;
    flow.requestRecommendations();
    if (stages->currentIndex() != 1 || flowRequests != 1 || latest.maxPreparationMinutes != 75) return 21;
    flow.setRecommendationResult(result);
    auto *originalCard = flow.findChild<QPushButton *>("recommendationExplanation");
    flow.showRecipeDetails();
    if (stages->currentIndex() != 2) return 22;
    flow.findChild<QPushButton *>("backToRecommendations")->click();
    if (stages->currentIndex() != 1 || flowRequests != 1 ||
        flow.findChild<QPushButton *>("recommendationExplanation") != originalCard) return 23;
    auto quick = [&](const QString &name) -> QPushButton * {
        for (auto *button : flow.findChildren<QPushButton *>("quickAdjustment"))
            if (button->text().startsWith(name)) return button;
        return nullptr;
    };
    auto *special = quick(QStringLiteral("更有特色"));
    if (!special) return 24;
    special->click();
    if (flowRequests != 2 || latest.refinements.size() != 1 ||
        latest.refinements[0] != V::Refinement::MoreSpecial || !special->isChecked()) return 25;
    flow.setRecommendationResult(result);
    flow.findChild<QPushButton *>("swapRecommendations")->click();
    if (flowRequests != 3 || latest.excludedRecipeIds != std::vector<int>{12} ||
        latest.refinements[0] != V::Refinement::MoreSpecial) return 26;
    auto *stock = quick(QStringLiteral("只看冰箱里的"));
    if (!stock) return 27;
    stock->click();
    if (latest.inventoryPolicy != V::InventoryPolicy::OnlyInventory || !latest.excludedRecipeIds.empty() ||
        latest.refinements[0] != V::Refinement::MoreSpecial || latest.maxPreparationMinutes != 75 ||
        !flow.findChild<QLabel *>("recommendationConditions")->text().contains(QStringLiteral("只用现有食材"))) return 28;
    V::RecommendationResult empty; empty.outcome = {V::OperationStatus::Succeeded, V::ErrorCode::None, {}};
    flow.setRecommendationResult(empty);
    if (flow.findChild<QPushButton *>("allowPurchase")->isHidden()) return 29;
    flow.findChild<QPushButton *>("allowPurchase")->click();
    if (latest.inventoryPolicy != V::InventoryPolicy::PreferInventory || latest.maxPreparationMinutes != 75 ||
        latest.refinements[0] != V::Refinement::MoreSpecial) return 30;
    quick(QStringLiteral("时间短一点"))->click();
    if (latest.maxPreparationMinutes != 65 || latest.mood != V::Mood::TreatMyself ||
        latest.refinements[0] != V::Refinement::MoreSpecial || !quick(QStringLiteral("时间短一点"))->isChecked()) return 31;
    flow.setRecommendationResult(result);
    flow.findChild<QPushButton *>("swapRecommendations")->click();
    if (latest.maxPreparationMinutes != 65 || !quick(QStringLiteral("时间短一点"))->isChecked()) return 38;
    flow.showRecipeDetails();
    flow.findChild<QPushButton *>("backToRecommendations")->click();
    if (!quick(QStringLiteral("时间短一点"))->isChecked()) return 39;
    flow.findChild<QPushButton *>("undoEmptyRefinements")->click();
    if (latest.maxPreparationMinutes != 75 || !latest.refinements.empty() ||
        latest.inventoryPolicy != V::InventoryPolicy::PreferInventory || stages->currentIndex() != 1) return 32;
    flow.findChild<QPushButton *>("editRecommendationConditions")->click();
    if (stages->currentIndex() != 0 || flow.findChild<QSpinBox *>("customTimeLimit")->value() != 75) return 33;
    flow.findChild<QComboBox *>("inventoryPolicy")->setCurrentIndex(int(V::InventoryPolicy::OnlyInventory));
    flow.requestRecommendations();
    if (!stock->isChecked() || latest.inventoryPolicy != V::InventoryPolicy::OnlyInventory) return 34;
    stock->click();
    if (stock->isChecked() || latest.inventoryPolicy != V::InventoryPolicy::PreferInventory) return 35;
    for (int i = 0; i < 10; ++i) quick(QStringLiteral("时间短一点"))->click();
    if (latest.maxPreparationMinutes != 5 || quick(QStringLiteral("时间短一点"))->isEnabled() ||
        !quick(QStringLiteral("时间短一点"))->isChecked()) return 36;
    flow.findChild<QPushButton *>("editRecommendationConditions")->click();
    flow.requestRecommendations();
    if (quick(QStringLiteral("时间短一点"))->isChecked()) return 40;
    return 0;
}
