# 固定联调样例

本 PR 的 ContractCompileTest / ContractImplementationTest / QtContractCompileTest 只检查类型、服务签名可实现性、现有 C 公式及 A/C 名称兼容、样例自洽，**不表示下列新业务已实现**。
业务验收由对应成员后续 PR 实现真实服务测试；不得用样例硬编码返回值代替服务。
[ContractFixtures.h](../../tests/contracts/ContractFixtures.h) 可供大家建立 mock；其中9001是合成菜谱ID，不加入正式菜谱库，营养值仅是计算测试数据。

统一测试日期 2026-10-28，不依赖运行电脑当天日期；每项使用独立临时存储，不操作个人 shixu.db。

| 编号/负责人 | 输入/操作 | 必须验证的实际结果 |
|---|---|---|
| A01 A/B/D | 番茄10个，每个150g，2026-10-30到期 | 原数量10、单件150、总1500；D评估300g需求为Enough |
| B01 B | A01消耗300g | batch101剩余8个、1200g；consumedExpiring=true |
| B02 B | 鸡蛋6个×50g，消耗100g | 剩4个、200g |
| B03 B | 同上消耗75g | 剩4.5个、225g；无界面舍入写回 |
| B04 B | 鸡蛋100g需求，已知50g并有未知重量批次 | Unknown、knownGap=50、hasUnknownWeight=true；不直接判定补买50g |
| B05 B | 已知150g并有未知批次，需求100g；再测完全无鸡蛋 | 前者Enough，后者Insufficient，不是Unknown |
| B06 B | 过期一批、今天到期一批、+3天一批、+4天一批、未知日期一批 | 过期排除；按日期/ID，未知日期最后；临期含今天与+3，不含+4 |
| B07 B | 同名两批每个50g/60g，跨批消耗 | 各用自己的重量比例，不能按统一50g换算 |
| B08 B | 预览后另一处改变库存再执行 | NeedsConfirmation/InventoryChanged，零扣减，重新预览后方可确认 |
| B09 B | 不足或未知未确认；再逐项确认外部准备 | 首次零扣减；确认后仅扣已知量；未知批次原样保留；零库存也记录事件 |
| B10 B | 多批扣减第二条SQL故障 | 整批回滚，无事件成功记录；修复后同事件重试成功一次 |
| B11 A/B | 500g、0.5kg、1斤；个未填重量；500ml | 前三者总重500g；后两者未知，不假设500ml=500g |
| C01 C | 正确/不存在菜谱ID、空/无法读取饮食文件 | 正确菜谱/NotFound；空库成功空列表/读取失败明确错误 |
| C02 C | 同eventId写MealRecord两次并重启再写 | 一条记录，重放AlreadyApplied；改营养/份数/时间EventConflict |
| C03 C | 旧记录+新事件；模拟写失败和损坏旧文件 | 旧记录不丢，失败保留原文件，损坏报告InvalidData而不是覆盖 |
| D01 D/E | 五心情，各自默认时间；犒劳改30；很累改60 | 默认30/45/30/40/75；用户上限30/60真正生效，不偷偷取较小值 |
| D02 D/E | 只用库存/允许补2种/未知量 | OnlyInventory仅Enough；SmallPurchase最多2种不足且无Unknown |
| D03 D/E | 空库存+普通推荐；NoNoodles；换批与条件变化 | 仍从全部菜谱筛选；排除面食；整数ID不重复，改条件清空排除集 |
| D04 D/E | 只有2道满足约束或无剩余菜谱 | 返回2/0张，不突破约束；hasMore正确；首选/备选角色和理由真实 |
| E01 E | 空成长记录，首个实际临期消耗事件 | +5+10+15=30，做饭1，打卡日1 |
| E02 E | 同日第二次、不用临期；重放第一事件 | 第二次+10，总40，做饭2，打卡日仍1；重放零新增，返回原30奖励结果 |
| I01 集成 | B成功后C写失败 | stage=InventorySaved；不发奖励；列表可重试，不能显示全部成功 |
| I02 集成 | I01重启后、次日重试、菜谱此时被修改 | 不重复扣库存；用旧快照、旧时间和营养；C/E只成功一次 |
| I03 集成 | 任一模块提交后进度保存前崩溃 | 重试命中该模块AlreadyApplied，补写进度，无重复效果 |
| I04 集成/E | 相同eventId不同请求；用户取消预览 | 前者EventConflict；后者库存/饮食/奖励无变化 |
| I05 集成 | Prepared后库存变化、重新预览确认 | 冻结菜谱/时间不变，只更新扣减预览；不自动同意新分配 |
| I06 集成 | 主程序与旧测试、无Qt C编译 | 旧功能不回归，接口在Qt/无Qt两种环境均编译 |

