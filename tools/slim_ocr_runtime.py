"""Build a compact offline runtime without modifying the known-good source bundle.

Keep native libraries, model files, licenses and filesystem-sensitive packages intact.
Use zipimport for the stdlib and runtime_loader for dependency source/bytecode.
This builder targets the pinned Python 3.12 / PaddleOCR runtime, not arbitrary environments.
"""
import argparse
import json
import importlib.util
import marshal
from pathlib import Path
import shutil
import struct
import sys
import zipfile

DEVELOPMENT_SUFFIXES = {'.pyc', '.pyo', '.pdb', '.pyi', '.lib', '.a', '.h', '.hpp', '.cmake'}
UNUSED_CLOUD_PACKAGES = {'baidubce', 'Crypto', 'hf_xet', 'httpcore2', 'httpx2'}
UNUSED_MODELSCOPE_TASKS = {'models', 'pipelines', 'trainers', 'metrics', 'msdatasets',
                         'preprocessors', 'exporters'}
# Retain startup caches for the image/inference path, without duplicating bytecode
# for every optional dependency. All remaining sources can compile in memory.
PRECOMPILED_PACKAGES = {'paddle', 'paddlex', 'paddleocr', 'numpy', 'PIL'}


def excluded(rel):
    parts = rel.parts
    if any(p in {'__pycache__', '.pytest_cache', 'tests', 'test'} for p in parts):
        return True
    if rel.suffix.lower() in DEVELOPMENT_SUFFIXES:
        return True
    if parts[:3] == ('runtime', 'python', 'Lib'):
        if len(parts) > 3 and parts[3] in {'idlelib', 'tkinter', 'ensurepip'}:
            return True
    name = rel.as_posix()
    if '/site-packages/' in name:
        package = name.split('/site-packages/', 1)[1].split('/')[0]
        if package == 'pip' or package.startswith('pip-'):
            return True
        if package in UNUSED_CLOUD_PACKAGES:
            return True
        package_path = Path(name.split('/site-packages/', 1)[1])
        if package == 'modelscope' and len(package_path.parts) > 1 and package_path.parts[1] in UNUSED_MODELSCOPE_TASKS:
            return True
        if '/paddle/include/' in name or '/cv2/data/' in name:
            return True
        # Photo recognition never invokes OpenCV's video decoder.
        if rel.name.startswith('opencv_videoio_ffmpeg'):
            return True
    return False


def build(source, destination):
    if sys.version_info[:2] != (3, 12):
        raise RuntimeError('Run this builder with the bundled Python 3.12 to generate matching bytecode')
    source = source.resolve()
    destination = destination.resolve()
    destination.mkdir(parents=True, exist_ok=False)
    python = destination / 'runtime/python'
    site = python / 'Lib/site-packages'
    site.mkdir(parents=True)
    removed, archived = [], []
    before_count = before_bytes = 0
    def add_module(archive, path, name, precompile=True):
        archive.write(path, name)
        if path.suffix == '.py' and precompile:
            data = path.read_bytes()
            code = compile(data, name, 'exec', dont_inherit=True)
            stat = path.stat()
            header = importlib.util.MAGIC_NUMBER + struct.pack('<III', 0, int(stat.st_mtime), len(data))
            info = zipfile.ZipInfo.from_file(path, name + 'c')
            info.compress_type = zipfile.ZIP_DEFLATED
            archive.writestr(info, header + marshal.dumps(code))

    with zipfile.ZipFile(python/'python312.zip', 'x', zipfile.ZIP_DEFLATED, compresslevel=9) as stdlib, \
         zipfile.ZipFile(site/'packages.zip', 'x', zipfile.ZIP_DEFLATED, compresslevel=9) as packages:
        for folder in ('runtime', 'ocr/models'):
            for path in sorted((source/folder).rglob('*')):
                if not path.is_file():
                    continue
                rel = path.relative_to(source)
                before_count += 1
                before_bytes += path.stat().st_size
                if excluded(rel):
                    removed.append(rel.as_posix())
                    continue
                if rel.parts[:4] == ('runtime', 'python', 'Lib', 'site-packages'):
                    inside = Path(*rel.parts[4:])
                    # cv2 executes config.py by filesystem path; leave its bootstrap intact.
                    if inside.suffix == '.py' and inside.parts[0] not in {'cv2', '_distutils_hack', 'setuptools'}:
                        (site/inside).parent.mkdir(parents=True, exist_ok=True)
                        add_module(packages, path, inside.as_posix(), inside.parts[0] in PRECOMPILED_PACKAGES)
                        archived.append(rel.as_posix())
                        continue
                elif rel.parts[:3] == ('runtime', 'python', 'Lib'):
                    add_module(stdlib, path, Path(*rel.parts[3:]).as_posix())
                    archived.append(rel.as_posix())
                    continue
                target = destination / rel
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(path, target)
    (python/'python312._pth').write_text(
        '.\nDLLs\npython312.zip\nLib/site-packages\nLib/site-packages/packages.zip\nimport site\n',
        encoding='ascii')
    shutil.copy2(Path(__file__).resolve().parents[1]/'ocr/runtime_loader.py', site/'sitecustomize.py')
    # Keep third-party notices accessible even when their package sources are archived.
    shutil.copytree(source/'licenses', destination/'licenses')
    all_files = [p for p in destination.rglob('*') if p.is_file()]
    manifest = {'source_files':before_count, 'source_bytes':before_bytes,
                'files':len(all_files), 'bytes':sum(p.stat().st_size for p in all_files),
                'packed_source_modules':len(archived), 'precompiled_packages':sorted(PRECOMPILED_PACKAGES),
                'removed':removed, 'archived':archived}
    (destination/'runtime/SLIM-MANIFEST.json').write_text(json.dumps(manifest,indent=2),encoding='utf-8')
    print(json.dumps({k:v for k,v in manifest.items() if k not in ('removed','archived')},indent=2))


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    build(args.source, args.output)
