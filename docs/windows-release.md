# Windows 便携分发

最终版采用主程序与可选离线图片识别扩展分开分发。主程序包含 Qt 运行库，不需要安装 Qt 或 Python；扩展包含独立 Python、PaddleOCR 依赖、两个本地模型和 VC++ 运行库安装文件。

## 构建与打包

使用 Qt 6.11.2 MinGW x64、Release 配置构建。可使用 `-DCMAKE_EXE_LINKER_FLAGS=-s` 移除调试符号，其他资源和功能保留。先运行完整 CTest，再运行：

```powershell
python tools/package_windows.py --build build-final-release --qt D:/QT/6.11.2/mingw_64 --runtime-source <已验证完整运行包目录> --output <空的交付目录> --version 20261030-final
```

脚本使用白名单打包程序、Qt 依赖、资源许可证和 OCR 文件；不会复制工作目录下的库存、做饭记录、个人资料、营养目标或头像。扩展仅省略 Python 缓存、测试和开发用头文件，保留第三方许可及运行依赖。重复运行使用新的输出目录，避免覆盖已交付文件。

## 使用

1. 将主程序 ZIP 完整解压到可写的较短路径（例如 `D:\Shixu`），双击 `Start-Shixu.cmd`。OCR 依赖包含较深的子目录，避免放在多层嵌套目录中，以免触发 Windows 路径长度限制。
2. 需要包装图片识别时，将扩展 ZIP 的 `runtime` 与 `ocr` 文件夹解压到 `Shixu.exe` 同级。无需联网加载模型。
3. 若 OCR 提示缺少 VC++ DLL，可运行 `runtime/vcredist` 中的微软安装程序后重试。
4. 主程序通过 `portable.flag` 将个人数据存到同级 `data` 文件夹。首次启动为空；分享时发送原始 ZIP，勿发送自己的 `data`。
5. 升级前退出要升级的程序并备份整个 `data` 文件夹。新旧版分别在不同文件夹运行时，数据互不覆盖。

发布验证应覆盖：干净 PATH 下独立解压包的启动及做饭流程、实际本地 OCR 识别、ZIP 完整性、SHA-256、无个人数据。现有开发窗口无需关闭。
