# Solar Panel Cleaning Robot System

This repository contains the software stack for our 6VWO graduation masterproof. The system is divided into a C++ Firmware (for the physical robot hardware) and a web interface (for simulation).

## System Architecture

### 1. Firmware
Language: C++ <br>
Hardware: Raspberry Pi <br><br>
Functions:<br>
Motor Control <br>
Sensor edge detection<br>
Mapping algorithm<br>
Navigation<br><br>
Compilation:<br>
    ```g++ main.cpp mapping.cpp navigation.cpp executor.cpp stains.cpp stain_planner.cpp -o robot_core -lwiringPi -std=c++17
    ```

### 2. Web interface and simulation
Backend: Python<br>
Frontend: HTML5<br><br>
Features:<br>
robot status and control.<br>
Simulation mode: A python simulation of the robot's kinematics and sensor logic.<br>

## Simulation

1.  Install dependencies:
    ```bash
    pip install fastapi uvicorn websockets
    ```

2.  Run the server:
    ```bash
    python Software\src\Simulation\server.py
    ```

3.  Access the UI:
    Open `http://localhost:8000` in your web browser.

4.  Simulate:
Toggle "Sim Mode" to ON.<br>
Click "Start mapping".<br>

