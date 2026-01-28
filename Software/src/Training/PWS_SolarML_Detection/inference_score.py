# inference_score.py
import numpy as np
import json
from tensorflow.keras.preprocessing.image import load_img, img_to_array
from skimage.util import view_as_windows
import cv2
from model_siamese import build_encoder
from panel_mask import get_panel_mask
import os
import sys

BASE_DIR = os.path.dirname(os.path.abspath(__file__))

MODEL_DIR = os.path.join(BASE_DIR, "models")

ENCODER_PATH = os.path.join(MODEL_DIR, "encoder.h5")
CLEAN_REF_NPY = os.path.join(MODEL_DIR, "clean_reference.npy")
CALIB_JSON = os.path.join(MODEL_DIR, "calibration.json")
IMG_SIZE = (224,224)

print("Rebuilding encoder...")
encoder = build_encoder()
encoder.load_weights(ENCODER_PATH)
print("Encoder loaded.")

clean_ref = np.load(CLEAN_REF_NPY)
with open(CALIB_JSON, "r") as f:
    calib = json.load(f)
MIN_D = calib["min_dist"]
MAX_D = calib["max_dist"]

def preprocess(path_or_array):
    if isinstance(path_or_array, str):
        img = load_img(path_or_array, target_size=IMG_SIZE)
        arr = img_to_array(img) / 255.0
        return np.expand_dims(arr, axis=0)
    else:
        # assume already array (H,W,3) scaled 0-1
        arr = cv2.resize(path_or_array, IMG_SIZE)
        return np.expand_dims(arr.astype("float32") / 255.0, axis=0)

def embedding_from_array(arr):
    return encoder.predict(arr, verbose=0)[0]
#Verbose is set to 0, to remove all the progress bars for individual images

def normalize_distance(d):
    # Map d relative to calibration. MIN_D -> 0, MAX_D -> 100
    # allow >100 if d > MAX_D
    if d <= MIN_D:
        return 0.0
    span = MAX_D - MIN_D
    if span <= 0:
        return float(d / (MAX_D + 1e-6) * 100.0)
    score = (d - MIN_D) / span * 100.0
    return float(score)

def compute_distance_to_clean(emb):
    return float(np.linalg.norm(emb - clean_ref))

def get_dirt_score(image_path):
    arr = preprocess(image_path)
    emb = embedding_from_array(arr)
    d = compute_distance_to_clean(emb)
    score = normalize_distance(d)
    return score

# Tiling helpers: split an image into tiles with overlap if needed
def get_tile_scores(image_path, tile_px=224, stride_px=None, show_heatmap=False, out_heatmap=None):
    img = cv2.imread(image_path)
    if img is None:
        raise ValueError("Could not read image: " + image_path)

    h, w, _ = img.shape

    if stride_px is None:
        stride_px = tile_px

    # NEW: compute panel mask once
    panel_mask = get_panel_mask(img)

    tiles = []
    positions = []

    for y in range(0, h - tile_px + 1, stride_px):
        for x in range(0, w - tile_px + 1, stride_px):

            tile_panel_mask = panel_mask[y:y+tile_px, x:x+tile_px]
            panel_ratio = np.sum(tile_panel_mask > 0) / (tile_px * tile_px)

            # CHANGE: ignore tiles not mostly panel
            # Tiles are only evaluated if ≥ 60% of pixels belong to the panel
            if panel_ratio < 0.6:
                continue

            patch = img[y:y+tile_px, x:x+tile_px]
            tiles.append(patch)
            positions.append((x, y))

    scores = []

    for t in tiles:
        arr = cv2.resize(t, IMG_SIZE)
        arr = arr.astype("float32") / 255.0
        emb = embedding_from_array(np.expand_dims(arr, axis=0))
        d = compute_distance_to_clean(emb)
        score = normalize_distance(d)
        scores.append(score)

    if show_heatmap:
        heatmap = np.zeros((h, w), dtype=np.float32)
        counts = np.zeros((h, w), dtype=np.int32)

        for (x, y), s in zip(positions, scores):
            heatmap[y:y+tile_px, x:x+tile_px] += s
            counts[y:y+tile_px, x:x+tile_px] += 1

        counts[counts == 0] = 1
        heatmap = heatmap / counts

        vis = np.clip(heatmap, 0, 100) / 100.0 * 255.0
        vis = vis.astype("uint8")
        vis_color = cv2.applyColorMap(vis, cv2.COLORMAP_JET)
        overlay = cv2.addWeighted(img, 0.6, vis_color, 0.4, 0)

        if out_heatmap:
            cv2.imwrite(out_heatmap, overlay)

        return positions, scores, overlay

    return positions, scores, None

def classify_dirt(score):
    if score < 10:
        return "CLEAN"
    elif score < 40:
        return "LIGHT"
    elif score < 70:
        return "MODERATE"
    else:
        return "HEAVY"

