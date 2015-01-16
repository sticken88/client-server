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

#include "udp_socket.h"

#define SA struct sockaddr

#define BUFLEN 1000
#define MAX_DIM 1200
#define NAME 100
#define MAX_CHAR 200
#define CHOICE 5
#define TIMEOUT 30
#define MAX_TIMEOUT 300
#define LOG_EXT ".log"
#define CDIR "./"

struct nodo{
            char scelta[CHOICE];
            char nome_file[NAME];

            struct nodo *next;
};

typedef struct nodo richiesta;
typedef richiesta *richiesta_ptr;

extern int errno;

/* prototipi per la gestione delle operazioni */

int my_socket(int family,int type,int protocol);
void my_sendto(int fd, void *bufptr, size_t nbytes, int flags, const SA *sa, socklen_t salen);
ssize_t my_recvfrom (int fd, void *bufptr, size_t nbytes, int flags, SA *sa, socklen_t *salenptr);

int my_openR(int fd, char *nome);
int my_openW(int fd, char *nome);
void my_close(int fd);

void clear(char *nome_file);
void close_and_quit(int fd, int s, richiesta_ptr *head);

/* prototipi per gestire la lista di richieste */

richiesta_ptr nuova(char *scelta,char *nome);
void estrai(richiesta_ptr *tail,richiesta_ptr *head,char *scelta,char *nome);
void inserisci(richiesta_ptr *head,richiesta_ptr *tail,char *choice,char *nome);
int vuota(richiesta_ptr *head);
void print_list(richiesta_ptr *head);
void free_list(richiesta_ptr *head);

/* funzioni per la gestione del file di log */
void create_log(char *file,char *log_name);
void check_log(void);

