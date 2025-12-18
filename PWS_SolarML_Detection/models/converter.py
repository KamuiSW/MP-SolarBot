import os
import sys
import tensorflow as tf

BASE_DIR = os.path.dirname(__file__)  # .../models
PROJECT_ROOT = os.path.abspath(os.path.join(BASE_DIR, ".."))
sys.path.insert(0, PROJECT_ROOT)

from encoder_only import build_encoder

MODEL_H5 = os.path.join(BASE_DIR, "encoder.h5")
OUT_TFLITE = os.path.join(BASE_DIR, "encoder.tflite")

if not os.path.exists(MODEL_H5):
    raise FileNotFoundError(f"Missing file: {MODEL_H5}")

print("[Converter] Building encoder architecture...")
encoder = build_encoder()

print("[Converter] Loading weights from encoder.h5...")
encoder.load_weights(MODEL_H5)

print("[Converter] Converting to TFLite (float32)...")
converter = tf.lite.TFLiteConverter.from_keras_model(encoder)
tflite_model = converter.convert()

with open(OUT_TFLITE, "wb") as f:
    f.write(tflite_model)

print("[Converter] Saved:", OUT_TFLITE)
