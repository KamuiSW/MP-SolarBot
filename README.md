# Solar Panel Cleaning Robot

you need to do this to install any packages btw 
(starting venv ".\.venv\Scripts\Activate.ps1")

A Raspberry Pi 5 powered robot designed to autonomously clean solar panels using a combination of mapping, sensors, and machine learning.  
It efficiently maps panel edges, plans an optimal cleaning path, detects uncleaned spots with a camera, and revisits them, all while minimising energy use.

---

## Concept Overview

The robot operates directly on solar panels.  
Its main goals are:
- Clean each panel surface fully and safely (no falling off edges).  
- Detect and re-clean dirty or stained areas using a rear-facing camera.  
- Conserve as much power as possible for long, unattended operation.

---

## System Workflow

1. **Edge Mapping Phase**
   - The robot activates ultrasonic sensors to detect the edges of the solar panel.
   - While driving in a rectangular pattern, it records how long the wheels turn between edges.
   - From this data, it builds a simple 2D map of the panel area.

2. **Path Planning**
   - Using the mapped boundaries, the robot calculates a full cleaning path that covers the panel.
   - The path data (distances and turning points) is stored locally.

3. **Cleaning & Camera Detection**
   - During cleaning, the robot follows the saved path using wheel encoder (or similar sensors) feedback.
   - A rear-mounted camera runs a machine learning model to detect uncleaned or stained areas.
   - When a dirty area is detected, the coordinates are logged for re-cleaning later.

4. **Re-Cleaning Pass**
   - After finishing the planned path, the robot revisits the recorded dirty spots.
   - Once all areas are confirmed clean, the robot shuts down to conserve energy.

---

## Hardware Components

- **Raspberry Pi 5** — main controller, handles mapping, motion, and ML image detection.  
- **Ultrasonic sensors** — detect panel edges and prevent falls.  
- **Wheel encoders / IMU** — estimate movement and orientation.  
- **Camera module** — captures rear images for stain detection.  
- **Motors + Driver** — move and steer the robot.  
- **Battery Pack** — powers the entire system (single shared source).

---

## Energy Optimisation

Even though the Pi 5 and camera must stay on for ML tasks, energy efficiency is achieved through:
- **Duty-cycled sensors:** Ultrasonic sensors only activate when approaching corners or uncertain areas.  
- **Lower camera FPS & resolution:** 640×480 @ 5 FPS is enough for stain detection.  
- **ML models:** use efficient networks.  
- **System optimizations:** disable HDMI, Bluetooth, LEDs; use `powersave` CPU governor.  
- **software scheduling:** run ML inference every 1–2 seconds instead of every frame.

---

## Future Improvements

- Implement automatic recharging via a docking station.  
- Use optical flow or visual odometry for more precise movement tracking.  
- Improve ML model accuracy for dirt classification.  
- Add adaptive cleaning intensity based on dirt severity.

---

### Author
Developed by [Team 2 KKC]  
