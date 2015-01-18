#ifndef __UDP_SOCKET_H_
#define __UDP_SOCKET_H_

#include <stdio.h>
#include <stdlib.h> // getenv()
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h> // inet_aton()
#include <string.h>
#include <stdint.h>
#include <unistd.h>

class udp_socket
{
   public:
      // send and receive functions
      ssize_t recv_data(void *bufptr, size_t nbytes, int flags, struct sockaddr *sa, socklen_t *salenptr);
      void send_data(void *bufptr, size_t nbytes, int flags, const struct sockaddr *sa, socklen_t salen);
      int get_fd(void);
      // constructor and distructor
      udp_socket(int family,int type,int protocol);
      ~udp_socket();
   private:
      int fd;
      int shut_mode; // used to specify how to shut down the socket: 2 means both ways
};

#endif
