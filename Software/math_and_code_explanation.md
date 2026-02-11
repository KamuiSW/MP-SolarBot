# Robot Simulation: Code & Math Explanation

This document explains the core logic behind the **SolarBot Simulation**, bridging the gap between the Python code and the physics/mathematics it represents.

## 1. Robot Movement (Kinematics)

The robot is simulated as a **Differential Drive** system (like a tank or a Roomba), but for simplicity in this simulation, we approximate it with direct velocity vectors.

### The Math
In a localized 2D plane:
- **Position**: $ (x, y) $ coordinates in cm.
- **Heading**: $ \theta $ (angle) in radians.
- **Velocity**: $ v $ (linear speed) in cm/s.

When the robot moves forward with velocity $ v $ for a time step $ \Delta t $:
$$ x_{new} = x_{old} + v \cdot \cos(\theta) \cdot \Delta t $$
$$ y_{new} = y_{old} + v \cdot \sin(\theta) \cdot \Delta t $$

### The Code (`simulation.py`)
In the `move_to` function, we calculate the movement step size `mv` (which is $ v \cdot \Delta t $).

```python
# projection of movement along X and Y axes
self.x += mv * math.sin(self.angle) # Note: In this sim, sin/cos are swapped due to 0-angle definition
self.y += mv * math.cos(self.angle) 
```
> **Note**: In standard math, $0^\circ$ is usually East ($+X$). In this simulation's coordinate system, $0^\circ$ is defined as "Up" ($+Y$), so `sin` and `cos` roles are swapped relative to standard textbooks.

## 2. Path Planning (Spiral)

To cover a rectangular area efficiently, the robot uses an **Inward Spiral** pattern.

### The Logic
1. Define the boundary box ($min\_x, max\_x, min\_y, max\_y$).
2. Move along the perimeter.
3. Shrink the boundary inward by the robot's width (minus overlap).
4. Repeat until the box is too small.

### The Code (`generate_spiral_path`)
The code uses a state machine (`direction`) to cycle through Up, Right, Down, Left.

```python
step = max(5.0, self.robot_size * (1.0 - self.overlap_percent))
# ...
if direction == 0: # Up
    # Add points from Bottom to Top
    # ...
    l += step # Shrink Left boundary
```

## 3. Stain Cleaning (Greedy Nearest Neighbor)

When the robot switches to **Cleaning Mode**, it needs to visit all detected stains. We use a **Greedy Nearest Neighbor** algorithm. This is a heuristic for the *Traveling Salesperson Problem (TSP)*.

### The Math
Given a set of stain locations $ S = \{s_1, s_2, ... s_n\} $ and current robot position $ P $:
1. Calculate distance to all remaining stains: $ d_i = \sqrt{(x_{s_i} - x_p)^2 + (y_{s_i} - y_p)^2} $
2. Select $ s_k $ where $ d_k $ is minimized.
3. Move to $ s_k $, remove it from $ S $, and set $ P = s_k $.
4. Repeat until $ S $ is empty.

### The Code (`run_cleaning_phase`)
```python
while remaining:
    # Find stain with minimum Euclidean distance
    nearest = min(remaining, key=lambda s: math.hypot(s['x']-curr['x'], s['y']-curr['y']))
    clean_path.append(nearest)
    # ...
```
`math.hypot(dx, dy)` is Python's efficient way to calculate $ \sqrt{dx^2 + dy^2} $.

## 4. Homing (Return to Base)

After cleaning, the robot returns to $(x_0, y_0)$. To ensure it docks correctly, we added a final orientation correction step.

### The Logic
1. Navigate to $(x_0, y_0)$.
2. Rotate in place until $\theta \approx 0$.

### The Code
```python
target_angle = 0.0
while abs(self.angle - target_angle) > 0.05:
    # Incrementally adjust angle
    if self.angle > target_angle: self.angle -= 0.1
    else: self.angle += 0.1
```
This simple "P-controller" (Proportional control with fixed step) ensures the robot aligns perfectly with the station.
