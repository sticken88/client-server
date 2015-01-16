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
#include <sys/file.h>
#include <sys/stat.h> // aggiunta per usare stat()
#include <netinet/in.h>
#include <arpa/inet.h> // inet_aton()
#include <netdb.h>
#include <errno.h>
#include <memory.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>
#include <dirent.h>
#include <limits.h>
#include <fcntl.h> 
#include <signal.h>

#define SA struct sockaddr

#define MAX_CHOICE 5
#define MAX_CHAR 200
#define ACKLEN 100
#define BUFLEN 1200
#define DATALEN 1000
#define TIMEOUT 30
#define WAIT_PIPES 500000
#define FIGLI 4
#define READ 0
#define WRITE 1
#define LOG_EXT ".log"
#define CDIR "./"

typedef struct s{
                 int pipeFC[2]; // usata per comunicare
                 int pipeCF[2]; // usata per comunicare
                 int busy;
                 int pid;
                 int porta;
}child_info;

struct nodo{
            struct sockaddr_in client_address;
            char scelta[MAX_CHOICE];
            char nome_file[MAX_CHAR];

            struct nodo *next;
};

typedef struct nodo richiesta;
typedef richiesta *richiesta_ptr;

typedef int SOCKET;

/* prototipi delle funzioni per la gestione della connessione */

void my_sendto(int fd, void *bufptr, size_t nbytes, int flags, const SA *sa, socklen_t salen);
ssize_t my_recvfrom (int fd, void *bufptr, size_t nbytes, int flags, SA *sa, socklen_t *salenptr);
void init_struct(child_info *elenco,int dim);
int check_for_free(child_info *elenco,int dim,int *child_index,int *child_port);
int my_openR(int fd, char *nome);
int my_openW(int fd, char *nome);
void my_close(int fd);
SOCKET my_socket(int family,int type,int protocol);
int check_down_abort(int32_t recv_seq,char* nome,int fdK);
int check_up_abort(int32_t recv_seq,char* nome,int fdK,FILE *log_ptr);
void manage_children(void);
void clear(char *nome_file);

/* prototipi per gestire la lista di richieste */

richiesta_ptr nuova(char *scelta,char *nome,struct sockaddr_in caddr);
void estrai(richiesta_ptr *tail,richiesta_ptr *head,char *scelta,char *nome,struct sockaddr_in *porta);
void inserisci(richiesta_ptr *head,richiesta_ptr *tail,char *choice,char *nome,struct sockaddr_in caddr);
int vuota(richiesta_ptr *head);
void free_list(richiesta_ptr *head);

void my_init(int *sem);

/* prototipi delle funzioni per la gestione del file di log*/
FILE *create_log(char *file,char *log_name);
void check_log(void);

/* vettore di strutture usato dal padre per gestire i figli e altre info globali: non posso passarle a funzione da eseguire in seguito a ricezione di segnale*/
child_info elenco[FIGLI];
richiesta_ptr tail,head;
SOCKET attesa;
extern int errno;
int figli[FIGLI];

