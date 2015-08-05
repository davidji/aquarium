#!/usr/bin/python

import sys
import os
from time import sleep
from datetime import date, datetime, time, timedelta
import math

LOG_FILE="/var/log/aquarium-light.log"
STEPS=1000
LIGHT_PIN=17
MIDDAY=time(hour=13)
MOONLIGHT=0.02
RADIANS_PER_SECOND=math.pi/timedelta(hours=12).total_seconds()
PI_BLASTER='/dev/pi-blaster'
light_current=0.0

def set_light(fraction):
    global light_current
    if abs(light_current - fraction) >= 0.001:
        ctrl = open(PI_BLASTER, 'w')
        ctrl.write('{}={}\n'.format(LIGHT_PIN, 1.0-fraction))
        ctrl.close()
        light_current = fraction

def daylight_from_delta(delta):
    return max(MOONLIGHT, math.cos(delta.total_seconds()*RADIANS_PER_SECOND))

def daylight(at=None):
    now = at or datetime.now()
    return daylight_from_delta(now - datetime.combine(date.today(), MIDDAY))

def set_daylight(at=None):
    set_light(daylight(at))

def strobe():
    for i in range(STEPS):
        set_light(float(i)/STEPS)
	sleep(0.01)

def main(args):
    set_light(0.0)
    while True:
        set_daylight()
        sleep(10)


if __name__ == "__main__":
    logfile = os.open(LOG_FILE, os.O_WRONLY|os.O_APPEND|os.O_CREAT)
    os.dup2(os.open('/dev/null', os.O_RDONLY), 0)
    os.dup2(logfile, 1)
    os.dup2(logfile, 2)
    main(sys.argv)
