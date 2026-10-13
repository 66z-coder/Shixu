#pragma once

#include <QString>
#include <QStringList>
#include <vector>

namespace Shixu::RecommendationEngine {

// Temporary D-module input model. Replace the mock recipe source with C's
// agreed recipe type when that interface is ready.
struct RecipeCandidate {
    QString id;
    QString name;
    QStringList ingredients;
    QStringList tags;
    int preparationMinutes = 0;
    double nutritionScore = 0.0; // Expected range: 0 to 100.
};

// D defines this input contract; E can assemble it from B/C data later.
struct UserContext {
    std::vector<RecipeCandidate> recipeCandidates;
    QStringList availableIngredients;
    QStringList expiringIngredients;
    QStringList excludedIngredients;
    QStringList preferredTags;
    int maxPreparationMinutes = 60;
    int topK = 5;
};

struct Recommendation {
    QString recipeId;
    QString dishName;
    double totalScore = 0.0; // Range: 0 to 100.
    double ingredientMatchScore = 0.0;
    double expiryUrgencyScore = 0.0;
    double preferenceMatchScore = 0.0;
    double nutritionScore = 0.0;
    double preparationTimeScore = 0.0;
};

struct ExplainResult {
    QString dishName;
    double totalScore = 0.0;
    double ingredientMatchScore = 0.0;
    double expiryUrgencyScore = 0.0;
    double preferenceMatchScore = 0.0;
    double nutritionScore = 0.0;
    double preparationTimeScore = 0.0;
    QStringList reasons;
};

std::vector<Recommendation> recommend(const UserContext& context);
Recommendation generateRecommendation(const RecipeCandidate& candidate,
                                      const UserContext& context);
std::vector<Recommendation> getTopKRecommendations(
    const std::vector<RecipeCandidate>& candidates,
    const UserContext& context,
    int k);
ExplainResult explainRecommendation(const Recommendation& recommendation);

// Demonstration data only. This is not connected to B/C or the database.
UserContext makeDemoUserContext();

} // namespace Shixu::RecommendationEngine
