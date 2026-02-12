# run_demo.py
import os
from inference_score import get_dirt_score, get_tile_scores, classify_dirt
os.environ['TF_CPP_MIN_LOG_LEVEL'] = '3'  # suppresses most TF logs
import os
BASE_DIR = os.path.dirname(os.path.abspath(__file__))

import tensorflow as tf
tf.get_logger().setLevel('ERROR')  # suppress TF warnings

# simple single image score
MLREF_DIR = os.path.abspath(
    os.path.join(BASE_DIR, "MLReferences")
)
# img = os.path.join(MLREF_DIR, "Dirty", "Snow-Covered", "Snow (40).jpg") # frame path
# img = os.path.join(MLREF_DIR, "Dirty", "Dusty", "Dust (189).jpg") # frame path
img = os.path.join(MLREF_DIR, "Clean", "Clean (6).jpg") # frame path
score = get_dirt_score(img)
print("Overall dirt score for image:", score)
print("Category:", classify_dirt(score))
print("Estimated dirt coverage:", f"{min(score,100):.1f}%")

# tile-based heatmap
positions, scores, overlay = get_tile_scores(img, tile_px=224, stride_px=112, show_heatmap=True, out_heatmap="./demo_heatmap.jpg")
print("Tiles:", len(scores))
print("Sample tile scores (first 10):", scores[:10])
print("Heatmap saved to demo_heatmap.jpg")


# I tested a couple of images, clean vs dirty and set the following thresholds:
# score < 10               CLEAN
# 10 <= score < 40         LIGHT DIRT
# 40 <= score < 70         MODERATE
# 70 <= score              HEAVY
