import cv2
import numpy as np

def get_panel_mask(image_bgr):
    """
    Returns a binary mask where solar panel = 255, background = 0
    """

    hsv = cv2.cvtColor(image_bgr, cv2.COLOR_BGR2HSV)

    # These may need to be adjusted
    lower = np.array([90, 20, 20])   # dark blue
    upper = np.array([140, 255, 180])

    mask = cv2.inRange(hsv, lower, upper)

    # Clean noise
    kernel = np.ones((7,7), np.uint8)
    mask = cv2.morphologyEx(mask, cv2.MORPH_CLOSE, kernel)
    mask = cv2.morphologyEx(mask, cv2.MORPH_OPEN, kernel)

    # Keep largest contour only
    contours, _ = cv2.findContours(mask, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
    if not contours:
        return np.zeros_like(mask)

    largest = max(contours, key=cv2.contourArea)
    panel_mask = np.zeros_like(mask)
    cv2.drawContours(panel_mask, [largest], -1, 255, thickness=cv2.FILLED)

    return panel_mask
