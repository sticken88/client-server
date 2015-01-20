#include "log_manager.h"

void log_manager::create_log(char *file_name, char *log_name)
{

   sprintf(log_name, "%s%s", file_name, LOG_EXT);

   log_ptr = fopen(log_name,"w+");
   if(log_ptr == NULL) // creating lig file
   { 
         printf("Error while opening log file '%s' in writing, closing..\n", log_name);
         exit(1);
   }

   fprintf(log_ptr, "%s", file_name); 
   fclose(log_ptr);
}

void log_manager::check_log(void){

   int dim, dim_name;
   char trash[LENGTH]; 

   dirP = opendir(CDIR);
   if(dirP == NULL)
   {
      printf("Error while opening the directory '%s'..\n", CDIR);
      exit(1);
   }

   while((visit = readdir(dirP)) != NULL){
      dim = strlen(visit->d_name); // complete log name
      dim_name = dim-4; // because ".log" has 4 char

      // if it has '.log' extension the I open it, read the content and delete the file
      if((strncmp((visit->d_name) + dim_name, ".log", 4)) == 0)
      {  

         if((log = fopen(visit->d_name,"r")) == NULL){
            printf("Error while opening log file '%s' in reading mode..\n", visit->d_name);
            exit(1);
         }

         fscanf(log, "%s", trash);

         if(fclose(log) != 0)
         {
            printf("Error while closing the file '%s'..\n", visit->d_name);
         }

         clear(trash);
         clear(visit->d_name);
      }
   }

   if(closedir(dirP) == -1)
   {
      printf("Error while closing the directory '%s'..\n", CDIR);
   }

}

void log_manager::clear(char *file_name){

   if(remove(file_name) == -1)
   {
      printf("Error while eliminating file '%s'..\n", file_name);
   }else
   {
      printf("File '%s' correctly removed..\n", file_name);
   }
}
