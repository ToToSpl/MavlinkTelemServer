'''
    simple class to use server in frendlier way
'''
import socket
import json
import select
import time

MAX_TIMEOUT = 2


class Drone:
    def __init__(self, ip, port):
        self.ip = ip
        self.port = port
        self.udp_telem = False

    def registerUDP(self):
        command = {
            "command": "add_udp"
        }

        self.udp_telem = True
        return self.__sendPacket(command)

    def getTelem(self):
        if self.udp_telem:
            udp_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            udp_sock.bind(("0.0.0.0", 6969))
            data, _ = udp_sock.recvfrom(2048)
            udp_sock.close()
            return json.loads(data)
        else:
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

    def actuator(self, index, value):
        command = {
            "command": "actuator",
            "index": int(index),
            "value": float(value)
        }
        return self.__sendPacket(command)

    def land(self):
        command = {
            "command": "land"
        }
        return self.__sendPacket(command)
        
    def hold(self):
        command = {
            "command": "hold"
        }
        return self.__sendPacket(command)

    def offboard_start(self):
        command = {
            "command": "offboard_start"
        }
        return self.__sendPacket(command)

    def offboard_stop(self):
        command = {
            "command": "offboard_stop"
        }
        return self.__sendPacket(command)

    def offboard_cmd(self, x, y, z):
        command = {
            "command": "offboard_cmd",
            "x": x,
            "y": y,
            "z": z
        }
        return self.__sendPacket(command)


def main():
    import time
    #drone = Drone("6.tcp.ngrok.io", 16052)
    drone = Drone("10.8.0.3", 6969)

    print(drone.getTelem())

    altitude = 4.0

    # drone.arm_takeoff(5.0)

    # time.sleep(1.0)

    # while drone.getTelem()['position']['alt_rel'] < (altitude - 0.5):
    #     print("...")
    #     time.sleep(0.5)

    # print("finished")

    drone.goto(50.288620, 18.679854, altitude, 10.0)

    # drone.rtl()

    # altitude = 15.0

    # if not drone.arm_takeoff(altitude):
    #     print("error in takeoff!")
    #     return

    # print("wait 3s")
    # time.sleep(3)
    # print("RTL!")

    # if not drone.rtl():
    #     print("error in RTL!")
    #     return

    # while drone.getTelem()['misc']['inAir']:
    #     print("...")
    #     time.sleep(0.5)
    # print("landed!")

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
