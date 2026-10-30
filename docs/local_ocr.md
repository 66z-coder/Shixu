# 包装图片本地 OCR
运行 Shixu.exe 的“智能录入”页（food_entry_demo.exe 为不入库的独立演示），点击“导入包装图片”→“识别包装文字”→“解析日期信息”，核对后添加到待提交列表。支持取消和 120 秒超时。日期录入见 [包装日期说明](packaging_dates.md)。

## 本地环境

Windows x64、Python 3.12、CPU 推理。项目使用独立 `.venv-ocr` 环境，不依赖全局 Python 包。新电脑先安装 Python 3.12，再在项目目录执行：

```powershell
./ocr/setup_ocr.ps1
# 或明确指定 Python 3.12 路径：
./ocr/setup_ocr.ps1 -PythonExe 'C:/Python312/python.exe'
```

安装需要联网下载依赖及官方 PP-OCRv5 mobile 检测/识别模型。日常识别只读取 `ocr/models` 中的本地文件；模型缺失时明确报错，不在识别阶段自动下载。图片不上传。当前不启用整页旋转分类和弯曲矫正，应选择文字朝上、清晰的包装标签；尊重照片 EXIF 方向。

`requirements.txt` 标明主要依赖，`requirements-lock.txt` 记录本次已验证环境的全部包版本，安装脚本使用后者。本机虚拟环境基于 Codex 提供的 Python 3.12.14；若其运行时以后被移除，需要用自行安装的 Python 3.12 重建虚拟环境。

## 当前调用链
PackagingEntryWidget → ReceiptOcrService → ocr/receipt_ocr.py → 本地 PaddleOCR。
ReceiptOcrService、ReceiptOcrResult、receipt_ocr.py 保留历史名称，实际是包装识别正在使用的通用图片转文字组件，不是废弃的小票解析逻辑。
ReceiptOcrResult 保存文字、置信度和坐标；PackagingDateParser 负责日期解析。
Python 只做模型加载和推理，业务规则由 C++ 处理。

识别服务首次请求时启动一个无控制台的 Python 工作进程，后续请求复用同一个 PaddleOCR 实例。原有模型、CPU 参数、EXIF 方向处理、文字/置信度/坐标输出不变。第一次仍需加载运行库和模型；同一程序内连续识别无需重复加载。关闭程序时结束工作进程；取消、超时或进程崩溃后，下次请求自动重新启动。损坏图片返回错误，不阻止下一张图片识别。

工作进程通过标准输入逐行接收 JSON（`id`、`image`、`output`），原子写入每次独立的结果文件，并输出带请求 ID 的完成标记；普通日志不会作为识别结果解析。仍支持原有 `--image --models --output` 单次命令行用法。

程序按编译配置查找项目目录，移动源码后需要重新配置 CMake。
可用 SHIXU_OCR_PYTHON、SHIXU_OCR_SCRIPT、SHIXU_OCR_MODELS 指定绝对路径。
分发程序不能只复制 exe，还需要 Qt、Python 环境及模型。

## 验证
cmake --build build
ctest --test-dir build --output-on-failure

保留四项测试：名称单位标准化、本地 OCR 进程协议、包装日期解析、包装录入交互。
receipt_ocr_test 使用临时模拟脚本覆盖进程成功、取消、超时等情况，不代表真实模型准确率。
模拟进程测试需要 Python；未配置 `.venv-ocr` 时设置 `SHIXU_OCR_PYTHON` 为独立运行包的 `runtime/python/python.exe`。现在也覆盖连续复用、损坏结果、过大结果、分段输出、错误请求标记、崩溃/取消/超时后重新识别和 Windows 无控制台。

精简运行包必须在实际解压后验证。`tests/recognition/test_runtime_loader.py` 检查归档模块的导入、源码查看、资源读取和命名空间；可选构建目标 `receipt_ocr_live_test` 接收一个 JSON 数组文件，各项为 `{"image":"绝对图片路径","expected":"旧版识别结果 JSON 路径"}`，逐项比较真实 Qt 调用后的文字、置信度和坐标。该测试不读写库存或个人数据。测试时将 `TEMP`、`TMP` 指向可写临时目录，并设置三个 OCR 路径环境变量。
模型和第三方依赖不计入自行编写的代码行数。
