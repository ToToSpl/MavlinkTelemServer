from random import randint
import socket
import json
import struct
import cv2
import numpy as np
import select

MAX_DGRAM = 2**10
MAX_TIMEOUT = 2
COMM_PORT = 6970


class MiscClient:
    def __init__(self, ip="10.8.0.8", port=COMM_PORT):
        self.ip = ip
        self.port = port
        self.udpRegistered = False

    def __sendPacket(self, command):
        raw_command = bytes(json.dumps(command), 'utf-8')
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.connect((self.ip, self.port))
        sock.setblocking(0)
        sock.sendall(raw_command)
        ready = select.select([sock], [], [], MAX_TIMEOUT)
        if ready[0]:
            response = sock.recv(256)
            print(response)
            sock.close()
            return response == b'success\x00'
        else:
            sock.close()
            return self.__sendPacket(command)

    def shot_parch(self):
        command = {"command": "shot_parch"}
        return self.__sendPacket(command)

    def shot_macz(self):
        command = {"command": "shot_macz"}
        return self.__sendPacket(command)


if __name__ == "__main__":
    import random
    import time
    misc = MiscClient()
    num = 0
    while True:
        if num % 2:
            print("parch")
            misc.shot_parch()
        else:
            print("macz")
            misc.shot_macz()
        num += 1
        time.sleep(10)
