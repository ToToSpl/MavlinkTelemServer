'''
    simple class to use server in frendlier way
'''
import socket
import json


class Drone:
    def __init__(self, ip, port):
        self.ip = ip
        self.port = port

    def getTelem(self):
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.connect((self.ip, self.port))
        sock.sendall(b'get')
        data = json.loads(sock.recv(2048))
        sock.close()
        return data

    def goto(self, lat, lon, alt, heading):
        command = {
            "command": "goto",
            "lat": lat,
            "lon": lon,
            "alt": alt,
            "heading": heading
        }
        raw_command = bytes(json.dumps(command), 'utf-8')
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.connect((self.ip, self.port))
        sock.sendall(raw_command)
        if sock.recv(256) != b'success\x00':
            print("error in goto!")
        sock.close()


if __name__ == "__main__":
    drone = Drone("127.0.0.1", 6969)
    print(drone.getTelem())

    drone.goto(
        47.39807052349714,
        8.54562064372404,
        15.0,
        180.0
    )
