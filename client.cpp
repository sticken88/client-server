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
        //char choice[CHOICE];
        //char nome[LENGTH];
        //char new_choice[CHOICE];
        //char new_nome[LENGTH];
        char log_name[50];
        char recv_file[MAX_CHAR];
        char send_ack[MAX_CHAR];
        char recv_data[BUFLEN];
        //char ext_choice[CHOICE],ext_nome[LENGTH];
        int fdR,fdW;
	int empty,ret,check_file,dim2,flag,cont,check_port; /* sarà riempito dal server*/
	int size,dim,recv_bytes,new_port,check,tentativi,prove; /* n byte letti da rcvfrom*/

        unsigned long int up_seq, down_seq, recv_seq; 

	socklen_t father_srvlen = sizeof(father_srv);
        socklen_t child_srvlen = sizeof(child_srv);
	struct timeval timeout;
        struct timeval max_timeout;
	fd_set u_cset,d_cset;

        //richiesta_ptr tail,head;

	//tail=head=NULL;

        if(argc!=3){
           printf("Error while passing initial parameters: IP address and port needed, closing..\n");
           exit(1);
        }

        fd_manager *fd = new fd_manager();   
 
        log_manager *log = new log_manager();
        log->check_log(); // check whether there are log files to remove

        check_port=atoi(argv[2]);

        if((check_port<0)||((check_port>=0)&&(check_port<=1023))){
           printf("Errore: numero di porta non consentito (deve essere maggiore di 1023).\n");
           exit(1);
        }

      /*  val = inet_aton(argv[1], &addr);  // inet_aton restiuisce l'indirizzo in "addr"*/
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
        ret=getaddrinfo(argv[1],argv[2],&hints,&results);

	//s=my_socket(PF_INET,SOCK_DGRAM,IPPROTO_UDP);

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
           ////my_close(s);
           exit(1);
        }

        //inserisci(&head,&tail,choice,nome);
        list->insert(choice, file_name);

  while(1){

        //empty=vuota(&head);
       // printf("VALORE DI EMPTY = %d\n",empty);
       // sleep(5);
        if(list->is_empty() == 0){ // lista non vuota, posso estrarre
           list->extract(ext_choice, ext_file_name);
           //print_list(&head);
        }else{ // lista vuota, non estraggo
           //print_list(&head);
           printf("\nEmpty command list, nothing to extract..\n");
           printf("Type 'UP' or 'DOWN' followed by the name of the file to transfer. Type 'ABORT ALL' to exit..\n");

           scanf("%s %s", choice, file_name);
           
           if((strcmp(choice,"ABORT"))==0){
              /* se entro qui la lista è vuota, dunque non devo fae nessuna free*/
              ////my_close(s); /* chiusura socket */
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

           if(check_file==0){ /* il file è presente */

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
           //my_sendto(s,buf_send,size,0,(struct sockaddr *)&father_srv,father_srvlen);

           printf("UP command sent..\n");

           memset(buf_recv,0,sizeof(buf_recv));

           max_timeout.tv_sec=MAX_TIMEOUT;
	   max_timeout.tv_usec=0;

           if(select(FD_SETSIZE,&u_cset,NULL,NULL,&max_timeout)>0){ /* attendo al massimo 5 minuti la risposta del server */

           max_timeout.tv_sec=MAX_TIMEOUT;
	   max_timeout.tv_usec=0;

           FD_ZERO(&u_cset);
	   FD_SET(fileno(stdin), &u_cset);
	   FD_SET(connection->get_fd(), &u_cset);

           connection->recv_data(buf_recv,BUFLEN,0,(struct sockaddr*)&father_srv,&father_srvlen);
           //my_recvfrom(s,buf_recv,BUFLEN,0,(struct sockaddr*)&father_srv,&father_srvlen); // ricezione nuovo numero di porta con cui connettersi con il figlio
           new_port=atoi(buf_recv);

	   child_srv.sin_family = AF_INET; // address family IPv4
           child_srv.sin_addr = father_srv.sin_addr;
           child_srv.sin_port = htons(new_port);

           memset(buf_send,0,sizeof(buf_send));
           sprintf(buf_send,"POSSO ?");
           dim2=strlen(buf_send);
           buf_send[dim2]='\0';


           connection->send_data(buf_send,size,0,(struct sockaddr *)&child_srv,child_srvlen);
           //my_sendto(s,buf_send,size,0,(struct sockaddr *)&child_srv,child_srvlen); /* usata solo per far conoscere al figlio l'identità del client */
           memset(buf_send,0,sizeof(buf_send));

           if(select(FD_SETSIZE,&u_cset,NULL,NULL,&max_timeout)>0){

              FD_ZERO(&u_cset);
	      FD_SET(fileno(stdin), &u_cset);
	      FD_SET(connection->get_fd(), &u_cset);


              connection->recv_data(buf_recv,BUFLEN,0,(struct sockaddr*)&child_srv,&child_srvlen);
              //my_recvfrom(s,buf_recv,BUFLEN,0,(struct sockaddr*)&child_srv,&child_srvlen);

              if((strcmp(buf_recv,"GO"))==0){
                 flag=1; /* abilito il whil3 */
                 //fdR=my_openR(fdR, ext_file_name);
                 fd->open_read(&fdR, ext_file_name);
                 printf("File '%s' has been opened in reading mode..\n", ext_file_name);
              }

              if((strcmp(buf_recv,"STOP"))==0){
                 flag=0; /* blocco il while*/
                 printf("Server is busy, try again later..\n");
              }

           }else{
             printf("No answer from the server after 5 minutes, closing..\n");
             list->free_list(); /* libero la lista allocata perchè il server non mi servirà */
             //my_close(s);
             exit(1);
           }

           memset(buf_send,0,sizeof(buf_send));
           memset(buf_tmp,0,sizeof(buf_tmp));

	   while(((size=read(fdR, buf_tmp, BUFLEN))>0) && (flag==1)){  
          
              sprintf(buf_send,"%s %lu %d ",ext_file_name, up_seq, size);
              dim=strlen(buf_send);
              memcpy(buf_send+dim,buf_tmp, size); // a questo punto in buf_send ho correttamente il nome del file, il num seq e "size" byte letti/ da inviare
          //    printf("Letto il frammento numero %d.\r\n",up_seq);
          

              connection->send_data(buf_send,(dim+size),0,(struct sockaddr *)&child_srv,child_srvlen);
	      //my_sendto(s,buf_send,(dim+size),0,(struct sockaddr *)&child_srv,child_srvlen);

           /*   printf("Inviato il frammento numero %d, contenente %d byte.\r\n",up_seq,size);*/
              // controllo sul numero di sequenza e timeout finchè i numeri di sequenza non coincidono
              tentativi=0;
              prove=0;

              do{
                 timeout.tv_sec=TIMEOUT;
	         timeout.tv_usec=0;

                 FD_ZERO(&u_cset);
	         FD_SET(fileno(stdin), &u_cset);
	         FD_SET(connection->get_fd(), &u_cset);

                 if(select(FD_SETSIZE,&u_cset,NULL,NULL,&timeout)>0){ // se non scatta il timeout ma ricevo dati, ne controllo il numero di sequenza

                  if(FD_ISSET(fileno(stdin),&u_cset)){

                     scanf("%s %s", choice, file_name);  
                     /*printf("(interfaccia attiva) Scelta = %s --- Nome = %s",choice,nome);*/

                     if((strcmp(choice,"ABORT"))==0){

                        up_seq=-2;
                        size=0; /* da mettere nel pacchetto affinchè la lettura da parte del server sia corretta*/

                        sprintf(buf_send,"%s %lu %d ",ext_file_name,up_seq,size);
                        dim=strlen(buf_send);
                        buf_send[dim]='\0';
                        
                        connection->send_data(buf_send,dim,0,(struct sockaddr *)&child_srv,child_srvlen);
                        //my_sendto(s,buf_send,dim,0,(struct sockaddr *)&child_srv,child_srvlen);

                        // the following function must be checked and replaced if needed
                        //close_and_quit(fdR, connection->get_fd(), &head);
                     }

                     if((strcmp(choice,"QUIT"))==0){

                        up_seq=-1;
                        size=0; /* da mettere nel pacchetto affinchè la lettura da parte del server sia corretta*/

                        sprintf(buf_send,"%s %lu %d ", ext_file_name, up_seq, size);
                        dim=strlen(buf_send);
                        buf_send[dim]='\0';

			connection->send_data(buf_send,dim,0,(struct sockaddr *)&child_srv,child_srvlen);
                        //my_sendto(s,buf_send,dim,0,(struct sockaddr *)&child_srv,child_srvlen);

                        connection->recv_data(buf_recv,BUFLEN,0,(struct sockaddr*) &child_srv,&child_srvlen);
                        //my_recvfrom(s,buf_recv,BUFLEN,0,(struct sockaddr*) &child_srv,&child_srvlen); /* ricevo ack e non lo controllo */

                        fd->close_file(&fdR);/* chiudo soltanto il file descriptor e posso ancora fare altre operazioni: non libero lista e non chiudo socket*/
                        break;
                     }

                     list->insert(choice, file_name);
                     //inserisci(&head,&tail,choice,nome);      // inserimento in lista
                     FD_SET(connection->get_fd(),&u_cset);
                     
                  }

		  if(FD_ISSET(connection->get_fd(), &u_cset)){

                    memset(buf_recv,0,sizeof(buf_recv));
                    //dim2 = my_recvfrom(s,buf_recv,BUFLEN,0,(struct sockaddr*) &child_srv,&child_srvlen); // controllo il numero di sequenza
                    dim2 = connection->recv_data(buf_recv,BUFLEN,0,(struct sockaddr*) &child_srv,&child_srvlen);
                    buf_recv[dim2]='\0';

                    sscanf(buf_recv,"%lu",&recv_seq);
                    /*printf("Ricevuto ack %"PRId32"\n",recv_seq);*/
                       if(up_seq!=recv_seq){
                         printf("\n corrente: %lu - ricevuto: %lu",up_seq,recv_seq);

                         //my_sendto(s,buf_send,(dim+size),0,(struct sockaddr *)&child_srv,child_srvlen);
                         connection->send_data(buf_send,(dim+size),0,(struct sockaddr *)&child_srv,child_srvlen);
                         prove++; 

                         if(prove==3){
                            break;
                         }
                       }
                      FD_SET(fileno(stdin),&u_cset);
                     }
                    }else{ // se scatta il timeout senza aver ricevuto risposta reinvio il pacchetto
                       printf("\nTimeout has expired: send again the packet with ID %lu. Tentativo [%d]\n",up_seq,tentativi+1);

                       //my_sendto(s,buf_send,(dim+size),0,(struct sockaddr *)&child_srv,child_srvlen);
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

	      if(size<BUFLEN){ // se leggo meno della dim del buffer allora è l'ultimo pacchetto
                 strcpy(buf_send,"EOF");
                 memset(buf_recv,0,sizeof(buf_recv));
                 size=strlen(buf_send);
		 buf_send[size]='\0';

                 //my_sendto(s,buf_send,size,0,(struct sockaddr *)&child_srv,child_srvlen);
                 connection->send_data(buf_send,size,0,(struct sockaddr *)&child_srv,child_srvlen);
              }
              
	      up_seq++;
 
              memset(buf_send,0,sizeof(buf_send));
              memset(buf_tmp,0,sizeof(buf_tmp));

           } // fine ciclo invio del file

           if((tentativi!=3)&&(prove!=3)&&(up_seq!=-1)&&(flag==1)){ /* sono uscito normalmente */
              tentativi=0; /* riutilizzo le variabili */
              prove=0;
              do{

                 FD_ZERO(&u_cset);
	         FD_SET(fileno(stdin), &u_cset);
	         FD_SET(connection->get_fd(), &u_cset);
                 
                 timeout.tv_sec=TIMEOUT;
	         timeout.tv_usec=0;

                 if(select(FD_SETSIZE,&u_cset,NULL,NULL,&timeout)>0){

                    //my_recvfrom(s,buf_recv,BUFLEN,0,(struct sockaddr*) &child_srv,&child_srvlen);
                    connection->recv_data(buf_recv,BUFLEN,0,(struct sockaddr*) &child_srv,&child_srvlen);

              	    if(strcmp(buf_recv,"OK")==0){
                       printf("\nFile '%s' corrctly sent.\n", ext_file_name);
	               fd->close_file(&fdR);
                    }else{
                       printf("[%d]No positive answer from the server, trying again..\n",prove+1);

                       //my_sendto(s,buf_send,size,0,(struct sockaddr *)&child_srv,child_srvlen);
                       connection->send_data(buf_send,size,0,(struct sockaddr *)&child_srv,child_srvlen);

                       prove++;
                    }
                 }else{ // timeout scaduto, invio di nuovo..
                    printf("[%d]Timeout over: sending again last packet..\n",tentativi+1);

                    //my_sendto(s,buf_send,size,0,(struct sockaddr *)&child_srv,child_srvlen);
                    connection->send_data(buf_send,size,0,(struct sockaddr *)&child_srv,child_srvlen);
                    tentativi++;
                 }
              }while(((strcmp(buf_recv,"OK")!=0) && (tentativi<3)) || ((strcmp(buf_recv,"OK")!=0) && (tentativi<3)));
           }

	  // FD_SET(fileno(stdin), &u_cset);

          }else{
             printf("No answer from the server after 5 minutes, closing..\n");
             list->free_list(); /* libero la lista allocata perchè il server non mi servirà */
             //my_close(s);
             exit(1);
          }

         }else{ /* file assente nel client */
            printf("The requested file doesn't exist: check the name or try again later..\n");
         }

        }else{ // fine if per l'UPLOAD

/*#########################################################################################
                     DOWNLOAD DEL FILE
############################################################################################*/

        if(strcmp(ext_choice,"DOWN")==0){;

           memset(buf_recv,0,sizeof(buf_recv));
           memset(buf_send,0,sizeof(buf_send));

           check=0; /* usato per controllare se devo fare abort o no*/

           sprintf(buf_send,"%s %s",ext_choice,ext_file_name);
           size=strlen(buf_send);
           buf_send[size]='\0';

           //my_sendto(s,buf_send,size,0,(struct sockaddr *)&father_srv,father_srvlen); // invio comando di DOWNLOAD
           connection->send_data(buf_send,size,0,(struct sockaddr *)&father_srv,father_srvlen);


           printf("Sent '%s %s'.\n", ext_choice, ext_file_name);

           FD_ZERO(&d_cset);
	   FD_SET(fileno(stdin), &d_cset);
	   FD_SET(connection->get_fd(), &d_cset);

           memset(buf_recv,0,sizeof(buf_recv));

           max_timeout.tv_sec=MAX_TIMEOUT;
	   max_timeout.tv_usec=0;

           if(select(FD_SETSIZE,&d_cset,NULL,NULL,&max_timeout)>0){ /* attendo al massimo 5 minuti la risposta del server */

           FD_ZERO(&d_cset);
	   FD_SET(fileno(stdin), &d_cset);
	   FD_SET(connection->get_fd(), &d_cset);

           //size=my_recvfrom(s,buf_recv,BUFLEN,0,(struct sockaddr*) &father_srv,&father_srvlen); /* ricevo conferma o meno dal server*/
           size = connection->recv_data(buf_recv,BUFLEN,0,(struct sockaddr*) &father_srv,&father_srvlen);


           buf_recv[size]='\0';

           if((strcmp(buf_recv,"OK"))==0){ /* file presente sul server*/

              down_seq=0;
              tentativi=0;
              prove=0;
              cont=0;

	      printf("File doesn't exist on the server..\n");

              max_timeout.tv_sec=MAX_TIMEOUT;
	      max_timeout.tv_usec=0;

              if(select(FD_SETSIZE,&d_cset,NULL,NULL,&max_timeout)>0){ /* attendo al massimo 5 minuti la risposta del server */

              memset(buf_recv,0,sizeof(buf_recv));

              //my_recvfrom(s,buf_recv,BUFLEN,0,(struct sockaddr*) &father_srv,&father_srvlen); // ricezione nuovo numero di porta con cui connettersi con il figlio
 
              connection->recv_data(buf_recv,BUFLEN,0,(struct sockaddr*) &father_srv,&father_srvlen);

              new_port=atoi(buf_recv);

              child_srv.sin_family = AF_INET; // address family IPv4
              child_srv.sin_addr = father_srv.sin_addr;
              child_srv.sin_port = htons(new_port);

              memset(buf_send,0,sizeof(buf_send));
              sprintf(buf_send,"POSSO ?");
              dim2=strlen(buf_send);
              buf_send[dim2]='\0';

              //my_sendto(s,buf_send,size,0,(struct sockaddr *)&child_srv,child_srvlen); /* usata solo per far conoscere al figlio l'identità del client */

              connection->send_data(buf_send,size,0,(struct sockaddr *)&child_srv,child_srvlen);

              memset(buf_send,0,sizeof(buf_send));

           if(select(FD_SETSIZE,&u_cset,NULL,NULL,&max_timeout)>0){

              //my_recvfrom(s,buf_recv,BUFLEN,0,(struct sockaddr*)&child_srv,&child_srvlen);
              connection->recv_data(buf_recv,BUFLEN,0,(struct sockaddr*)&child_srv,&child_srvlen);

              if((strcmp(buf_recv,"GO"))==0){
                 cont=1; /* abilito il whil3 */
                 printf("File '%s' opened in writing mode..\n", ext_file_name);
                 printf("Riceiving file '%s'..\n", ext_file_name);
                 log->create_log(ext_file_name, log_name); /* se il file è presente creo il log*/
              }

              if((strcmp(buf_recv,"STOP"))==0){
                 cont=0; /* blocco il while*/
                 printf("Server busy, try again later..\n");
              }

           }else{
             printf("No answer from the server after 5 minutes, closing..\n");
             list->free_list(); /* libero la lista allocata perchè il server non mi servirà */
             //my_close(s);
             exit(1);
           }

           if(cont!=0){ /* se flag è diverso da 0, posso trasferire */
          
              FD_ZERO(&d_cset);
	      FD_SET(fileno(stdin), &d_cset);
	      FD_SET(connection->get_fd(), &d_cset);

              //fdW = my_openW(fdW, ext_file_name); // apertura file in scrittura
              fd->open_write(&fdW, ext_file_name);

               do{
		  check=0;

		  timeout.tv_sec=TIMEOUT;
		  timeout.tv_usec=0;
		  memset(buf_recv,0,sizeof(buf_recv));

		  sprintf(buf_send,"%d",BUFLEN);
		  size=strlen(buf_send);
                  buf_send[size]='\0';

		  //my_sendto(s,buf_send,size,0,(struct sockaddr *)&child_srv,child_srvlen); /* invio n° byte che voglio ricevere */
                  connection->send_data(buf_send,size,0,(struct sockaddr *)&child_srv,child_srvlen);
 
		  if(select(FD_SETSIZE,&d_cset,NULL,NULL,&timeout)>0){ // se non scatta il timeout ma ricevo dati, ne controllo il numero di sequenza

                     if(FD_ISSET(fileno(stdin),&d_cset)){
                        scanf("%s %s", new_choice, new_file_name);

                        if((strcmp(new_choice,"QUIT")!=0) && (strcmp(new_choice,"ABORT")!=0)){ // se non ho inserito il comando di terminazione operazioni
                             
                           /*printf("(interfaccia attiva) Scelta = %s --- Nome = %s\n",new_choice,new_nome);*/
                           //inserisci(&head,&tail,new_choice,new_nome);      // inserimento in lista
                           list->insert(new_choice, new_file_name);

                        }else{ // se è inserito QUIT elimino i file e svuoto la lista

                           if((strcmp(new_choice,"QUIT"))==0){
                             // my_close(fdW); /* non devo ricevere più, dunque chiudo il fd */
                              fd->remove_file(ext_file_name); // funzione che riceve il nome del file interrotto
                              log->remove_file(log_name);

                              sprintf(buf_send,"%d",-1);
                              size=strlen(buf_send);
                              buf_send[size]='\0';

                              //my_sendto(s,buf_send,size,0,(struct sockaddr *)&child_srv,child_srvlen);// devo inviare il comando al server
                              connection->send_data(buf_send,size,0,(struct sockaddr *)&child_srv,child_srvlen);

                              //my_recvfrom(s,buf_recv,BUFLEN,0,(struct sockaddr*) &child_srv,&child_srvlen); /* ricevo l'ultimo pacchetto che il server mi manda*/
                              connection->recv_data(buf_recv,BUFLEN,0,(struct sockaddr*) &child_srv,&child_srvlen);

                              memset(buf_send,0,sizeof(buf_send));
                              memset(buf_recv,0,sizeof(buf_recv));
                              memset(ext_file_name,0,sizeof(ext_file_name));
                              memset(ext_choice,0,sizeof(ext_choice));
                              memset(file_name,0,sizeof(file_name));
                              memset(choice,0,sizeof(choice));
                              fflush(stdin);
                              check=1;
                              break; // vado al prossimo trasferimento nella lista
                           }

                           if((strcmp(new_choice,"ABORT"))==0){

                              fd->remove_file(ext_file_name);
                              log->remove_file(log_name);

                              sprintf(buf_send,"%d", -2); /* -1 per QUIT, -2 per ABORT*/
                              size=strlen(buf_send);
                              buf_send[size]='\0';

                              //my_sendto(s,buf_send,size,0,(struct sockaddr *)&child_srv,child_srvlen);// devo inviare il comando al server
                              connection->send_data(buf_send,size,0,(struct sockaddr *)&child_srv,child_srvlen);

                              memset(buf_send,0,sizeof(buf_send));

                              // ragionare sul da farsi
                              //close_and_quit(fdW,s,&head);
                           }
                        }

                        FD_SET(connection->get_fd(), &d_cset); 
                     }

                     if(FD_ISSET(connection->get_fd(), &d_cset)){

                        memset(buf_recv,0,sizeof(buf_recv));

			//size = my_recvfrom(s,buf_recv,MAX_DIM,0,(struct sockaddr*)&child_srv,&child_srvlen);
                        size = connection->recv_data(buf_recv,MAX_DIM,0,(struct sockaddr*)&child_srv,&child_srvlen);

		          // se ricevo dati controllo numero di sequenza e file e poi scrivo se è il caso
		        memset(recv_data,0,sizeof(recv_data));
		        sscanf(buf_recv,"%s %lu %d",recv_file,&recv_seq,&recv_bytes);
		        dim=size-recv_bytes;

		        memcpy(recv_data,buf_recv+dim,recv_bytes);

//		        printf("Ricevuto frammento %d, atteso %d\n",recv_seq,down_seq);

		        if((strcmp(recv_file,ext_file_name)==0) && (down_seq==recv_seq)){

		       	   if(write(fdW,recv_data,recv_bytes)!=recv_bytes){  
		              printf("Errore scrittura dati.\n");
		              exit(1);
		           }
				// invio ACK
		           sprintf(send_ack,"%lu",down_seq);
		           dim=strlen(send_ack);
		           send_ack[dim]='\0';
                            // printf("ACK inviato = %s\n",send_ack);

		           //my_sendto(s,send_ack,dim,0,(struct sockaddr*)&child_srv,child_srvlen);
                           connection->send_data(send_ack,dim,0,(struct sockaddr*)&child_srv,child_srvlen);

		           down_seq++;
			   memset(send_ack,0,sizeof(send_ack));

		    	}else{  // numeri di sequenza diversi: richiedo il frammento giusto
		           /*sleep(1);*/
		           printf("[%d]Error! Expected seq. number : (%lu) - received (%lu)..\n",prove+1, down_seq,recv_seq);
                           printf("Expected file : %s -- received: %s", ext_file_name,recv_file);
                           memset(buf_send,0,sizeof(buf_send));
                           buf_send[0]='\0';
		           sprintf(buf_send,"%lu",down_seq);
		           dim=strlen(buf_send);
                           buf_send[dim]='\0';

		           //my_sendto(s,buf_send,dim,0,(struct sockaddr*)&child_srv,child_srvlen);
                           connection->send_data(buf_send,dim,0,(struct sockaddr*)&child_srv,child_srvlen);

			   prove++;

                           if(prove==3){
                              printf("Received 3 wrong sequence numbers: deleting the file..\n");
                              check=1;
                              fd->close_file(&fdW);
                              log->remove_file(log_name); /* elimino il file di log */
                              fd->remove_file(ext_file_name);
                              break;  /* forzo l'uscita dal ciclo di ricezione file e impedisco l'invio del paccketto finale*/
                           }
		        }

                        FD_SET(fileno(stdin),&d_cset);
                     }

		  }else{ // timeout scaduto
		     printf("[%d]Timeout expired - packet not received..\n",tentativi+1);

		     //my_sendto(s,buf_send,dim,0,(struct sockaddr*)&child_srv,child_srvlen);
                     connection->send_data(buf_send,dim,0,(struct sockaddr*)&child_srv,child_srvlen);
                     tentativi++;
                   
                     if(tentativi==3){ /* esco dal ciclo, chiudo fd, rimuovo log e file incompleto */
                        printf("Received 3 wrong sequence number: deleting the file..\n");
                        check=1;
                        fd->close_file(&fdW);
                        log->remove_file(log_name); /* elimino il file di log se tutto va bene */
                        fd->remove_file(ext_file_name);
                        break;  /* forzo l'uscita dal ciclo di ricezione file e impedisco l'invio del paccketto finale*/
                     }
		  }     

               }while(recv_bytes==BUFLEN);  // si continua finchè i byte ricevuti sono uguali a quelli richiesti    recv_bytes==BUFLEN 

               if(check==0){

                  sprintf(buf_send,"%d",0);
                  dim=strlen(buf_send);
                  buf_send[dim]='\0';

                  //my_sendto(s,buf_send,dim,0,(struct sockaddr*)&child_srv,child_srvlen); // invio 0 byte da leggere
                  connection->send_data(buf_send,dim,0,(struct sockaddr*)&child_srv,child_srvlen);

                  fd->close_file(&fdW);
                  log->remove_file(log_name); /* elimino il file di log se tutto va bene */
                  printf("\nFile '%s' has been received correctly..\n",ext_file_name);
                  printf("Closing file descriptor..\n\n");
               }

              } /* fine if sul valore di flag*/

             }else{
               printf("No answer from the server after 5 minutes, closing..\n");
               list->free_list();
               //my_close(s);
               exit(1);
             }
               
            }else{ /* else attivo se sul server non c'è il file richiesto*/
               printf("File '%s' not stored on the server, try again later..\n", ext_file_name);
               /*printf("BUF_RECV: %s\n\n",buf_recv);*/
            }

          }else{
             printf("No answer from the server after 5 minutes, closing..\n");
             list->free_list();
             //my_close(s);
             exit(1);
          }
	} /* fine if per DOWN*/
     } /* file else dell'UP*/
  } /* fine while*/	

      //my_close(s);

return(0);
}
