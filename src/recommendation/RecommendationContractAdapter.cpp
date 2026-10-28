#include "contracts/RecommendationContract.h"
#include "recipe/RecipeService.h"

#include <QDate>
#include <algorithm>
#include <cmath>
#include <map>
#include <queue>
#include <set>

namespace Shixu::Contracts::V1 {
namespace {
using Card = Shixu::RecommendationEngine::Recommendation;
constexpr double Epsilon = 1e-6;
QString text(const std::string& s) { return QString::fromUtf8(s.data(), int(s.size())); }
OperationResult success(std::string message = {})
{ return {OperationStatus::Succeeded, ErrorCode::None, std::move(message)}; }
RecommendationResult failed(ErrorCode code, const std::string& message)
{ return {{OperationStatus::Failed, code, message}, {}, false}; }
bool tag(const Recipe& r, const char* t)
{ return std::find(r.tags.begin(), r.tags.end(), t) != r.tags.end(); }
double tagged(const Recipe& r, std::initializer_list<const char*> tags)
{
    int hits = 0;
    for (const char* t : tags) hits += tag(r, t);
    return 100.0 * hits / tags.size();
}
double speed(const Recipe& r, int cap)
{ return std::clamp(100.0 * (1.0 - double(r.cookingTimeMinutes) / cap), 0.0, 100.0); }
double simple(const Recipe& r, int cap)
{
    return .55 * tagged(r, {Tags::Quick, Tags::Easy}) + .30 * speed(r, cap)
        + .15 * std::max(0.0, 100.0 - 10.0 * r.steps.size());
}
double moodScore(const Recipe& r, Mood mood, double stock, int cap)
{
    switch (mood) {
    case Mood::Tired: return .65 * simple(r, cap) + .35 * stock;
    case Mood::WantGoodFood: return tagged(r, {Tags::Hearty, Tags::Appetizing});
    case Mood::LowAppetite: return tagged(r, {Tags::Appetizing, Tags::Light});
    case Mood::WantLight: return tagged(r, {Tags::Light, Tags::VegetableRich, Tags::HighProtein});
    case Mood::TreatMyself: return tagged(r, {Tags::Hearty, Tags::Special});
    default: return 0;
    }
}
double refinementScore(const Recipe& r, Refinement refinement, int cap)
{
    switch (refinement) {
    case Refinement::Simpler: return simple(r, cap);
    case Refinement::Heartier: return tagged(r, {Tags::Hearty, Tags::Appetizing});
    case Refinement::Lighter: return tagged(r, {Tags::Light});
    case Refinement::MoreAppetizing: return tagged(r, {Tags::Appetizing});
    case Refinement::MoreVegetables: return tagged(r, {Tags::VegetableRich});
    case Refinement::MoreProtein: return tagged(r, {Tags::HighProtein});
    case Refinement::MoreSpecial: return tagged(r, {Tags::Special, Tags::Hearty});
    default: return 0;
    }
}
double alternativeFocus(const Recipe& r, Mood mood, int index, int cap)
{
    switch (mood) {
    case Mood::Tired: return index == 1 ? simple(r, cap) : tagged(r, {Tags::Easy});
    case Mood::WantGoodFood: return index == 1 ? simple(r, cap) : tagged(r, {Tags::Hearty, Tags::Appetizing});
    case Mood::LowAppetite: return index == 1 ? tagged(r, {Tags::Light}) : simple(r, cap);
    case Mood::WantLight: return index == 1 ? tagged(r, {Tags::VegetableRich}) : tagged(r, {Tags::HighProtein});
    case Mood::TreatMyself: return index == 1 ? tagged(r, {Tags::Hearty, Tags::Special})
        : .5 * simple(r, cap) + .5 * tagged(r, {Tags::Special});
    default: return 0;
    }
}
std::string mainIngredient(const Recipe& r)
{
    std::map<std::string, double> weights;
    for (const auto& i : r.ingredients) if (i.amountGrams > 0) weights[i.foodName] += i.amountGrams;
    std::string name;
    double largest = -1;
    for (const auto& i : weights) if (i.second > largest) { name = i.first; largest = i.second; }
    return name;
}
double diversity(const Recipe& a, const Recipe& b)
{
    std::set<std::string> left, right;
    for (const auto& i : a.ingredients) if (i.amountGrams > 0) left.insert(i.foodName);
    for (const auto& i : b.ingredients) if (i.amountGrams > 0) right.insert(i.foodName);
    std::size_t common = 0;
    for (const auto& n : left) common += right.count(n);
    double value = 50.0 * (1.0 - double(common) / (left.size() + right.size() - common));
    if (mainIngredient(a) != mainIngredient(b)) value += 30;
    if (!a.category.empty() && !b.category.empty() && a.category != b.category) value += 20;
    return value;
}
bool finiteNutrition(const NutritionInfo& n)
{
    for (double x : {n.calories, n.protein, n.carbohydrates, n.fat})
        if (!std::isfinite(x) || x < 0) return false;
    return true;
}
QString stockReason(const Card& card)
{
    QStringList gaps;
    for (const auto& a : card.availability) {
        if (a.state == AvailabilityState::Unknown)
            gaps << QStringLiteral("%1已知库存还差%2 g，另有重量未确认")
                .arg(text(a.foodName)).arg(a.knownGapGrams, 0, 'g', 8);
        else if (a.state == AvailabilityState::Insufficient)
            gaps << QStringLiteral("%1还需%2 g").arg(text(a.foodName)).arg(a.knownGapGrams, 0, 'g', 8);
    }
    if (gaps.isEmpty()) return QStringLiteral("必要食材均能确认充足");
    return gaps.join(QStringLiteral("；"));
}
QString traits(const Recipe& recipe)
{
    QStringList labels;
    const std::pair<const char*, const char*> names[] = {
        {Tags::Quick,"快手"}, {Tags::Easy,"简单"}, {Tags::Light,"清淡"},
        {Tags::Appetizing,"开胃"}, {Tags::Hearty,"丰盛"}, {Tags::Special,"特色"},
        {Tags::VegetableRich,"蔬菜丰富"}, {Tags::HighProtein,"高蛋白标签"}};
    for (const auto& n : names) if (tag(recipe, n.first)) labels << QString::fromUtf8(n.second);
    return labels.isEmpty() ? QStringLiteral("结合库存、心情与制作时间评分")
        : QStringLiteral("菜谱标签：%1").arg(labels.join(QStringLiteral("、")));
}
struct Ranked { double score; int id; std::size_t index; };
struct BetterFirst {
    bool operator()(const Ranked& a, const Ranked& b) const
    { return a.score != b.score ? a.score < b.score : a.id > b.id; }
};

class RecommendationService final : public IRecommendationService {
public:
    RecommendationService(IRecipeService& r, IInventoryService& i) : recipes(r), inventory(i) {}
    RecommendationResult recommend(const Shixu::RecommendationEngine::UserContext& context) const override
    {
        if (!context.preferences) return failed(ErrorCode::InvalidArgument, "缺少本轮推荐条件");
        const auto& p = *context.preferences;
        const QDate date = QDate::fromString(text(p.referenceDate), Qt::ISODate);
        if (p.mood < Mood::Tired || p.mood > Mood::TreatMyself || p.maxPreparationMinutes <= 0
            || p.servings != 1 || !date.isValid() || date.toString(Qt::ISODate) != text(p.referenceDate)
            || p.inventoryPolicy < InventoryPolicy::PreferInventory || p.inventoryPolicy > InventoryPolicy::SmallPurchase
            || p.refinement < Refinement::None || p.refinement > Refinement::MoreSpecial
            || !std::isfinite(context.targetCaloriesPerServing) || context.targetCaloriesPerServing < 0
            || context.inventorySnapshot.revision.empty()
            || std::any_of(p.excludedRecipeIds.begin(), p.excludedRecipeIds.end(), [](int id) { return id <= 0; }))
            return failed(ErrorCode::InvalidArgument, "请选择心情、有效日期、正数时间上限和一份菜谱，并提供库存快照");

        const auto query = recipes.getAllRecipes();
        if (query.outcome.status != OperationStatus::Succeeded)
            return failed(query.outcome.errorCode, query.outcome.message);
        const std::set<int> excluded(p.excludedRecipeIds.begin(), p.excludedRecipeIds.end());
        std::set<int> seen;
        std::vector<Card> candidates;
        for (const auto& recipe : query.value) {
            if (recipe.id <= 0 || recipe.name.empty()) return failed(ErrorCode::InvalidData, "菜谱标识或名称无效");
            if (!seen.insert(recipe.id).second || excluded.count(recipe.id)) continue;
            if (recipe.cookingTimeMinutes <= 0 || recipe.cookingTimeMinutes > p.maxPreparationMinutes
                || (p.refinement == Refinement::NoNoodles && tag(recipe, Tags::Noodle))) continue;
            std::map<std::string, double> merged;
            bool forbidden = false;
            for (const auto& ingredient : recipe.ingredients) {
                if (!std::isfinite(ingredient.amountGrams) || ingredient.amountGrams < 0)
                    return failed(ErrorCode::InvalidData, "菜谱食材重量无效");
                if (ingredient.amountGrams == 0) continue;
                if (ingredient.foodName.empty()) return failed(ErrorCode::InvalidData, "菜谱食材名称为空");
                forbidden |= context.excludedIngredients.contains(text(ingredient.foodName));
                merged[ingredient.foodName] += ingredient.amountGrams;
                if (!std::isfinite(merged[ingredient.foodName])) return failed(ErrorCode::InvalidData, "菜谱食材重量超出范围");
            }
            if (forbidden || merged.empty()) continue;
            EvaluationRequest request{context.inventorySnapshot, {}, p.referenceDate};
            for (const auto& demand : merged) request.requirements.push_back({demand.first, demand.second});
            const auto evaluation = inventory.evaluateRequirements(request);
            if (evaluation.outcome.status != OperationStatus::Succeeded)
                return failed(evaluation.outcome.errorCode, evaluation.outcome.message);
            if (evaluation.value.size() != merged.size()) return failed(ErrorCode::InvalidData, "库存评估未完整返回食材明细");
            std::set<std::string> assessed;
            int enough = 0, shortage = 0, unknown = 0, expiring = 0;
            Card card;
            for (const auto& a : evaluation.value) {
                const auto required = merged.find(a.foodName);
                if (required == merged.end() || !assessed.insert(a.foodName).second
                    || !std::isfinite(a.requiredGrams) || std::abs(a.requiredGrams - required->second) > Epsilon
                    || !std::isfinite(a.knownAvailableGrams) || a.knownAvailableGrams < 0
                    || !std::isfinite(a.knownGapGrams) || a.knownGapGrams < 0
                    || a.state < AvailabilityState::Enough || a.state > AvailabilityState::Unknown)
                    return failed(ErrorCode::InvalidData, "库存评估明细无效");
                enough += a.state == AvailabilityState::Enough;
                shortage += a.state == AvailabilityState::Insufficient;
                unknown += a.state == AvailabilityState::Unknown;
                expiring += a.hasExpiringStock;
                if (a.state == AvailabilityState::Enough) card.availableIngredients << text(a.foodName);
                else card.missingIngredients << text(a.foodName);
            }
            if ((p.inventoryPolicy == InventoryPolicy::OnlyInventory && enough != int(merged.size()))
                || (p.inventoryPolicy == InventoryPolicy::SmallPurchase && (unknown || shortage > 2))) continue;
            card.recipeId = recipe.id;
            card.dishName = text(recipe.name);
            card.recipe = recipe;
            card.servings = 1;
            card.availability = evaluation.value;
            card.nutrition = calculateNutrition(recipe, 1);
            if (!finiteNutrition(card.nutrition)) return failed(ErrorCode::InvalidData, "菜谱营养数据无效");
            card.ingredientMatchScore = 100.0 * enough / merged.size();
            card.expiryUrgencyScore = 100.0 * expiring / merged.size();
            card.moodMatchScore = moodScore(recipe, p.mood, card.ingredientMatchScore, p.maxPreparationMinutes);
            card.effectiveMaxPreparationMinutes = p.maxPreparationMinutes;
            card.preparationTimeReferenceMinutes = p.maxPreparationMinutes;
            card.preparationTimeScore = speed(recipe, p.maxPreparationMinutes);
            card.nutritionScoreAvailable = context.targetCaloriesPerServing > 0 && card.nutrition.calories > 0;
            if (card.nutritionScoreAvailable)
                card.nutritionScore = std::clamp(100.0 * (1.0 - std::abs(card.nutrition.calories
                    - context.targetCaloriesPerServing) / context.targetCaloriesPerServing), 0.0, 100.0);
            double weight = .85;
            double weighted = .35 * card.ingredientMatchScore + .25 * card.expiryUrgencyScore
                + .20 * card.moodMatchScore + .05 * card.preparationTimeScore;
            if (card.nutritionScoreAvailable) { weight += .15; weighted += .15 * card.nutritionScore; }
            if (p.refinement != Refinement::None && p.refinement != Refinement::NoNoodles) {
                weight += .35; weighted += .35 * refinementScore(recipe, p.refinement, p.maxPreparationMinutes);
            }
            card.totalScore = std::clamp(weighted / weight, 0.0, 100.0);
            candidates.push_back(std::move(card));
        }

        RecommendationResult result{success(), {}, false};
        std::set<int> selected;
        for (int position = 0; position < 3 && selected.size() < candidates.size(); ++position) {
            std::priority_queue<Ranked, std::vector<Ranked>, BetterFirst> ranked;
            for (std::size_t n = 0; n < candidates.size(); ++n) {
                const auto& c = candidates[n];
                if (selected.count(c.recipeId)) continue;
                double score = c.totalScore;
                if (position > 0) {
                    score = .55 * c.totalScore + .45 * alternativeFocus(c.recipe, p.mood, position, p.maxPreparationMinutes);
                    if (position == 2) {
                        double distance = 100;
                        for (const auto& previous : result.cards) distance = std::min(distance, diversity(c.recipe, previous.recipe));
                        score = .55 * score + .45 * distance;
                    }
                }
                ranked.push({score, c.recipeId, n});
            }
            auto chosen = candidates[ranked.top().index];
            chosen.role = position == 0 ? CardRole::Primary
                : position == 1 ? CardRole::AlternativeOne : CardRole::AlternativeTwo;
            QString label = QStringLiteral("今晚首选，综合当前条件");
            if (position > 0) {
                const Recipe& r = chosen.recipe;
                bool matchesFocus = false;
                switch (p.mood) {
                case Mood::Tired:
                    matchesFocus = tag(r, Tags::Quick) || tag(r, Tags::Easy); break;
                case Mood::WantGoodFood:
                    matchesFocus = position == 1 ? tag(r, Tags::Quick) || tag(r, Tags::Easy)
                        : tag(r, Tags::Hearty) || tag(r, Tags::Appetizing); break;
                case Mood::LowAppetite:
                    matchesFocus = position == 1 ? tag(r, Tags::Light)
                        : tag(r, Tags::Quick) || tag(r, Tags::Easy); break;
                case Mood::WantLight:
                    matchesFocus = position == 1 ? tag(r, Tags::VegetableRich) : tag(r, Tags::HighProtein); break;
                case Mood::TreatMyself:
                    matchesFocus = position == 1 ? tag(r, Tags::Hearty) || tag(r, Tags::Special)
                        : tag(r, Tags::Special); break;
                default: break;
                }
                label = position == 1 ? QStringLiteral("备选一") : QStringLiteral("备选二");
                label += matchesFocus ? QStringLiteral("，根据实际菜谱标签与综合评分选择")
                    : QStringLiteral("，未命中该备选侧重标签，按综合评分和制作时间选择");
                if (position == 2) {
                    bool differentMain = true, differentCategory = !r.category.empty();
                    for (const auto& previous : result.cards) {
                        differentMain &= mainIngredient(r) != mainIngredient(previous.recipe);
                        differentCategory &= !previous.recipe.category.empty() && r.category != previous.recipe.category;
                    }
                    if (differentMain) label += QStringLiteral("；主要食材与前两道不同");
                    else if (differentCategory) label += QStringLiteral("；类别与前两道不同");
                    else label += QStringLiteral("；保留另一道符合当前约束的选择");
                }
            }
            chosen.reasons << QStringLiteral("%1：%2；预计%3分钟，上限%4分钟")
                .arg(label, traits(chosen.recipe)).arg(chosen.recipe.cookingTimeMinutes).arg(p.maxPreparationMinutes);
            QString inventoryReason = stockReason(chosen);
            if (chosen.expiryUrgencyScore > 0) inventoryReason += QStringLiteral("；可利用临期库存");
            chosen.reasons << inventoryReason;
            selected.insert(chosen.recipeId);
            result.cards.push_back(std::move(chosen));
        }
        result.hasMore = candidates.size() > result.cards.size();
        if (result.cards.empty()) result.outcome.message = p.excludedRecipeIds.empty()
            ? "当前条件没有合适菜谱，请调整条件" : "没有更多结果，请重新查看第一批或调整条件";
        return result;
    }
private:
    IRecipeService& recipes;
    IInventoryService& inventory;
};
} // namespace

std::unique_ptr<IRecommendationService> createRecommendationService(
    IRecipeService& recipes, IInventoryService& inventory)
{ return std::make_unique<RecommendationService>(recipes, inventory); }
} // namespace Shixu::Contracts::V1
