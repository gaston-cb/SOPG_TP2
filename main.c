/**
 * @file main.c
 * @author Gaston Valdez gvaldez@iar.unlp.edu.ar 
 * @brief  Proyecto creado para la catedra Sistemas Operativos de propósito 
 * 		   General 
 * @version 0.1
 * @date 2022-08-09
 * 
 * @copyright Copyright (c) 2022
 *	1) La función que lee datos del puerto serie no es bloqueante.
 *	2) La función que lee datos del cliente tcp es bloqueante. No cambiar este comportamiento.
 *	3) Dadas las condiciones de los puntos 1 y 2, se recomienda lanzar un thread para manejar la
 * 	   comunicación con el cliente TCP y otro (opcional, no necesario) para la comunicación con el puerto 
 * 	   serie 
 *	4) No generar condiciones donde el uso del CPU llegue al 100% (bucles sin ejecución bloqueante).
 *	5) El programa debe soportar que el cliente se desconecte, se vuelva a conectar y siga funcionando
 *	el sistema.
 *	6) El programa debe poder terminar correctamente si se le envía la signal SIGINT o SIGTERM.
 *	7) Se debe considerar sobre qué hilo de ejecución se ejecutarán los handlers de las signals.
 *	8) Los temas que deberán implementarse en el desarrollo son los siguientes:
 *   - Sockets
 *   - Threads
 *   - Signals
 *   - Mutexes (De ser necesario)
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h> 
#include "SerialManager.h"
#include <assert.h>
#include <pthread.h>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define PORT_TCP_INTERFACE_SERVICE 10000 ///! DEL TP 
#define SOCKET_CONNECT_VALUE 	 1 
#define SOCKET_DISCONNECT_VALUE -1 
#define DISCONNECT_SERIAL_SERVICE 1 



void* serial_service_uart(void *pv) ; 
void* tcp_server_connect (void *par) ; 
void* tcp_server_receive (void *par) ; 
void tcp_server_transmit(char *par,int size_buff)  ; 
void signals_set() ; 
void block_signals() ; 
void unlock_signals() ; 

volatile sig_atomic_t socket_connected_flag	    = 0 ; 
volatile sig_atomic_t socket_file_descriptor_tx   = 0 ; 
volatile sig_atomic_t serial_interface_connected  = 0 ; 
volatile sig_atomic_t socket_connected_accept 	= 0 ; 
volatile sig_atomic_t cancel_program_thread =  0 ; 

pthread_mutex_t mutexData = PTHREAD_MUTEX_INITIALIZER ;//  connected_sockets_flag_protect ; 

//!ctrl + c 
void sighandler(int signal) { 
 
	serial_interface_connected = 0 ; 
	socket_connected_flag   = 0	; 
	socket_connected_accept = 0 ; 
	socket_connected_accept = 0 ; 
	cancel_program_thread = 0 ; 
}





int main(void)
{
	
	signals_set() ; 
	pthread_t phtread_tcp ; 
	pthread_t phtread_serial ; 
	block_signals() ; 
	pthread_create(&phtread_tcp, NULL, tcp_server_connect, NULL);

 	pthread_create(&phtread_serial, NULL, serial_service_uart, NULL);
	unlock_signals() ; 
	cancel_program_thread = 1 ; 	
	while (cancel_program_thread == 1 ){ sleep(2) ; }
 	pthread_cancel(phtread_tcp) ;  
	pthread_join(phtread_tcp, NULL);
	pthread_join(phtread_serial, NULL);
	exit(EXIT_SUCCESS);
}


void signals_set(){ 
    struct sigaction sa;
	sa.sa_handler = sighandler;
    //sa.sa_flags = (SIGPIPE | SIGTERM | SIGINT); // or SA_RESTART
    int sigaction_code_error ; 
	sigemptyset(&sa.sa_mask);
    if ( (sigaction_code_error = sigaction(SIGTERM ,&sa, NULL)) <0){
        printf("error sigaction with SIGTERM, error code: %d",sigaction_code_error) ; 
        exit(1) ; 
    }
    if ( (sigaction_code_error = sigaction(SIGPIPE ,&sa, NULL)) <0){
        printf("error sigaction with SIGPIPE, error code: %d",sigaction_code_error) ; 
        exit(1) ; 
    }
    if ( (sigaction_code_error = sigaction(SIGINT ,&sa, NULL)) <0){
        printf("error sigaction with SIGINT, error code: %d",sigaction_code_error) ; 
        exit(1) ; 
    }
}

void block_signals() { 
	sigset_t set;
    int s;
    sigemptyset(&set);
    sigaddset(&set, SIGINT | SIGPIPE | SIGTERM);
    pthread_sigmask(SIG_BLOCK, &set, NULL);

}

void unlock_signals(){ 
	sigset_t set;
    int s;
    sigemptyset(&set);
    sigaddset(&set, SIGINT);
    sigaddset(&set, SIGPIPE);
	sigaddset(&set, SIGTERM);
    pthread_sigmask(SIG_UNBLOCK, &set, NULL);
}




void* tcp_server_connect (void *par){ 
	pthread_t phtread_rx,phtread_tx  ;  
	socklen_t addr_len;
	struct sockaddr_in clientaddr;
	struct sockaddr_in serveraddr;
	
	int newfd;
	int socket_connected  = 0 ; 
	int s = socket(AF_INET,SOCK_STREAM, 0);
	bzero((char*) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_port = htons(PORT_TCP_INTERFACE_SERVICE);
    if(inet_pton(AF_INET, "127.0.0.1", &(serveraddr.sin_addr))<=0)
    {
      	fprintf(stderr,"ERROR invalid server IP\r\n");
    }
	if (bind(s, (struct sockaddr*)&serveraddr, sizeof(serveraddr)) == -1) {
		perror("reintentando conexión ");
		close(s) ; 
		exit(1) ; 
	}

	// Seteamos socket en modo Listening
	if (listen (s, 10) == -1) // backlog=10--> consultar ! 
  	{
		perror("error en listen");
    	exit(1);
  	}
	socket_connected_accept = 1 ; 
	while(socket_connected_accept)
	{
		addr_len = sizeof(struct sockaddr_in);
    	if ( (newfd = accept(s, (struct sockaddr *)&clientaddr,&addr_len)) == -1)
    	{
	    	perror("error accept"); 
	    	exit(1);
		}
		pthread_mutex_lock (&mutexData);
		socket_file_descriptor_tx = newfd ; 
		socket_connected_flag = 1 ; 
		pthread_mutex_unlock (&mutexData);
		pthread_create(&phtread_tx, NULL, tcp_server_receive, (void *)&newfd);
		pthread_join(phtread_tx,NULL ) ; 	
	}
	close(newfd) ; 
	close(s) ; 
}





void* tcp_server_receive(void *par)
{ 
		int *file_descriptor_socket ; 
		int socket_connected = 1 ; 
		char buffer[128];
		int n;	
		file_descriptor_socket = (int *)par ; 
		while(socket_connected_flag) 
		{ 
			n = read(*file_descriptor_socket, buffer, 128) ; 
			if( n == -1 )
			{
				perror("Error leyendo mensaje en socket");
				exit(1) ; 
			}else if(n == 0) { 
				pthread_mutex_lock (&mutexData);
				socket_connected_flag = 0 ; 
				pthread_mutex_unlock (&mutexData);
			}
			buffer[n]=0x00 ; //'\0' 
			printf("recibi %d bytes.:%s\n",n,buffer);	
			
			pthread_mutex_lock (&mutexData);
			if (serial_interface_connected == 1){
				serial_send(buffer,n ) ; 
			}
			pthread_mutex_unlock (&mutexData);
		
		}	
	printf("end program after receive \r\n") ; 
}



void tcp_server_transmit(char *buffer_tx, int size_buffer){
		printf("tcp_server_transmit \r\n") ; 
		if (socket_file_descriptor_tx==0){
			return ; 
		}
		int socket_connected = 1 ; 
		int n;	
		 
		if(socket_connected_flag) 
		{ 
			n = write(socket_file_descriptor_tx, buffer_tx, size_buffer) ; 
			if( n == -1 )
			{
				perror("Error leyendo mensaje en socket");
				socket_connected_flag = 0 ; 
				//exit(1) ; 
			}else if(n == 0) { 
				pthread_mutex_lock (&mutexData);
				socket_connected_flag = 0 ; //protejer con mutex 
				pthread_mutex_unlock (&mutexData);
			}
			printf("escribi %d bytes.:%s\r\n",n, buffer_tx);	
		}	
	printf("end transmit socket connect  \r\n") ; 
}


void* serial_service_uart(void *pv) { 
	int response_open  = serial_open(1,115200) ; 
	serial_interface_connected = 1 ; 
	int rx_value = 0 ; 
	char buff[10]  ; //if (!respose_open) -> response_open == 0 
	printf("response_open = %d\r\n",response_open) ; 
pthread_mutex_lock (&mutexData);
	while(serial_interface_connected) ///! consultar sobre este caso 
	{ 		
		rx_value = serial_receive(buff,10) ; 
		if (rx_value >0){ 
			printf("buffer rx: %s\r\n", buff) ; 
			tcp_server_transmit(buff,rx_value)  ; 
			memset(buff,'\0',10)  ;  ///! clean buffer 
		}else if (rx_value == 0 ){ 
			serial_interface_connected = 0 ;
			printf("disconnect to serial port\r\n") ; 
			serial_close() ; 
			response_open = DISCONNECT_SERIAL_SERVICE ; 
			socket_connected_flag = 1 ; 
		}
pthread_mutex_unlock (&mutexData);
		usleep(1000) ; 
	}
	printf("end thread of serial manage") ; 
pthread_mutex_lock (&mutexData);
	socket_file_descriptor_tx = 0 ; 
	socket_connected_flag = 0 ; //protejer con mutex 
	serial_interface_connected = 0 ; 
	socket_connected_accept = 0 ; 
pthread_mutex_unlock (&mutexData);
	printf("end thread serial manage") ; 
}