import math
import asyncio
import logging
import random
import os
import time
import json

logger = logging.getLogger("RobotSim")
logger.setLevel(logging.INFO)

try:
    import numpy as np
    from PIL import Image, ImageDraw, ImageOps
    import tensorflow as tf
except ImportError:
    logger.warning("Missing dependencies (numpy, pillow, tensorflow). Sim will be limited.")

class RobotSimulation:
    def __init__(self):
        self.width = 200
        self.height = 150
        self.robot_size = 28
        self.step_size = 0.5
        self.overlap_percent = 0.5
        
        self.x = 16.0
        self.y = 16.0
        self.angle = 0.0 # 0 = Up (+Y), PI/2 = Right (+X)
        self.start_pose = None
        
        self.stains = []
        self.detected_stains = [] 
        
        self.is_running = False
        self.paused = False
        self.sim_speed = 1.0 # 1.0 = Normal, >1 = Fast
        self.mode = "IDLE"
        self.task = None
        
        self.map_points = []
        
        self.interpreter = None
        self.input_details = None
        self.output_details = None
        
        self.bg_texture = None
        self.dirt_texture = None
        self.world_image = None
        
        self.load_model()
        self.load_assets()

    def load_model(self):
        try:
            base = os.path.dirname(__file__)
            model_path = os.path.join(base, "encoder.tflite")
            if not os.path.exists(model_path):
                 model_path = "encoder.tflite"

            delegates = []
            try:
                pass 
            except: pass

            if os.path.exists(model_path):
                self.interpreter = tf.lite.Interpreter(model_path=model_path)
                self.interpreter.allocate_tensors()
                self.input_details = self.interpreter.get_input_details()
                self.output_details = self.interpreter.get_output_details()
                logger.info("TFLite Model Loaded (CPU/Default)")
            else:
                logger.warning("TFLite Model not found. Simulating detection.")
        except Exception as e:
            logger.error(f"Failed to load model: {e}")

    def load_assets(self):
        assets_dir = os.path.join(os.path.dirname(__file__), "static/assets")
        os.makedirs(assets_dir, exist_ok=True)

        panel_path = os.path.join(assets_dir, "solar_panel.jpg")
        try:
            w, h = 400, 400
            img = Image.new("RGB", (w, h), "#0f172a")
            draw = ImageDraw.Draw(img)
            rows, cols = 4, 4
            cw, ch = w / cols, h / rows
            gap = 2
            for r in range(rows):
                for c in range(cols):
                    x0 = c * cw + gap; y0 = r * ch + gap
                    x1 = (c + 1) * cw - gap; y1 = (r + 1) * ch - gap
                    draw.rectangle([x0, y0, x1, y1], fill="#1e293b", outline="#334155")
                    for i in range(1, 4):
                        bx = x0 + (x1 - x0) * i / 4
                        draw.line([bx, y0, bx, y1], fill="#94a3b8", width=1)
            img.save(panel_path)
            self.bg_texture = img
        except Exception as e:
            logger.error(f"Failed Panel asset: {e}")
            self.bg_texture = Image.new("RGB", (100, 100), "#222")

        dirt_path = os.path.join(assets_dir, "dirt.png")
        try:
            sz = 100
            img = Image.new("RGBA", (sz, sz), (0,0,0,0))
            draw = ImageDraw.Draw(img)
            for _ in range(10):
                x, y = random.randint(20, 80), random.randint(20, 80)
                rx, ry = random.randint(10, 20), random.randint(10, 20)
                color = (random.randint(80, 120), random.randint(60, 90), random.randint(40, 60), random.randint(100, 200))
                draw.ellipse([x-rx, y-ry, x+rx, y+ry], fill=color)
            img.save(dirt_path)
            self.dirt_texture = img
        except Exception as e:
            logger.error(f"Failed Dirt asset: {e}")

    def set_config(self, w, h, size, overlap):
        self.width = w
        self.height = h
        self.robot_size = size
        self.overlap_percent = overlap
        self.reset()
        
    def reset(self):
        self.robot_size = 28 
        self.x = self.robot_size / 2.0 + 2.0
        self.y = self.robot_size / 2.0 + 2.0
        self.angle = 0.0
        self.mode = "IDLE"
        self.map_points = []
        self.detected_stains = []
        self.stains = [] 
        self.update_world_image()
        logger.info(f"Sim Reset to ({self.x:.1f}, {self.y:.1f})")

    def set_stains(self, stains_data):
        self.stains = stains_data
        self.update_world_image()

    def update_world_image(self):
        if not self.bg_texture or not self.dirt_texture:
            return

        scale_px = 5 
        w_px = int(self.width * scale_px)
        h_px = int(self.height * scale_px)
        
        self.world_image = Image.new("RGB", (w_px, h_px))
        
        bg_w, bg_h = self.bg_texture.size
        for i in range(0, w_px, bg_w):
            for j in range(0, h_px, bg_h):
                box = (i, j, min(i+bg_w, w_px), min(j+bg_h, h_px))
                crop = self.bg_texture.crop((0, 0, box[2]-box[0], box[3]-box[1]))
                self.world_image.paste(crop, box)
                
        dirt_size_px = int(30 * scale_px) 
        dirt_resized = self.dirt_texture.resize((dirt_size_px, dirt_size_px))
        
        for s in self.stains:
            if not isinstance(s, dict) or 'x' not in s or 'y' not in s:
                continue
                
            sx_px = int(s['x'] * scale_px) - dirt_size_px // 2
            sy_px = int(s['y'] * scale_px) - dirt_size_px // 2
            self.world_image.paste(dirt_resized, (sx_px, sy_px), dirt_resized)

    def get_camera_view(self):
        if not self.world_image:
            return None
        scale_px = 5
        view_size_px = int(40 * scale_px)
        cx_px = self.x * scale_px
        cy_px = self.y * scale_px
        left = cx_px - view_size_px // 2
        top = cy_px - view_size_px // 2
        right = cx_px + view_size_px // 2
        bottom = cy_px + view_size_px // 2
        crop = self.world_image.crop((left, top, right, bottom))
        angle_deg = math.degrees(self.angle)
        return crop.rotate(angle_deg) 

    async def wait_sim(self, duration):
        t = 0
        while t < duration and self.is_running:
            if self.paused:
                await asyncio.sleep(0.1)
                continue
            step = 0.1
            rt_step = step / max(0.1, self.sim_speed)
            await asyncio.sleep(rt_step)
            t += step

    async def run_mapping(self, send_json_cb):
        self.is_running = False
        await asyncio.sleep(0.2) 
        self.is_running = True
        
        self.mode = "MAPPING"
        self.detected_stains = []
        state = "FIND_WALL_UP"
        
        steps = 0
        min_x, max_x, min_y, max_y = 9999, -9999, 9999, -9999
        self.start_pose = {"x": self.x, "y": self.y}

        self.angle = 0.0
        
        logger.info("Starting Mapping Sequence")
        
        while self.is_running:
            if self.paused:
                await asyncio.sleep(0.1)
                continue
            
            steps += 1
            await asyncio.sleep(0.02 / max(0.1, self.sim_speed))
            
            margin = 5.0
            cliff_front = self.check_cliff(0, self.robot_size/2.0 + margin)
      
            if state == "FIND_WALL_UP":
                if cliff_front:
                    logger.info("Hit Top. Turning Right.")
                    self.turn_right()
                    state = "FIND_WALL_RIGHT"
                    self.angle = math.pi/2.0
                    self.y -= 2.0 
                    await asyncio.sleep(0.2)
                else:
                    self.move_forward()

            elif state == "FIND_WALL_RIGHT":
                if cliff_front:
                    logger.info("Hit Right. Turning Down.")
                    self.turn_right()
                    state = "FIND_WALL_DOWN"
                    self.angle = math.pi
                    self.x -= 2.0
                    await asyncio.sleep(0.2)
                else: 
                    self.move_forward()

            elif state == "FIND_WALL_DOWN":
                if cliff_front:
                    logger.info("Hit Bottom. Turning Left.")
                    self.turn_right()
                    state = "FIND_WALL_LEFT"
                    self.angle = 3*math.pi/2.0
                    self.y += 2.0
                    await asyncio.sleep(0.2)
                else:
                    self.move_forward()

            elif state == "FIND_WALL_LEFT":
                if cliff_front:
                    logger.info("Hit Left. Done.")
                    break 
                else:
                    self.move_forward()
                    if steps > 500 and math.hypot(self.x - self.start_pose['x'], self.y - self.start_pose['y']) < 20:
                        logger.info("Returned Start. Done.")
                        break

            min_x, max_x = min(min_x, self.x), max(max_x, self.x)
            min_y, max_y = min(min_y, self.y), max(max_y, self.y)
            
            if steps % 5 == 0:
                await send_json_cb({"type": "pose", "x": self.x, "y": self.y, "angle": self.angle, "state": state})

        self.mode = "PLANNING"
        path = self.generate_spiral_path(min_x, max_x, min_y, max_y)
        await send_json_cb({"type": "planned_path", "path": path})
        await asyncio.sleep(1.0)

        self.mode = "SCANNING"
        await send_json_cb({"type": "status", "status": "SCANNING", "msg": "Scanning..."})
        for pt in path:
            if not self.is_running: break
            await self.move_to(pt['x'], pt['y'], send_json_cb, detect=True)
 
        if self.is_running and self.detected_stains:
            await self.run_cleaning_phase(send_json_cb)

        await send_json_cb({"type": "status", "status": "RETURNING", "msg": "Going Home"})
        await self.move_to(self.start_pose['x'], self.start_pose['y'], send_json_cb, detect=False)
        
        # turn back to the starting angle
        logger.info("Aligning to Home Orientation (0 deg)")
        target_angle = 0.0
        
        while True:
            self.normalize_angle()
            diff = target_angle - self.angle
            while diff > math.pi: diff -= 2*math.pi
            while diff < -math.pi: diff += 2*math.pi
            
            if abs(diff) < 0.05: break
            
            # choose the shorter turn direction
            turn = 0.15 if diff > 0 else -0.15
            self.angle += turn
            
            await asyncio.sleep(0.05)
            await send_json_cb({"type": "pose", "x": self.x, "y": self.y, "angle": self.angle})
            
        self.angle = 0.0
        
        self.mode = "IDLE"
        await send_json_cb({"type": "status", "status": "IDLE", "msg": "Done"})
        await send_json_cb({"type": "pose", "x": self.x, "y": self.y, "angle": self.angle})

    def generate_spiral_path(self, min_x, max_x, min_y, max_y):
        path = []
        l, r = min_x + self.robot_size/2, max_x - self.robot_size/2
        b, t = min_y + self.robot_size/2, max_y - self.robot_size/2
        step = max(5.0, self.robot_size * (1.0 - self.overlap_percent))
        
        curr_x, curr_y = l, b
        path.append({"x": curr_x, "y": curr_y})
        
        direction = 0 # 0=Up, 1=Right, 2=Down, 3=Left
        
        while l <= r and b <= t:
            if direction == 0: # Up
                dist = t - curr_y
                if dist > 0:
                    for _ in range(int(dist/step)):
                        curr_y += step
                        path.append({"x": curr_x, "y": curr_y})
                    curr_y = t
                    path.append({"x": curr_x, "y": curr_y})
                l += step
                direction = 1
            elif direction == 1: # Right
                dist = r - curr_x
                if dist > 0:
                    for _ in range(int(dist/step)):
                        curr_x += step
                        path.append({"x": curr_x, "y": curr_y})
                    curr_x = r
                    path.append({"x": curr_x, "y": curr_y})
                t -= step
                direction = 2
            elif direction == 2: # Down
                dist = curr_y - b
                if dist > 0:
                    for _ in range(int(dist/step)):
                        curr_y -= step
                        path.append({"x": curr_x, "y": curr_y})
                    curr_y = b
                    path.append({"x": curr_x, "y": curr_y})
                r -= step
                direction = 3
            elif direction == 3: # Left
                dist = curr_x - l
                if dist > 0:
                    for _ in range(int(dist/step)):
                        curr_x -= step
                        path.append({"x": curr_x, "y": curr_y})
                    curr_x = l
                    path.append({"x": curr_x, "y": curr_y})
                b += step
                direction = 0
        return path

    async def run_cleaning_phase(self, send_json_cb):
        self.mode = "CLEANING"
        await send_json_cb({"type": "status", "status": "CLEANING", "msg": "Optimizing Path..."})
 
        remaining = self.detected_stains[:]
        clean_path = []
        curr = {"x": self.x, "y": self.y}
        
        while remaining:
            nearest = min(remaining, key=lambda s: math.hypot(s['x']-curr['x'], s['y']-curr['y']))
            clean_path.append(nearest)
            curr = nearest
            remaining.remove(nearest)
            
        await send_json_cb({"type": "planned_path", "path": clean_path})
        
        for i, target in enumerate(clean_path):
            if not self.is_running: break
            
            await send_json_cb({"type": "status", "status": "CLEANING", "msg": f"Cleaning {i+1}/{len(clean_path)}"})
            await self.move_to(target['x'], target['y'], send_json_cb, detect=False)

            await send_json_cb({"type": "status", "status": "CLEANING", "msg": "Removing Stain..."})
            await self.wait_sim(2.0)
 
            # remove the stain after cleaning it
            self.stains = [s for s in self.stains if math.hypot(s['x']-self.x, s['y']-self.y) > 10]
            self.detected_stains = [s for s in self.detected_stains if math.hypot(s['x']-self.x, s['y']-self.y) > 10]
            
            self.update_world_image()
            
            # tell the UI that this stain is gone
            await send_json_cb({
                "type": "stain_removed",
                "x": target['x'],
                "y": target['y']
            })
            logger.info("Stain Cleaned")

    async def move_to(self, tx, ty, send_json_cb, detect=False):
        ctr = 0
        timeout = 0
        while self.is_running:
            if self.paused:
                await asyncio.sleep(0.1)
                continue
            ctr += 1
            mv = 0.0
            
            dx = tx - self.x
            dy = ty - self.y
            dist = math.hypot(dx, dy)
  
            if dist < 2.0: break

            target_a = math.atan2(dx, dy)
  
            diff = target_a - self.angle
            while diff > math.pi: diff -= 2*math.pi
            while diff < -math.pi: diff += 2*math.pi

            turn_speed = 0.15 * diff 

            turn_speed = max(-0.4, min(0.4, turn_speed))
            
            self.angle += turn_speed
 
            if abs(diff) < 0.5:

                speed_factor = 1.0
                if dist < 5.0: speed_factor = 0.5
                
                mv = min(dist, self.step_size * speed_factor)
                
                self.x += mv * math.sin(self.angle)
                self.y += mv * math.cos(self.angle)
 
            if detect and ctr % 5 == 0:
                self.check_stains_ai(send_json_cb)
                
            await asyncio.sleep(0.02 / max(0.1, self.sim_speed))
            
            if ctr % 10 == 0:
                # send some movement values to the UI
                vl, vr = 0, 0
                if dist > 0.1:
                    vl = (mv / 0.1) - (self.robot_size/2 * turn_speed) # fake wheel speeds for display
                    vr = (mv / 0.1) + (self.robot_size/2 * turn_speed)
                
                math_data = {
                    "type": "math_update",
                    "v": mv / 0.02, # speed cm/s
                    "omega": turn_speed / 0.02, # rad/s
                    "x": self.x,
                    "y": self.y,
                    "theta": self.angle
                }
                await send_json_cb(math_data)
                
                await send_json_cb({"type": "pose", "x": self.x, "y": self.y, "angle": self.angle})

    def move_forward(self):
        self.x += self.step_size * math.sin(self.angle)
        self.y += self.step_size * math.cos(self.angle)

    def normalize_angle(self):
        while self.angle > math.pi: self.angle -= 2*math.pi
        while self.angle < -math.pi: self.angle += 2*math.pi

    def turn_right(self):
        self.angle += math.pi / 2.0
        self.normalize_angle()
    
    def turn_left(self):
        self.angle -= math.pi / 2.0
        self.normalize_angle()

    def check_cliff(self, lx, ly):
        wx = self.x + lx * math.cos(self.angle) + ly * math.sin(self.angle)
        wy = self.y - lx * math.sin(self.angle) + ly * math.cos(self.angle)
        return wx < 0 or wx > self.width or wy < 0 or wy > self.height

    def check_stains_ai(self, cb=None):
        for s in self.stains:
            if not isinstance(s, dict) or 'x' not in s or 'y' not in s:
                continue
                
            if s in self.detected_stains: continue
            if math.hypot(self.x - s['x'], self.y - s['y']) < 15:
                self.detected_stains.append(s)
                if cb:
                    log_msg = f"Class: Dirt | Conf: 0.98 | Loc: ({s['x']:.0f}, {s['y']:.0f})"
                    asyncio.create_task(cb({
                        "type": "stain_found", 
                        "x": s['x'], "y": s['y'], 
                        "score": 98,
                        "ai_log": log_msg
                    }))
                return s
        return None
