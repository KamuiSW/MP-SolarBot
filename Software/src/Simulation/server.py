from fastapi import FastAPI, WebSocket, WebSocketDisconnect
from fastapi.staticfiles import StaticFiles
from fastapi.responses import HTMLResponse
import asyncio
import json
import logging
import os
import uvicorn
from contextlib import asynccontextmanager

# logging for the server
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger("RobotServer")

from simulation import RobotSimulation

sim_robot = RobotSimulation()

class ConnectionManager:
    def __init__(self):
        self.active_connections: list[WebSocket] = []

    async def connect(self, websocket: WebSocket):
        await websocket.accept()
        self.active_connections.append(websocket)

    async def disconnect(self, websocket: WebSocket):
        self.active_connections.remove(websocket)

    async def broadcast(self, message: str):
        for connection in self.active_connections:
            try:
                await connection.send_text(message)
            except:
                pass

manager = ConnectionManager()

# lets the simulation send messages to the browser
async def sim_output_bridge(data):
    await manager.broadcast(json.dumps(data))

@asynccontextmanager
async def lifespan(app: FastAPI):
    logger.info("Server Starting - Sim Mode Ready")
    yield
    sim_robot.is_running = False

app = FastAPI(lifespan=lifespan)

base_dir = os.path.dirname(os.path.abspath(__file__))
static_dir = os.path.join(base_dir, "static")
assets_dir = os.path.join(static_dir, "assets")

# create this folder if it is missing
os.makedirs(assets_dir, exist_ok=True)

app.mount("/assets", StaticFiles(directory=assets_dir), name="assets")
app.mount("/static", StaticFiles(directory=static_dir), name="static")

@app.get("/")
async def get():
    index_path = os.path.join(static_dir, "index.html")
    if os.path.exists(index_path):
        with open(index_path) as f:
            return HTMLResponse(f.read())
    return HTMLResponse("<h1>Index missing</h1>", status_code=404)

@app.websocket("/ws")
async def websocket_endpoint(websocket: WebSocket):
    await manager.connect(websocket)
    try:
        while True:
            data = await websocket.receive_text()
            cmd_obj = json.loads(data)
            cmd = cmd_obj.get("cmd")
            
            if cmd == "set_sim_config":
                sim_robot.set_config(
                    cmd_obj.get("w", 200),
                    cmd_obj.get("h", 150),
                    cmd_obj.get("size", 28),
                    cmd_obj.get("overlap", 0.5)
                )
                await manager.broadcast(json.dumps({"type": "status", "status": "IDLE", "msg": "Config Updated"}))
            
            elif cmd == "set_speed":
                speed = float(cmd_obj.get("speed", 1.0))
                sim_robot.sim_speed = speed
                
            elif cmd == "toggle_pause":
                sim_robot.paused = not sim_robot.paused
                s = "PAUSED" if sim_robot.paused else "RUNNING"
                await manager.broadcast(json.dumps({"type": "status", "status": s, "msg": f"Sim {s}"}))
                
            elif cmd == "update_sim_stains":
                stains_data = cmd_obj.get("stains", [])
                sim_robot.set_stains(stains_data)
                
            elif cmd == "reset_sim":
                sim_robot.is_running = False
                sim_robot.reset()
                await manager.broadcast(json.dumps({"type": "status", "status": "IDLE", "msg": "Sim Reset"}))
                await manager.broadcast(json.dumps({"type": "pose", "x": sim_robot.x, "y": sim_robot.y}))
                
            elif cmd == "start_mapping":
                sim_robot.is_running = False
                await asyncio.sleep(0.1)
                sim_robot.is_running = True
                asyncio.create_task(sim_robot.run_mapping(sim_output_bridge))
                await manager.broadcast(json.dumps({"type": "status", "status": "BUSY", "msg": "Mapping Started"}))
                
            elif cmd == "stop":
                sim_robot.is_running = False
                await manager.broadcast(json.dumps({"type": "status", "status": "STOPPED", "msg": "Stopped"}))

    except WebSocketDisconnect:
        await manager.disconnect(websocket)

if __name__ == "__main__":
    uvicorn.run("server:app", host="0.0.0.0", port=8000, reload=True)
