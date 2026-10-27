#pragma once

#include <QString>
#include <QStringList>
#include <vector>

#include "model/FoodBatch.h"
#include "model/NutritionInfo.h"
#include "model/Recipe.h"

namespace Shixu::RecommendationEngine {

// E supplies preferences and a usable inventory snapshot. B's FoodCandidate
// is converted by RecommendationContextBuilder; C's FoodBatch.amount is grams.
struct UserContext {
    std::vector<FoodBatch> inventory;
    QStringList expiringIngredients;
    QStringList excludedIngredients;
    QStringList preferredTags;
    int maxPreparationMinutes = 60; // <= 0 means no time limit.
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
    double preferenceMatchScore = 0.0;
    double nutritionScore = 0.0; // Energy-target fit, not a medical health rating.
    double preparationTimeScore = 0.0;
    bool nutritionScoreAvailable = false;
};

struct ExplainResult {
    QString dishName;
    double totalScore = 0.0;
    double ingredientMatchScore = 0.0;
    double expiryUrgencyScore = 0.0;
    double preferenceMatchScore = 0.0;
    double nutritionScore = 0.0;
    double preparationTimeScore = 0.0;
    bool nutritionScoreAvailable = false;
    QStringList missingIngredients;
    QStringList reasons;
};

// Calls C's findRecipesByFoods, then filters, scores and selects Top-K.
std::vector<Recommendation> recommend(const UserContext& context);
Recommendation generateRecommendation(const Recipe& recipe, const UserContext& context);
std::vector<Recommendation> getTopKRecommendations(
    const std::vector<Recipe>& candidates, const UserContext& context, int k);
ExplainResult explainRecommendation(const Recommendation& recommendation);

// Example inventory only; candidates and nutrition still come from C's module.
// Production callers should use readUserContext/buildUserContext instead.
UserContext makeDemoUserContext();

} // namespace Shixu::RecommendationEngine
