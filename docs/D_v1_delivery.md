# D 第一轮推荐服务交付

本轮基于已合入 PR #9 的 develop `51e2448`，分支 `feature/recommend`。
组长的最新通知优先于旧文档中“接口尚未合入”的说明。

## 已实现

生产实现为 `src/recommendation/RecommendationContractAdapter.cpp`，实现已冻结的
`IRecommendationService` 和 `createRecommendationService(IRecipeService&, IInventoryService&)`。
借用两个服务，不拥有数据库，不扣库存、不记录饮食、不发积分。
旧推荐入口、三心情联调演示和公共头文件保持兼容；新五心情功能通过新服务调用。

新路径只读取 `UserContext.preferences`、`inventorySnapshot`，保留
`excludedIngredients` 的硬排除和 `targetCaloriesPerServing` 的可选能量目标。
不混读旧 mood、maxPreparationMinutes、servings、topK、inventory、expiringIngredients。

- 五心情按标准标签、库存满足程度、时间及步骤数评分；所有分数为 0—100。
- 时间只使用 preferences.maxPreparationMinutes 的最终正数上限，允许用户提高或降低默认值。
- 从 C.getAllRecipes 获取全量候选，空库存也可普通推荐。
- 按名称精确合并配方全部正重量项（含食用油），交由 B.evaluateRequirements 评估同一份快照及日期。
- 普通模式允许不足和未知；OnlyInventory 仅全部 Enough；SmallPurchase 最多两种 Insufficient 且无 Unknown。
- 未知库存保留 availability 明细，理由写“已知库存还差…，另有重量未确认”。不推测个数、体积重量，不另写 B 的数量状态判断。
- 使用优先队列选择首选、侧重备选、兼顾主要食材与类别差异的第二备选；不是简单总分前三。
- ID 去重，稳定同分按整数 ID 升序；不足三张不补齐、不突破约束。
- excludedRecipeIds 排除本轮已展示内容；hasMore 仅表示还有合格未展示菜谱。
- 每卡两条主要理由，标签只来自 C；临期标记只来自 B，措辞为“可利用”，不声称已消耗。
- B/C 查询失败透传错误码，不返回空成功；非法条件及不完整评估明确失败。

## 与 E 及组长的接线

使用新命名空间 `Shixu::Contracts::V1` 的 Mood，不能把旧三心情枚举转数字使用。
E 选心情时填默认时间：Tired=30、WantGoodFood=45、LowAppetite=30、WantLight=40、TreatMyself=75。
新服务不把 0 当默认时间，未选心情、时间<=0或份数非1返回 InvalidArgument。

E 在控件中保存唯一 refinement（重复点击切回 None）、独立的 inventoryPolicy 和时间。
按钮点击立即发请求；NoNoodles 是硬排除，其余 refinement 调整排序。
“时间短一点”由 E 设置 min(当前上限,45)，不另发明 Refinement 枚举。
心情、时间、偏好、库存模式、日期或库存版本变化时，E/集成层清空已展示 ID。
服务本身不保存会话历史，避免多个页面或请求互相影响。

集成层先调用 B.getInventoryBatches，查询失败直接显示错误，不能用空快照代替。
成功后把 snapshot 放入 context，再调用 D。D 不暗中重读库存，确保整轮评估共用同一快照。
返回 cards 直接传给 setRecommendationResult，保留完整 recipe、role、availability、reasons。
新卡片旧 mood 字段不用于 UI；当前心情从请求 preferences 获取。

```cpp
namespace V1 = Shixu::Contracts::V1;
namespace D = Shixu::RecommendationEngine;
// recipeService / inventoryService 由集成负责人构造，并比推荐服务活得更久。
auto recommendations = V1::createRecommendationService(*recipeService, *inventoryService);
const auto stock = inventoryService->getInventoryBatches();
if (stock.outcome.status != V1::OperationStatus::Succeeded) {
    // 显示 stock.outcome，停止；不能伪装成空库存继续。
    return;
}
D::UserContext context;
context.preferences = selectedPreferences; // E 提供，日期由集成层填入。
context.inventorySnapshot = stock.value;
const auto result = recommendations->recommend(context);
// E 直接展示 result；换一批时将上次 cards 的 recipeId 追加到 excludedRecipeIds。
```

