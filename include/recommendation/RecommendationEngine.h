#pragma once

#include <QString>
#include <QStringList>
#include <vector>

#include "model/FoodBatch.h"
#include "model/NutritionInfo.h"
#include "model/Recipe.h"

namespace Shixu::RecommendationEngine {

// Values are shared with E's selection controls. Do not infer mood from data.
enum class Mood {
    NotSelected = 0,
    ReadyToCook = 1, // 有心情做饭: 60 minutes.
    Neutral = 2,     // 状态一般: 30 minutes.
    Tired = 3        // 疲惫、想省事: 15 minutes.
};

QString moodDisplayName(Mood mood);
int moodTimeLimitMinutes(Mood mood); // 0 for missing/invalid selection.
// Independent time scoring uses this reference if no user time cap is supplied.
inline constexpr int DefaultTimeScoringReferenceMinutes = 60;

// E supplies preferences and a usable inventory snapshot. B's FoodCandidate
// is converted by RecommendationContextBuilder; C's FoodBatch.amount is grams.
struct UserContext {
    std::vector<FoodBatch> inventory;
    QStringList expiringIngredients;
    QStringList excludedIngredients;
    Mood mood = Mood::NotSelected; // E must collect an explicit selection.
    int maxPreparationMinutes = 0; // <= 0 means no EXTRA cap; mood still applies.
    int topK = 5;
    int servings = 1;
    double targetCaloriesPerServing = 0.0; // User supplied; <= 0 disables this factor.
};

struct Recommendation {
    int recipeId = 0; // C/E use the same integer recipe ID.
    QString dishName;
    Recipe recipe; // Original C recipe for the details page / recordMeal.
    int servings = 1;
    NutritionInfo nutrition; // C's calculated totals for the requested servings.
    QStringList availableIngredients;
    QStringList missingIngredients;
    double totalScore = 0.0;
    double ingredientMatchScore = 0.0;
    double expiryUrgencyScore = 0.0;
    double moodMatchScore = 0.0; // 100 when the dish satisfies the selected mood cap.
    Mood mood = Mood::NotSelected;
    int effectiveMaxPreparationMinutes = 0;
    double nutritionScore = 0.0; // Energy-target fit, not a medical health rating.
    double preparationTimeScore = 0.0;
    int preparationTimeReferenceMinutes = 0; // Independent of the selected mood.
    bool nutritionScoreAvailable = false;
};

struct ExplainResult {
    QString dishName;
    double totalScore = 0.0;
    double ingredientMatchScore = 0.0;
    double expiryUrgencyScore = 0.0;
    double moodMatchScore = 0.0; // 100 when the dish satisfies the selected mood cap.
    Mood mood = Mood::NotSelected;
    int effectiveMaxPreparationMinutes = 0;
    double nutritionScore = 0.0;
    double preparationTimeScore = 0.0;
    int preparationTimeReferenceMinutes = 0; // Independent of the selected mood.
    bool nutritionScoreAvailable = false;
    QStringList missingIngredients;
    QStringList reasons;
};

// min(mood cap, positive user cap); 0 means mood has not been selected.
int effectivePreparationTimeLimit(const UserContext& context);

// Missing/invalid mood returns no results. Calls C, filters and selects Top-K.
std::vector<Recommendation> recommend(const UserContext& context);
Recommendation generateRecommendation(const Recipe& recipe, const UserContext& context);
std::vector<Recommendation> getTopKRecommendations(
    const std::vector<Recipe>& candidates, const UserContext& context, int k);
ExplainResult explainRecommendation(const Recommendation& recommendation);

// Example inventory only; candidates and nutrition still come from C's module.
// Production callers should use readUserContext/buildUserContext instead.
UserContext makeDemoUserContext();

} // namespace Shixu::RecommendationEngine
