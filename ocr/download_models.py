"""Download official PP-OCRv5 models once; recognition itself uses local files."""
from pathlib import Path
import tarfile
import tempfile
import urllib.request

root = Path(__file__).resolve().parent / "models"
root.mkdir(exist_ok=True)
base = "https://paddle-model-ecology.bj.bcebos.com/paddlex/official_inference_model/paddle3.0.0/"
for name in ("PP-OCRv5_mobile_det", "PP-OCRv5_mobile_rec"):
    target = root / name
    required = ("inference.json", "inference.pdiparams", "inference.yml")
    if all((target / item).is_file() for item in required):
        continue
    with tempfile.TemporaryDirectory(dir=root) as temporary:
        archive = Path(temporary) / "model.tar"
        print(f"Downloading {name}", flush=True)
        urllib.request.urlretrieve(base + name + "_infer.tar", archive)
        with tarfile.open(archive) as package:
            package.extractall(Path(temporary) / "unpacked", filter="data")
        candidates = list((Path(temporary) / "unpacked").rglob("inference.yml"))
        if len(candidates) != 1:
            raise RuntimeError("Unexpected model archive structure")
        source = candidates[0].parent
        target.mkdir(exist_ok=True)
        for item in required:
            (target / item).write_bytes((source / item).read_bytes())
print("Local OCR models ready.")
