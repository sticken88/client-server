#ifndef __NODE_H_
#define __NODE_H_

#include <stdio.h>
#include <stdlib.h> // getenv()
#include <string.h>

#define LENGTH 100
#define CHOICE 5

class node
{
   public:
      // getters
      char *get_choice(void);
      char *get_file_name(void);
      // constructor and distructor
      node(char *choice, char *file_name);
      ~node();
   private:
      char *choice;
      char *file_name;
};

#endif