int main(int argc, char **argv){

   setbuf(stdout,NULL);  // per non bufferizzare

	//SOCKET attesa; 
        SOCKET nuovo;
	struct sockaddr_in saddr,caddr;
        struct sockaddr_in child_saddr;
        int size,i;
        char buf_send[BUFLEN];
        char buf_tmp[BUFLEN];
        char recv_data[DATALEN];
	char buf_recv[BUFLEN]; 
        char choice[MAX_CHOICE];
        char new_choice[MAX_CHOICE];
        char file[MAX_CHAR];
        char log_name[50];
        char new_file[MAX_CHAR];
        char buffer1[MAX_CHAR];
        char ack_recv[ACKLEN];
        char buffer2[MAX_CHAR];
        char buffer3[MAX_CHAR];
        char recv_file[MAX_CHAR];
	socklen_t caddrlen = sizeof(caddr);
	int fdR,fdW,dim,recv_bytes,c_size,j,continua,ext_porta,f_port,tentativi,check_file;
        int dim3,ret_val,child_index,child_port,empty,dim2,flag,abort,prove,check_port;

        FILE *log_ptr;

        int32_t up_seq,down_seq,recv_seq; 

	fd_set sset;
	struct timeval timeout;        

	if(argc!=2){
	   printf("Errore passaggio parametri: serve il numero di porta.\n");
           exit(1);
	}

        check_port=atoi(argv[1]);

        if((check_port<0)||((check_port>=0)&&(check_port<=1023))){
           printf("Errore: numero di porta non consentito (deve essere maggiore di 1023).\n");
           exit(1);
        }

        check_log(); /* controllo se ci sono file di log ed eventualmente rimuovo i file*/

	attesa=my_socket(PF_INET,SOCK_DGRAM,IPPROTO_UDP);

        init_struct(elenco,FIGLI); // inizializza elenco 

	f_port=atoi(argv[1]);
	saddr.sin_family = AF_INET;
	saddr.sin_port = htons(f_port);
	saddr.sin_addr.s_addr = htonl(INADDR_ANY);

	if(bind(attesa,(struct sockaddr *) &saddr,sizeof(saddr))!=0){
           printf("Errore nella bind().\n");
           exit(1);
        }
	
	printf("Creazione figli..\n");

        for(i=0; i<FIGLI; i++){
        
           figli[i]=fork();

           if(figli[i]==0){ //  riceve  la richiesta dal padre e la processa
  
              my_close(attesa); /* chiusura socket del padre per evitre danni*/
              my_close(elenco[i].pipeFC[WRITE]);
              my_close(elenco[i].pipeCF[READ]);
              elenco[i].pid=getpid();
       
	      nuovo=my_socket(PF_INET,SOCK_DGRAM,IPPROTO_UDP);
       
              memset(buffer2,0,MAX_CHAR);
             /* printf("[%d] Server pronto..\n",elenco[i].pid);*/

	      read(elenco[i].pipeFC[READ],buffer2,MAX_CHAR); // leggo la nuova porta comunicata dal padre
              elenco[i].porta=atoi(buffer2);
           /*   printf("[%d]Porta Nuova = %d\n",elenco[i].pid,elenco[i].porta);*/
        
              memset(buffer2,0,sizeof(buffer2));
              sprintf(buffer2,"%d %d",elenco[i].pid,elenco[i].busy);
              write(elenco[i].pipeCF[WRITE],buffer2,strlen(buffer2)); // scrivo PID e stato relativo al processo figlio

	      child_saddr.sin_family = AF_INET;
	      child_saddr.sin_port = htons(elenco[i].porta); // genero una porta casuale
	      child_saddr.sin_addr.s_addr = htonl(INADDR_ANY);

              if(bind(nuovo,(struct sockaddr *) &child_saddr,sizeof(child_saddr))!=0){
                 printf("Errore nella bind del figlio = '%s'.\n",strerror(errno));
                 exit(1);
              }


      while(1){

         memset(ack_recv,0,sizeof(ack_recv));

         printf("[%d] Server pronto, in attesa di richieste dal client..\n",elenco[i].pid);

         memset(buffer1,0,sizeof(buffer1));
         read(elenco[i].pipeFC[READ],buffer1,MAX_CHAR); // leggo da padre comando e nome file
         sscanf(buffer1,"%s %s",choice,file);

      if(strcmp(choice,"DOWN")==0 || strcmp(choice,"down")==0){

/*#########################################################################################
                     DOWNLOAD DEL FILE
############################################################################################*/
         // invio file(s)
          /* printf("Ricevuto comando '%s'.\n",choice);*/
           down_seq=0;
           ret_val=0;
           continua=0;

           my_recvfrom(nuovo,buf_recv,BUFLEN,0,(struct sockaddr*) &caddr,&caddrlen); /* usata dal figlio per conoscere l'identità del client*/
           memset(buf_recv,0,sizeof(buf_recv));

           fdR=my_openR(fdR,file);

           //printf("Aperto file '%s' in lettura.\n",file);

	    if((flock(fdR,LOCK_SH | LOCK_NB))==-1){

               continua=0;

               memset(buf_send,0,sizeof(buf_send));
               sprintf(buf_send,"STOP");
               dim2=strlen(buf_send);
               buf_send[dim2]='\0';
               my_sendto(nuovo,buf_send,dim2,0,(struct sockaddr*) &caddr,caddrlen);
               memset(buf_send,0,sizeof(buf_send));

               memset(choice,0,sizeof(choice));
               memset(file,0,sizeof(file));

            }else{

               printf("[%d] RICEVUTO DA PADRE = %s %s\n",elenco[i].pid,choice,file);

               memset(buf_send,0,sizeof(buf_send));
               sprintf(buf_send,"GO");
               dim2=strlen(buf_send);
               buf_send[dim2]='\0';
               my_sendto(nuovo,buf_send,dim2,0,(struct sockaddr*) &caddr,caddrlen);
               memset(buf_send,0,sizeof(buf_send));

               printf("Acquisito lock condiviso su file descriptor..\n\n");

               printf("Aperto descrittore file in lettura.\n");
               printf("Invio il file '%s'.\n",file);

               continua=1;
            }

           if(continua!=0){
 
           memset(buf_send,0,sizeof(buf_send));
           memset(buf_tmp,0,sizeof(buf_tmp));
           memset(buf_recv,0,sizeof(buf_recv));

           timeout.tv_sec=TIMEOUT;
	   timeout.tv_usec=0;
           FD_ZERO(&sset);
           FD_SET(nuovo,&sset);

           if(select(FD_SETSIZE,&sset,NULL,NULL,&timeout)>0){ 
              my_recvfrom(nuovo,buf_recv,BUFLEN,0,(struct sockaddr*) &caddr,&caddrlen);
              sscanf(buf_recv,"%d",&c_size);
           }

            continua=1;

	    while(c_size>0 && continua==1){
                 
	      memset(buf_recv,0,sizeof(buf_recv));   // se non funziona prova a fare buf_recv[0]='\0';
              memset(buf_send,0,sizeof(buf_send));
              buf_send[0]='\0';
              size=read(fdR, buf_tmp,c_size);

    //          printf("Letti %d bytes.\n",size);
              sprintf(buf_send,"%s %"PRId32" %d ",file,down_seq,size);
              dim=strlen(buf_send);
              memcpy(buf_send+dim,buf_tmp, size); // a questo punto in buf_send ho correttamente il nome del file, il num seq e "size" byte letti/ da inviare
  //            printf("Letto il frammento numero %d.\n",down_seq);

	      my_sendto(nuovo,buf_send,(dim+size),0,(struct sockaddr *)&caddr,caddrlen);

              /*printf("Inviato il frammento numero %d, contenente %d byte.\n",down_seq,size); */

              tentativi=0;
              prove=0;
              do{
                 timeout.tv_sec=TIMEOUT;
	         timeout.tv_usec=0;

                 FD_ZERO(&sset);
                 FD_SET(nuovo,&sset);

                 if(select(FD_SETSIZE,&sset,NULL,NULL,&timeout)>0){ // se non scatta il timeout ma ricevo dati, ne controllo il numero di sequenza

                    dim3=my_recvfrom(nuovo,ack_recv,ACKLEN,0,(struct sockaddr*) &caddr,&caddrlen); // controllo il numero di sequenza
                    ack_recv[dim3]='\0';
                    sscanf(ack_recv,"%"PRId32"",&recv_seq);

		    ret_val=check_down_abort(recv_seq,file,fdR); // controlla se deve essere fatto abort o quit del trasferimento

                    if(ret_val==1){ /* il client mi ha chiesto QUIT o ABORT*/
                       break;
                    }

                    if(down_seq!=recv_seq){
                     //  sleep(1);
                       printf("\nCORRENTE: %"PRId32" - RICEVUTO: %"PRId32"",down_seq,recv_seq);
                       //printf("\nRICEVUTO: %s",ack_recv);
                       my_sendto(nuovo,buf_send,(dim+size),0,(struct sockaddr *)&caddr,caddrlen);
                       prove++;
                     
                       if(prove==3){
                          ret_val=1;
                          
                          flock(fdR,LOCK_UN);
                          my_close(fdR);
                          printf("Rilasciato lock condiviso su file descriptor..\n\n");
                          break;
                       }
                    }
                 }else{ // se scatta il timeout senza aver ricevuto risposta reinvio il pacchetto
                    printf("\nTimeout scaduto: invio ancora il pacchetto %"PRId32". Tentativo [%d]\n",down_seq,tentativi+1);
                    my_sendto(nuovo,buf_send,(dim+size),0,(struct sockaddr *)&caddr,caddrlen);
                    tentativi++;

                    if(tentativi==3){
                       ret_val=1;
                       
                       flock(fdR,LOCK_UN);
                       my_close(fdR);
                       printf("Rilasciato lock condiviso su file descriptor..\n\n");
                       break;
                    }
                 } 
              }while((tentativi<3 && down_seq!=recv_seq) || (prove<3 && down_seq!=recv_seq));
               
              if(ret_val==0){
	      down_seq++;

	      timeout.tv_sec=TIMEOUT;
	      timeout.tv_usec=0;
              FD_ZERO(&sset);
              FD_SET(nuovo,&sset);

              memset(buf_recv,0,sizeof(buf_recv));

              if(select(FD_SETSIZE,&sset,NULL,NULL,&timeout)>0){ 
                 size=my_recvfrom(nuovo,buf_recv,BUFLEN,0,(struct sockaddr*) &caddr,&caddrlen);
                 buf_recv[size]='\0';
                 c_size=atoi(buf_recv);

                 if(c_size==0){
                    continua=0;
                 }
              }
 
              memset(buf_send,0,sizeof(buf_send));
              memset(buf_tmp,0,sizeof(buf_tmp));

             }else{
                break;
             }

           } // fine invio file

           if(ret_val==0){
              printf("\nFile '%s' inviato correttamente.\n\n",file);

              flock(fdR,LOCK_UN);
              my_close(fdR);
              printf("Rilasciato lock condiviso su file descriptor..\n\n");
           }else{
              printf("Interrotto invio di '%s'.\n",file); /* in questo caso il descrittore del file è chiuso appena è chiesto QUIT o ABORT*/
           }

          }
        
           memset(buffer2,0,sizeof(buffer2));
           sprintf(buffer2,"%d %d",elenco[i].pid,0);	
           dim=strlen(buffer2);
           buffer2[dim]='\0';

           write(elenco[i].pipeCF[WRITE],buffer2,dim);
           kill(getppid(),SIGUSR1);
      }else{ /* fine DOWN, vai con UP*/

/*#########################################################################################
                     UPLOAD DEL FILE
############################################################################################*/

         if(strcmp(choice,"UP")==0 || strcmp(choice,"up")==0){

           /* printf("LOG NAME: %s\n",log_name);*/
         // invio file(s)
            printf("Ricevuto comando '%s %s'.\n",choice,file);
            up_seq=0;
            flag=0;
            prove=0;
            tentativi=0;
            abort=0;

            my_recvfrom(nuovo,buf_recv,BUFLEN,0,(struct sockaddr*) &caddr,&caddrlen); /* usata dal figlio per conoscere l'identità del client*/
            memset(buf_recv,0,sizeof(buf_recv));

            fdW=my_openW(fdW,file);

	    // lock sul file descriptor per la fase di UP
	    if((flock(fdW,LOCK_EX | LOCK_NB))==-1){

               abort=1;

               memset(buf_send,0,sizeof(buf_send));
               sprintf(buf_send,"STOP");
               dim2=strlen(buf_send);
               buf_send[dim2]='\0';
               my_sendto(nuovo,buf_send,dim2,0,(struct sockaddr*) &caddr,caddrlen);
               memset(buf_send,0,sizeof(buf_send));

               memset(choice,0,sizeof(choice));
               memset(file,0,sizeof(file));

            }else{

               printf("[%d] RICEVUTO DA PADRE = %s %s\n",elenco[i].pid,choice,file);

               ftruncate(fdW,0); /* tronca il file al byte 0 */
               log_ptr=create_log(file,log_name);

               memset(buf_send,0,sizeof(buf_send));
               sprintf(buf_send,"GO");
               dim2=strlen(buf_send);
               buf_send[dim2]='\0';
               my_sendto(nuovo,buf_send,dim2,0,(struct sockaddr*) &caddr,caddrlen);
               memset(buf_send,0,sizeof(buf_send));

               printf("Acquisito lock esclusivo su file descriptor..\n\n");

               printf("Aperto descrittore file in scrittura.\n");
               printf("Ricevo il file '%s'.\n",file);
            }

           if(abort==0){
            do{
               timeout.tv_sec=TIMEOUT;
	       timeout.tv_usec=0;
               FD_ZERO(&sset);
               FD_SET(nuovo,&sset);

               memset(buf_recv,0,sizeof(buf_recv));

               if(select(FD_SETSIZE,&sset,NULL,NULL,&timeout)>0){ // se non scatta il timeout ma ricevo dati, ne controllo il numero di sequenza
	          size=my_recvfrom(nuovo,buf_recv,BUFLEN,0,(struct sockaddr*) &caddr,&caddrlen);
                  // se ricevo dati controllo numero di sequenza e file e poi scrivo se è il caso
                  memset(recv_data,0,sizeof(recv_data));
                  sscanf(buf_recv,"%s %"SCNd32" %d",recv_file,&recv_seq,&recv_bytes);
                  
                  abort=check_up_abort(recv_seq,file,fdR,log_ptr);
                  if(abort==1){
                     clear(log_name); /* l'eliminazioe del file è fatta nella check_up_abort */
                     break;
                  }
                  
                  if(abort==2){
                     clear(log_name); /* l'eliminazioe del file è fatta nella check_up_abort */
                     break;
                  }

                if((strcmp(recv_file,"EOF"))!=0){
                  dim=size-recv_bytes;
                    // printf("\n dim=%d size=%d recv_bytes=%d",dim,size,recv_bytes);
                  memcpy(recv_data,buf_recv+dim,recv_bytes);
                  /* printf("Ricevuto frammento %d, atteso %d\n",recv_seq,up_seq);*/
  
                  if((strcmp(recv_file,file)==0) && (up_seq==recv_seq)){

               	     if(write(fdW,recv_data,recv_bytes)!=recv_bytes){  
                        printf("Errore scrittura dati.\n");
               		exit(1);
                     }
		     // invio ACK
                     sprintf(buf_send,"%"PRId32"",up_seq);
                     dim=strlen(buf_send);
                     buf_send[dim]='\0';
                     my_sendto(nuovo,buf_send,dim,0,(struct sockaddr*) &caddr,caddrlen);

                     up_seq++;
                     memset(buf_send,0,sizeof(buf_send));

            	     }else{  // numeri di sequenza diversi: richiedo il frammento giusto
                       /* sleep(1);*/
                        printf("[%d]Nome file o seq sbagliati..\n",prove+1);
                        sprintf(buf_send,"%"PRId32"",up_seq);
                        dim=strlen(buf_send);
                        buf_send[dim]='\0';
                        my_sendto(nuovo,buf_send,dim,0,(struct sockaddr*) &caddr,caddrlen);

                        prove++;

                        if(prove==3){
                           printf("Ho ricevuto 3 volte la sequenza sbagliata e non quella giusta: cancello il file.\n");
                           abort=1;

                           my_close(fdW);
                           flock(fileno(log_ptr),LOCK_UN);
                           flock(fdW,LOCK_UN);
                           clear(file);
                           clear(log_name);
                           printf("Rilasciato lock esclusivo su file descriptor..\n\n");

                           break;  /* forzo l'uscita dal ciclo di ricezione file e impedisco l'invio del paccketto finale*/
                        }
                     }
                  }

                  }else{ // timeout scaduto

                     printf("[%d]Timeout scaduto - frammento non ricevuto.\n",tentativi+1);
                     my_sendto(nuovo,buf_send,dim,0,(struct sockaddr*) &caddr,caddrlen);

                     tentativi++;
                   
                     if(tentativi==3){ /* esco dal ciclo, chiudo fd, rimuovo log e file incompleto */
                        printf("Dopo 3 tentativi di ricezione ack, non ci sono riuscito: cancello il file.\n");
                        abort=1;
 
                        my_close(fdW);
                        flock(fileno(log_ptr),LOCK_UN);
                        flock(fdW,LOCK_UN);
                        clear(file);
                        clear(log_name);
                        printf("Rilasciato lock esclusivo su file descriptor..\n\n");

                        break;  /* forzo l'uscita dal ciclo di ricezione file e impedisco l'invio del paccketto finale*/
                     }
                  }
            }while((strcmp(recv_file,"EOF"))!=0);  /* condizione di terminazione file*/

            if((abort!=1)&&(abort!=2)){

               sprintf(buf_send,"OK");
               dim=strlen(buf_send);
               buf_send[dim]='\0';   
 
               my_sendto(nuovo,buf_send,dim,0,(struct sockaddr*) &caddr,caddrlen);
               my_close(fdW);
               printf("File '%s' ricevuto correttamente.\n",file);
               clear(log_name); /* elimino il file di log se tutto va bene */
               flock(fdW,LOCK_UN);
               flock(fileno(log_ptr),LOCK_UN);
               printf("Rilasciato lock esclusivo su file descriptor..\n\n");

            }
          }
//            printf("Mi preparo a rilasciare il lock..\n");
	    // rilascio lock sul file descriptor per la fase di U
            

            memset(buffer2,0,sizeof(buffer2));
            sprintf(buffer2,"%d %d",elenco[i].pid,0);	
            dim=strlen(buffer2);
            buffer2[dim]='\0';

            write(elenco[i].pipeCF[WRITE],buffer2,dim);
            kill(getppid(),SIGUSR1);

	 }else{
	    // invio codice di errore
            printf("Richiesta non valida da parte del client.\n");

         }
      } // fine else upload
     
    } // fine while


    } // chiusura if per i figli
    } // chiusura for dei figli

        child_index=0;
        empty=0;
        ext_porta=0;

        signal(SIGUSR1,(void*)manage_children);

        for(j=0; j<FIGLI; j++){
           my_close(elenco[j].pipeCF[WRITE]);
           my_close(elenco[j].pipeFC[READ]);

	   memset(buffer2,0,sizeof(buffer2));
           f_port+=4;
	   sprintf(buffer2,"%d",f_port);
           dim=strlen(buffer2);
           buffer2[dim]='\0';
           write(elenco[j].pipeFC[WRITE],buffer2,dim); /* scrivo la nuova porta al figlio e la salvo*/
           elenco[j].porta=f_port;
           memset(buffer2,0,sizeof(buffer2));
           //printf("PADRE: HO COMUNICATO LA PORTA AL FIGLIO\n");

           read(elenco[j].pipeCF[READ],buffer3,MAX_CHAR); // leggo pid e stato figlio
           dim=strlen(buffer3);
           buffer3[dim]='\0';
           sscanf(buffer3,"%d %d",&(elenco[i].pid),&(elenco[i].busy));
          // printf("CHILD_PID: %d --- CHILD_STATUS: %d\n",elenco[i].pid,elenco[i].busy);
         //  my_signal(elenco[j].semaforo);
        }
       
   while(1){
     //   memset(buf_recv,0,sizeof(buf_recv));     // ricevo comando e nome file da client -- la recvfrom è bloccante
        size=my_recvfrom(attesa,buf_recv,BUFLEN,0,(struct sockaddr*) &caddr,&caddrlen);
        /*printf("PADRE: ricevuta richiesta da client..\n");*/
        buf_recv[size]='\0';

        sscanf(buf_recv,"%s %s",new_choice,new_file);
        check_file=access(new_file,F_OK);

        if((check_file==0) || ((strcmp(new_choice,"UP"))==0)){ /* il file è presente o ricevo UP*/

           if((strcmp(new_choice,"DOWN"))==0){
              printf("File '%s' presente..\n",new_file);
              sprintf(buffer3,"OK");
              dim3=strlen(buffer3);
              buffer3[dim3]='\0';
           
              my_sendto(attesa,buffer3,dim3,0,(struct sockaddr *)&caddr,caddrlen);
           }
    
           if((check_for_free(elenco,FIGLI,&child_index,&child_port))!=0){ // il primo figlio libero può servire una nuova richiesta

              memset(buffer3,0,sizeof(buffer3));

	      sprintf(buffer3,"%d",child_port);
              dim3=strlen(buffer3);
              buffer3[dim3]='\0';
           
              my_sendto(attesa,buffer3,dim3,0,(struct sockaddr *)&caddr,caddrlen); // comunico al client la porta del server figlio con cui deve comunicare
              write(elenco[child_index].pipeFC[WRITE],buf_recv,size); // comunico al figlio comando e nome

              elenco[child_index].busy=1;
           
           }else{
              inserisci(&head,&tail,new_choice,new_file,caddr);
           }

            memset(buf_recv,0,sizeof(buf_recv));
         }else{
            
            printf("File '%s' assente, lo comunico al client..\n",new_file);
            sprintf(buffer3,"NO");
            dim3=strlen(buffer3);
            buffer3[dim3]='\0';
           
            my_sendto(attesa,buffer3,dim3,0,(struct sockaddr *)&caddr,caddrlen);
         }
     }

   my_close(attesa);

return(0);
}

