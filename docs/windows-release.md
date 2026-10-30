# Windows 便携分发

最终版采用主程序与可选离线图片识别扩展分开分发。主程序包含 Qt 运行库，不需要安装 Qt 或 Python；扩展包含独立 Python、PaddleOCR 依赖、两个本地模型和 VC++ 运行库安装文件。

## 构建与打包

使用 Qt 6.11.2 MinGW x64、Release 配置构建。主程序设置 `WIN32_EXECUTABLE`，避免启动时创建控制台；OCR 子进程使用 `CREATE_NO_WINDOW`，仍通过管道收集错误信息。可使用 `-DCMAKE_EXE_LINKER_FLAGS=-s` 移除调试符号，其他资源和功能保留。先运行完整 CTest，再运行：

```powershell
# 用已验证运行包里的 Python 3.12 生成精简副本，原始运行包不变：
& <原始运行包>/runtime/python/python.exe tools/slim_ocr_runtime.py --source <原始运行包> --output <新的精简目录>
# 检查精简目录中的实际离线 OCR 后打包；PATH 包含匹配的 MinGW bin：
python tools/package_windows.py --build build-final-release --qt D:/QT/6.11.2/mingw_64 --runtime-source <新的精简目录> --output <空的交付目录> --version 20261030-ocr-fast
```

脚本使用白名单打包程序、Qt 依赖、资源许可证和 OCR 文件；不会复制工作目录下的库存、做饭记录、个人资料、营养目标或头像。重复运行使用新的输出目录，避免覆盖已交付文件。

精简工具只面向锁定版本的 Python 3.12 / PaddleOCR 环境。移除测试、开发头文件、pip、视频解码器及当前离线照片识别不用的云端/训练模块；保留模型、原生库、图像读写、许可证和运行数据。标准库合并为 `python312.zip`；依赖源码及预编译字节码合并为 `packages.zip`，由 `ocr/runtime_loader.py` 保持逻辑路径、源码检查及物理资源读取。不能把这个目录当作通用 Python 开发环境，也不能只更换个别依赖版本。清单保存在 `runtime/SLIM-MANIFEST.json`。打包包含必要的空包目录，确保解压后命名空间查找不变。

OCR 适配脚本只由主程序包提供，扩展不再覆盖它，避免降级进程协议。

预编译缓存只保留在标准库及 Paddle、PaddleX、PaddleOCR、NumPy、Pillow 核心包中，其余归档源码按需在内存编译，避免为很少使用的功能重复存储字节码。

仅更新主程序、OCR 扩展没有变化时，可添加 `--skip-ocr-addon` 复用已交付扩展。

## 使用

1. 将主程序 ZIP 完整解压到可写的较短路径（例如 `D:\Shixu`），双击 `Shixu.exe`，不会创建控制台。`Start-Shixu.cmd` 保留为兼容启动入口，运行脚本可能短暂闪现命令窗口。OCR 依赖包含较深的子目录，避免放在多层嵌套目录中，以免触发 Windows 路径长度限制。
2. 需要包装图片识别时，将配套扩展 ZIP 的 `runtime` 与 `ocr` 文件夹解压到 `Shixu.exe` 同级。无需联网加载模型。首次识别需要加载，后续图片复用模型。
3. 若 OCR 提示缺少 VC++ DLL，可运行 `runtime/vcredist` 中的微软安装程序后重试。
4. 主程序通过 `portable.flag` 将个人数据存到同级 `data` 文件夹。首次启动为空；分享时发送原始 ZIP，勿发送自己的 `data`。
5. 本版放在新目录中安装配套扩展，勿混用旧版 runtime 或用旧适配脚本覆盖新文件。要迁移数据时，先退出旧版、备份并复制整个 `data` 文件夹到新目录。新旧版分别在不同文件夹运行时，数据互不覆盖。

发布验证应覆盖：主程序 PE 子系统为 Windows GUI、启动不创建控制台、干净 PATH 下独立解压包的启动及做饭流程、实际本地 OCR 识别、ZIP 完整性、SHA-256、无个人数据。现有开发窗口无需关闭。
