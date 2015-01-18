#ifndef __LINKED_LIST_H_
#define __LINKED_LIST_H_

#include "node.h"

class linked_list
{
   public:
      // insert, remove, search etc. functions
      void insert(char *choice, char *file_name);
      void free_list(void);
      void print_list(void);
      void extract(char *choice, char *file_name);
      int is_empty(void);
      // getters and setters
      node *get_head(void);
      node *get_tail(void);
      // constructor and distructor
      linked_list(void);
      ~linked_list();
   private:
      node *head;
      node *tail;
};

#endif
