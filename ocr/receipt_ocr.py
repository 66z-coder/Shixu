"""Offline image-to-text adapter, supporting single-shot and reusable worker modes."""
import argparse
import contextlib
import json
import os
from pathlib import Path
import sys

DONE_PREFIX = "SHIXU_OCR_DONE "


class OcrEngine:
    def __init__(self, models):
        self.models = Path(models)
        self.engine = None

    def recognize(self, image):
        result = {"success": False, "lines": [], "errorMessage": ""}
        try:
            os.environ["PADDLE_PDX_DISABLE_MODEL_SOURCE_CHECK"] = "True"
            os.environ["PADDLE_PDX_EAGER_INIT"] = "False"
            os.environ["HF_HUB_OFFLINE"] = "1"
            det = self.models / "PP-OCRv5_mobile_det"
            rec = self.models / "PP-OCRv5_mobile_rec"
            for folder in (det, rec):
                for name in ("inference.json", "inference.pdiparams", "inference.yml"):
                    if not (folder / name).is_file():
                        raise RuntimeError("本地识别模型不完整，请重新安装离线 OCR 扩展")
            from PIL import Image, ImageOps
            import numpy as np
            with Image.open(image) as source:
                rgb = np.asarray(ImageOps.exif_transpose(source).convert("RGB"))
            if self.engine is None:
                from paddleocr import PaddleOCR
                self.engine = PaddleOCR(
                    text_detection_model_name=det.name,
                    text_detection_model_dir=str(det),
                    text_recognition_model_name=rec.name,
                    text_recognition_model_dir=str(rec),
                    use_doc_orientation_classify=False,
                    use_doc_unwarping=False,
                    use_textline_orientation=False,
                    device="cpu", enable_mkldnn=False, cpu_threads=4,
                )
            for page in self.engine.predict(input=rgb[:, :, ::-1].copy()):
                for text, score, box in zip(page["rec_texts"], page["rec_scores"], page["rec_polys"]):
                    if text.strip():
                        result["lines"].append({"text": text, "confidence": float(score),
                                                "box": box.tolist()})
            result["success"] = True
        except Exception as error:
            result["errorMessage"] = str(error)
        return result


def save_result(output, result):
    output = Path(output)
    temporary = output.with_suffix(output.suffix + ".tmp")
    temporary.write_text(json.dumps(result, ensure_ascii=False), encoding="utf-8")
    os.replace(temporary, output)


def serve(engine):
    # Unique result files and completion identifiers separate results from library logs.
    for line in sys.stdin:
        try:
            request = json.loads(line)
            if not isinstance(request.get("id"), str):
                raise ValueError("Missing request identifier")
            with contextlib.redirect_stdout(sys.stderr):
                result = engine.recognize(request["image"])
            save_result(request["output"], result)
            print("\n" + DONE_PREFIX + request["id"], flush=True)
        except Exception as error:
            print(f"OCR worker protocol error: {error}", file=sys.stderr, flush=True)
            return 1  # Fail immediately if the response channel is broken.
    return 0


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--models", required=True)
    parser.add_argument("--worker", action="store_true")
    parser.add_argument("--image")
    parser.add_argument("--output")
    args = parser.parse_args()
    engine = OcrEngine(args.models)
    if args.worker:
        return serve(engine)
    if not args.image or not args.output:
        parser.error("--image and --output are required without --worker")
    result = engine.recognize(args.image)
    save_result(args.output, result)
    return 0 if result["success"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
