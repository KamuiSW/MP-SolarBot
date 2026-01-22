import math
import asyncio
import logging
import random

logger = logging.getLogger("RobotSim")

class RobotSimulation:
    def __init__(self):
        self.width = 200
        self.height = 150
        self.robot_size = 28
        self.step_size = 0.5
        self.overlap_percent = 0.5
        
        self.x = 16.0
        self.y = 16.0
        self.angle = 0.0
        
        self.stains = []
        self.detected_stains = [] 
        
        self.is_running = False
        self.mode = "IDLE"
        self.task = None
        
        self.map_points = []
        self.output_ops = []

    def set_config(self, w, h, size, overlap):
        self.width = w
        self.height = h
        self.robot_size = size
        self.overlap_percent = overlap
        self.reset()
        
    def reset(self):
        self.x = self.robot_size / 2.0 + 2.0
        self.y = self.robot_size / 2.0 + 2.0
        self.angle = 0.0
        self.mode = "IDLE"
        self.map_points = []
        self.detected_stains = []
        logger.info(f"Sim Reset to ({self.x:.1f}, {self.y:.1f})")

    def set_stains(self, stains_data):
        self.stains = stains_data

    async def run_mapping(self, send_json_cb):
        self.mode = "MAPPING"
        state = "FIND_INITIAL_EDGE"
        
        steps = 0
        corners_found = 0
        max_steps = 25000
        
        min_x, max_x, min_y, max_y = 9999, -9999, 9999, -9999
        
        while self.is_running and steps < max_steps:
            steps += 1
            await asyncio.sleep(0.01)
            
            margin = 5.0
            cliff_front = self.check_cliff(0, self.robot_size/2.0 + margin)
            cliff_left = self.check_cliff(-self.robot_size/2.0 - margin, 0)
            
            if state == "FIND_INITIAL_EDGE":
                if cliff_front:
                    self.turn_right()
                    state = "TRACE"
                else:
                    self.move_forward()
                    
            elif state == "TRACE":
                if not cliff_left:
                    self.turn_left()
                    self.move_forward()
                    if self.check_cliff(0, self.robot_size/2.0 + margin):
                         self.turn_right()
                elif cliff_front:
                    self.turn_right()
                    corners_found += 1
                    self.move_forward()
                else:
                    self.move_forward()
                
                if corners_found >= 4 and steps > 500:
                    break

            lx = -math.cos(self.angle)
            ly = math.sin(self.angle)
            
            edge_dist = self.robot_size / 2.0
            
            map_x = self.x + lx * edge_dist
            map_y = self.y + ly * edge_dist
            
            min_x = min(min_x, map_x)
            max_x = max(max_x, map_x)
            min_y = min(min_y, map_y)
            max_y = max(max_y, map_y)
            
            if steps % 15 == 0:
                self.map_points.append({"x": map_x, "y": map_y})
                await send_json_cb({"x": round(map_x), "y": round(map_y), "type": "map_point"})

            if steps % 5 == 0:
                await send_json_cb({
                    "type": "pose",
                    "x": self.x, "y": self.y,
                    "angle": self.angle,
                    "state": state
                })
        
        self.mode = "PLANNING"
        await send_json_cb({"type": "status", "status": "PLANNING", "msg": "Generating Spiral Path..."})
        
        path = self.generate_path(min_x, max_x, min_y, max_y)
        await send_json_cb({"type": "planned_path", "path": path})
        
        await asyncio.sleep(1.0)
        
        self.mode = "EXECUTION"
        await send_json_cb({"type": "status", "status": "EXECUTION", "msg": "Starting Coverage..."})
        
        for pt in path:
            if not self.is_running: break
            await self.move_to(pt['x'], pt['y'], send_json_cb)
            
        self.mode = "IDLE"
        await send_json_cb({"type": "status", "status": "IDLE", "msg": "Mission Complete"})

    def generate_path(self, min_x, max_x, min_y, max_y):
        safety_margin = self.robot_size / 2.0 + 2.0
        
        l = min_x + safety_margin
        r = max_x - safety_margin
        b = min_y + safety_margin
        t = max_y - safety_margin
        
        path = []
        
        if l > r or b > t:
            return [{"x": (l+r)/2, "y": (b+t)/2}]
            
        step_sz = self.robot_size * (1.0 - self.overlap_percent)
        if step_sz < 2.0: step_sz = 2.0

        while l <= r and b <= t:
            path.append({"x": l, "y": b})
            path.append({"x": l, "y": t})
            l += step_sz
            if l > r: break
            
            path.append({"x": l, "y": t})
            path.append({"x": r, "y": t})
            t -= step_sz
            if t < b: break
            
            path.append({"x": r, "y": t})
            path.append({"x": r, "y": b})
            r -= step_sz
            if r < l: break
            
            path.append({"x": r, "y": b})
            path.append({"x": l, "y": b})
            b += step_sz
            
        return path

    async def move_to(self, target_x, target_y, send_json_cb):
        while self.is_running:
            dx = target_x - self.x
            dy = target_y - self.y
            dist = math.sqrt(dx*dx + dy*dy)
            
            if dist < 2.0: break
            
            target_angle = math.atan2(dx, dy)
            
            angle_diff = target_angle - self.angle
            while angle_diff > math.pi: angle_diff -= 2*math.pi
            while angle_diff < -math.pi: angle_diff += 2*math.pi
            
            if abs(angle_diff) > 0.05:
                turn_step = 0.2
                if angle_diff < 0: turn_step = -0.2
                if abs(angle_diff) < 0.2: self.angle = target_angle
                else: self.angle += turn_step
            else:
                move_d = min(self.step_size, dist)
                self.x += move_d * math.sin(self.angle)
                self.y += move_d * math.cos(self.angle)
                
            stain = self.check_stains()
            if stain and stain not in self.detected_stains:
                self.detected_stains.append(stain)
                await send_json_cb({
                    "type": "stain_found",
                    "x": stain['x'], "y": stain['y'], "score": 95.0
                })

            await asyncio.sleep(0.01)
            
            if random.random() < 0.1:
                 await send_json_cb({"type": "pose", "x": self.x, "y": self.y, "angle": self.angle})

    def move_forward(self):
        self.x += self.step_size * math.sin(self.angle)
        self.y += self.step_size * math.cos(self.angle)
        
    def turn_right(self):
        self.angle += math.pi / 2.0
        
    def turn_left(self):
        self.angle -= math.pi / 2.0

    def check_cliff(self, local_x, local_y):
        wx = self.x + local_x * math.cos(self.angle) + local_y * math.sin(self.angle)
        wy = self.y + local_x * -math.sin(self.angle) + local_y * math.cos(self.angle)
        
        if wx < 0 or wx > self.width: return True
        if wy < 0 or wy > self.height: return True
        return False

    def check_stains(self):
        for s in self.stains:
            dx = self.x - s['x']
            dy = self.y - s['y']
            if math.sqrt(dx*dx + dy*dy) < 15.0:
                 return s
        return None
