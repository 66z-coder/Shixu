#pragma once
// V1 reviewed contract: declarations only. Implementations arrive in member PRs.
// Normative rules: docs/contracts/v1.md. No Qt dependency in this header.
#include <cstdint>
#include <optional>
#include <string>
#include <vector>
#include "model/Recipe.h"
#include "model/MealRecord.h"

namespace Shixu::Contracts::V1 {
inline constexpr int ContractVersion = 1;
using BatchId = std::int64_t; // Stable B database ID, > 0; never a vector index.
using RecipeId = int;        // Existing Recipe::id, > 0.
using EventId = std::string; // Lowercase UUID without braces.
using LocalDate = std::string; // YYYY-MM-DD; empty only for unknown expiry.
using LocalDateTime = std::string; // YYYY-MM-DD HH:mm:ss, Asia/Shanghai.

enum class OperationStatus { Succeeded, AlreadyApplied, NeedsConfirmation, Failed };
enum class ErrorCode {
    None, InvalidArgument, NotFound, StorageFailure, EventConflict,
    InventoryChanged, UnconfirmedIngredients, InvalidData, InvalidState
};
struct OperationResult {
    OperationStatus status = OperationStatus::Failed;
    ErrorCode errorCode = ErrorCode::InvalidState;
    std::string message; // UTF-8, user-facing; never branch on this string.
};
template<class T> struct QueryResult {
    OperationResult outcome;
    T value{}; // Valid only when outcome.status == Succeeded.
};

// B's read-only projection of FoodCandidate plus DB identity, not a new input model.
struct InventoryBatch {
    BatchId batchId = 0;
    std::string name;
    double quantity = 0.0;
    std::string unit;
    LocalDate expiryDate;
    double gramsPerUnit = 0.0;
    double totalWeightGrams = 0.0; // 0 means UNKNOWN if quantity > 0.
};
struct InventorySnapshot {
    std::string revision; // Opaque B revision; changes on every inventory mutation.
    std::vector<InventoryBatch> batches;
};
struct IngredientRequirement {
    std::string foodName; // A's canonical UTF-8 name.
    double requiredGrams = 0.0; // Positive finite, already scaled by servings.
};
enum class AvailabilityState { Enough, Insufficient, Unknown };
struct IngredientAvailability {
    std::string foodName;
    double requiredGrams = 0.0;
    double knownAvailableGrams = 0.0;
    double knownGapGrams = 0.0;
    bool hasUnknownWeight = false;
    bool hasExpiringStock = false;
    AvailabilityState state = AvailabilityState::Unknown;
};
struct EvaluationRequest {
    InventorySnapshot inventory;
    std::vector<IngredientRequirement> requirements;
    LocalDate referenceDate;
};
struct ConsumptionRequest {
    EventId eventId;
    LocalDateTime completedAt; // Frozen first confirmation time; never retry time.
    std::vector<IngredientRequirement> requirements;
    std::vector<std::string> externallyPreparedFoods;
};
struct BatchConsumption {
    BatchId batchId = 0;
    std::string foodName;
    double consumedGrams = 0.0;
    double quantityBefore = 0.0;
    double quantityAfter = 0.0;
    double weightBeforeGrams = 0.0;
    double weightAfterGrams = 0.0;
    bool wasExpiring = false;
};
struct ConsumptionPreview {
    OperationResult outcome;
    std::string previewToken; // Bound to payload + inventory revision, not client-generated.
    std::vector<IngredientAvailability> availability;
    std::vector<BatchConsumption> planned;
};
struct ConsumptionResult {
    OperationResult outcome;
    EventId eventId;
    std::vector<IngredientAvailability> availability; // Pre-deduction assessment.
    std::vector<BatchConsumption> consumed;
    bool consumedExpiring = false;
};
class IInventoryService {
public:
    virtual ~IInventoryService() = default;
    virtual QueryResult<InventorySnapshot> getInventoryBatches() const = 0;
    virtual QueryResult<std::vector<IngredientAvailability>> evaluateRequirements(
        const EvaluationRequest&) const = 0;
    // Read committed event before checking stale preview tokens; NotFound means no commit.
    virtual QueryResult<ConsumptionResult> getConsumption(const EventId&) const = 0;
    virtual ConsumptionPreview previewConsumption(const ConsumptionRequest&) const = 0;
    virtual ConsumptionResult consumeForCooking(
        const ConsumptionRequest&, const std::string& previewToken) = 0;
};

struct MealWriteRequest {
    EventId eventId;
    MealRecord record; // Frozen name/id/servings/time/date/total nutrition.
};
struct MealWriteResult {
    OperationResult outcome;
    EventId eventId;
    std::optional<MealRecord> savedRecord; // Set on success/already applied.
};
class IRecipeService {
public:
    virtual ~IRecipeService() = default;
    virtual QueryResult<std::vector<Recipe>> getAllRecipes() const = 0;
    virtual QueryResult<Recipe> findRecipeById(RecipeId) const = 0;
    virtual QueryResult<std::vector<MealRecord>> getMealsByDate(const LocalDate&) const = 0;
    virtual QueryResult<NutritionInfo> getNutritionByDate(const LocalDate&) const = 0;
    virtual MealWriteResult recordMealOnce(const MealWriteRequest&) = 0;
};

// D/E share these choices. Never cast the old three-mood enum to this enum.
enum class Mood { NotSelected = 0, Tired = 1, WantGoodFood = 2,
                  LowAppetite = 3, WantLight = 4, TreatMyself = 5 };
enum class InventoryPolicy { PreferInventory, OnlyInventory, SmallPurchase };
enum class Refinement { None, Simpler, NoNoodles, Heartier, Lighter,
                        MoreAppetizing, MoreVegetables, MoreProtein, MoreSpecial };
enum class CardRole { Primary, AlternativeOne, AlternativeTwo };
struct RecommendationPreferences {
    Mood mood = Mood::NotSelected;
    int maxPreparationMinutes = 0; // REQUIRED final cap (>0); no hidden mood minimum.
    InventoryPolicy inventoryPolicy = InventoryPolicy::PreferInventory;
    Refinement refinement = Refinement::None;
    std::vector<RecipeId> excludedRecipeIds;
    int servings = 1; // V1 accepts only 1.
    LocalDate referenceDate;
};
namespace Tags {
inline constexpr char Quick[] = "quick";
inline constexpr char Easy[] = "easy";
inline constexpr char Light[] = "light";
inline constexpr char Appetizing[] = "appetizing";
inline constexpr char Hearty[] = "hearty";
inline constexpr char Special[] = "special";
inline constexpr char HighProtein[] = "high_protein";
inline constexpr char VegetableRich[] = "vegetable_rich";
inline constexpr char Noodle[] = "noodle";
}

struct CookingRewardRequest {
    EventId eventId;
    LocalDateTime completedAt;
    bool consumedExpiring = false; // From B's committed result, never UI guess.
};
struct CookingRewardResult {
    OperationResult outcome;
    EventId eventId;
    int pointsAdded = 0; // Original event award, also returned on AlreadyApplied.
    bool newCheckIn = false;
    int totalCookingCount = 0; // Snapshot at original commit, not necessarily current total.
    int totalPoints = 0;
    std::vector<std::string> newlyUnlockedAchievements;
};
struct GrowthSnapshot {
    int totalCookingCount = 0;
    int totalCheckIns = 0;
    int totalPoints = 0;
    int currentStreak = 0;
    int bestStreak = 0;
    bool checkedInOnReferenceDate = false;
    std::vector<std::string> unlockedAchievements;
    std::vector<LocalDate> recentCheckInDates; // Latest 7 check-in dates, descending.
};
class IRewardService {
public:
    virtual ~IRewardService() = default;
    virtual CookingRewardResult applyCookingRewardOnce(const CookingRewardRequest&) = 0;
    virtual QueryResult<GrowthSnapshot> getGrowthSnapshot(const LocalDate& referenceDate) const = 0;
};

struct CookingCompletionRequest {
    EventId eventId;
    RecipeId recipeId = 0;
    int servings = 1;
    LocalDateTime completedAt;
    std::vector<std::string> externallyPreparedFoods;
};
struct CookingSnapshot {
    CookingCompletionRequest request;
    Recipe recipe;
    std::vector<IngredientRequirement> requirements;
    NutritionInfo nutrition; // Total for request.servings; calculated by existing C formula.
};
enum class CompletionStage { NotStarted, Prepared, InventorySaved, MealSaved, RewardSaved, Completed, Cancelled };
struct CookingCompletionPreview {
    OperationResult outcome;
    std::string confirmationToken; // Opaque coordinator token binds frozen snapshot + B preview.
    CookingSnapshot snapshot;
    ConsumptionPreview inventory;
};
struct CookingCompletionResult {
    OperationResult outcome;
    EventId eventId;
    CompletionStage stage = CompletionStage::NotStarted; // Last durable step.
    std::optional<ConsumptionResult> inventory;
    std::optional<MealWriteResult> meal;
    std::optional<CookingRewardResult> reward;
};
struct CookingCompletionDetails {
    CookingSnapshot snapshot;
    CookingCompletionResult progress;
};
class ICookingCompletionService {
public:
    virtual ~ICookingCompletionService() = default;
    virtual CookingCompletionPreview previewCompletion(const CookingCompletionRequest&) = 0;
    virtual CookingCompletionResult completeCooking(
        const CookingCompletionRequest&, const std::string& confirmationToken) = 0;
    virtual CookingCompletionResult retryCompletion(const EventId&) = 0;
    virtual QueryResult<CookingCompletionDetails> getCompletion(const EventId&) const = 0;
    // Only before B committed; retain cancellation tombstone, never undo a consumed event.
    virtual CookingCompletionResult cancelPreparedCompletion(const EventId&) = 0;
    virtual QueryResult<std::vector<CookingCompletionResult>> getPendingCompletions() const = 0;
};
} // namespace Shixu::Contracts::V1