int main(int argc, char **argv){

   setbuf(stdout,NULL);  // per non bufferizzare

	int s;

	struct sockaddr_in father_srv,child_srv; /* struttura usata per connettersi con il server*/
        struct addrinfo hints, *results;
        char buf_tmp[BUFLEN];
        char buf_send[MAX_DIM];
	char buf_recv[BUFLEN];
        char choice[CHOICE];
        char nome[NAME];
        char new_choice[CHOICE];
        char new_nome[NAME];
        char log_name[50];
        char recv_file[MAX_CHAR];
        char send_ack[MAX_CHAR];
        char recv_data[BUFLEN];
        char ext_choice[CHOICE],ext_nome[NAME];
        int fdR,fdW;
	int empty,ret,check_file,dim2,flag,cont,check_port; /* sarà riempito dal server*/
	int size,dim,recv_bytes,new_port,check,tentativi,prove; /* n byte letti da rcvfrom*/

        int32_t up_seq,down_seq,recv_seq; 

	socklen_t father_srvlen = sizeof(father_srv);
        socklen_t child_srvlen = sizeof(child_srv);
	struct timeval timeout;
        struct timeval max_timeout;
	fd_set u_cset,d_cset;

        richiesta_ptr tail,head;

	tail=head=NULL;

        if(argc!=3){
           printf("Error while passing initial parameters: IP address and port needed, closing..\n");
           exit(1);
        }

        check_port=atoi(argv[2]);

        if((check_port<0)||((check_port>=0)&&(check_port<=1023))){
           printf("Errore: numero di porta non consentito (deve essere maggiore di 1023).\n");
           exit(1);
        }

        check_log(); /* controllo se ci sono file di log ed eventualmente rimuovo i file*/

      /*  val = inet_aton(argv[1], &addr);  // inet_aton restiuisce l'indirizzo in "addr"*/

        memset(&hints, 0, sizeof(hints));
        hints.ai_family = PF_INET;
        hints.ai_socktype = SOCK_DGRAM;
        ret=getaddrinfo(argv[1],argv[2],&hints,&results);

	s=my_socket(PF_INET,SOCK_DGRAM,IPPROTO_UDP);

        udp_socket *connection = new udp_socket(PF_INET,SOCK_DGRAM,IPPROTO_UDP);

	father_srv.sin_family = AF_INET;
        father_srv.sin_addr = ((struct sockaddr_in*)(results->ai_addr))->sin_addr;
	father_srv.sin_port = htons(atoi(argv[2]));

        freeaddrinfo(results);

        FD_ZERO(&u_cset);
	FD_SET(fileno(stdin), &u_cset);
	FD_SET(s, &u_cset);

        FD_ZERO(&d_cset);
	FD_SET(fileno(stdin), &d_cset);
	FD_SET(s, &d_cset);

        printf("Cosa vuoi fare? UP per 'upload', DOWN per 'download', QUIT NOW per terminare un download corrente e ABORT ALL per terminare ogni operazione..\n");

        scanf("%s %s",choice,nome);

        if((strcmp(choice,"ABORT"))==0){
           my_close(s);
           exit(1);
        }

        inserisci(&head,&tail,choice,nome);

  while(1){

        empty=vuota(&head);
       // printf("VALORE DI EMPTY = %d\n",empty);
       // sleep(5);
        if(empty==0){ // lista non vuota, posso estrarre
           estrai(&tail,&head,ext_choice,ext_nome);
           //print_list(&head);
        }else{ // lista vuota, non estraggo
           //print_list(&head);
           printf("\nLista dei comandi vuota, impossibile estrarre.\n");
           printf("Se si desidera svolgere altri trasferimenti digitare\nUP/DOWN seguito da nome file. 'ABORT ALL' per terminare..\n");

           scanf("%s %s",choice,nome);
           
           if((strcmp(choice,"ABORT"))==0){
              /* se entro qui la lista è vuota, dunque non devo fae nessuna free*/
              my_close(s); /* chiusura socket */
              exit(1);
           }else{
              inserisci(&head,&tail,choice,nome);
              estrai(&tail,&head,ext_choice,ext_nome);
           }
        }

/*#########################################################################################
                     UPLOAD DEL FILE
############################################################################################*/

        if(strcmp(ext_choice,"UP")==0){  // aggiungere codice timeout, perdita pacchetti ecc.

           check_file=access(ext_nome,F_OK);

           if(check_file==0){ /* il file è presente */

           up_seq=0;
           flag=1;

           FD_ZERO(&u_cset);
	   FD_SET(fileno(stdin), &u_cset);
	   FD_SET(s, &u_cset);

	   memset(buf_send,0,sizeof(buf_send));

           sprintf(buf_send,"%s %s",ext_choice,ext_nome);
           size=strlen(buf_send);
           buf_send[size]='\0';
   
           my_sendto(s,buf_send,size,0,(struct sockaddr *)&father_srv,father_srvlen);

           printf("Inviato comando UP.\n");

           memset(buf_recv,0,sizeof(buf_recv));

           max_timeout.tv_sec=MAX_TIMEOUT;
	   max_timeout.tv_usec=0;

           if(select(FD_SETSIZE,&u_cset,NULL,NULL,&max_timeout)>0){ /* attendo al massimo 5 minuti la risposta del server */

           max_timeout.tv_sec=MAX_TIMEOUT;
	   max_timeout.tv_usec=0;

           FD_ZERO(&u_cset);
	   FD_SET(fileno(stdin), &u_cset);
	   FD_SET(s, &u_cset);

           my_recvfrom(s,buf_recv,BUFLEN,0,(struct sockaddr*)&father_srv,&father_srvlen); // ricezione nuovo numero di porta con cui connettersi con il figlio
           new_port=atoi(buf_recv);

	   child_srv.sin_family = AF_INET; // address family IPv4
           child_srv.sin_addr = father_srv.sin_addr;
           child_srv.sin_port = htons(new_port);

           memset(buf_send,0,sizeof(buf_send));
           sprintf(buf_send,"POSSO ?");
           dim2=strlen(buf_send);
           buf_send[dim2]='\0';
           my_sendto(s,buf_send,size,0,(struct sockaddr *)&child_srv,child_srvlen); /* usata solo per far conoscere al figlio l'identità del client */
           memset(buf_send,0,sizeof(buf_send));

           if(select(FD_SETSIZE,&u_cset,NULL,NULL,&max_timeout)>0){

              FD_ZERO(&u_cset);
	      FD_SET(fileno(stdin), &u_cset);
	      FD_SET(s, &u_cset);

              my_recvfrom(s,buf_recv,BUFLEN,0,(struct sockaddr*)&child_srv,&child_srvlen);

              if((strcmp(buf_recv,"GO"))==0){
                 flag=1; /* abilito il whil3 */
                 fdR=my_openR(fdR,ext_nome);
                 printf("Aperto file '%s' in lettura.\n",ext_nome);
              }

              if((strcmp(buf_recv,"STOP"))==0){
                 flag=0; /* blocco il while*/
                 printf("Server bloccato, riprovare più tardi.\n");
              }

           }else{
             printf("Nessuna risposta dal server dopo 5 minuti, chiudo socket ed esco..\n");
             free_list(&head); /* libero la lista allocata perchè il server non mi servirà */
             my_close(s);
             exit(1);
           }

           memset(buf_send,0,sizeof(buf_send));
           memset(buf_tmp,0,sizeof(buf_tmp));

	   while(((size=read(fdR, buf_tmp, BUFLEN))>0) && (flag==1)){  
          
              sprintf(buf_send,"%s %"PRIu32" %d ",ext_nome,up_seq,size);
              dim=strlen(buf_send);
              memcpy(buf_send+dim,buf_tmp, size); // a questo punto in buf_send ho correttamente il nome del file, il num seq e "size" byte letti/ da inviare
          //    printf("Letto il frammento numero %d.\r\n",up_seq);
          
	      my_sendto(s,buf_send,(dim+size),0,(struct sockaddr *)&child_srv,child_srvlen);

           /*   printf("Inviato il frammento numero %d, contenente %d byte.\r\n",up_seq,size);*/
              // controllo sul numero di sequenza e timeout finchè i numeri di sequenza non coincidono
              tentativi=0;
              prove=0;

              do{
                 timeout.tv_sec=TIMEOUT;
	         timeout.tv_usec=0;

                 FD_ZERO(&u_cset);
	         FD_SET(fileno(stdin), &u_cset);
	         FD_SET(s, &u_cset);

                 if(select(FD_SETSIZE,&u_cset,NULL,NULL,&timeout)>0){ // se non scatta il timeout ma ricevo dati, ne controllo il numero di sequenza

                  if(FD_ISSET(fileno(stdin),&u_cset)){

                     scanf("%s %s",choice,nome);  
                     /*printf("(interfaccia attiva) Scelta = %s --- Nome = %s",choice,nome);*/

                     if((strcmp(choice,"ABORT"))==0){

                        up_seq=-2;
                        size=0; /* da mettere nel pacchetto affinchè la lettura da parte del server sia corretta*/

                        sprintf(buf_send,"%s %"PRId32" %d ",ext_nome,up_seq,size);
                        dim=strlen(buf_send);
                        buf_send[dim]='\0';
                        my_sendto(s,buf_send,dim,0,(struct sockaddr *)&child_srv,child_srvlen);

                        close_and_quit(fdR,s,&head);
                     }

                     if((strcmp(choice,"QUIT"))==0){

                        up_seq=-1;
                        size=0; /* da mettere nel pacchetto affinchè la lettura da parte del server sia corretta*/

                        sprintf(buf_send,"%s %"PRId32" %d ",ext_nome,up_seq,size);
                        dim=strlen(buf_send);
                        buf_send[dim]='\0';
                        my_sendto(s,buf_send,dim,0,(struct sockaddr *)&child_srv,child_srvlen);
                        my_recvfrom(s,buf_recv,BUFLEN,0,(struct sockaddr*) &child_srv,&child_srvlen); /* ricevo ack e non lo controllo */
                        my_close(fdR); /* chiudo soltanto il file descriptor e posso ancora fare altre operazioni: non libero lista e non chiudo socket*/
                        break;
                     }

                     inserisci(&head,&tail,choice,nome);      // inserimento in lista
                     FD_SET(s,&u_cset);
                     
                  }

		  if(FD_ISSET(s,&u_cset)){

                    memset(buf_recv,0,sizeof(buf_recv));
                    dim2=my_recvfrom(s,buf_recv,BUFLEN,0,(struct sockaddr*) &child_srv,&child_srvlen); // controllo il numero di sequenza

                    buf_recv[dim2]='\0';

                    sscanf(buf_recv,"%"SCNu32"",&recv_seq);
                    /*printf("Ricevuto ack %"PRId32"\n",recv_seq);*/
                       if(up_seq!=recv_seq){
                         printf("\n corrente: %"PRId32" - ricevuto: %"PRId32"",up_seq,recv_seq);
                         my_sendto(s,buf_send,(dim+size),0,(struct sockaddr *)&child_srv,child_srvlen);
                         prove++; 

                         if(prove==3){
                            break;
                         }
                       }
                      FD_SET(fileno(stdin),&u_cset);
                     }
                    }else{ // se scatta il timeout senza aver ricevuto risposta reinvio il pacchetto
                       printf("\nTimeout scaduto: invio ancora il pacchetto %"PRId32". Tentativo [%d]\n",up_seq,tentativi+1);
                       my_sendto(s,buf_send,(dim+size),0,(struct sockaddr *)&child_srv,child_srvlen);
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
                 my_sendto(s,buf_send,size,0,(struct sockaddr *)&child_srv,child_srvlen);
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
	         FD_SET(s, &u_cset);
                 
                 timeout.tv_sec=TIMEOUT;
	         timeout.tv_usec=0;

                 if(select(FD_SETSIZE,&u_cset,NULL,NULL,&timeout)>0){
                    my_recvfrom(s,buf_recv,BUFLEN,0,(struct sockaddr*) &child_srv,&child_srvlen);
              	    if(strcmp(buf_recv,"OK")==0){
                       printf("\nFile '%s' inviato correttamente.\n",ext_nome);
	               my_close(fdR);
                    }else{
                       printf("[%d]Non ho ricevuto risposta positiva dal server, riprovo.\n",prove+1);
                       my_sendto(s,buf_send,size,0,(struct sockaddr *)&child_srv,child_srvlen);
                       prove++;
                    }
                 }else{ // timeout scaduto, invio di nuovo..
                    printf("[%d]Timeout scaduto: invio ancora pacchetto finale.\n",tentativi+1);
                    my_sendto(s,buf_send,size,0,(struct sockaddr *)&child_srv,child_srvlen);
                    tentativi++;
                 }
              }while(((strcmp(buf_recv,"OK")!=0) && (tentativi<3)) || ((strcmp(buf_recv,"OK")!=0) && (tentativi<3)));
           }

	  // FD_SET(fileno(stdin), &u_cset);

          }else{
             printf("Nessuna risposta dal server dopo 5 minuti, chiudo socket ed esco..\n");
             free_list(&head); /* libero la lista allocata perchè il server non mi servirà */
             my_close(s);
             exit(1);
          }

         }else{ /* file assente nel client */
            printf("File assente: controllare il nome o riprovare più tardi..\n");
         }

        }else{ // fine if per l'UPLOAD

/*#########################################################################################
                     DOWNLOAD DEL FILE
############################################################################################*/

        if(strcmp(ext_choice,"DOWN")==0){;

           memset(buf_recv,0,sizeof(buf_recv));
           memset(buf_send,0,sizeof(buf_send));

           check=0; /* usato per controllare se devo fare abort o no*/

           sprintf(buf_send,"%s %s",ext_choice,ext_nome);
           size=strlen(buf_send);
           buf_send[size]='\0';
           my_sendto(s,buf_send,size,0,(struct sockaddr *)&father_srv,father_srvlen); // invio comando di DOWNLOAD
           printf("Inviato '%s %s'.\n",ext_choice,ext_nome);

           FD_ZERO(&d_cset);
	   FD_SET(fileno(stdin), &d_cset);
	   FD_SET(s, &d_cset);

           memset(buf_recv,0,sizeof(buf_recv));

           max_timeout.tv_sec=MAX_TIMEOUT;
	   max_timeout.tv_usec=0;

           if(select(FD_SETSIZE,&d_cset,NULL,NULL,&max_timeout)>0){ /* attendo al massimo 5 minuti la risposta del server */

           FD_ZERO(&d_cset);
	   FD_SET(fileno(stdin), &d_cset);
	   FD_SET(s, &d_cset);

           size=my_recvfrom(s,buf_recv,BUFLEN,0,(struct sockaddr*) &father_srv,&father_srvlen); /* ricevo conferma o meno dal server*/
           buf_recv[size]='\0';

           if((strcmp(buf_recv,"OK"))==0){ /* file presente sul server*/

              down_seq=0;
              tentativi=0;
              prove=0;
              cont=0;

	      printf("File presente sul server..\n");

              max_timeout.tv_sec=MAX_TIMEOUT;
	      max_timeout.tv_usec=0;

              if(select(FD_SETSIZE,&d_cset,NULL,NULL,&max_timeout)>0){ /* attendo al massimo 5 minuti la risposta del server */

              memset(buf_recv,0,sizeof(buf_recv));
              my_recvfrom(s,buf_recv,BUFLEN,0,(struct sockaddr*) &father_srv,&father_srvlen); // ricezione nuovo numero di porta con cui connettersi con il figlio
              new_port=atoi(buf_recv);

              child_srv.sin_family = AF_INET; // address family IPv4
              child_srv.sin_addr = father_srv.sin_addr;
              child_srv.sin_port = htons(new_port);

              memset(buf_send,0,sizeof(buf_send));
              sprintf(buf_send,"POSSO ?");
              dim2=strlen(buf_send);
              buf_send[dim2]='\0';
              my_sendto(s,buf_send,size,0,(struct sockaddr *)&child_srv,child_srvlen); /* usata solo per far conoscere al figlio l'identità del client */
              memset(buf_send,0,sizeof(buf_send));

           if(select(FD_SETSIZE,&u_cset,NULL,NULL,&max_timeout)>0){

              my_recvfrom(s,buf_recv,BUFLEN,0,(struct sockaddr*)&child_srv,&child_srvlen);

              if((strcmp(buf_recv,"GO"))==0){
                 cont=1; /* abilito il whil3 */
                 printf("Aperto file '%s' in scrittura.\n",ext_nome);
                 printf("Ricevo il file '%s'.\n",ext_nome);
                 create_log(ext_nome,log_name); /* se il file è presente creo il log*/
              }

              if((strcmp(buf_recv,"STOP"))==0){
                 cont=0; /* blocco il while*/
                 printf("Server bloccato, riprovare più tardi.\n");
              }

           }else{
             printf("Nessuna risposta dal server dopo 5 minuti, chiudo socket ed esco..\n");
             free_list(&head); /* libero la lista allocata perchè il server non mi servirà */
             my_close(s);
             exit(1);
           }

           if(cont!=0){ /* se flag è diverso da 0, posso trasferire */
          
              FD_ZERO(&d_cset);
	      FD_SET(fileno(stdin), &d_cset);
	      FD_SET(s, &d_cset);

              fdW=my_openW(fdW,ext_nome); // apertura file in scrittura

               do{
		  check=0;

		  timeout.tv_sec=TIMEOUT;
		  timeout.tv_usec=0;
		  memset(buf_recv,0,sizeof(buf_recv));

		  sprintf(buf_send,"%d",BUFLEN);
		  size=strlen(buf_send);
                  buf_send[size]='\0';
		  my_sendto(s,buf_send,size,0,(struct sockaddr *)&child_srv,child_srvlen); /* invio n° byte che voglio ricevere */
 
		  if(select(FD_SETSIZE,&d_cset,NULL,NULL,&timeout)>0){ // se non scatta il timeout ma ricevo dati, ne controllo il numero di sequenza

                     if(FD_ISSET(fileno(stdin),&d_cset)){
                        scanf("%s %s",new_choice,new_nome);

                        if((strcmp(new_choice,"QUIT")!=0) && (strcmp(new_choice,"ABORT")!=0)){ // se non ho inserito il comando di terminazione operazioni
                             
                           /*printf("(interfaccia attiva) Scelta = %s --- Nome = %s\n",new_choice,new_nome);*/
                           inserisci(&head,&tail,new_choice,new_nome);      // inserimento in lista

                        }else{ // se è inserito QUIT elimino i file e svuoto la lista

                           if((strcmp(new_choice,"QUIT"))==0){
                             // my_close(fdW); /* non devo ricevere più, dunque chiudo il fd */
                              clear(ext_nome); // funzione che riceve il nome del file interrotto
                              clear(log_name);

                              sprintf(buf_send,"%d",-1);
                              size=strlen(buf_send);
                              buf_send[size]='\0';
                              my_sendto(s,buf_send,size,0,(struct sockaddr *)&child_srv,child_srvlen);// devo inviare il comando al server
                              my_recvfrom(s,buf_recv,BUFLEN,0,(struct sockaddr*) &child_srv,&child_srvlen); /* ricevo l'ultimo pacchetto che il server mi manda*/
                              memset(buf_send,0,sizeof(buf_send));
                              memset(buf_recv,0,sizeof(buf_recv));
                              memset(ext_nome,0,sizeof(ext_nome));
                              memset(ext_choice,0,sizeof(ext_choice));
                              memset(nome,0,sizeof(nome));
                              memset(choice,0,sizeof(choice));
                              fflush(stdin);
                              check=1;
                              break; // vado al prossimo trasferimento nella lista
                           }

                           if((strcmp(new_choice,"ABORT"))==0){

                              clear(ext_nome);
                              clear(log_name);

                              sprintf(buf_send,"%d",-2); /* -1 per QUIT, -2 per ABORT*/
                              size=strlen(buf_send);
                              buf_send[size]='\0';
                              my_sendto(s,buf_send,size,0,(struct sockaddr *)&child_srv,child_srvlen);// devo inviare il comando al server
                              memset(buf_send,0,sizeof(buf_send));

                              close_and_quit(fdW,s,&head);
                           }
                        }

                        FD_SET(s,&d_cset); 
                     }

                     if(FD_ISSET(s,&d_cset)){

                        memset(buf_recv,0,sizeof(buf_recv));
			size=my_recvfrom(s,buf_recv,MAX_DIM,0,(struct sockaddr*)&child_srv,&child_srvlen);
		          // se ricevo dati controllo numero di sequenza e file e poi scrivo se è il caso
		        memset(recv_data,0,sizeof(recv_data));
		        sscanf(buf_recv,"%s %"SCNd32" %d",recv_file,&recv_seq,&recv_bytes);
		        dim=size-recv_bytes;

		        memcpy(recv_data,buf_recv+dim,recv_bytes);

//		        printf("Ricevuto frammento %d, atteso %d\n",recv_seq,down_seq);

		        if((strcmp(recv_file,ext_nome)==0) && (down_seq==recv_seq)){

		       	   if(write(fdW,recv_data,recv_bytes)!=recv_bytes){  
		              printf("Errore scrittura dati.\n");
		              exit(1);
		           }
				// invio ACK
		           sprintf(send_ack,"%"PRId32"",down_seq);
		           dim=strlen(send_ack);
		           send_ack[dim]='\0';
                            // printf("ACK inviato = %s\n",send_ack);
		           my_sendto(s,send_ack,dim,0,(struct sockaddr*)&child_srv,child_srvlen);

		           down_seq++;
			   memset(send_ack,0,sizeof(send_ack));

		    	}else{  // numeri di sequenza diversi: richiedo il frammento giusto
		           /*sleep(1);*/
		           printf("[%d]Errore!  Seq attesa: (%"PRId32") - ricevuta (%"PRId32")..\n",down_seq,recv_seq,prove+1);
                           printf("FILE ATTESO: %s -- RICEVUTO: %s",ext_nome,recv_file);
                           memset(buf_send,0,sizeof(buf_send));
                           buf_send[0]='\0';
		           sprintf(buf_send,"%"PRId32"",down_seq);
		           dim=strlen(buf_send);
                           buf_send[dim]='\0';
		           my_sendto(s,buf_send,dim,0,(struct sockaddr*)&child_srv,child_srvlen);

			   prove++;

                           if(prove==3){
                              printf("Ho ricevuto 3 volte la sequenza sbagliata e non quella giusta: cancello il file.\n");
                              check=1;
                              my_close(fdW);
                              clear(log_name); /* elimino il file di log */
                              clear(ext_nome);
                              break;  /* forzo l'uscita dal ciclo di ricezione file e impedisco l'invio del paccketto finale*/
                           }
		        }

                        FD_SET(fileno(stdin),&d_cset);
                     }

		  }else{ // timeout scaduto
		     printf("[%d]Timeout scaduto - frammento non ricevuto.\n",tentativi+1);
		     my_sendto(s,buf_send,dim,0,(struct sockaddr*)&child_srv,child_srvlen);
                     tentativi++;
                   
                     if(tentativi==3){ /* esco dal ciclo, chiudo fd, rimuovo log e file incompleto */
                        printf("Dopo 3 tentativi di ricezione del numero di sequenza, non ci sono riuscito: cancello il file.\n");
                        check=1;
                        my_close(fdW);
                        clear(log_name); /* elimino il file di log se tutto va bene */
                        clear(ext_nome);
                        break;  /* forzo l'uscita dal ciclo di ricezione file e impedisco l'invio del paccketto finale*/
                     }
		  }     

               }while(recv_bytes==BUFLEN);  // si continua finchè i byte ricevuti sono uguali a quelli richiesti    recv_bytes==BUFLEN 

               if(check==0){

                  sprintf(buf_send,"%d",0);
                  dim=strlen(buf_send);
                  buf_send[dim]='\0';
                  my_sendto(s,buf_send,dim,0,(struct sockaddr*)&child_srv,child_srvlen); // invio 0 byte da leggere

                  my_close(fdW);
                  clear(log_name); /* elimino il file di log se tutto va bene */
                  printf("\nFile '%s' ricevuto con successo.\n",ext_nome);
                  printf("Chiusura file descriptor.\n\n");
               }

              } /* fine if sul valore di flag*/

             }else{
               printf("Nessuna risposta dal server dopo 5 minuti, chiudo socket ed esco..\n");
               free_list(&head);
               my_close(s);
               exit(1);
             }
               
            }else{ /* else attivo se sul server non c'è il file richiesto*/
               printf("File '%s' non presente sul server, riprovare più tardi..\n",ext_nome);
               /*printf("BUF_RECV: %s\n\n",buf_recv);*/
            }

          }else{
             printf("Nessuna risposta dal server dopo 5 minuti, chiudo socket ed esco..\n");
             free_list(&head);
             my_close(s);
             exit(1);
          }
	} /* fine if per DOWN*/
     } /* file else dell'UP*/
  } /* fine while*/	

      my_close(s);

return(0);
}

int my_socket(int family,int type,int protocol){

   int s;

	if((s=socket(family,type,protocol))<0){
           printf("Errore creazione socket.\n");
           exit(1);
        }
   return(s);
}

void close_and_quit(int fd,int s,richiesta_ptr *head){ /* chiude socket, file descriptor e libera la lista */
   
   my_close(fd);
   my_close(s);

   free_list(head);
  
   exit(1);
}

void create_log(char *file,char *log_name){

   int dim;
   FILE *log_ptr;

   sprintf(log_name,"%s%s",file,LOG_EXT);
   dim=strlen(log_name);
   log_name[dim]='\0';

   if((log_ptr=fopen(log_name,"w+"))==NULL){ /* creo il file di log */
         printf("Errore apertura file di log '%s' in scrittura.\n",log_name);
         printf("Termino le operazioni.\n");
         return;
   }

   fprintf(log_ptr,"%s",file);
   
   fclose(log_ptr);
}

void check_log(void){

   int dim,dim_nome;
   DIR *dirP;
   struct dirent *visit;
   char trash[MAX_CHAR]; 
   FILE *log;

   if((dirP=opendir(CDIR))==NULL){
      printf("Apertura errata del direttorio '%s'.\n",CDIR);
      return;
   }

   while((visit=readdir(dirP))!=NULL){
      dim=strlen(visit->d_name);
      dim_nome=dim-4; /* 4 caratteri pari a ".log" */

      if((strncmp((visit->d_name)+dim_nome,".log",4))==0){  /* se ha estensione .log, apro, leggo ed elimino il file*/

         if((log=fopen(visit->d_name,"r"))==NULL){
            printf("Errore apertura file di log '%s' in lettura.\n",visit->d_name);
            return;
         }

         fscanf(log,"%s",trash);

         if((fclose(log))!=0){
            printf("Errore chiusura file '%s'.\n",visit->d_name);
         }

         clear(trash);
         clear(visit->d_name);
      }
   }

   if((closedir(dirP))==-1){
      printf("Errore chiusura cartella '%s'.\n",CDIR);
   }

}

int my_openR(int fd, char *nome){

   if((fd=open(nome, O_RDONLY))<0){
      printf("Errore apertura file in lettura.\n");
      exit(1);
   }
   return(fd);
}

int my_openW(int fd, char *nome){

   if((fd=open(nome,O_WRONLY | O_CREAT | O_TRUNC,S_IRUSR | S_IWUSR))<0){
      printf("Errore apertura file in lettura.\n");
      exit(1);
   }
   return(fd);
}

void my_close(int fd){

   if(close(fd)<0){
      printf("Errore chiusura file descriptor.\n");
      return;
   }
}

ssize_t my_recvfrom(int fd, void *bufptr, size_t nbytes, int flags, SA *sa, socklen_t *salenptr){

	ssize_t n;
        memset(bufptr,0,sizeof(bufptr));
	if((n = recvfrom(fd,bufptr,nbytes,flags,sa,salenptr))<0){
	   printf("Errore nella recvfrom.\n");
           return(0);
        }
	return n;
}

void my_sendto(int fd, void *bufptr, size_t nbytes, int flags, const SA *sa, socklen_t salen){
	if(((size_t)sendto(fd,bufptr,nbytes,flags,sa,salen))!=nbytes){
           printf("Errore nella sendto: '%s' \n",strerror(errno));
           return;
        }
        memset(bufptr,0,sizeof(bufptr));
}

void clear(char *nome_file){

   if((remove(nome_file))==-1){
      printf("Errore eliminazione file '%s'.\n",nome_file);
   }else{
      printf("File '%s' eliminato correttamente.\n",nome_file);
   }
}

richiesta_ptr nuova(char *scelta,char *nome){
   
   richiesta_ptr nuovo=NULL;

   nuovo= (richiesta_ptr) malloc(sizeof(richiesta));

   if(nuovo==NULL){
      printf("Errore allocazione nodo lista.\n");
      exit(1);
   }

   strcpy(nuovo->scelta,scelta);
   strcpy(nuovo->nome_file,nome);
   nuovo->next=NULL;

   return(nuovo);
}

int vuota(richiesta_ptr *head){

   richiesta_ptr tmp=*head;

   if(tmp==NULL){
      return(1); // 1 se la lista è vuota
   }else{
      return(0);  // 0 se la lista non è vuota
   }
}

void estrai(richiesta_ptr *tail,richiesta_ptr *head,char *scelta,char *nome){

     richiesta_ptr tmp;

     if((*head)!=NULL){
        strcpy(scelta,(*head)->scelta);
        strcpy(nome,(*head)->nome_file);

        tmp=*head;
        *head=(*head)->next;
     }else{
        *tail=NULL;
        //printf("Lista dei comandi vuota, impossibile estrarre..\n");
     }
     free(tmp);
}

void print_list(richiesta_ptr *head){

   while((*head)!=NULL){
       printf("ANCORA PRESENTE: %s %s\n",(*head)->scelta,(*head)->nome_file);
      *head=(*head)->next;
   }
if((*head)==NULL){
   printf("LISTA VUOTA !!!!!!!!!!!!!!!!!!!!");}
}

void free_list(richiesta_ptr *head){

   richiesta_ptr tmp;

   while((*head)!=NULL){
      tmp=*head;
      *head=(*head)->next;

      free(tmp);
   }
}

void inserisci(richiesta_ptr *head,richiesta_ptr *tail,char *choice,char *nome){

   richiesta_ptr nuovo;

   nuovo=nuova(choice,nome);

   if((*head)==NULL){
      *head=nuovo;
   }else{
      (*tail)->next=nuovo;
   }

   *tail=nuovo;
}
