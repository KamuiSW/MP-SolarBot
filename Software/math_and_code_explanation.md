# Robot Simulation Explanation

This document explains the main ideas I used in the simulation. I tried to connect the code to the maths, because otherwise it is easy to lose track of why the robot moves in a certain way.

## 1. Robot Movement (Kinematics)

The real robot uses two wheels, so it behaves a bit like a tank. In the simulation I made this simpler: the robot has a position, an angle, and it moves a small step in the direction it is facing.

### Math idea
In a localized 2D plane:
- **Position**: $ (x, y) $ coordinates in cm.
- **Heading**: $ \theta $ (angle) in radians.
- **Velocity**: $ v $ (linear speed) in cm/s.

When the robot moves forward with velocity $ v $ for a time step $ \Delta t $:
$$ x_{new} = x_{old} + v \cdot \cos(\theta) \cdot \Delta t $$
$$ y_{new} = y_{old} + v \cdot \sin(\theta) \cdot \Delta t $$

### Code part (`simulation.py`)
In `move_to`, the variable `mv` is the distance the robot moves in one small update.

```python
# movement in the x and y direction
self.x += mv * math.sin(self.angle) # sin/cos are swapped because 0 degrees is up in this sim
self.y += mv * math.cos(self.angle) 
```
In normal maths, 0 degrees usually points to the right. In my canvas/simulation, 0 degrees means up, so that is why `sin` and `cos` look swapped here.

## 2. Path Planning (Spiral)

After mapping the border, the robot needs to cover the inside of the panel. I used an inward spiral for this because it is not too hard to program and it covers the rectangle quite well.

### Logic
1. Define the boundary box ($min\_x, max\_x, min\_y, max\_y$).
2. Move along the perimeter.
3. Shrink the boundary inward by the robot's width (minus overlap).
4. Repeat until the box is too small.

### Code part (`generate_spiral_path`)
The code uses `direction` to go up, right, down, and left. After each side it moves the border inwards.

```python
step = max(5.0, self.robot_size * (1.0 - self.overlap_percent))
# ...
if direction == 0: # Up
    # Add points from bottom to top
    # ...
    l += step # move left border inwards
```

## 3. Stain Cleaning (Greedy Nearest Neighbor)

When the robot finds stains, it has to visit them. I used a simple nearest-neighbor method: from the current position, go to the closest stain first, then repeat. This is not always the shortest possible route, but it is easy to understand and works well enough for our test.

### Math idea
Given a set of stain locations $ S = \{s_1, s_2, ... s_n\} $ and current robot position $ P $:
1. Calculate distance to all remaining stains: $ d_i = \sqrt{(x_{s_i} - x_p)^2 + (y_{s_i} - y_p)^2} $
2. Select $ s_k $ where $ d_k $ is minimized.
3. Move to $ s_k $, remove it from $ S $, and set $ P = s_k $.
4. Repeat until $ S $ is empty.

### Code part (`run_cleaning_phase`)
```python
while remaining:
    # Find stain with minimum Euclidean distance
    nearest = min(remaining, key=lambda s: math.hypot(s['x']-curr['x'], s['y']-curr['y']))
    clean_path.append(nearest)
    # ...
```
`math.hypot(dx, dy)` calculates this distance in Python.

## 4. Homing (Return to Base)

After cleaning, the robot goes back to its starting position. I also added a small angle correction at the end, because otherwise it could arrive at the right position but face the wrong direction.

### Logic
1. Navigate to $(x_0, y_0)$.
2. Rotate in place until $\theta \approx 0$.

### Code part
```python
target_angle = 0.0
while abs(self.angle - target_angle) > 0.05:
    # turn a little bit until the angle is close enough
    if self.angle > target_angle: self.angle -= 0.1
    else: self.angle += 0.1
```
This is a very simple correction, not a perfect controller. It just turns the robot step by step until the angle is close to 0.
