compile:
	g++ client.cpp socket/udp_socket.cpp list/linked_list.cpp list/node.cpp log/log_manager.cpp -o client

clean:
	rm client
	rm *~