## 评分与卡片选择

基础权重沿用库存35%、临期25%、心情20%、营养目标15%、时间5%；
无能量目标时去掉该项并归一化。快捷排序偏好额外权重35%，与基础项一起归一化。
很累偏向 quick/easy、短时间、少步骤、库存满足；想吃点好的偏 hearty/appetizing；
没胃口偏 appetizing/light；想清爽一点偏 light/vegetable_rich/high_protein；
想犒劳自己偏 hearty/special。标签由 C 审核，D 不映射旧中文、不根据名字猜标签。

首选按综合分；备选一结合综合分55%和本心情侧重点45%；
备选二再兼顾相对已选两卡的最小差异（配方食材集合、主食材、类别）。
这些是可解释的项目排序规则，不作医学建议；营养数值调用 C 的公开 calculateNutrition。
没有符合侧重点的菜时，不虚构标签；在合格候选中选实际可用备选。

## 构建交接

全局 CMakeLists.txt、MainWindow 和公共头文件由组长维护，本 PR 不修改。
组长将以下源文件加入原 ShixuRecommendation 的 source 列表即可链接新工厂：

`src/recommendation/RecommendationContractAdapter.cpp`

D 的独立验证入口是 `tests/recommend/v1/CMakeLists.txt`，复用根工程构建旧目标并新增
ShixuRecommendationV1（仅独立检查用）及 RecommendationV1Tests。
B/C 测试替身只存在 tests/recommend/v1/test_recommendation_v1.cpp，不进入生产路径。

```powershell
cmake -S tests/recommend/v1 -B build/recommendation-v1 -G Ninja -DCMAKE_PREFIX_PATH=<Qt路径> -DBUILD_TESTING=ON
cmake --build build/recommendation-v1 --parallel 4
ctest --test-dir build/recommendation-v1 --output-on-failure
```

本机工具链仍使用已安装的 Qt 6.11.2 MinGW 64-bit。测试使用固定日期2026-10-28和服务替身；
旧回归测试继续使用各自临时存储，不改个人库存。

覆盖 D01—D05：五种默认上限边界、用户提高/降低上限、三库存模式、未知量、空库存、
全部快捷偏好、面食/忌口硬排除、三卡定位、去重换批与重置、hasMore、0/2张、服务错误、
同一快照、重复需求合并、独立于旧条件及 C 营养公式。
界面按钮行为由 E 实现；B/C 真实业务适配器及主窗口接线合入后，按组长安排做完整联调。

## 本次验证结果

- Qt完整工程：编译成功，18/18项通过（原17项加D新业务测试）。
- 无Qt工程：编译成功，6/6项通过。
- A的receipt_ocr_protocol测试默认查找项目.venv-ocr，本机该解释器不可用。
  本次仅在测试命令的进程环境中设置SHIXU_OCR_PYTHON为已有的Python解释器后通过；
  未修改A代码、未安装OCR依赖，也未验证真实图片OCR模型识别效果。
- 推荐V1业务测试不连接真实B/C适配器；真实联调待B/C/E本轮实现及组长接线合入。
- 旧演示窗口仍走旧接口，不能用它验收本轮五心情页面。

## 查看推荐内容

独立工程构建后运行 `build/recommendation-v1/RecommendationV1Tests.exe --show-output`，
会先执行D业务测试，再输出五心情、排除面食、严格库存、少量补购、提高上限与换批示例。
可追加 `--output build/recommendation-v1/recommendation-output.txt` 保存UTF-8报告。
这些样例通过测试替身提供合成菜谱和B评估，调用真实V1推荐算法；不读写个人库存。
为避免与真实菜谱混淆，名称明确带“测试菜”，营养及标签也不是生产数据。
