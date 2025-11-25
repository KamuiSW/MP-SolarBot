# calibrate_reference.py
import os
import json
import numpy as np
from glob import glob
from tensorflow.keras.preprocessing.image import load_img, img_to_array
from model_siamese import build_encoder   # import your encoder builder

# --- CONFIG ---
ENCODER_PATH = "./models/encoder.h5"
DATASET_ROOT = "/Users/jkaur/Documents/GitHub/PWS-SolarML/MLReferences"
CLEAN_DIR = os.path.join(DATASET_ROOT, "Clean")
DIRTY_DIR = os.path.join(DATASET_ROOT, "Dirty")
IMG_SIZE = (224,224)
OUT_DIR = "./models"
os.makedirs(OUT_DIR, exist_ok=True)

CLEAN_REF_NPY = os.path.join(OUT_DIR, "clean_reference.npy")
CALIB_JSON = os.path.join(OUT_DIR, "calibration.json")

# --- HELPERS ---
def list_images(folder):
    exts = ("**/*.jpg", "**/*.jpeg", "**/*.JPG", "**/*.png")
    paths = []
    for ext in exts:
        paths.extend(glob(os.path.join(folder, ext), recursive=True))
    return sorted(paths)


def load_preprocess(path):
    img = load_img(path, target_size=IMG_SIZE)
    arr = img_to_array(img).astype("float32") / 255.0
    return arr

print("Building encoder model...")
encoder = build_encoder()                  # build model structure
encoder.load_weights(ENCODER_PATH)         # load trained weights
print("Encoder loaded.")

clean_paths = list_images(CLEAN_DIR)
dirty_paths = list_images(DIRTY_DIR)
print("Num clean:", len(clean_paths))
print("Num dirty:", len(dirty_paths))

# Load all clean images in one batch
clean_imgs = np.stack([load_preprocess(p) for p in clean_paths], axis=0)
clean_embs = encoder.predict(clean_imgs, batch_size=16, verbose=0)

# compute reference vector
clean_ref = np.mean(clean_embs, axis=0)
np.save(CLEAN_REF_NPY, clean_ref)
print("Saved clean reference to", CLEAN_REF_NPY)

# now load dirty in batches
dirty_imgs = np.stack([load_preprocess(p) for p in dirty_paths], axis=0)
dirty_embs = encoder.predict(dirty_imgs, batch_size=16, verbose=0)

# compute distances
dists = np.linalg.norm(dirty_embs - clean_ref, axis=1)
min_d = float(np.min(dists))
max_d = float(np.percentile(dists, 95))
mean_d = float(np.mean(dists))

calib = {"min_dist": min_d, "max_dist": max_d, "mean_dist": mean_d}
with open(CALIB_JSON, "w") as f:
    json.dump(calib, f, indent=2)

print("Calibration complete")
print(calib)
