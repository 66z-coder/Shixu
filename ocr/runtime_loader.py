"""Packed-source loader for the pinned portable OCR runtime (installed as sitecustomize).

Native extensions and data stay on disk. Python code retains its original logical
file location, package paths, source introspection and resource access.
"""
import importlib.abc
import importlib.machinery
import importlib.util
import marshal
from pathlib import Path
import sys
import types
import zipfile

ROOT = Path(__file__).resolve().parent
ARCHIVE = ROOT / 'packages.zip'


class PackedLoader(importlib.abc.InspectLoader):
    def __init__(self, archive, name, source, package):
        self.archive, self.name, self.source, self.package = archive, name, source, package
        self.path = str(ROOT / source)

    def create_module(self, spec):
        return None

    def exec_module(self, module):
        exec(self.get_code(module.__name__), module.__dict__)

    def get_filename(self, fullname):
        return self.path

    def is_package(self, fullname):
        return self.package

    def get_source(self, fullname):
        return importlib.util.decode_source(self.archive.read(self.source))

    def get_code(self, fullname):
        if self.source + 'c' not in self.archive.NameToInfo:
            return compile(self.archive.read(self.source), self.path, 'exec', dont_inherit=True)
        bytecode = self.archive.read(self.source + 'c')
        if bytecode[:4] != importlib.util.MAGIC_NUMBER:
            raise ImportError('OCR runtime bytecode does not match its Python version')
        def relocate(code):
            constants = tuple(relocate(value) if isinstance(value, types.CodeType) else value
                              for value in code.co_consts)
            return code.replace(co_filename=self.path, co_consts=constants)
        return relocate(marshal.loads(bytecode[16:]))

    def get_data(self, path):
        relative = Path(path).resolve().relative_to(ROOT).as_posix()
        if relative in self.archive.NameToInfo:
            return self.archive.read(relative)
        return Path(path).read_bytes()

    def get_resource_reader(self, fullname):
        if self.package:
            from importlib.resources.readers import FileReader
            return FileReader(self)
        return None


class PackedFinder(importlib.abc.MetaPathFinder):
    def __init__(self, archive):
        self.archive = zipfile.ZipFile(archive)
        self.names = frozenset(self.archive.namelist())

    def find_spec(self, fullname, path=None, target=None):
        leaf = fullname.rsplit('.', 1)[-1]
        for parent in path if path is not None else [str(ROOT)]:
            try:
                relative = Path(parent).resolve().relative_to(ROOT).as_posix()
            except (ValueError, OSError):
                continue
            prefix = '' if relative == '.' else relative + '/'
            for source, package in ((prefix + leaf + '/__init__.py', True), (prefix + leaf + '.py', False)):
                if source not in self.names:
                    continue
                loader = PackedLoader(self.archive, fullname, source, package)
                locations = [str((ROOT/source).parent), str(ARCHIVE) + '/' + source.rsplit('/', 1)[0]] if package else None
                return importlib.util.spec_from_file_location(fullname, loader.path, loader=loader,
                                                              submodule_search_locations=locations)
        return None


if ARCHIVE.is_file():
    # Built-in/frozen modules retain priority; native extensions still use PathFinder.
    index = next((i for i, finder in enumerate(sys.meta_path)
                  if finder is importlib.machinery.PathFinder), len(sys.meta_path))
    sys.meta_path.insert(index, PackedFinder(ARCHIVE))
