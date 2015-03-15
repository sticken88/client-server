/*
  Author: Matteo Sticco
*/

#include <stdio.h>
#include <stdlib.h> // getenv()
#include <sys/types.h>
#include <inttypes.h>
#include <sys/socket.h>
#include <sys/time.h> // timeval
#include <sys/select.h>
#include <sys/wait.h>
#include <sys/stat.h> // aggiunta per usare stat()
#include <netinet/in.h>
#include <arpa/inet.h> // inet_aton()
#include <netdb.h>
#include <errno.h>
#include <unistd.h>
#include <stddef.h>
#include <string.h>
#include <stdarg.h>
#include <dirent.h>
#include <limits.h>
#include <fcntl.h> 
#include <stdint.h>
#include <signal.h>

#include "./socket/udp_socket.h"
#include "./list/linked_list.h"
#include "./log/log_manager.h"
#include "./fd/fd_manager.h"

#define BUFLEN 1000
#define MAX_DIM 1200
#define LENGTH 100
#define MAX_CHAR 200
#define CHOICE 5
#define TIMEOUT 30
#define MAX_TIMEOUT 300

extern int errno;
//void close_and_quit(int fd, int s, richiesta_ptr *head);

int main(int argc, char **argv){

   setbuf(stdout,NULL);  // per non bufferizzare

	int s;

	struct sockaddr_in father_srv,child_srv; /* struttura usata per connettersi con il server*/
        struct addrinfo hints, *results;
        char buf_tmp[BUFLEN];
        char buf_send[MAX_DIM];
	char buf_recv[BUFLEN];
        char log_name[50], _choice[50], _file_name[50];
        char recv_file[MAX_CHAR];
        char send_ack[MAX_CHAR];
        char recv_data[BUFLEN];
    
        int fdR,fdW;
	int empty,ret,check_file,dim2,flag,cont,check_port; /* sarà riempito dal server*/
	int size,dim,recv_bytes,new_port,check,tentativi,prove; /* n byte letti da rcvfrom*/

        unsigned long int up_seq, down_seq, recv_seq; 

	socklen_t father_srvlen = sizeof(father_srv);
        socklen_t child_srvlen = sizeof(child_srv);
	struct timeval timeout;
        struct timeval max_timeout;
	fd_set u_cset,d_cset;

        if(argc!=3){
           printf("Error while passing initial parameters: IP address and port needed, closing..\n");
           exit(1);
        }

        fd_manager *fd = new fd_manager();   
 
        log_manager *log = new log_manager();
        log->check_log(); // check whether there are log files to remove

        check_port = atoi(argv[2]);

        if((check_port<0)||((check_port>=0)&&(check_port<=1023))){
           printf("Errore: numero di porta non consentito (deve essere maggiore di 1023).\n");
           exit(1);
        }

        char *choice, *file_name, *ext_choice, *ext_file_name, *new_choice, *new_file_name;

        choice = (char *)malloc(CHOICE * sizeof(char));
        if(choice == NULL)
        {
           printf("Error while allocating the 'choice' string, closing..\n");
           exit(1);
        }

        file_name = (char *)malloc(LENGTH * sizeof(char));
        if(file_name == NULL)
        {
           printf("Error while allocating the 'file_name' string, closing..\n");
           exit(1);
        }

        ext_choice = (char *)malloc(CHOICE * sizeof(char));
        if(ext_choice == NULL)
        {
           printf("Error while allocating the 'ext_choice' string, closing..\n");
           exit(1);
        }

        ext_file_name = (char *)malloc(LENGTH * sizeof(char));
        if(ext_file_name == NULL)
        {
           printf("Error while allocating the 'ext_file_name' string, closing..\n");
           exit(1);
        }

        new_choice = (char *)malloc(CHOICE * sizeof(char));
        if(new_choice == NULL)
        {
           printf("Error while allocating the 'new_choice' string, closing..\n");
           exit(1);
        }

        new_file_name = (char *)malloc(LENGTH * sizeof(char));
        if(new_file_name == NULL)
        {
           printf("Error while allocating the 'new_file_name' string, closing..\n");
           exit(1);
        }

        memset(&hints, 0, sizeof(hints));
        hints.ai_family = PF_INET;
        hints.ai_socktype = SOCK_DGRAM;
        ret = getaddrinfo(argv[1],argv[2],&hints,&results);

        udp_socket *connection = new udp_socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
        linked_list *list = new linked_list();

	father_srv.sin_family = AF_INET;
        father_srv.sin_addr = ((struct sockaddr_in*)(results->ai_addr))->sin_addr;
	father_srv.sin_port = htons(atoi(argv[2]));

        freeaddrinfo(results);

        FD_ZERO(&u_cset);
	FD_SET(fileno(stdin), &u_cset);
	FD_SET(connection->get_fd(), &u_cset);

        FD_ZERO(&d_cset);
	FD_SET(fileno(stdin), &d_cset);
	FD_SET(connection->get_fd(), &d_cset);

        printf("Cosa vuoi fare? UP per 'upload', DOWN per 'download', QUIT NOW per terminare un download corrente e ABORT ALL per terminare ogni operazione..\n");

        scanf("%s %s", choice, file_name);

        if((strcmp(choice,"ABORT"))==0){
           exit(1);
        }

        list->insert(choice, file_name);

  while(1){

        if(list->is_empty() == 0){ // list not empty, there are commands to process..
           list->extract(ext_choice, ext_file_name);
           printf("Correctly extracted\n");
        }else{ // Empty list           
           printf("\nEmpty command list, nothing to extract..\n");
           printf("Type 'UP' or 'DOWN' followed by the name of the file to transfer. Type 'ABORT ALL' to exit..\n");

           scanf("%s %s", choice, file_name);
           
           if((strcmp(choice,"ABORT"))==0){
              // If I got here, the list is empty for sure              
              exit(1);
           }else{
              list->insert(choice, file_name);
              list->extract(ext_choice, ext_file_name);
           }
        }

/*#########################################################################################
                     UPLOAD DEL FILE
############################################################################################*/

        if(strcmp(ext_choice,"UP")==0){  // aggiungere codice timeout, perdita pacchetti ecc.
           check_file=access(ext_file_name,F_OK);

           if(check_file==0){ /* File exists */
           up_seq=0;
           flag=1;

           FD_ZERO(&u_cset);
	   FD_SET(fileno(stdin), &u_cset);
	   FD_SET(connection->get_fd(), &u_cset);

	   memset(buf_send,0,sizeof(buf_send));
           sprintf(buf_send,"%s %s", ext_choice, ext_file_name);
           size=strlen(buf_send);
           buf_send[size]='\0';

           connection->send_data(buf_send,size,0,(struct sockaddr *)&father_srv,father_srvlen);

           memset(buf_recv,0,sizeof(buf_recv));

           max_timeout.tv_sec=MAX_TIMEOUT;
	   max_timeout.tv_usec=0;

           if(select(FD_SETSIZE,&u_cset,NULL,NULL,&max_timeout)>0){ // wait for 5 minutes max
           max_timeout.tv_sec=MAX_TIMEOUT;
	   max_timeout.tv_usec=0;

           FD_ZERO(&u_cset);
	   FD_SET(fileno(stdin), &u_cset);
	   FD_SET(connection->get_fd(), &u_cset);

           connection->recv_data(buf_recv,BUFLEN,0,(struct sockaddr*)&father_srv,&father_srvlen);           
           new_port=atoi(buf_recv);

	   child_srv.sin_family = AF_INET; // address family IPv4
           child_srv.sin_addr = father_srv.sin_addr;
           child_srv.sin_port = htons(new_port);

           memset(buf_send,0,sizeof(buf_send));
           sprintf(buf_send,"POSSO ?");
           dim2=strlen(buf_send);
           buf_send[dim2]='\0';


           connection->send_data(buf_send,size,0,(struct sockaddr *)&child_srv,child_srvlen);
           memset(buf_send,0,sizeof(buf_send));

           if(select(FD_SETSIZE,&u_cset,NULL,NULL,&max_timeout)>0){
              FD_ZERO(&u_cset);
	      FD_SET(fileno(stdin), &u_cset);
	      FD_SET(connection->get_fd(), &u_cset);


              int _dim = connection->recv_data(buf_recv,BUFLEN,0,(struct sockaddr*)&child_srv,&child_srvlen);
              buf_recv[_dim] = '\0';
              if((strcmp(buf_recv,"GO"))==0){
                 flag=1; /* while enabled */
                 fd->open_read(&fdR, ext_file_name);
                 printf("File '%s' has been opened in reading mode..\n", ext_file_name);
              }

              if((strcmp(buf_recv,"STOP"))==0){
                 flag=0; /* while not enabled */
                 printf("Server is busy, try again later..\n");
              }

           }else{
             printf("No answer from the server after 5 minutes, closing..\n");
             list->free_list(); // free the list
             exit(1);
           }

           memset(buf_send,0,sizeof(buf_send));
           memset(buf_tmp,0,sizeof(buf_tmp));

	   while(((size=read(fdR, buf_tmp, BUFLEN))>0) && (flag==1)){  
          
              sprintf(buf_send,"%s %lu %d ", ext_file_name, up_seq, size);
              dim=strlen(buf_send);
              memcpy(buf_send+dim,buf_tmp, size); // it will contain the name of th file, sequence number and "size" read byte

              connection->send_data(buf_send,(dim+size),0,(struct sockaddr *)&child_srv,child_srvlen);
              printf("Data sent..\n");

              // check sequence number until the sequence numbers are not equal
              tentativi=0;
              prove=0;

              do{
                 timeout.tv_sec=TIMEOUT;
	         timeout.tv_usec=0;

                 FD_ZERO(&u_cset);
	         FD_SET(fileno(stdin), &u_cset);
	         FD_SET(connection->get_fd(), &u_cset);

                 if(select(FD_SETSIZE,&u_cset,NULL,NULL,&timeout)>0){

                  if(FD_ISSET(fileno(stdin),&u_cset)){

                     scanf("%s %s", choice, file_name);  

                     if((strcmp(choice,"ABORT"))==0){

                        up_seq=-2;
                        size=0; // put it in the packet so that the server can read properly

                        sprintf(buf_send,"%s %lu %d ",ext_file_name,up_seq,size);
                        dim=strlen(buf_send);
                        buf_send[dim]='\0';
                        
                        connection->send_data(buf_send,dim,0,(struct sockaddr *)&child_srv,child_srvlen);
                     }

                     if((strcmp(choice,"QUIT"))==0){

                        up_seq=-1;
                        size=0; // put it in the packet so that the server can read properly

                        sprintf(buf_send,"%s %lu %d ", ext_file_name, up_seq, size);
                        dim=strlen(buf_send);
                        buf_send[dim]='\0';

			                  connection->send_data(buf_send,dim,0,(struct sockaddr *)&child_srv,child_srvlen);

                        connection->recv_data(buf_recv,BUFLEN,0,(struct sockaddr*) &child_srv,&child_srvlen);

                        fd->close_file(&fdR); // close fd but I still can do other things
                        break;
                     }

                     list->insert(choice, file_name);                     
                     FD_SET(connection->get_fd(),&u_cset);
                     
                  }

		  if(FD_ISSET(connection->get_fd(), &u_cset)){

                    memset(buf_recv,0,sizeof(buf_recv));
                    dim2 = connection->recv_data(buf_recv,BUFLEN,0,(struct sockaddr*) &child_srv,&child_srvlen);
                    buf_recv[dim2]='\0';

                    sscanf(buf_recv,"%lu",&recv_seq);
                       if(up_seq!=recv_seq){
                         printf("\n corrente: %lu - ricevuto: %lu",up_seq,recv_seq);

                         connection->send_data(buf_send,(dim+size),0,(struct sockaddr *)&child_srv,child_srvlen);
                         prove++; 

                         if(prove==3){
                            break;
                         }
                       }
                      FD_SET(fileno(stdin),&u_cset);
                     }
                    }else{ // timeout expired without receiving reply: resend the packet
                       printf("\nTimeout has expired: send again the packet with ID %lu. Tentativo [%d]\n",up_seq,tentativi+1);

                       connection->send_data(buf_send,(dim+size),0,(struct sockaddr *)&child_srv,child_srvlen);

                       tentativi++;

                       if(tentativi==3){
                          break;
                       }
                    }
                    
              }while((tentativi<3 && up_seq!=recv_seq) || (prove<3 && up_seq!=recv_seq));

              if((tentativi==3) || (prove==3) || (up_seq==-1)){
                 break;
              }

	      if(size<BUFLEN){ // I read less than the dimension of the buffer then it is the last one
                 strcpy(buf_send,"EOF");
                 memset(buf_recv,0,sizeof(buf_recv));
                 size=strlen(buf_send);
		 buf_send[size]='\0';

                 connection->send_data(buf_send,size,0,(struct sockaddr *)&child_srv,child_srvlen);
              }
              
	      up_seq++;
 
              memset(buf_send,0,sizeof(buf_send));
              memset(buf_tmp,0,sizeof(buf_tmp));

           } // end loop used to send the file

           if((tentativi!=3)&&(prove!=3)&&(up_seq!=-1)&&(flag==1)){ // normal exit
              tentativi=0;
              prove=0;
              do{

                 FD_ZERO(&u_cset);
	         FD_SET(fileno(stdin), &u_cset);
	         FD_SET(connection->get_fd(), &u_cset);
                 
                 timeout.tv_sec=TIMEOUT;
	         timeout.tv_usec=0;

                 if(select(FD_SETSIZE,&u_cset,NULL,NULL,&timeout)>0){

                    connection->recv_data(buf_recv,BUFLEN,0,(struct sockaddr*) &child_srv,&child_srvlen);

              	    if(strcmp(buf_recv,"OK")==0){
                       printf("\nFile '%s' corrctly sent.\n", ext_file_name);
	               fd->close_file(&fdR);
                    }else{
                       printf("[%d]No positive answer from the server, trying again..\n",prove+1);

                       connection->send_data(buf_send,size,0,(struct sockaddr *)&child_srv,child_srvlen);

                       prove++;
                    }
                 }else{ // timeout expired, send again.
                    printf("[%d]Timeout over: sending again last packet..\n",tentativi+1);

                    connection->send_data(buf_send,size,0,(struct sockaddr *)&child_srv,child_srvlen);
                    tentativi++;
                 }
              }while(((strcmp(buf_recv,"OK")!=0) && (tentativi<3)) || ((strcmp(buf_recv,"OK")!=0) && (tentativi<3)));
           }

          }else{
             printf("No answer from the server after 5 minutes, closing..\n");
             list->free_list(); // free the list because the server will not reply to me
             exit(1);
          }

         }else{ // file not existing in client */
            printf("The requested file doesn't exist: check the name or try again later..\n");
         }

        }else{ // end upload if

/*#########################################################################################
                     DOWNLOAD DEL FILE
############################################################################################*/

        if(strcmp(ext_choice,"DOWN")==0){;

           memset(buf_recv,0,sizeof(buf_recv));
           memset(buf_send,0,sizeof(buf_send));

           check=0; // check to decide whether to abort or no

           sprintf(buf_send,"%s %s",ext_choice,ext_file_name);
           size=strlen(buf_send);
           buf_send[size]='\0';

           connection->send_data(buf_send,size,0,(struct sockaddr *)&father_srv,father_srvlen);

           printf("Sent '%s %s'.\n", ext_choice, ext_file_name);

           FD_ZERO(&d_cset);
	   FD_SET(fileno(stdin), &d_cset);
	   FD_SET(connection->get_fd(), &d_cset);

           memset(buf_recv,0,sizeof(buf_recv));

           max_timeout.tv_sec=MAX_TIMEOUT;
	         max_timeout.tv_usec=0;

           if(select(FD_SETSIZE,&d_cset,NULL,NULL,&max_timeout)>0){ // wait for 5 min

           FD_ZERO(&d_cset);
	   FD_SET(fileno(stdin), &d_cset);
	   FD_SET(connection->get_fd(), &d_cset);

           size = connection->recv_data(buf_recv,BUFLEN,0,(struct sockaddr*) &father_srv,&father_srvlen);

           buf_recv[size]='\0';
           printf("DEBUG DOWNLOAD SERVER: %s\n", buf_recv);
           if((strcmp(buf_recv,"OK"))==0){ // file exists on server

              down_seq=0;
              tentativi=0;
              prove=0;
              cont=0;

	      printf("File exists on the server..\n");

              max_timeout.tv_sec=MAX_TIMEOUT;
	      max_timeout.tv_usec=0;

              if(select(FD_SETSIZE,&d_cset,NULL,NULL,&max_timeout)>0){ // wait for max 5 min

              memset(buf_recv,0,sizeof(buf_recv));
 
              connection->recv_data(buf_recv,BUFLEN,0,(struct sockaddr*) &father_srv,&father_srvlen);
              printf("RECEIVED SOMETHING\n");
              new_port=atoi(buf_recv);

              child_srv.sin_family = AF_INET; // address family IPv4
              child_srv.sin_addr = father_srv.sin_addr;
              child_srv.sin_port = htons(new_port);

              memset(buf_send,0,sizeof(buf_send));
              sprintf(buf_send,"POSSO ?");
              dim2=strlen(buf_send);
              buf_send[dim2]='\0';

              connection->send_data(buf_send,size,0,(struct sockaddr *)&child_srv,child_srvlen);

              memset(buf_send,0,sizeof(buf_send));

           if(select(FD_SETSIZE,&u_cset,NULL,NULL,&max_timeout)>0){
            
              int _dim = connection->recv_data(buf_recv,BUFLEN,0,(struct sockaddr*)&child_srv,&child_srvlen);
              buf_recv[_dim] = '\0';
              if((strcmp(buf_recv,"GO"))==0){
                 cont=1; /* abilito il whil3 */
                 printf("File '%s' opened in writing mode..\n", ext_file_name);
                 printf("Riceiving file '%s'..\n", ext_file_name);
                 log->create_log(ext_file_name, log_name); // if file exists, creates the log
              }

              if((strcmp(buf_recv,"STOP"))==0){
                 cont=0; // block the while
                 printf("Server busy, try again later..\n");
              }

           }else{
             printf("No answer from the server after 5 minutes, closing..\n");
             list->free_list(); // free the list because the server won't reply
             exit(1);
           }

           if(cont != 0){ // if flag is not 0 I can transfer
          
              FD_ZERO(&d_cset);
	      FD_SET(fileno(stdin), &d_cset);
	      FD_SET(connection->get_fd(), &d_cset);

              fd->open_write(&fdW, ext_file_name);

               do{
		  check=0;

		  timeout.tv_sec=TIMEOUT;
		  timeout.tv_usec=0;
		  memset(buf_recv,0,sizeof(buf_recv));

		  sprintf(buf_send,"%d",BUFLEN);
		  size=strlen(buf_send);
                  buf_send[size]='\0';

                  connection->send_data(buf_send,size,0,(struct sockaddr *)&child_srv,child_srvlen);
 
		  if(select(FD_SETSIZE,&d_cset,NULL,NULL,&timeout)>0){ // if I receive data I check the sequence number

                     if(FD_ISSET(fileno(stdin),&d_cset)){
                        scanf("%s %s", new_choice, new_file_name);

                        if((strcmp(new_choice,"QUIT")!=0) && (strcmp(new_choice,"ABORT")!=0)){ // If not abort
                           
                           list->insert(new_choice, new_file_name);

                        }else{ // if QUIT, then remove the file and empty the list

                           if((strcmp(new_choice,"QUIT"))==0){
                              fd->remove_file(ext_file_name); // the name of the file being interrupted
                              log->remove_file(log_name);

                              sprintf(buf_send,"%d",-1);
                              size=strlen(buf_send);
                              buf_send[size]='\0';

                              connection->send_data(buf_send,size,0,(struct sockaddr *)&child_srv,child_srvlen);
                              connection->recv_data(buf_recv,BUFLEN,0,(struct sockaddr*) &child_srv,&child_srvlen);

                              memset(buf_send,0,sizeof(buf_send));
                              memset(buf_recv,0,sizeof(buf_recv));
                              memset(ext_file_name,0,sizeof(ext_file_name));
                              memset(ext_choice,0,sizeof(ext_choice));
                              memset(file_name,0,sizeof(file_name));
                              memset(choice,0,sizeof(choice));
                              fflush(stdin);
                              check=1;
                              break; // go to the next command in the list
                           }

                           if((strcmp(new_choice,"ABORT"))==0){

                              fd->remove_file(ext_file_name);
                              log->remove_file(log_name);

                              sprintf(buf_send,"%d", -2); // -1 means QUIT, -2 to ABORT
                              size=strlen(buf_send);
                              buf_send[size]='\0';

                              connection->send_data(buf_send,size,0,(struct sockaddr *)&child_srv,child_srvlen);

                              memset(buf_send,0,sizeof(buf_send));
                           }
                        }

                        FD_SET(connection->get_fd(), &d_cset); 
                     }

                     if(FD_ISSET(connection->get_fd(), &d_cset)){

                        memset(buf_recv,0,sizeof(buf_recv));
                        size = connection->recv_data(buf_recv,MAX_DIM,0,(struct sockaddr*)&child_srv,&child_srvlen);

		          //check sequence number and file and then I write if necessary
		        memset(recv_data,0,sizeof(recv_data));
		        sscanf(buf_recv,"%s %lu %d",recv_file,&recv_seq,&recv_bytes);
		        dim=size-recv_bytes;

		        memcpy(recv_data,buf_recv+dim,recv_bytes);

		        if((strcmp(recv_file,ext_file_name)==0) && (down_seq==recv_seq)){

		       	   if(write(fdW,recv_data,recv_bytes)!=recv_bytes){  
		              printf("Errore scrittura dati.\n");
		              exit(1);
		           }
				// send ACK
		           sprintf(send_ack,"%lu",down_seq);
		           dim=strlen(send_ack);
		           send_ack[dim]='\0';
                           connection->send_data(send_ack,dim,0,(struct sockaddr*)&child_srv,child_srvlen);

		           down_seq++;
			   memset(send_ack,0,sizeof(send_ack));

		    	}else{  // different sequence numbers: ask the right fragment
		           printf("[%d]Error! Expected seq. number : (%lu) - received (%lu)..\n",prove+1, down_seq,recv_seq);
                           printf("Expected file : %s -- received: %s", ext_file_name,recv_file);
                           memset(buf_send,0,sizeof(buf_send));
                           buf_send[0]='\0';
		           sprintf(buf_send,"%lu",down_seq);
		           dim=strlen(buf_send);
                           buf_send[dim]='\0';

                           connection->send_data(buf_send,dim,0,(struct sockaddr*)&child_srv,child_srvlen);

			   prove++;

                           if(prove==3){
                              printf("Received 3 wrong sequence numbers: deleting the file..\n");
                              check=1;
                              fd->close_file(&fdW);
                              log->remove_file(log_name); // remove file log
                              fd->remove_file(ext_file_name);
                              break;  // exit the loop used to receive the file and don't send the last packet
                           }
		        }

                        FD_SET(fileno(stdin),&d_cset);
                     }

		  }else{ // timeout expired
		     printf("[%d]Timeout expired - packet not received..\n",tentativi+1);

                     connection->send_data(buf_send,dim,0,(struct sockaddr*)&child_srv,child_srvlen);
                     tentativi++;
                   
                     if(tentativi==3){ // exit the loop, close fd, remove the log and incomplete file
                        printf("Received 3 wrong sequence number: deleting the file..\n");
                        check=1;
                        fd->close_file(&fdW);
                        log->remove_file(log_name); // remove log file
                        fd->remove_file(ext_file_name);
                        break;  // exit the receive file loop and don't send the final packet
                     }

               }while(recv_bytes == BUFLEN);  // continuing until eceived bytes are equal to requested bytes  

               if(check==0){

                  sprintf(buf_send,"%d",0);
                  dim=strlen(buf_send);
                  buf_send[dim]='\0';

                  connection->send_data(buf_send,dim,0,(struct sockaddr*)&child_srv,child_srvlen);

                  fd->close_file(&fdW);
                  log->remove_file(log_name); //remove the log file
                  printf("\nFile '%s' has been received correctly..\n",ext_file_name);
                  printf("Closing file descriptor..\n\n");
               }

              }

             }else{
               printf("No answer from the server after 5 minutes, closing..\n");
               list->free_list();
               exit(1);
             }
               
            }else{ // if the file doesn't exist on the server
               printf("File '%s' not stored on the server, try again later..\n", ext_file_name);
            }

          }else{
             printf("No answer from the server after 5 minutes, closing..\n");
             list->free_list();
             exit(1);
          }
	} // end if to process DOWN command
     } // end if to process UP command
  } // end while(1)	

return(0);
}
