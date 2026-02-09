import RPi.GPIO as GPIO
import time

# Left motor driver (U2)
L_STEP = 22
L_DIR  = 27
L_EN   = 17

# Right motor driver (U3)
R_STEP = 15
R_DIR  = 14 
R_EN   = 25

STEPS_PER_CM = 157
DISTANCE_CM = 30
TOTAL_STEPS = int(STEPS_PER_CM * DISTANCE_CM)

STEP_DELAY = 0.0008  # seconds

GPIO.setmode(GPIO.BCM)

for pin in (L_STEP, L_DIR, L_EN, R_STEP, R_DIR, R_EN):
    GPIO.setup(pin, GPIO.OUT)
    GPIO.output(pin, GPIO.LOW)

def enable_drivers(enable: bool):
    # TMC2209 EN is usually active-low: LOW = enabled, HIGH = disabled
    GPIO.output(L_EN, GPIO.LOW if enable else GPIO.HIGH)
    GPIO.output(R_EN, GPIO.LOW if enable else GPIO.HIGH)

def set_direction(forward: bool):
    # If robot spins instead of going straight, invert one DIR.
    GPIO.output(L_DIR, GPIO.HIGH if forward else GPIO.LOW)
    GPIO.output(R_DIR, GPIO.HIGH if forward else GPIO.LOW)

def step_both(steps: int):
    for _ in range(steps):
        GPIO.output(L_STEP, GPIO.HIGH)
        GPIO.output(R_STEP, GPIO.HIGH)
        time.sleep(STEP_DELAY)

        GPIO.output(L_STEP, GPIO.LOW)
        GPIO.output(R_STEP, GPIO.LOW)
        time.sleep(STEP_DELAY)

try:
    enable_drivers(True)
    time.sleep(0.05)

    print(f"Forward {DISTANCE_CM} cm (~{TOTAL_STEPS} steps)")
    set_direction(True)
    step_both(TOTAL_STEPS)

    time.sleep(1.0)

    print(f"Backward {DISTANCE_CM} cm (~{TOTAL_STEPS} steps)")
    set_direction(False)
    step_both(TOTAL_STEPS)

finally:
    enable_drivers(False)
    GPIO.cleanup()
