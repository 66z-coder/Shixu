"""Portable packed-code semantics, independent of the OCR models."""
import importlib.util
import inspect
import marshal
from pathlib import Path
import pkgutil
import sys
import tempfile
import unittest
import zipfile

ROOT = Path(__file__).resolve().parents[2]


class RuntimeLoaderTest(unittest.TestCase):
    def test_sources_resources_namespaces_and_relocation(self):
        spec = importlib.util.spec_from_file_location('runtime_loader_test', ROOT/'ocr/runtime_loader.py')
        loader_module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(loader_module)
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            loader_module.ROOT = root
            loader_module.ARCHIVE = root/'packages.zip'
            sources = {
                'packed_fixture/__init__.py': 'from .mod import read_value\n',
                'packed_fixture/mod.py': 'from importlib.resources import files\ndef read_value():\n    return files("packed_fixture").joinpath("value.txt").read_text()\n',
                'packed_namespace/child.py': 'value = 42\n',
            }
            with zipfile.ZipFile(loader_module.ARCHIVE, 'w') as archive:
                for name, text in sources.items():
                    (root/name).parent.mkdir(parents=True, exist_ok=True)
                    archive.writestr(name, text)
                    code = compile(text, 'old/build/location/'+name, 'exec')
                    if name != 'packed_namespace/child.py':
                        archive.writestr(name+'c', importlib.util.MAGIC_NUMBER + b'\0'*12 + marshal.dumps(code))
            (root/'packed_fixture/value.txt').write_text('asset content')
            finder = loader_module.PackedFinder(loader_module.ARCHIVE)
            sys.meta_path.insert(0, finder)
            sys.path.insert(0, str(root))
            try:
                import packed_fixture
                from packed_namespace import child
                self.assertEqual(packed_fixture.read_value(), 'asset content')
                self.assertEqual(child.value, 42)
                self.assertIn('def read_value', inspect.getsource(packed_fixture.read_value))
                self.assertEqual(packed_fixture.read_value.__code__.co_filename, str(root/'packed_fixture/mod.py'))
                self.assertIn('mod', {item.name for item in pkgutil.iter_modules(packed_fixture.__path__)})
                self.assertFalse((root/'packed_fixture/mod.py').exists())
            finally:
                sys.meta_path.remove(finder)
                sys.path.remove(str(root))
                for name in list(sys.modules):
                    if name.startswith(('packed_fixture', 'packed_namespace')):
                        del sys.modules[name]
                finder.archive.close()


if __name__ == '__main__':
    unittest.main()
