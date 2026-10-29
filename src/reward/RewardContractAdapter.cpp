#include "contracts/ServiceFactories.h"
#include "reward/RewardSystem.h"

namespace Shixu::Contracts::V1 {
namespace {
class RewardContractAdapter final : public IRewardService
{
public:
    explicit RewardContractAdapter(RewardSystem& rewards) : m_rewards(rewards) {}

    CookingRewardResult applyCookingRewardOnce(const CookingRewardRequest& request) override
    {
        return m_rewards.applyCookingRewardOnce(request);
    }

    QueryResult<GrowthSnapshot> getGrowthSnapshot(const LocalDate& referenceDate) const override
    {
        return m_rewards.getGrowthSnapshot(referenceDate);
    }

private:
    RewardSystem& m_rewards;
};
} // namespace

std::unique_ptr<IRewardService> createRewardService(RewardSystem& rewards)
{
    return std::make_unique<RewardContractAdapter>(rewards);
}
} // namespace Shixu::Contracts::V1