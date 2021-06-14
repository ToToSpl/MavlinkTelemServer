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

    def __sendPacket(self, command):
        raw_command = bytes(json.dumps(command), 'utf-8')
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.connect((self.ip, self.port))
        sock.sendall(raw_command)
        response = sock.recv(256)
        sock.close()
        return response == b'success\x00'

    def goto(self, lat, lon, alt, heading):
        command = {
            "command": "goto",
            "lat": lat,
            "lon": lon,
            "alt": alt,
            "heading": heading
        }
        return self.__sendPacket(command)

    def takeoff(self, alt):
        command = {
            "command": "takeoff",
            "alt": alt
        }
        return self.__sendPacket(command)

    def arm_takeoff(self, alt):
        command = {
            "command": "arm_takeoff",
            "alt": alt
        }
        return self.__sendPacket(command)

    def rtl(self):
        command = {
            "command": "rtl",
        }
        return self.__sendPacket(command)


def main():
    import time
    #drone = Drone("6.tcp.ngrok.io", 16052)
    drone = Drone("localhost", 6969)

    altitude = 15.0

    if not drone.arm_takeoff(altitude):
        print("error in takeoff!")
        return

    while drone.getTelem()['position']['alt_rel'] < (altitude - 0.5):
        print("...")
        time.sleep(0.5)

    print("wait 3s")
    time.sleep(3)
    print("RTL!")

    if not drone.rtl():
        print("error in RTL!")
        return

    while drone.getTelem()['misc']['inAir']:
        print("...")
        time.sleep(0.5)
    print("landed!")

    # while True:
    #     print(drone.getTelem())
    #     time.sleep(0.1)

    # print(drone.getTelem())

    # drone.goto(
    #     47.39807052349714,
    #     8.54562064372404,
    #     15.0,
    #     180.0
    # )
if __name__ == "__main__":
    main()
