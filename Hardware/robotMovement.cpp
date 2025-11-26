#include <wiringPi.h>
#include <cmath>
#include <iostream>
#include "movement.h"

const int LEFT_STEP_PIN  = 17;
const int LEFT_DIR_PIN   = 27;

const int RIGHT_STEP_PIN = 22;
const int RIGHT_DIR_PIN  = 23;

const float STEPS_PER_REV = 200.0f;
const float MICROSTEPPING = 16.0f;//DRV8825 microstepping
const float WHEEL_DIAMETER_MM = 65.0f;
const float WHEEL_BASE_MM = 150.0f;//distance between left/right wheels

float stepsPerMM;
float stepsPerDegree;

void stepMotor(int pinStep, int steps, int delayMicros)
{
    for (int i = 0; i < steps; i++)
    {
        digitalWrite(pinStep, HIGH);
        delayMicroseconds(delayMicros);
        digitalWrite(pinStep, LOW);
        delayMicroseconds(delayMicros);
    }
}

void setupPins()
{
    wiringPiSetupGpio();

    pinMode(LEFT_STEP_PIN, OUTPUT);
    pinMode(LEFT_DIR_PIN, OUTPUT);

    pinMode(RIGHT_STEP_PIN, OUTPUT);
    pinMode(RIGHT_DIR_PIN, OUTPUT);

    float wheelCircumference = M_PI * WHEEL_DIAMETER_MM;
    stepsPerMM = (STEPS_PER_REV * MICROSTEPPING) / wheelCircumference;

    float turnCircumference = M_PI * WHEEL_BASE_MM;
    float mmPerDegree = turnCircumference / 360.0f;
    stepsPerDegree = stepsPerMM * mmPerDegree;
}

void moveStraight(float mm)
{
    int steps = std::abs(mm * stepsPerMM);
    digitalWrite(LEFT_DIR_PIN, mm > 0 ? HIGH : LOW);
    digitalWrite(RIGHT_DIR_PIN, mm > 0 ? HIGH : LOW);

    for (int i = 0; i < steps; i++)
    {
        digitalWrite(LEFT_STEP_PIN, HIGH);
        digitalWrite(RIGHT_STEP_PIN, HIGH);
        delayMicroseconds(500);

        digitalWrite(LEFT_STEP_PIN, LOW);
        digitalWrite(RIGHT_STEP_PIN, LOW);
        delayMicroseconds(500);
    }
}

void moveBackward(float mm)
{
    moveStraight(-mm);
}

void tankTurn(float degrees)
{
    int steps = std::abs(degrees * stepsPerDegree);

    if (degrees > 0) {
        digitalWrite(LEFT_DIR_PIN, HIGH);
        digitalWrite(RIGHT_DIR_PIN, LOW);
    } else {
        digitalWrite(LEFT_DIR_PIN, LOW);
        digitalWrite(RIGHT_DIR_PIN, HIGH);
    }

    for (int i = 0; i < steps; i++)
    {
        digitalWrite(LEFT_STEP_PIN, HIGH);
        digitalWrite(RIGHT_STEP_PIN, HIGH);
        delayMicroseconds(500);

        digitalWrite(LEFT_STEP_PIN, LOW);
        digitalWrite(RIGHT_STEP_PIN, LOW);
        delayMicroseconds(500);
    }
}
