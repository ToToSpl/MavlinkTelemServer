import socket
import json
import RPi.GPIO as GPIO
import math
import cv2
import struct
from picamera.array import PiRGBArray
from picamera import PiCamera

COMM_PORT = 6970
MAX_DGRAM = 2**16
MAX_IMAGE_DGRAM = MAX_DGRAM - 64
IMAGE_QUALITY = 80
SHOT_NEUTRAL = 0
SHOT_L_PWM = 40
SHOT_R_PWM = 80
PWM_PIN = 12


class Server:
    def __init__(self):
        self.sock_tcp = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.sock_tcp.bind(("", COMM_PORT))
        self.sock_tcp.listen(2)
        self.camera = PiCamera()
        self.camera.resolution = (640, 480)
        self.rawCapture = PiRGBArray(self.camera, size=(640, 480))
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

    def compress_send(self, image, addr):
        conn = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        buffer = cv2.imencode('.jpg', image)[1].tobytes()
        size = len(buffer)
        num_of_segments = math.ceil(size / MAX_IMAGE_DGRAM)
        array_pos_start = 0
        while num_of_segments:
            array_pos_end = min(size, array_pos_start + MAX_IMAGE_DGRAM)
            conn.sendto(
                struct.pack('B', num_of_segments) +
                buffer[array_pos_start:array_pos_end],
                (addr[0], COMM_PORT)
            )
            array_pos_start = array_pos_end
            num_of_segments -= 1
        conn.close()

    def loop(self):
        while True:
            conn, addr = self.sock_tcp.accept()
            command = json.loads(conn.recv(2048).decode('utf8'))
            if command['command'] == "send_photo":
                self.camera.capture(self.rawCapture, format="bgr")
                image = self.rawCapture.array
                self.compress_send(image, addr)
                self.rawCapture.truncate(0)
            elif command['command'] == "shot_left":
                duty = SHOT_L_PWM / 18 + 2
                self.pwm.ChangeDutyCycle(duty)
                conn.sendall(b'success')
            elif command['command'] == "shot_right":
                duty = SHOT_R_PWM / 18 + 2
                self.pwm.ChangeDutyCycle(duty)
                conn.sendall(b'success')
            elif command['command'] == "shot_neutral":
                duty = SHOT_NEUTRAL / 18 + 2
                self.pwm.ChangeDutyCycle(duty)
                conn.sendall(b'success')
            conn.close()


if __name__ == "__main__":
    serv = Server()
    serv.loop()