SOCKET my_socket(int family,int type,int protocol){

   SOCKET s;

	if((s=socket(family,type,protocol))<0){
           printf("Errore creazione socket.\n");
           exit(1);
        }
   return(s);
}

int check_for_free(child_info *elenco,int dim,int *child_index,int *child_port){
 
   int i;

   for(i=0; i<dim; i++){
      if(elenco[i].busy==0){
         (*child_index)=i;
         (*child_port)=elenco[i].porta;
         return(1);
      }
   }
   return(0);
}

void init_struct(child_info *elenco,int dim){

   int i;

   for(i=0; i<dim; i++){
      my_init(elenco[i].pipeFC);
      my_init(elenco[i].pipeCF);
      elenco[i].busy=0;
      elenco[i].pid=0;
      elenco[i].porta=0;
   }
}

FILE *create_log(char *file,char *log_name){

   int dim;
   FILE *log_ptr;

   sprintf(log_name,"%s%s",file,LOG_EXT);
   dim=strlen(log_name);
   log_name[dim]='\0';

   if((log_ptr=fopen(log_name,"w+"))==NULL){ /* creo il file di log */
         printf("Errore apertura file di log '%s' in scrittura.\n",log_name);
         printf("Termino le operazioni.\n");
         return (NULL);
   }

   flock(fileno(log_ptr),LOCK_EX);

   fprintf(log_ptr,"%s",file);
   
   fclose(log_ptr);
   return(log_ptr);
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

void clear(char *nome_file){

   if((remove(nome_file))==-1){
      printf("Errore eliminazione file '%s'.\n",nome_file);
   }else{
      printf("File '%s' eliminato correttamente.\n",nome_file);
   }
}

void manage_children(void){ /* per capire se uno o più figli terminano*/
   // leggo da pipe le info del figlio: pid e stato
   // imposto lo stato nella struct
   int i,empty,dim,dim2;
   char buffer[9];
   char buf_send[20];
   char buffer2[100];
   char ext_file[MAX_CHAR];
   char ext_choice[MAX_CHAR];
   fd_set fdset;
   struct timeval timeout;
   struct sockaddr_in ext_caddr;
   socklen_t caddrlen = sizeof(ext_caddr);

   for(i=0; i<FIGLI; i++){ /* controllo tutte le pipe per vedere se altri hanno finito*/
                           /* uso select perchè se un figlio non scrive su pipe mi bloccherei*/
      timeout.tv_sec=0;
      timeout.tv_usec=WAIT_PIPES;
      FD_ZERO(&fdset);
      FD_SET(elenco[i].pipeCF[READ],&fdset);

      if(select(FD_SETSIZE,&fdset,NULL,NULL,&timeout)>0){
         read(elenco[i].pipeCF[READ],buffer,9);
         sscanf(buffer,"%d %d",&(elenco[i].pid),&(elenco[i].busy));
         printf("Il figlio [%d] ha finito.\n",elenco[i].pid);

	 /* check lista per vedere se devo servire qualcuno */
         empty=vuota(&head);

         if(empty==0){ /* lista NON vuota: estraggo e servo */
          
            estrai(&tail,&head,ext_choice,ext_file,&ext_caddr);

	    sprintf(buf_send,"%d",elenco[i].porta);  /* porta del figlio libero */
            dim=strlen(buf_send);
            buf_send[dim]='\0';

            memset(buffer2,0,sizeof(buffer2));

            sprintf(buffer2,"%s %s",ext_choice,ext_file);  /* comando e nome */
            dim2=strlen(buffer2);
            buffer2[dim2]='\0';

            my_sendto(attesa,buf_send,dim,0,(struct sockaddr *)&ext_caddr,caddrlen); // comunico al client la porta del server figlio con cui deve comunicare
            write(elenco[i].pipeFC[WRITE],buffer2,dim2); // comunico al figlio comando e nome
         }

      }
   }

   signal(SIGUSR1,(void*)manage_children);
}

int my_openR(int fd, char *nome){

   if((fd=open(nome, O_RDONLY))<0){
      printf("Errore apertura file in lettura: %s.\n",strerror(errno));
      exit(1);
   }
   return(fd);
}

int my_openW(int fd, char *nome){

   if((fd=open(nome,O_WRONLY | O_CREAT,S_IRUSR | S_IWUSR))<0){
      printf("Errore apertura file in lettura.\n");
      exit(1);
   }
   return(fd);
}

int check_up_abort(int32_t recv_seq,char* nome,int fdK,FILE *log_ptr){ /* se il client fa ABORT chiudo fd e rimuovo il file*/

   if(recv_seq==-2){
      printf("Ricevuto segnale ABORT, termino ogni operazione con il client.\n");

      flock(fdK,LOCK_UN);
      my_close(fdK);
      flock(fileno(log_ptr),LOCK_UN);
      printf("Rilasciato lock esclusivo su file descriptor..\n\n");
      clear(nome);
      return(1);
   }

   if(recv_seq==-1){
      printf("Ricevuto segnale QUIT, termino ricezione corrente.\n");

      flock(fdK,LOCK_UN);
      my_close(fdK);
      flock(fileno(log_ptr),LOCK_UN);
      printf("Rilasciato lock esclusivo su file descriptor..\n\n");
      clear(nome);
      return(2);
   }

return(0);
}

void my_init(int *sem){
   
   if(pipe(sem)<0){
      printf("Error in semaphore initialization.\n");
      exit(1);
   }
}

int check_down_abort(int32_t recv_seq,char* nome,int fdK){

   if(recv_seq==-2){
      printf("Ricevuto segnale ABORT, termino ogni operazione con il client.\n");

      flock(fdK,LOCK_UN);
      my_close(fdK);
      printf("Rilasciato lock condiviso su file descriptor..\n\n");
      return(1);
   }

   if(recv_seq==-1){
      printf("Ricevuto segnale QUIT, termino invio file '%s'.\n",nome);

      flock(fdK,LOCK_UN);
      my_close(fdK);
      printf("Rilasciato lock condiviso su file descriptor..\n\n");
      return(1);
   }
return(0);
}

void my_close(int fd){

   if(close(fd)<0){
      printf("Errore chiusura file descriptor: '%s'.\n",strerror(errno));
      return;
   }
}

ssize_t my_recvfrom (int fd, void *bufptr, size_t nbytes, int flags, SA *sa, socklen_t *salenptr){

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

richiesta_ptr nuova(char *scelta,char *nome,struct sockaddr_in caddr){
   
   richiesta_ptr nuovo=NULL;
   int dim;
   int dim2;

   nuovo=malloc(sizeof(richiesta));

   if(nuovo==NULL){
      printf("Errore allocazione nodo lista.\n");
      exit(1);
   }
/* mannaggia alla strcpy !!!*/
   sprintf((nuovo->scelta),"%s",scelta);
   dim=strlen(nuovo->scelta);
   nuovo->scelta[dim]='\0';

   sprintf((nuovo->nome_file),"%s",nome);
   dim2=strlen(nuovo->nome_file);
   nuovo->nome_file[dim2]='\0';

/*   strcpy(nuovo->scelta,scelta);
   strcpy(nuovo->nome_file,nome);*/
   nuovo->client_address=caddr;
   nuovo->next=NULL;
 
 /*  printf("VALORE DI SCELTA: %s\n",scelta);

   printf("INSERITO: %s %s\n",nuovo->scelta,nuovo->nome_file);*/

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

void free_list(richiesta_ptr *head){

   richiesta_ptr tmp;

   while((*head)!=NULL){
      tmp=*head;
      *head=(*head)->next;

      free(tmp);
   }
}

void estrai(richiesta_ptr *tail,richiesta_ptr *head,char *scelta,char *nome,struct sockaddr_in *caddr){

     richiesta_ptr tmp;

     if((*head)!=NULL){
        strcpy(scelta,(*head)->scelta);
        strcpy(nome,(*head)->nome_file);
        (*caddr)=(*head)->client_address;

      /*  printf("ESTRATTO: %s %s",scelta,nome);    */

        tmp=*head;
        *head=(*head)->next;
     }else{
        *tail=NULL;
        //printf("Lista dei comandi vuota, impossibile estrarre..\n");
     }
     free(tmp);
}

void inserisci(richiesta_ptr *head,richiesta_ptr *tail,char *choice,char *nome,struct sockaddr_in caddr){

   richiesta_ptr nuovo;

   nuovo=nuova(choice,nome,caddr);

   if((*head)==NULL){
      *head=nuovo;
   }else{
      (*tail)->next=nuovo;
   }

   *tail=nuovo;
}
