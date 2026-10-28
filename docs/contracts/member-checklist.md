# 成员直接开工清单

技术接口审查已完成。以下是明确任务，不再要求 B/C/D/E 各自重复审查；发现新的实际阻碍再集中反馈。当前 PR #9 不合并 develop，业务实现仍待各成员完成和验收。
先读 [接口规则](v1.md) 和 [审查结论](review.md)，再按自己的段落开发。字段和签名以 include/contracts 下的头文件为唯一依据。

## 现在如何开工

先保存自己的未提交内容，不要强制切换。各人执行：

```powershell
git fetch origin
git switch -c feature/<自己的模块>-v1 origin/feature/core-contracts-v1
```

把分支名中的占位文字替换为 inventory / recipe / recommendation / ui-reward 等，不要四个人共用一个分支。
这会包含公共接口 PR 的提交，不用手动复制文件或只挑头文件。新建后记下 `git rev-parse HEAD` 作为共同基线。
模块业务 PR 目标仍为 develop，说明依赖 #9；公共接口尚未合入时业务 PR 保持草稿，不能先行合并。
组长安排公共接口合入后，各业务分支合并最新 origin/develop 再交付；如果 #9 后续更新，先同步其提交再开发，不重复定义接口。

## B 库存

- 修改：src/inventory/inventory.h、inventory.cpp；新增 src/inventory/InventoryContractAdapter.cpp；补 tests/inventory。
- 实现 IInventoryService 全部五个方法，包括 getConsumption。实现 ServiceFactories.h 中 createInventoryService(InventoryManager&)；借用同一实例，不新开库存连接。
- getInventoryBatches 从 foods.id 返回稳定批次；同一读事务返回 revision。addFoods 和扣减更新版本。旧 getAllItems/临期/首页查询排除已耗尽批次。
- 实现统一 Enough/Insufficient/Unknown 评估、FEFO预览、版本令牌校验、逐项外部准备与事务扣减；量与重量同步，不设默认重量。
- 在B内增加友元适配类或内部方法访问数据库，不能让下游通过getAllItems自行拼ID。事件请求+结果与扣减同事务；getConsumption区分未找到和读取失败。
- 验收：A01、B01—B11、B12—B14；读写只用临时数据库，覆盖回滚、耗尽、重复、重启。
- 不修改 C/D 的算法、公共头文件、MainWindow 或全局 CMake。

## C 菜谱与营养

- 修改：src/recipe/RecipeData.cpp、RecipeService.cpp、MealRecordStorage.h/.cpp；新增 src/recipe/RecipeContractAdapter.cpp；补 tests/recipe。
- 实现 IRecipeService 五个方法和 createRecipeService(const std::string& mealRecordsPath)。构造时与旧 recordMeal 统一存储路径；不依赖 Qt。
- 从已有 repository().all() 提供全量与按整数ID查询；新接口有明确读取错误。严格校验菜谱正重量、正时间、有限营养；保留原30道和营养公式。
- 按 v1.md 已定标准审核时间和标签；ingredients中的食用油也属必要食材，不偷偷从库存匹配中删除。
- 实现版本化、可可靠替换的事件+饮食记录文件；严格读取失败不覆盖原数据；旧记录继续可读；同事件不同内容拒绝。
- 验收：C01—C04、无Qt原4项测试和契约测试；旧API的兼容行为不随新严格查询无意改变。

## D 推荐

- 修改：src/recommendation/RecommendationEngine.cpp（复用可用评分助手，旧入口语义保留）；新增 src/recommendation/RecommendationContractAdapter.cpp；补 tests/recommend。
- 实现 IRecommendationService 与 createRecommendationService(IRecipeService&, IInventoryService&)；依赖服务引用，不依赖B私有数据库或C内部头文件。
- 只读 context.preferences / inventorySnapshot 的新条件，其他新路径规则见 v1.md。通过C全量查询、B对同一快照评估，避免旧候选入口丢掉无库存菜谱。
- 实现五心情、硬限制、一个快捷偏好、三个卡片角色、真实理由、整数ID去重、hasMore与换批。每个缺口保留Unknown明细。
- 为模块测试注入假的IRecipeService/IInventoryService即可独立开发，不等B/C业务完成；mock不得用于生产路径。
- 验收：D01—D05；B查询失败不能显示成“无可用菜谱”；不同过滤组合仍遵守硬约束。

## E 页面与奖励

