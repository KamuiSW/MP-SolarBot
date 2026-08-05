# Solar Panel Cleaning Robot

This is the code and hardware files for our 6VWO profielwerkstuk. The project is about a small robot that can map a solar panel, plan a path over it, and detect dirty spots.

## Parts of the project

### 1. Firmware
Language: C++ <br>
Hardware: Raspberry Pi <br><br>
What it does:<br>
Motor control <br>
Edge detection with sensors<br>
Mapping the panel<br>
Navigation over the mapped area<br><br>
Compilation:<br>
    ```g++ main.cpp mapping.cpp navigation.cpp executor.cpp stains.cpp stain_planner.cpp -o robot_core -lwiringPi -std=c++17
    ```

### 2. Web interface and simulation
Backend: Python<br>
Frontend: HTML5<br><br>
The web interface shows the robot position, the path, and detected stains. There is also a Python simulation so we can test the logic without always using the real robot.<br>

## Simulation

1.  Install dependencies:
    ```bash
    pip install fastapi uvicorn websockets
    ```

2.  Run the server:
    ```bash
    python Software\src\Simulation\server.py
    ```

3.  Open the UI:
    Open `http://localhost:8000` in your web browser.

4.  Start the simulation:
Click "Start Simulation".<br>

