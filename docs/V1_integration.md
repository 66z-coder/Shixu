# V1 主程序接线与联调

B #11、C #12、D #10、E #13 已合入 develop。主窗口现在直接使用 V1 服务，不再通过旧三心情推荐路径调用新页面。

## 已接入的用户流程

- 智能录入提交成功后刷新库存与首页，清除推荐换批排除记录。
- 首页和推荐页同步五心情；新推荐服务读取真实菜谱和同一库存快照，日期按 Asia/Shanghai 填入。
- 推荐卡片与菜谱目录均可打开详情；详情查询最新库存并显示营养和缺口。
- 点击“我已做完”生成新 UUID 和固定时间，进入预览。缺少或重量未知的食材逐项确认外部备好，重新预览成功后才能提交。
- 依次扣库存、记饮食、发奖励；失败保留最后已保存阶段，关闭窗口不删除待处理事件。
- “待处理做饭记录”入口在重启后仍可使用。已扣库存只能继续结算；只有 B 明确未扣减的 Prepared 事件允许取消。
- 库存变化后必须重新预览并由用户确认，不会后台重新确认后自动扣减。

## 存储与恢复

库存继续使用工作目录的 shixu.db；饮食继续使用同目录 meal_records.txt；奖励保留 RewardSystem 原有 rewards.ini 默认位置。
新增同目录 cooking_events.sqlite，保存版本化、带 SHA-256 完整性校验的二进制快照和每一步结果，浮点数据不通过显示字符串舍入。
快照包含原始菜谱、食材需求、营养、请求、时间和外部准备清单。重试不重新计算菜谱，也不更换事件编号。

协调服务用独立 SQLite 连接存日志，借用 B/C/E 服务。操作按日志路径锁串行执行；B/C/E 各自的持久化幂等处理弥合“业务已提交、日志未更新”的窗口。
日志读取失败、损坏、B 查询失败均明确报告，不当作没有记录。Cancelled 保留为不可重试的记录。

旧库存别名在主窗口初始化时由 A 的 RecognitionService 规范化，B 只负责在原连接内原子更新名称与 revision；不合并批次、不修改已有消费事件。

## 测试

Qt 6.11.2 / MinGW 13.1 下完整构建，包括 Shixu 主程序。根 CTest 接入原测试及 D/E 独立业务测试，共 25 项。
禁用 Qt 的构建保持 7 项测试。测试使用临时数据库、饮食文件与奖励文件，不依赖个人库存。

新增 CookingCompletionTest 使用真实 B/C/D/E 服务，覆盖：

- 五心情真实 B/C 输入、正常完成、实际临期奖励、重复确认与重启重放。
- C 写入失败、E 写入失败及恢复，验证库存版本、饮食记录数、做饭次数不重复增加。
- 多批库存更新中途 SQL 故障，逐批核对数量与重量回滚，版本和事件也回滚。
- 库存变化、冻结请求冲突、扣减前取消、取消后拒绝重试、扣减后拒绝取消。
- 重启后重新预览及明确确认。
- B 已提交、日志保存失败后重启恢复，验证 B 不再次扣减。
- 旧别名迁移与 revision 推进；日志不可写与内容损坏时停止。

新增 MainWindowFlowTest 在 offscreen Qt 中实际点击：首页心情→推荐卡→详情→外部食材确认→完成→重建主窗口，核对实际库存事件和奖励次数。
原 PackagingIntegrationTest 覆盖录入入库；OCR 测试覆盖协议及已有录入流程，本轮未用摄像头或新拍摄小票做人工识别验收。

构建示例（Qt/编译器路径按本机配置）：

```powershell
cmake -S . -B build-integration -G Ninja -DCMAKE_PREFIX_PATH=<Qt安装目录>
cmake --build build-integration
ctest --test-dir build-integration --output-on-failure
cmake -S . -B build-noqt -G Ninja -DCMAKE_DISABLE_FIND_PACKAGE_QT=TRUE
cmake --build build-noqt
ctest --test-dir build-noqt --output-on-failure
```

限制写入的运行环境下，将 TEMP/TMP 指向构建目录内的可写临时目录。真实应用运行时应使用固定工作目录，避免读取到另一份 shixu.db。
