"""Package a clean Windows app and optional offline OCR add-on, using stdlib only."""
import argparse
import hashlib
import os
from pathlib import Path
import shutil
import subprocess
import zipfile


def archive(path, files):
    with zipfile.ZipFile(path, "x", zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        for source, name in sorted(files, key=lambda item: item[1]):
            z.write(source, name)
    with path.open("rb") as stream:
        digest = hashlib.file_digest(stream, "sha256").hexdigest()
    path.with_suffix(path.suffix + ".sha256").write_text(f"{digest}  {path.name}\n", encoding="utf-8")
    print(f"{path.name}: {path.stat().st_size / 1024**2:.1f} MiB, SHA256 {digest}", flush=True)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--build", type=Path, required=True)
    parser.add_argument("--qt", type=Path, required=True)
    parser.add_argument("--runtime-source", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--version", required=True)
    args = parser.parse_args()
    repo = Path(__file__).resolve().parents[1]
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    root_name = "Shixu-Windows-x64-" + args.version
    app = output / root_name
    app.mkdir()
    shutil.copy2(args.build / "Shixu.exe", app)
    env = os.environ.copy()
    env["PATH"] = str(args.qt / "bin") + os.pathsep + env.get("PATH", "")
    subprocess.run([str(args.qt / "bin/windeployqt.exe"), "--release", "--no-translations",
                    "--no-opengl-sw", "--no-system-d3d-compiler", "--no-system-dxc-compiler",
                    "--skip-plugin-types", "generic,networkinformation,tls",
                    "--exclude-plugins", "qsqlibase,qsqlmimer,qsqloci,qsqlodbc,qsqlpsql",
                    str(app / "Shixu.exe")], env=env, check=True)
    (app / "portable.flag").touch()
    (app / "qt.conf").write_text("[Paths]\nPrefix=.\nPlugins=.\n", encoding="ascii")
    (app / "ocr").mkdir()
    shutil.copy2(repo / "ocr/receipt_ocr.py", app / "ocr")
    (app / "Start-Shixu.cmd").write_text(
        '@echo off\nsetlocal\ncd /d "%~dp0"\n'
        'set "PYTHONHOME="\nset "PYTHONPATH="\n'
        'set "QT_PLUGIN_PATH="\nset "QT_QPA_PLATFORM_PLUGIN_PATH="\n'
        'set "QT_QPA_PLATFORM=windows"\nset "QT_SCALE_FACTOR="\n'
        'set "SHIXU_OCR_PYTHON=%~dp0runtime\\python\\python.exe"\n'
        'set "SHIXU_OCR_SCRIPT=%~dp0ocr\\receipt_ocr.py"\n'
        'set "SHIXU_OCR_MODELS=%~dp0ocr\\models"\n'
        'start "" "%~dp0Shixu.exe"\n', encoding="ascii", newline="\r\n")
    shutil.copytree(args.runtime_source / "licenses", app / "licenses")
    compiler = shutil.which("g++", path=env["PATH"])
    if not compiler:
        raise RuntimeError("Add the matching MinGW bin directory to PATH before packaging")
    shutil.copytree(Path(compiler).resolve().parents[1] / "licenses", app / "licenses/MinGW")
    (app / "licenses/THIRD-PARTY-NOTICES.txt").write_text(
        "Qt 6.11.2 / MinGW x64, dynamically linked. Corresponding licenses are included here.\n"
        "Qt library replacement and debugging modifications are permitted under their licenses.\n"
        "Qt source: https://download.qt.io/archive/qt/6.11/6.11.2/single/\n"
        "App source: https://github.com/66z-coder/Shixu/tree/develop\n"
        "Optional OCR: Python license and dependency notices are preserved under runtime/python.\n"
        "PaddleOCR / PP-OCRv5: https://github.com/PaddlePaddle/PaddleOCR (Apache-2.0).\n",
        encoding="utf-8")
    revision = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=repo, text=True).strip()
    (app / "VERSION.txt").write_text(f"{args.version}\nSource commit: {revision}\n", encoding="ascii")
    (app / "使用说明.txt").write_text(
        "食序 Windows 10/11 x64 便携版\n\n"
        "1. 完整解压到较短路径（如 D:\\Shixu）后双击 Start-Shixu.cmd，不要在压缩软件里直接运行。\n"
        "2. 主程序包含库存、手动录入、推荐、菜谱、营养目标与成长功能，无需安装开发环境。\n"
        "3. 图片识别需另下载离线 OCR 扩展，将 runtime、ocr 文件夹放到 Shixu.exe 同级。\n"
        "   如出现 VC++ DLL 缺失提示，可运行 runtime/vcredist 中的安装程序。\n"
        "4. 图片仅在本机识别，请核对识别结果后入库。\n"
        "5. 第一次启动没有个人数据；数据自动保存到同级 data 文件夹。\n"
        "6. 更新前备份整个 data 文件夹。分享给别人请发送原始 ZIP，不附带自己的 data。\n"
        "7. 适用于 Windows 10/11 64 位 x64 电脑；OCR 首次加载较慢。\n\n"
        f"版本：{args.version}\n源代码提交：{revision}\n"
        f"源代码：https://github.com/66z-coder/Shixu/tree/{revision}\n",
        encoding="utf-8-sig")
    main_files = [(p, root_name + "/" + p.relative_to(app).as_posix()) for p in app.rglob("*") if p.is_file()]
    archive(output / (root_name + ".zip"), main_files)
    addon_files = []
    for folder in ("runtime", "ocr/models"):
        for p in (args.runtime_source / folder).rglob("*"):
            if not p.is_file():
                continue
            rel = p.relative_to(args.runtime_source)
            if any(part in {"__pycache__", ".pytest_cache", "tests", "test"} for part in rel.parts):
                continue
            if p.suffix.lower() in {".pyc", ".pyo", ".pdb"} or "site-packages/paddle/include/" in rel.as_posix():
                continue
            addon_files.append((p, rel.as_posix()))
    note = output / "OCR扩展安装说明.txt"
    note.write_text("请先将主程序解压到较短路径（如 D:\\Shixu）。将本扩展中的 runtime、ocr 文件夹解压到 Shixu.exe 所在文件夹。\n"
                    "完成后重开程序即可进行包装图片识别，无需联网下载模型。\n"
                    "若提示缺少 VC++ DLL，请运行 runtime/vcredist 下的微软运行库安装程序。\n"
                    "扩展不会修改个人 data 文件夹。\n", encoding="utf-8-sig")
    addon_files += [(repo / "ocr/receipt_ocr.py", "ocr/receipt_ocr.py"), (note, note.name)]
    archive(output / ("Shixu-Offline-OCR-" + args.version + ".zip"), addon_files)


if __name__ == "__main__":
    main()