## 完成流程的固定消息示例

- 请求：eventId=`5f778a96-91c7-4e42-a5ae-d7ad189d6101`，recipeId=9001，servings=1，completedAt=`2026-10-28 18:00:00`。
- B：番茄300g，batch101从1500g/10个到1200g/8个；实际临期消耗=true。
- C：合成菜谱营养60kcal/蛋白质3g/碳水9g/脂肪0.6g；一条饮食记录。
- E：首次打卡5 + 做饭10 + 临期15 =30；重放返回同一结果但不再次增加总积分。
- 集成：Prepared→InventorySaved→MealSaved→RewardSaved→Completed。

## 编译样例

```powershell
cmake -S . -B build-contracts -DCMAKE_PREFIX_PATH=<Qt安装目录>
cmake --build build-contracts
ctest --test-dir build-contracts --output-on-failure
```

无需Qt：

```powershell
cmake -S . -B build-contracts-noqt -DCMAKE_DISABLE_FIND_PACKAGE_QT=TRUE
cmake --build build-contracts-noqt
ctest --test-dir build-contracts-noqt --output-on-failure
```

按本机编译器补充 -G 和工具路径。无Qt包含原C的4个测试及2个契约测试，共6项；Qt完整构建保留原14个测试并增加3个契约测试，共17项。


## 技术审查新增验收项

| 编号/负责人 | 输入/操作 | 必须验证的实际结果 |
|---|---|---|
| B12 B | 一批全部用完后读取新旧库存、首页临期查询 | quantity=0/weight=0，不继续显示为库存或Unknown |
| B13 B | getConsumption查询已有、不存在、数据库失败 | 已有原结果；NotFound；StorageFailure，三者不得混淆 |
| B14 B | addFoods后持旧revision预览令牌执行 | 版本同事务更新，原令牌失效；失败入库不推进版本 |
| C04 C | 高精度营养/中文路径/菜名含管道和换行，保存重启 | 可读取原内容，同事件重试不因精度或转义而冲突 |
| D05 D/E | 库存查询失败；只有未知重量；新偏好与旧mood矛盾 | 错误明确；未知不丢；新服务以preferences为唯一条件 |
| E03 E | 手动打卡后做饭以及反向顺序 | 每日5分只一次，原points()与GrowthSnapshot一致 |
| E04 E | 第二次做饭后重放第一事件，再查成长数据 | 原事件仍返回原奖励快照，实时总次数/总积分从getGrowthSnapshot获取 |
| I07 集成/E | 重启后打开Pending事件，获取详情 | getCompletion返回原请求、配方、营养、时间和真实进度，可重新预览 |
| I08 集成/B | Prepared后缺口变化，取消后重新确认；另测B已扣减或读取失败 | 仅明确未扣减可取消；Cancelled不再重试，不删除旧ID；另两种情况拒绝取消 |
| I09 集成/E | 重启丢失预览令牌但B未扣减 | 需要明确重新确认，禁止后台刷新令牌后自动扣减 |

联调计数仍区分契约测试和业务测试。成员不需要新增业务功能才能运行本PR的编译样例；构造函数只声明，由各业务PR实现，组长在主程序接线时再链接它们。
