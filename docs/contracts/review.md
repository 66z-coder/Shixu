# 跨模块接口审查结论

审查日期 2026-10-28。结论：**下面的接口缺口已补齐，可从PR #9的公共接口分支直接开始模块实现。** 不代表新业务已实现、成员已承诺工期或develop已合并。

## 核对范围

远端同步后 develop 仍为 `39bd9193e2ecf738cc9bb2fa33ba5e676da2da15`。
已核对 origin/feature/inventory、feature/recipe-nutrition、feature/recommend、feature/mood-recommend-ui、feature/ui-reward，均无未包含于develop的独有提交。因此以develop为共同实际代码基线，不依赖组员本机未推送代码。
审查包括B数据库、C菜谱/饮食文件、D候选与评分、E首页/推荐页/奖励/统计、MainWindow接线，以及A名称字典和重量输出。

## 发现与处理

| 问题与现有证据 | 处理后的约定 |
|---|---|
| inventory.cpp的getAllItems不返回id且错误返回空；数据库连接私有 | B适配器在B内部访问同一连接；新查询返回稳定id/revision/错误，不能拼造批次 |
| 旧库存查询没有排除quantity=0 | 扣减实现同时更新旧库存/首页/临期查询，耗尽记录不显示为未知 |
| 协调进度落后B实际提交时，原协议只能靠旧令牌重试 | 增加getConsumption；重放仍核对持久化原请求，先去重后检查令牌 |
| Pending只含eventId/结果，重启页面无法拿到请求和快照 | 增加getCompletion返回CookingCompletionDetails，用于恢复预览/外部准备清单 |
| Prepared冻结后库存变化可能需要新增外部准备，原协议会永远EventConflict | 增加cancelPreparedCompletion与Cancelled；只在B明确未提交时可取消并重新确认，已扣减禁止取消 |
| RewardSystem仅存打卡日期；points等于天数乘5，做饭统计仍占位 | 新增getGrowthSnapshot；新旧奖励写入共用RewardSystem，原points转读新累计，不从事件返回值再次加分 |
| HomeWidget与RecommendationWidget信号都是旧三心情，MainWindow还转成缺字段的ViewItem | 明确E新增V1信号/展示方法，保留旧接线到组长切换；V1直接传原卡片，不经旧ViewItem丢失role/availability |
| D旧readUserContext丢弃未知重量；findRecipesByFoods要求至少一种库存 | 新路径只使用InventorySnapshot、C全量查询和B统一评估；旧路径保留回归兼容 |
| C旧loadAll把打不开当空库、坏行静默跳过，append没有eventId且默认double精度可能变化 | 新接口严格读取，原子保存事件+记录，max_digits10持久化；保留旧API测试兼容，不用旧void结果判定成功 |
| C配方含食用油，默认“家里有调料”会让D与B需求不一致 | 所有正重量ingredients都参与匹配和扣减，配方外调料不新增；不推测油/可乐密度 |
| 食材名称标准化有Qt资源依赖，C不能直接调用而保持无Qt | 当前内置名称与A字典一致性自动检查；C维护标准名，旧库存迁移由A/集成层统一负责 |
| 只给抽象方法没有统一构造与所有权，成员容易新开B/E存储 | 新增ServiceFactories.h和D工厂，借用已有B/E实例，明确构造/销毁顺序与文件归属 |

## 规则已经确定 不再留给各成员重新协商

- A提供重量，B验证保存；重量未知不推测，计数可为小数，批次分别计重。
- 菜谱ID=int；一份指现有一份配方，不变更现有营养公式和菜谱数量。
- 五心情、默认时间、筛选模式、快捷按钮及标签规则以v1.md为准；D只决定内部评分权重，不改跨模块口径。
- 一次完成共用UUID和冻结日期快照；B/C/E结果持久化，失败可重试；取消仅限尚未扣减。
- 成员实现位置、服务工厂、E页面交接、CMake与MainWindow所有权已在member-checklist.md列明。

## 验证范围与限制

- 现有完整程序与原14项测试用于检查接口扩展不会破坏旧流程。
- 标准C++契约测试检查全部服务可按签名实现、恢复信息足够，以及C现有营养计算兼容；无Qt构建继续验证。
- Qt契约测试核对真实内置菜谱ID、用量与A名称标准化一致性，并验证新D请求和卡片类型可用。
- 这些不是新B扣减/C事件存储/E奖励/集成恢复的实际功能测试；后续成员必须按acceptance.md实现真实故障与重启验收，不能把mock当成功证据。
- 这轮没有发现仍需重新约定才能开工的公共接口阻碍。成员尚未推送的本地改动不在本次可见范围；若发生冲突按共同基线适配。

## 本轮实测结果

- Qt 6.11.2 / MinGW 13.1：完整主程序构建成功，CTest 17/17通过（原14项 + 3项契约测试）。
- 禁用Qt：构建成功，CTest 6/6通过（原C四项 + 标准C++契约两项）。
- 全部现有内置菜谱的ID、正用量、正时间、有限非负营养及A标准名称一致性检查通过。
- git diff --check通过；个人数据库及构建产物不在提交范围。
