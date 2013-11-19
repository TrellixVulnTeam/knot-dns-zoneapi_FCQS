#!/usr/bin/env python3

'''DNS packet header parsing tests. '''

import socket
from dnstest.test import Test

t = Test()
knot = t.server("knot")
zone = t.zone("example.com.")
t.link(zone, knot)

# Update configuration
knot.ratelimit = 5 # Check for crashes also in rate-limit code

t.start()

# Packet lengths shorter than DNS header
data = '\x00'
max_len = (12 + 5) # Header + minimal question size
udp_socket = knot.create_sock(socket.SOCK_DGRAM)
for i in range(1, max_len):
	knot.send_raw(data * i, udp_socket)
udp_socket.close()

# Check if the server is still alive 
resp = knot.dig("example.com", "SOA", timeout=5, tries=1)
resp.check(rcode="NOERROR")

t.end()
