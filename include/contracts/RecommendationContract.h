#pragma once
// D's new service reuses existing UserContext/Recommendation; no parallel card model.
#include "recommendation/RecommendationEngine.h"
namespace Shixu::Contracts::V1 {
struct RecommendationResult {
    OperationResult outcome;
    std::vector<Shixu::RecommendationEngine::Recommendation> cards; // 0..3, unique recipe IDs.
    bool hasMore = false; // More unseen eligible recipes, not permission to relax filters.
};
class IRecommendationService {
public:
    virtual ~IRecommendationService() = default;
    virtual RecommendationResult recommend(
        const Shixu::RecommendationEngine::UserContext&) const = 0;
};
} // namespace Shixu::Contracts::V1
