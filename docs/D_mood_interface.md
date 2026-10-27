# D 心情与时间接口约定

本版按 D/E 确认的规则替换口味标签：用户选择心情，D 决定时间限制并评分。心情由用户明确选择，不根据历史、文字或库存推断。

## E 必须更新的字段

公共头文件仍是 `recommendation/RecommendationEngine.h`，`recommend(context)` 等四个入口的函数名与参数形式保持本次 B/C 接入后的版本。

| 界面选项 | UserContext.mood | 枚举值 | 心情上限 |
| --- | --- | --- | --- |
| 请选择今天的心情 | Mood::NotSelected | 0 | 不生成推荐 |
| 有心情做饭 | Mood::ReadyToCook | 1 | 60 分钟 |
| 状态一般 | Mood::Neutral | 2 | 30 分钟 |
| 疲惫、想省事 | Mood::Tired | 3 | 15 分钟 |

`preferredTags` 和结果中的 `preferenceMatchScore` 已删除。C 菜谱自身的 `tags` 保留，但 D 不再据此计算口味偏好分。

`maxPreparationMinutes` 是用户额外设置的时间上限，默认 0。正数时与心情上限取较小值；非正数只表示没有额外上限，仍需遵守心情上限。例如疲惫时即使用户填 90 分钟，也只推荐 15 分钟内的菜；状态一般时用户填 20 分钟，就只推荐 20 分钟内的菜。

未选择心情或传入非法枚举值时，推荐、Top-K 返回空列表，单菜评分返回无效结果（recipeId 为 0）。E 应先提示用户选择心情，再调用推荐，不要把未选择提示当作“库存无菜可做”。制作时间未知、非正数的菜谱暂不进入推荐，避免把未知时间当成零分钟。

## E 的调用示例

```cpp
#include "recommendation/RecommendationContextBuilder.h"
using namespace Shixu::RecommendationEngine;

UserContext settings;
settings.mood = Mood::Tired; // 来自用户选择；不要在界面里重新写 60/30/15 的计算规则
settings.maxPreparationMinutes = 20;
settings.topK = 5;
settings.servings = 1;

const int timeLimit = effectivePreparationTimeLimit(settings); // 本例为 15
if (timeLimit <= 0) {
    // 显示“请选择今天的心情”，不生成推荐。
    return;
}
auto built = readUserContext(inventoryManager, settings);
auto results = recommend(built.context);
for (const auto& item : results) {
    auto explanation = explainRecommendation(item);
    // 分别显示 item.moodMatchScore（心情匹配分）、item.preparationTimeScore（时间分）
    // 以及 explanation.reasons。不要合为一个得分字段。
    // item.mood / item.effectiveMaxPreparationMinutes 保留本次计算用的选择和分钟数。
}
```

`moodDisplayName(Mood)` 提供统一中文名称，`moodTimeLimitMinutes(Mood)` 提供原始心情上限，`effectivePreparationTimeLimit(UserContext)` 负责与自设上限取较短值。E 调用这些辅助函数即可，避免界面和 D 各维护一套映射。

如果先通过 `buildUserContext` / `readUserContext` 生成库存快照，心情、自设时间上限、忌口、份数等输入都会保留。

## 结果解释与权重

心情项与制作时间项独立，不改变 B 的库存、C 的菜谱营养或菜谱原始制作时间。

- 心情匹配 `moodMatchScore`：按已确认的 60/30/15 分钟要求筛选。超出上限的菜排除；通过筛选的菜满足心情要求，得 100 分。因此这项目前负责决定候选范围，不在合格候选之间再区分快慢。
- 制作时间 `preparationTimeScore`：独立评价快慢，使用 `100 × (1 − 制作分钟 / 时间评分基准)`，限定在 0–100。有正数自设上限时以它作为基准；否则使用公开常量 `DefaultTimeScoringReferenceMinutes`（60 分钟）。更换心情不会改变同一道菜在同一用户时间设置下的时间分。
- 结果中的 `preparationTimeReferenceMinutes` 保留独立时间评分基准；`effectiveMaxPreparationMinutes` 则是实际筛选上限。两者用途不同。例如疲惫上限 15 分钟、用户自设 20 分钟时，筛选按 15，时间分按 20 分钟评价。

五项权重分别为：数量满足 35%、临期利用 25%、心情匹配 20%、能量目标匹配 15%、制作时间 5%。只把原偏好项替换为心情项，心情 20% 和制作时间 5% 不合并。没有有效能量目标时不参与该项评分，其余权重归一化。

结果中的 `mood`、`effectiveMaxPreparationMinutes` 和推荐理由对应实际使用的规则；界面应读取结果展示，不要在用户改选心情后给旧推荐套上新标签。

## 本地演示与成员影响

运行 `RecommendationLiveDemo`，先选择心情，再生成推荐。选择框最初停在“请选择今天的心情”，不会自动猜测。切换心情或自设时间上限会重新计算；`RecommendationDemo` 则使用显式的示例心情与示例库存。

A、B、C 的接口和代码无需为这次需求改动。E 需要替换原偏好标签控件、设置 `UserContext.mood`，并把原偏好分的显示换成心情匹配分，保留独立的制作时间分及其解释。此文档应随 D 的 PR 更新一起交给 E。
