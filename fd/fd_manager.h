#ifndef __FD_MANAGER_H_
#define __FD_MANAGER_H_

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h> 
#include <sys/stat.h> 
#include <fcntl.h> 

#define READ_MODE O_RDONLY
#define WRITE_MODE O_WRONLY | O_CREAT | O_TRUNC,S_IRUSR | S_IWUSR

class fd_manager
{
   public:
          void open_read(int *fd, char *file_name);
          void open_write(int *fd, char *file_name);
          void close_file(int *fd);
          void remove_file(char *file_name);
          // constructor
          fd_manager(void);
};

#endif
