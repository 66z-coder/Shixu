# 包装图片本地 OCR
运行 build/food_entry_demo.exe（Shixu.exe 为整合主界面），点击“导入包装图片”→“识别包装文字”→“解析日期信息”，核对后添加到待提交列表。支持取消和 120 秒超时。日期录入见 [包装日期说明](packaging_dates.md)。

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

程序按编译配置查找项目目录，移动源码后需要重新配置 CMake。
可用 SHIXU_OCR_PYTHON、SHIXU_OCR_SCRIPT、SHIXU_OCR_MODELS 指定绝对路径。
分发程序不能只复制 exe，还需要 Qt、Python 环境及模型。

## 验证
cmake --build build
ctest --test-dir build --output-on-failure

保留四项测试：名称单位标准化、本地 OCR 进程协议、包装日期解析、包装录入交互。
receipt_ocr_test 使用临时模拟脚本覆盖进程成功、取消、超时等情况，不代表真实模型准确率。
模型和第三方依赖不计入自行编写的代码行数。
