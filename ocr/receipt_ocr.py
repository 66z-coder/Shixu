"""Local image-to-text adapter. All food/business interpretation stays in C++."""
import argparse
import json
import os
from pathlib import Path


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--image", required=True)
    parser.add_argument("--models", required=True)
    parser.add_argument("--output", required=True)
    args = parser.parse_args()
    result = {"success": False, "lines": [], "errorMessage": ""}
    try:
        os.environ["PADDLE_PDX_DISABLE_MODEL_SOURCE_CHECK"] = "True"
        os.environ["HF_HUB_OFFLINE"] = "1"
        models = Path(args.models)
        det = models / "PP-OCRv5_mobile_det"
        rec = models / "PP-OCRv5_mobile_rec"
        for folder in (det, rec):
            for name in ("inference.json", "inference.pdiparams", "inference.yml"):
                if not (folder / name).is_file():
                    raise RuntimeError("本地识别模型不完整，请先运行 ocr/setup_ocr.ps1")
        from PIL import Image, ImageOps
        import numpy as np
        from paddleocr import PaddleOCR
        with Image.open(args.image) as source:
            rgb = np.asarray(ImageOps.exif_transpose(source).convert("RGB"))
        engine = PaddleOCR(
            text_detection_model_name=det.name,
            text_detection_model_dir=str(det),
            text_recognition_model_name=rec.name,
            text_recognition_model_dir=str(rec),
            use_doc_orientation_classify=False,
            use_doc_unwarping=False,
            use_textline_orientation=False,
            device="cpu", enable_mkldnn=False, cpu_threads=4,
        )
        for page in engine.predict(input=rgb[:, :, ::-1].copy()):
            for text, score, box in zip(page["rec_texts"], page["rec_scores"], page["rec_polys"]):
                if text.strip():
                    result["lines"].append({"text": text, "confidence": float(score),
                                            "box": box.tolist()})
        result["success"] = True
    except Exception as error:
        result["errorMessage"] = str(error)
    Path(args.output).write_text(json.dumps(result, ensure_ascii=False), encoding="utf-8")
    return 0 if result["success"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
