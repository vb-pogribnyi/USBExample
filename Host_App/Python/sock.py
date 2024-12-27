import socket
import struct
import os

import pygame
import numpy as np

pygame.init()
SCREEN_WIDTH = 320
SCREEN_HEIGHT = 240
NLMSG_DONE = 0x3
NLMSG_ENDIMG = 0x4

screen_buffer = np.zeros(SCREEN_WIDTH * SCREEN_HEIGHT * 3, np.uint8)
is_running = True

# Create GUI screen
screen = pygame.display.set_mode((SCREEN_WIDTH, SCREEN_HEIGHT))

# Create netlink socket. Send a message to the kernel
# so that it remembers the process to send image to.
sock = socket.socket(socket.AF_NETLINK, socket.SOCK_RAW, 31)
sock.bind((0, 0))
msg = "Hello from PYTHON"
nl_hdr = struct.pack("IHHII", 16 + len(msg), 0, 0, 0, os.getpid())
sock.send(nl_hdr + msg.encode())

buff_idx = 0
def nl_read_img():
	global buff_idx;
	msg = "IMG"
	nl_hdr = struct.pack("IHHII", 16 + len(msg), 0, 0, 0, os.getpid())
	sock.send(nl_hdr + msg.encode())
	while True:
		response = sock.recv(1024)
		nl_hdr = response[:16]
		length, msg_type, flags, seq, pid = struct.unpack("IHHII", nl_hdr)
		length -= 16  # Header does not count
		print(length, msg_type, flags, seq, pid)
		if msg_type == NLMSG_DONE or msg_type == NLMSG_ENDIMG:
			img_len = np.frombuffer(response[16:], dtype=np.int32)
			print(img_len)
			if msg_type == NLMSG_ENDIMG:
				buff_idx = 0;
			# if img_len > 0:
			# 	exit()
			break
		data = np.frombuffer(response[16:], dtype=np.uint8)
		# if len(data) > 0:
		# 	print(data)
		# 	print(data[:320])
		screen_buffer[buff_idx*3:buff_idx*3 + length*3] = data.repeat(3)
		buff_idx += length
		# if buff_idx*3 >= len(screen_buffer):
		# 	break

while is_running:
	print('.')
	nl_read_img()
	surf = pygame.surfarray.make_surface(screen_buffer.reshape((SCREEN_HEIGHT, SCREEN_WIDTH, 3)).transpose(1, 0, 2))
	for event in pygame.event.get():
		if event.type == pygame.QUIT:
			is_running = False
	screen.blit(surf, (0, 0))
	pygame.display.update()

pygame.quit()
