import socket
import json
import serial

COMM_PORT = 6970
MAX_DGRAM = 2**16
MAX_IMAGE_DGRAM = MAX_DGRAM - 64
IMAGE_QUALITY = 80
SHUTTER_SPEED = 30000  # 30ms


class Server:
    def __init__(self):
        self.sock_tcp = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.sock_tcp.bind(("", COMM_PORT))
        self.sock_tcp.listen(2)
        self.ser = serial.Serial('/dev/ttyAMA1', 9600)

    def __del__(self):
        self.sock_tcp.close()
        self.ser.close()

    def loop(self):
        while True:
            conn, addr = self.sock_tcp.accept()
            command = json.loads(conn.recv(2048).decode('utf8'))
            if command['command'] == "shot_parch":
                self.ser.write(b'P')
                conn.sendall(b'success')
            elif command['command'] == "shot_macz":
                self.ser.write(b'M')
                conn.sendall(b'success')
            conn.close()


if __name__ == "__main__":
    serv = Server()
    print("finished Init.")
    serv.loop()
