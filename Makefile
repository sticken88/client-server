compile:
	g++ client.cpp udp_socket.cpp linked_list.cpp node.cpp -o client

clean:
	rm client
	rm *~
