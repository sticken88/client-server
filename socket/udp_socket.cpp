#include "udp_socket.h"

ssize_t udp_socket::recv_data(void *bufptr, size_t nbytes, int flags, struct sockaddr *sa, socklen_t *salenptr)
{
   ssize_t bytes_received;

   bytes_received = recvfrom(fd, bufptr, nbytes, flags, sa, salenptr);

   if(bytes_received < 0){
      printf("[Error]: '%s' occurred while receiving data, closing..\n",  strerror(errno));
      exit(1);
   }

   return bytes_received;
}

void udp_socket::send_data(void *bufptr, size_t nbytes, int flags, const struct sockaddr *sa, socklen_t salen)
{
   size_t bytes_sent;

   bytes_sent = sendto(fd, bufptr, nbytes, flags, sa, salen);

   if(bytes_sent != nbytes){
      printf("[Error]: '%s' occurred while sending data, closing..\n", strerror(errno));
      exit(1);
   }
}

int udp_socket::get_fd(void)
{
   return fd;
}

udp_socket::udp_socket(int family, int type, int protocol)
{
   fd = socket(family, type, protocol);
   shut_mode = 2;

   if(fd < 0){
      printf("Error while creating socket, closing..\n");
      exit(1);
   }
}

udp_socket::~udp_socket()
{
   shutdown(fd, shut_mode);
   close(fd);
}
