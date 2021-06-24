from random import randint
import socket
import json
import struct
import cv2
import numpy as np

MAX_DGRAM = 2**16

class MiscClient:
    def __init__(self, ip="10.8.0.8", port=6970):
        self.ip = ip
        self.port = port

    def __sendPacket(self, command):
        raw_command = bytes(json.dumps(command), 'utf-8')
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.connect((self.ip, self.port))
        sock.sendall(raw_command)
        response = sock.recv(256)
        sock.close()
        return response == b'success\x00'

    def shot_left(self):
        command = {"command": "shot_left"}
        return self.__sendPacket(command)

    def shot_right(self):
        command = {"command": "shot_right"}
        return self.__sendPacket(command)

    def shot_neutral(self):
        command = {"command": "shot_neutral"}
        return self.__sendPacket(command)

    def recievePhoto(self):
        udp = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        udp.bind(("", 6970))
        command = {"command": "send_photo"}
        raw_command = bytes(json.dumps(command), 'utf-8')
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.connect((self.ip, self.port))
        s.sendall(raw_command) 
        dat = b''
        while True:
            seg, _ = udp.recvfrom(MAX_DGRAM)
            if len(seg) == 0:
                print("none")
                continue
            if struct.unpack('B', seg[0:1])[0] > 1:
                dat += seg[1:]
            else:
                dat += seg[1:]
                udp.close()
                s.close()
                frame = cv2.imdecode(np.fromstring(dat, dtype=np.uint8), 1)
                if frame is not None:
                    return frame
                else:
                    return self.recievePhoto()


if __name__ == "__main__":
    import random
    import time
    misc = MiscClient()
    prev_pos = 0
    while True:
        num = random.randint(0, 2)
        while num == prev_pos:
            num = random.randint(0,2)
        if num == 0:
            misc.shot_left()
        elif num == 1:
            misc.shot_right()
        else:
            misc.shot_neutral()
        prev_pos = num

        img = misc.recievePhoto()
        cv2.imshow('frame', img)
        if cv2.waitKey(1) & 0xFF == ord('q'):
            break