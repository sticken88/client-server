#include "linked_list.h"

void linked_list::insert(char *choice, char *file_name){

   node *new_node;

   new_node = new node(choice, file_name);
   printf("Node created\n");
   if(head == NULL){
      head = new_node;
   }else{
      tail->set_next(new_node);
   }

   tail = new_node;
   printf("Inserito\n");
}

void linked_list::extract(char *choice, char *file_name){

     node *tmp;

     if(head != NULL)
     {
        strcpy(choice, head->get_choice());
        strcpy(file_name, head->get_file_name());

        tmp = head;
        head = head->get_next();
     }else
     {
        tail = NULL;
        printf("Empty list, no items available for extraction..\n");
     }
     free(tmp);
}

void linked_list::print_list(void)
{
   int count = 0;

   while(head != NULL)
   {
      printf("Item[%d]: %s %s\n", count, head->get_choice(), head->get_file_name());
      head = head->get_next();
      count++;
   }

   if(head == NULL)
   {
      printf("Empty list..\n");
   }
}

void linked_list::free_list(void)
{
   node *tmp;

   while(head != NULL)
   {
      tmp = head;
      head = head->get_next();
      free(tmp);
   }
}

int linked_list::is_empty(void)
{
   return(head == NULL); // if head is NULL returns 1
}

node *linked_list::get_head(void)
{
   return head;
}

node *linked_list::get_tail(void)
{
   return tail;
}

linked_list::linked_list(void)
{
   head = NULL;
   tail = NULL;
}

linked_list::~linked_list(void)
{
   free_list();
}
