# DS19B20 Broadcaster

>In this example you can see how to get data from multiple 
>ds18b20 sensor and emit result over udb broadcaster address.

As a client server, you can use this simple udp receiver, writen in python:

```
import select, socket

s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
s.bind(('<broadcast>', 8005))
s.setblocking(0)

while True:
    result = select.select([s],[],[])
    msg = result[0][0].recv(1024)
    print msg.strip()

```