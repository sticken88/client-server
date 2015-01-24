#include "fd_manager.h"

fd_manager::fd_manager(void)
{
}

void fd_manager::open_read(int *fd, char *file_name)
{
   *fd = open(file_name, READ_MODE);

   if((*fd) < 0)
   {
      printf("Error while opening file '%s' in read mode, closing..\n");
      exit(1);
   }
}

void fd_manager::open_write(int *fd, char *file_name)
{
   *fd = open(file_name, WRITE_MODE);

   if((*fd) < 0)
   {
      printf("Error while opening file '%s' in write mode, closing..\n");
      exit(1);
   }
}

void fd_manager::close_file(int *fd)
{
   if(close(*fd) < 0)
   {
      printf("Error while closing file '%s', closing..\n");
      exit(1);
   }
}

void fd_manager::remove_file(char *file_name){

   if((remove(file_name)) != 0)
   {
      printf("Error while trying to remove file '%s'..\n", file_name);
   }else
   {
      printf("File '%s' correctly removed..\n", file_name);
   }
}
