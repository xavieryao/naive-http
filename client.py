import socket
import time
hostname = 'localhost'
port = 1234

# TEST: simple TCP socket
# create an INET, STREAMing socket
s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
# now connect to the naive HTTP file server
s.connect((hostname, port))
time.sleep(1)
s.close()


def send(msg):
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    # now connect to the naive HTTP file server
    s.connect((hostname, port))
    
    msg = msg.replace('\n', '\r\n')
    msg += '\r\n'
    
    data = msg.encode()
    
    assert('\r\n\r\n' in msg)
    print(len(data))
    
    s.sendall(msg.encode())
    
    time.sleep(1)
    s.close()


send("""GET /a.txt HTTP/1.1
""")

send("""POST /b.txt HTTP/1.1
Content-Length: 1000

""" + 'b'*1000)

