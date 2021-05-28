import socket
import json

HOST = "127.0.0.1"
PORT = 6969

sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sock.connect((HOST, PORT))

lat = 47.39807052349714
lon = 8.54562064372404
alt = 10.0
heading = 0.0

command = {
    "command": "goto",
    "lat": lat,
    "lon": lon,
    "alt": alt,
    "heading": heading
    }
raw_command = bytes(json.dumps(command), 'utf-8')
sock.sendall(raw_command)

print(sock.recv(256))

sock.close()

