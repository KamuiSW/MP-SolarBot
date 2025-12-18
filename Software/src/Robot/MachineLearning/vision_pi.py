import json
import time
import cv2
import numpy as np
import tensorflow.lite as tflite

CAM_INDEX = 0
IMG_SIZE = (224, 224)

ENCODER_TFLITE = "encoder.tflite"
CLEAN_REF_NPY = "clean_reference.npy"
CALIB_JSON = "calibration.json"

def load_calib(path):
    with open(path, "r") as f:
        c = json.load(f)
    return float(c["min_dist"]), float(c["max_dist"])

def normalize_distance(d, min_d, max_d):
    if d <= min_d:
        return 0.0
    span = max_d - min_d
    if span <= 1e-9:
        return float(d / (max_d + 1e-6) * 100.0)
    return float((d - min_d) / span * 100.0)

def preprocess_bgr(frame):
    img = cv2.resize(frame, IMG_SIZE)
    img = img.astype(np.float32) / 255.0
    img = np.expand_dims(img, axis=0)
    return img

def main():
    clean_ref = np.load(CLEAN_REF_NPY).astype(np.float32)
    min_d, max_d = load_calib(CALIB_JSON)

    interpreter = tflite.Interpreter(model_path=ENCODER_TFLITE)
    interpreter.allocate_tensors()
    inp = interpreter.get_input_details()[0]
    out = interpreter.get_output_details()[0]

    cap = cv2.VideoCapture(CAM_INDEX)
    if not cap.isOpened():
        print(json.dumps({"ok": False, "error": "camera_open_failed"}))
        return

    ret, frame = cap.read()
    cap.release()

    if not ret or frame is None:
        print(json.dumps({"ok": False, "error": "frame_read_failed"}))
        return

    x = preprocess_bgr(frame)
    interpreter.set_tensor(inp["index"], x)
    interpreter.invoke()
    emb = interpreter.get_tensor(out["index"])[0].astype(np.float32)

    d = float(np.linalg.norm(emb - clean_ref))
    score = normalize_distance(d, min_d, max_d)

    dirty = score >= 40.0
    level = "CLEAN" if score < 10 else ("LIGHT" if score < 40 else ("MODERATE" if score < 70 else "HEAVY"))

    print(json.dumps({
        "ok": True,
        "dirty": bool(dirty),
        "score": float(score),
        "level": level,
        "dist": d,
        "ts": time.time()
    }))

if __name__ == "__main__":
    main()
