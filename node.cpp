#include "node.h"

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

char *node::get_choice(void)
{
   return choice;
}

char *node::get_file_name(void)
{
   return file_name;
}

node::node(char *choice, char *file_name)
{
   this->choice = (char *)malloc(CHOICE * sizeof(char));
   if(this->choice)
   {
      printf("Error while allocating memory for 'choice', closing..\n");
      exit(1);
   }

   strcpy(this->file_name, file_name);

   this->file_name = (char *)malloc(LENGTH * sizeof(char));
   if(this->file_name)
   {
      printf("Error while allocating memory for 'choice', closing..\n");
      exit(1);
   }

   strcpy(this->file_name, file_name);
}

node::~node()
{
   free(choice);
   free(file_name);
}
