# model_siamese.py
import os
import random
import numpy as np
from glob import glob
from pathlib import Path
from tensorflow.keras.applications import MobileNetV2
from tensorflow.keras import layers, models, backend as K
from tensorflow.keras.preprocessing.image import load_img, img_to_array
from tensorflow.keras.optimizers import Adam
import tensorflow as tf

# ---- USER CONFIG ----
DATASET_ROOT = "/Users/jkaur/documents/Github/PWS-SolarML/MLReferences" #Need to change file path
CLEAN_DIR = os.path.join(DATASET_ROOT, "Clean")
DIRTY_DIR = os.path.join(DATASET_ROOT, "Dirty")
IMG_SIZE = (224, 224)
BATCH_SIZE = 16
EPOCHS = 12
MARGIN = 1.0
SAVE_DIR = "./models"
os.makedirs(SAVE_DIR, exist_ok=True)
ENCODER_SAVE = os.path.join(SAVE_DIR, "encoder.h5")
SIAMESE_SAVE = os.path.join(SAVE_DIR, "siamese.h5")
# ----------------------

def list_images(folder):
    exts = ("*.jpg", "*.jpeg", "*.JPG", "*.png")
    paths = []
    for ext in exts:
        paths.extend(glob(os.path.join(folder, ext)))
    return sorted(paths)

clean_imgs = list_images(CLEAN_DIR)
dirty_imgs = []
# dirty_dir can contain subfolders like Bird-drop etc
for root, dirs, files in os.walk(DIRTY_DIR):
    for ext in ("jpg","jpeg","JPG","png"):
        dirty_imgs.extend(glob(os.path.join(root, f"*.{ext}")))
dirty_imgs = sorted(dirty_imgs)

assert len(clean_imgs) > 10, "Need more clean images"
assert len(dirty_imgs) > 10, "Need more dirty images"

def load_and_preprocess(path):
    img = load_img(path, target_size=IMG_SIZE)
    arr = img_to_array(img)
    arr = arr / 255.0
    return arr

# Build encoder model (backbone)
def build_encoder(input_shape=IMG_SIZE + (3,)):
    base = MobileNetV2(weights="imagenet", include_top=False, input_shape=input_shape)
    x = base.output
    x = layers.GlobalAveragePooling2D()(x)
    x = layers.Dense(256, activation="relu")(x)
    x = layers.Lambda(lambda v: K.l2_normalize(v, axis=1))(x)  # normalize embedding
    encoder = models.Model(inputs=base.input, outputs=x, name="encoder")
    return encoder

def euclidean_distance(vects):
    x, y = vects
    return K.sqrt(K.maximum(K.sum(K.square(x - y), axis=1, keepdims=True), K.epsilon()))

def contrastive_loss(y_true, y_pred):
    # y_true: 0 = similar (clean/clean), 1 = dissimilar (clean/dirty)
    y_true = K.cast(y_true, y_pred.dtype)
    square_pred = K.square(y_pred)
    margin_square = K.square(K.maximum(MARGIN - y_pred, 0.0))
    return K.mean(y_true * square_pred + (1 - y_true) * margin_square)

def build_siamese(encoder):
    input_a = layers.Input(shape=encoder.input_shape[1:], name="input_a")
    input_b = layers.Input(shape=encoder.input_shape[1:], name="input_b")
    emb_a = encoder(input_a)
    emb_b = encoder(input_b)
    distance = layers.Lambda(lambda tensors: K.sqrt(K.maximum(K.sum(K.square(tensors[0] - tensors[1]), axis=1, keepdims=True), K.epsilon())),
                             name="euclidean_distance")([emb_a, emb_b])
    model = models.Model([input_a, input_b], distance)
    return model

def pair_generator(clean_list, dirty_list, batch_size=BATCH_SIZE):
    while True:
        xa = []
        xb = []
        ys = []
        for _ in range(batch_size):
            if random.random() < 0.5:
                a, b = random.sample(clean_list, 2)
                label = 0.0
            else:
                a = random.choice(clean_list)
                b = random.choice(dirty_list)
                label = 1.0

            xa.append(load_and_preprocess(a))
            xb.append(load_and_preprocess(b))
            ys.append([label])

        yield (np.array(xa), np.array(xb)), np.array(ys)

# ---- training ----
def train():
    encoder = build_encoder()
    siamese = build_siamese(encoder)
    siamese.compile(optimizer=Adam(1e-4), loss=contrastive_loss)
    steps_per_epoch = max(100, (len(clean_imgs) + len(dirty_imgs)) // BATCH_SIZE)
    gen = pair_generator(clean_imgs, dirty_imgs, batch_size=BATCH_SIZE)
    print("Training siamese network. Steps per epoch:", steps_per_epoch)
    history = siamese.fit(gen, steps_per_epoch=steps_per_epoch, epochs=EPOCHS, verbose=2)
    print("Saving encoder and siamese model...")
    encoder.save(ENCODER_SAVE)
    siamese.save(SIAMESE_SAVE)
    print("Done.")

if __name__ == "__main__":
    train()
