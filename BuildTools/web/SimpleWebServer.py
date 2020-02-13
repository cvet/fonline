import SimpleHTTPServer
import SocketServer

SERVER_PORT = 8000

handler = SimpleHTTPServer.SimpleHTTPRequestHandler
httpd = SocketServer.TCPServer(('', SERVER_PORT), handler)
print 'Now you can run game by typing in browser "localhost:' + str(SERVER_PORT) + '"'
httpd.serve_forever()
