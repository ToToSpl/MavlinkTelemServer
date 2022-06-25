#!/usr/bin/env python3

import socket
import json
import select

from time import sleep
from threading import Thread
from drone import Drone

HOST = "127.0.0.1"
PORT = 6969

drone = Drone(HOST, PORT)

def fly():
    for _ in range(100):
        drone.offboard_cmd(-2.0, 0.0, 0.0)
        sleep(0.05)

    drone.offboard_stop()

print(drone.getTelem())

thread = Thread(target=fly)
thread.start()
print("thread start")

drone.offboard_start()
print("offb start")
sleep(10.0)

thread.join()
