#ifndef __LOG_MANAGER_H_
#define __LOG_MANAGER_H_

#include <stdio.h>
#include <stdlib.h> 
#include <sys/types.h>
#include <dirent.h>
#include <string.h>

#define LENGTH 100
#define LOG_EXT ".log"
#define CDIR "./"

class log_manager
{
   public:
      // log file management functions
      void create_log(char *file_name, char *log_name);
      void check_log(void);
      void remove_file(char *file_name);
      // constructor and distructor
      log_manager(void);
      ~log_manager();
   private:
      FILE *log_ptr;
      FILE *log;
      DIR *dirP;
      struct dirent *visit;
};

#endif
