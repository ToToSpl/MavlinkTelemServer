from random import randint
import socket
import json
import struct
import cv2
import numpy as np
import select

MAX_DGRAM = 2**16
MAX_TIMEOUT = 2


class MiscClient:
    def __init__(self, ip="10.8.0.8", port=6970):
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

    def register_udp(self):
        command = {"command": "sub_photo"}
        result = self.__sendPacket(command)
        self.udpRegistered = True
        return result

    def __dump_buffer(self, s):
        """ Emptying buffer frame """
        while True:
            seg, _ = s.recvfrom(MAX_DGRAM)
            if len(seg) == 0:
                self.__dump_buffer(s)
                return
            if struct.unpack('B', seg[0:1])[0] == 1:
                return

    def recievePhoto(self):
        if not self.udpRegistered:
            self.register_udp()
        udp = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        udp.bind(("", 6970))
        self.__dump_buffer(udp)
        dat = b''
        while True:
            seg, _ = udp.recvfrom(MAX_DGRAM)
            if len(seg) == 0:
                udp.close()
                return self.recievePhoto()
            if struct.unpack('B', seg[0:1])[0] > 1:
                dat += seg[1:]
            else:
                dat += seg[1:]
                udp.close()
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
        # num = random.randint(0, 2)
        # while num == prev_pos:
        #     num = random.randint(0, 2)
        # if num == 0:
        #     misc.shot_left()
        # else:
        #     misc.shot_right()
        # prev_pos = num
        # time.sleep(10)

        img = misc.recievePhoto()
        cv2.imshow('frame', img)
        if cv2.waitKey(1) & 0xFF == ord('q'):
            break
