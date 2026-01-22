import asyncio
import sys
import json
import logging
import os
from contextlib import asynccontextmanager
from pathlib import Path
from typing import List

from fastapi import FastAPI, WebSocket, WebSocketDisconnect
from fastapi.staticfiles import StaticFiles
from fastapi.middleware.cors import CORSMiddleware

from simulation import RobotSimulation

logging.basicConfig(level=logging.INFO)
logger = logging.getLogger("RobotServer")

try:
    import websockets
except ImportError:
    logger.error("CRITICAL: 'websockets' library not found. Install it with: pip install websockets")
    sys.exit(1)

class ConnectionManager:
    def __init__(self):
        self.active_connections: List[WebSocket] = []

    async def connect(self, websocket: WebSocket):
        await websocket.accept()
        self.active_connections.append(websocket)

    def disconnect(self, websocket: WebSocket):
        if websocket in self.active_connections:
            self.active_connections.remove(websocket)

    async def broadcast(self, message: str):
        for connection in self.active_connections:
            try:
                await connection.send_text(message)
            except:
                pass

manager = ConnectionManager()
sim_robot = RobotSimulation()

async def sim_output_bridge(data):
    msg = json.dumps(data)
    await manager.broadcast(msg)

@asynccontextmanager
async def lifespan(app: FastAPI):
    logger.info("Server Starting - Sim Mode Ready")
    yield
    sim_robot.is_running = False

app = FastAPI(lifespan=lifespan)

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

@app.websocket("/ws")
async def websocket_endpoint(websocket: WebSocket):
    await manager.connect(websocket)
    try:
        while True:
            data = await websocket.receive_text()
            try:
                cmd_obj = json.loads(data)
                cmd = cmd_obj.get("cmd", "")
                
                if cmd == "set_sim_config":
                    sim_robot.set_config(
                        cmd_obj.get("w", 200),
                        cmd_obj.get("h", 150),
                        cmd_obj.get("size", 28),
                        cmd_obj.get("overlap", 0.5)
                    )
                    await manager.broadcast(json.dumps({"type": "status", "status": "IDLE", "msg": "Config Updated"}))
                    
                elif cmd == "update_sim_stains":
                    stains_data = cmd_obj.get("stains", [])
                    sim_robot.set_stains(stains_data)
                    await manager.broadcast(json.dumps({"type": "status", "status": "IDLE", "msg": f"Stains Updated ({len(stains_data)})"}))
                    
                elif cmd == "reset_sim":
                    sim_robot.is_running = False
                    sim_robot.reset()
                    await manager.broadcast(json.dumps({"type": "status", "status": "IDLE", "msg": "Sim Reset"}))
                    await manager.broadcast(json.dumps({"type": "pose", "x": sim_robot.x, "y": sim_robot.y, "dir": "UP"}))
                    
                elif cmd == "start_mapping":
                    if sim_robot.is_running:
                        sim_robot.is_running = False
                        await asyncio.sleep(0.1)
                        
                    sim_robot.is_running = True
                    asyncio.create_task(sim_robot.run_mapping(sim_output_bridge))
                    await manager.broadcast(json.dumps({"type": "status", "status": "BUSY", "msg": "Sim Mapping Started"}))
                 
                elif cmd == "stop":
                    sim_robot.is_running = False
                    await manager.broadcast(json.dumps({"type": "status", "status": "STOPPED", "msg": "Sim Stopped"}))

            except json.JSONDecodeError:
                pass
                
    except WebSocketDisconnect:
        manager.disconnect(websocket)
    except Exception as e:
        logger.error(f"WebSocket Error: {e}")
        manager.disconnect(websocket)

static_dir = Path(__file__).parent / "static"
if not static_dir.exists():
    os.makedirs(static_dir, exist_ok=True)

app.mount("/", StaticFiles(directory=str(static_dir), html=True), name="static")

if __name__ == "__main__":
    import uvicorn
    uvicorn.run(app, host="0.0.0.0", port=8000, ws="websockets")
