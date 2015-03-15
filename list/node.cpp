#include "node.h"

char *node::get_choice(void){
   return choice;
}

char *node::get_file_name(void){
   return file_name;
}

node *node::get_next(void){
   return next;
}

void node::set_next(node *next_node){
   this->next = next_node;
}

node::node(char *choice, char *file_name){

   this->choice = (char *)malloc(CHOICE * sizeof(char));
   if(this->choice == NULL){
      printf("Error while allocating memory for 'choice', closing..\n");
      exit(1);
   }

   strcpy(this->choice, choice);

   this->file_name = (char *)malloc(LENGTH * sizeof(char));
   if(this->file_name == NULL){
      printf("Error while allocating memory for 'choice', closing..\n");
      exit(1);
   }

   strcpy(this->file_name, file_name);
   this->next = NULL;
}

node::~node(){
   free(choice);
   free(file_name);
}
