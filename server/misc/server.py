import socket
import json

from numpy import add
import RPi.GPIO as GPIO
import math
import cv2
import struct
from picamera.array import PiRGBArray
from picamera import PiCamera
from threading import Thread
import time

COMM_PORT = 6970
MAX_DGRAM = 2**16
MAX_IMAGE_DGRAM = MAX_DGRAM - 64
IMAGE_QUALITY = 80
SHOT_NEUTRAL = 0
SHOT_L_PWM = 40
SHOT_R_PWM = 80
PWM_PIN = 12
SHUTTER_SPEED = 1000  # 1ms

addresses = []


class ImageServer(Thread):
    def __init__(self):
        Thread.__init__(self)
        self.daemon = True
        self.camera = PiCamera()
        self.camera.resolution = (640, 480)
        self.camera.shutter_speed = SHUTTER_SPEED
        self.rawCapture = PiRGBArray(self.camera, size=(640, 480))
        self.conn = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.start()

    def run(self):
        global addresses
        while True:
            self.camera.capture(self.rawCapture, format="bgr")
            image = self.rawCapture.array
            buffer = cv2.imencode('.jpg', image)[1].tobytes()
            size = len(buffer)
            num_of_segments = math.ceil(size / MAX_IMAGE_DGRAM)
            array_pos_start = 0
            while num_of_segments:
                array_pos_end = min(size, array_pos_start + MAX_IMAGE_DGRAM)
                for addr in addresses:
                    self.conn.sendto(
                        struct.pack('B', num_of_segments) +
                        buffer[array_pos_start:array_pos_end],
                        (addr[0], COMM_PORT)
                    )
                array_pos_start = array_pos_end
                num_of_segments -= 1
            self.rawCapture.truncate(0)


class Server:
    def __init__(self):
        self.sock_tcp = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.sock_tcp.bind(("", COMM_PORT))
        self.sock_tcp.listen(2)
        GPIO.setwarnings(False)
        GPIO.setmode(GPIO.BOARD)
        GPIO.setup(PWM_PIN, GPIO.OUT)
        self.pwm = GPIO.PWM(PWM_PIN, 50)
        GPIO.output(PWM_PIN, True)
        self.pwm.start(0)
        duty = SHOT_NEUTRAL / 18 + 2
        self.pwm.ChangeDutyCycle(duty)

    def __del__(self):
        self.sock_tcp.close()

    def loop(self):
        while True:
            conn, addr = self.sock_tcp.accept()
            command = json.loads(conn.recv(2048).decode('utf8'))
            if command['command'] == "sub_photo":
                if not (addr in addresses):
                    addresses.append(addr)
                    conn.sendall(b'success')
                else:
                    conn.sendall(b'failed')
            elif command['command'] == "shot_left":
                duty = SHOT_L_PWM / 18 + 2
                self.pwm.ChangeDutyCycle(duty)
                time.sleep(1)
                duty = SHOT_NEUTRAL / 18 + 2
                self.pwm.ChangeDutyCycle(duty)
                conn.sendall(b'success')
            elif command['command'] == "shot_right":
                duty = SHOT_R_PWM / 18 + 2
                self.pwm.ChangeDutyCycle(duty)
                time.sleep(1)
                duty = SHOT_NEUTRAL / 18 + 2
                self.pwm.ChangeDutyCycle(duty)
                conn.sendall(b'success')
            conn.close()


if __name__ == "__main__":
    serv = Server()
    ImageServer()
    print("finished Init.")
    serv.loop()
