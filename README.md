# Solar Panel Cleaning Robot System

This repository contains the software stack for our 6VWO graduation masterproof. The system is divided into a C++ Firmware Core (for the physical robot hardware) and a Python/Web Interface (for control, simulation, and visualization).

## System Architecture

### 1. Firmware Core (`Software/src/Robot/Core`)
*   **Language**: C++
*   **Hardware**: Raspberry Pi (wiringPi)
*   **Functionality**:
    *   Motor Control (Stepper Drivers)
    *   Sensor Fusion (Ultrasonic / Cliff Detection)
    *   Mapping Algorithm (Edge detection, Perimeter tracing)
    *   Navigation (Grid-based coverage path planning)
*   **Compilation**:
    ```bash
    g++ main.cpp mapping.cpp navigation.cpp executor.cpp stains.cpp stain_planner.cpp -o robot_core -lwiringPi -std=c++17
    ```

### 2. Web Interface & Simulation (`Software/src/Robot/Web`)
*   **Backend**: Python (FastAPI, WebSockets)
*   **Frontend**: HTML5, Vanilla JS, Canvas API
*   **Features**:
    *   Real-time robot status and control.
    *   **Simulation Mode**: A full pure-Python simulation of the robot's kinematics and sensor logic.
    *   Interactive Map: Pan, Zoom, and Live Grid.
    *   Virtual Stain Spawning: Drag-and-drop simulated dirt for testing detection logic.

## Getting Started (Simulation)

1.  **Install Dependencies**:
    ```bash
    pip install fastapi uvicorn websockets
    ```

2.  **Run the Server**:
    ```bash
    python Software/src/Simulation/server.py
    ```

3.  **Access the UI**:
    Open `http://localhost:8000` in your web browser.

4.  **Simulate**:
    *   Toggle **"Sim Mode"** to ON.
    *   Click **"Start Mapping (Sim)"**.
    *   Use the **"Spawn Dirt"** button to interact with the environment.

## Deployment (Real Hardware)

1.  Compile the C++ core on the Raspberry Pi using the command above.
2.  Run the Python server. It will automatically detect and manage the `robot_core` binary for physical operations if not in Sim Mode.
