# This uses a fork of RPIO:
# https://github.com/PaulPenson/RPIO

import RPi.GPIO as GPIO
from time import sleep

STEPS=1000
CHANNEL=0
LIGHT_PIN=17

GPIO.setmode(GPIO.BCM)
GPIO.setup(LIGHT_PIN, GPIO.OUT)
light=GPIO.PWM(LIGHT_PIN, 100)
# start with light off
light.start(100)

def set_light(percent):
    light.ChangeDutyCycle(percent)

def strobe():
    for i in range(STEPS):
        set_light(float(i*100)/STEPS)
	sleep(0.01)

if __name__ == "__main__":
    strobe()
    light.stop()
    GPIO.cleanup()


