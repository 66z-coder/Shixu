#pragma once
#include "contracts/CoreContracts.h"
#include <memory>

namespace Shixu::Contracts::V1 {
struct CookingAchievement {
    std::string code;
    LocalDateTime unlockedAt;
};
struct CookingActivitySnapshot {
    int totalCookingCount = 0;
    int expiringCookingCount = 0;
    int currentCookingStreak = 0;
    int bestCookingStreak = 0;
    int todayMealCount = 0; // Completed and legacy meal records on the reference date.
    NutritionInfo todayNutrition;
    std::vector<MealRecord> recentMeals; // Latest ten verified Completed events, descending.
    std::vector<MealRecord> legacyMeals; // No event ID; display only, not eligible for achievements.
    std::vector<CookingAchievement> achievements;
    std::vector<std::string> newlyUnlocked; // Only after an atomic successful save.
};
class ICookingActivityService {
  public:
    virtual ~ICookingActivityService() = default;
    // Reads real history and atomically reconciles newly earned achievements.
    virtual QueryResult<CookingActivitySnapshot> refresh(const LocalDate &referenceDate) = 0;
};
std::unique_ptr<ICookingActivityService> createCookingActivityService(IRecipeService &, IInventoryService &,
                                                                      ICookingCompletionService &,
                                                                      const std::string &achievementPath);
} // namespace Shixu::Contracts::V1