- 修改：include/ui 和 src/ui 下的 HomeWidget、RecommendationWidget、StatisticsWidget，以及 reward/RewardSystem；新增 RecipeDetailsWidget、CookingConfirmationWidget、src/reward/RewardContractAdapter.cpp；补 tests/ui、tests/reward。
- 实现 IRewardService 的 applyCookingRewardOnce/getGrowthSnapshot，及 createRewardService(RewardSystem&)；借用主窗口已有RewardSystem，手动打卡和做饭共享可靠持久化路径。
- points()也应读新总分，保持旧签名；成长统计从getGrowthSnapshot读当前值，不把历史事件pointsAdded再累加。做饭次数不再用占位文字。
- 同时改首页与推荐页的五心情入口，不能只改其中一页。新信号使用V1枚举，旧信号在旧主窗口仍使用时保留。
- 与集成层的页面接线采用下表确定的方法名；页面代码不拥有业务数据库，不自行实施扣减或跨存储恢复。
- 验收：E01—E04，配合I01—I09；测试注入服务结果即可完成页面开发，真实流程由集成负责人接线。

### E 与集成层固定的新增页面边界

以下方法由E实现并加入相应Qt头文件，作为UI内部交接约定；本PR不创建未实现的QWidget方法以免MOC链接失败。参数类型前缀V1代表Shixu::Contracts::V1，字符串错误使用QString。

| 页面 | 新增方法或信号 | 含义 |
|---|---|---|
| HomeWidget | selectMoodV1(V1::Mood)；信号 moodSelectedV1(V1::Mood)、moodRecommendationRequestedV1(V1::Mood) | 首页与推荐页共用新五心情；不转换旧枚举数字 |
| RecommendationWidget | selectMoodV1(V1::Mood)；信号 moodSelectedV1(V1::Mood) | 选心情设置默认，双向同步用信号阻断避免循环 |
| RecommendationWidget | 信号 recommendationRequestedV1(const V1::RecommendationPreferences&) | UI维护时间/偏好/模式/已展示ID，日期由集成层填，库存由集成层查询 |
| RecommendationWidget | setRecommendationResult(const V1::RecommendationResult&)；信号 recipeSelected(int) | 直接接收原Recommendation卡片；新路径不再丢字段转成旧ViewItem |
| RecipeDetailsWidget | setRecipe(const Recipe&, const NutritionInfo&, const std::vector<V1::IngredientAvailability>&)；信号 cookingRequested(int) | 集成层查询最新菜谱和库存后填充；点击做完进入预览 |
| CookingConfirmationWidget | setPreview(const V1::CookingCompletionPreview&)；setProgress(const V1::CookingCompletionResult&)；setBusy(bool) | 展示快照/缺口、实际阶段，提交期间禁用按钮 |
| CookingConfirmationWidget | 信号 previewRequested(const V1::CookingCompletionRequest&)、completionRequested(const V1::CookingCompletionRequest&, const QString& confirmationToken) | 修改外部准备清单重新预览；确认时携带令牌 |
| CookingConfirmationWidget | 信号 retryRequested(const QString& eventId)、cancelPreparedRequested(const QString& eventId) | 重试或取消已保存但未扣减事件；普通未保存弹窗关闭无写入 |
| RecommendationWidget | resetExcludedRecipeIds() | 入库/扣减成功或日期变化后集成层清空换批排除记录 |

这些UI对象使用主线程直接连接；若以后改排队连接，由集成负责人统一补Q_DECLARE_METATYPE与注册，不让各页面重复声明。
旧方法与信号保留到MainWindow新接线完成，再由组长统一清理。NoNoodles等偏好按钮、默认时间及标签中文依v1.md，不需另设计一套。

## A 与集成负责人

- 负责公共头文件、MainWindow.h/.cpp、全局CMake、现有A录入联调；成员内部实现和各自测试由成员负责。
- 新增 src/integration/CookingCompletionService.cpp，实现ICookingCompletionService和工厂；日志位于库存文件同目录 cooking_events.sqlite。
- 主窗口持有单一InventoryManager和RewardSystem，依次构造 B适配→C适配→E适配→D服务→完成服务。逆序销毁服务，再销毁B/E对象；服务引用不得悬空。
- 统一创建UUID/应用时区日期；规范化配方需求；完成预览冻结菜谱/营养；先持久化Prepared再写B→C→E。重启读取未完成事件，用getCompletion恢复界面。
- C失败后不再扣B；B状态未知时不继续C/E、不取消；Prepared且B未扣减才能取消并重新确认。奖励成功后刷新库存、首页、推荐、成长数据。
- 详情首次加载及完成预览读最新库存；入库成功也清空D已展示ID。旧别名库存迁移只在此处复用A标准化能力，不让B/C/D各写别名字典。
- 按ServiceFactories.h构造；工厂不做业务写入，初始化失败由首次查询/操作返回错误。CMake新增源文件由组长集中接入；成员可先在独立测试目标验证，并附所需源文件清单。

## 合并与验收

技术审查结论允许现在从公共接口分支开工；不是自动合并许可。PR #9保持未合并状态，最终业务合并需通过各模块测试及acceptance.md真实联调。
文档和旧Word清单出现接口细节差异时，以本PR更新后的头文件、v1.md及本清单为准。旧Word仍用于了解总体分工，不要求再做一轮重复确认。
