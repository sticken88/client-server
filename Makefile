compile:
	g++ client.cpp udp_socket.cpp -o client

clean:
	rm client
	rm *~
