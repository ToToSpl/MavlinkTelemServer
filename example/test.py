#!/usr/bin/env python3

import socket
import json

HOST = "127.0.0.1"
PORT = 6969

sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sock.connect((HOST, PORT))
sock.sendall(b'get')
data = json.loads(sock.recv(2048))

sock.close()

print(data)
#print(data["position"]["lat"])