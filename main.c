/**
 * @file main.c
 * @author Gaston Valdez gvaldez@iar.unlp.edu.ar 
 * @brief  Proyecto creado para la catedra Sistemas Operativos de propósito 
 * 		   General 
 * @version 0.1
 * @date 2022-08-09
 * 
 * @copyright Copyright (c) 2022
 * 
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h> 
#include "SerialManager.h"
#include <assert.h>
#include <pthread.h>
#include <unistd.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#define PORT_TCP_INTERFACE_SERVICE 10000 ///! DEL TP 
#define SOCKET_CONNECT_VALUE 	 1 
#define SOCKET_DISCONNECT_VALUE -1 
#define DISCONNECT_SERIAL_SERVICE 1 

void* serial_service_uart(void *pv) ; 
void* tcp_server_connect (void *par) ; 
void* tcp_server_receive (void *par) ; 
void tcp_server_transmit(void *par) ; 
int socket_connected_flag = 1 ; 
int socket_file_descriptor_tx = 0 ; 
/*
1) La función que lee datos del puerto serie no es bloqueante.
2) La función que lee datos del cliente tcp es bloqueante. No cambiar este comportamiento.
3) Dadas las condiciones de los puntos 1 y 2, se recomienda lanzar un thread para manejar la
   comunicación con el cliente TCP y otro (opcional, no necesario) para la comunicación con el puerto 
   serie 
4) No generar condiciones donde el uso del CPU llegue al 100% (bucles sin ejecución bloqueante).
5) El programa debe soportar que el cliente se desconecte, se vuelva a conectar y siga funcionando
el sistema.
6) El programa debe poder terminar correctamente si se le envía la signal SIGINT o SIGTERM.
7) Se debe considerar sobre qué hilo de ejecución se ejecutarán los handlers de las signals.
8) Los temas que deberán implementarse en el desarrollo son los siguientes:
   - Sockets
   - Threads
   - Signals
   - Mutexes (De ser necesario)
*/ 

///! void handlers_signals() { //SIGINT, SIGTERM ,  }

///! leer datos del puerto serial es non-blocking 




int main(void)
{
	pthread_t phtread_tcp ; 
	pthread_t phtread_serial ; 
 	pthread_create(&phtread_tcp, NULL, serial_service_uart, NULL);
 	pthread_create(&phtread_tcp, NULL, tcp_server_connect, NULL);
	
	pthread_join(phtread_tcp, NULL);
	pthread_join(phtread_serial, NULL);

	printf("end program ! ") ; 
	///tcp_server_receive() ; 
	exit(EXIT_SUCCESS);
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
		close(s);
		perror("listener: bind");
	}

	// Seteamos socket en modo Listening
	if (listen (s, 10) == -1) // backlog=10--> consultar ! 
  	{
		perror("error en listen");
    	exit(1);
  	}
	socket_connected_flag = 1 ; 
	while(socket_connected_flag)
	{
		addr_len = sizeof(struct sockaddr_in);
    	if ( (newfd = accept(s, (struct sockaddr *)&clientaddr,&addr_len)) == -1)
    	{
	    	perror("error accept"); 
	    	exit(1);
		}
		socket_file_descriptor_tx = newfd ; 
		pthread_create(&phtread_tx, NULL, tcp_server_receive, (void *)&newfd);
		pthread_create(&phtread_rx, NULL, tcp_server_receive, (void *)&newfd);
		pthread_join(phtread_tx,NULL ) ; 	
		pthread_join(phtread_rx,NULL ) ; 
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
				socket_connected_flag = 0 ; 
			}
			buffer[n]=0x00; //'\0' 
			printf("escribi %d bytes.:%s\n",n,buffer);	
		}	
	printf("end program after while \r\n") ; 
}



void tcp_server_transmit(void *buffer_tx){
		//! check error for socket connect 
		if (socket_file_descriptor_tx==0){
			return ; 
		}
		int socket_connected = 1 ; 
		char buffer[128];
		memcpy(buffer, buffer_tx,128) ;  //fuente, destindo
		int n;	
		 
		while(socket_connected_flag) 
		{ 
			n = write(socket_file_descriptor_tx, buffer, 20) ; 
			if( n == -1 )
			{
				perror("Error leyendo mensaje en socket");
				exit(1) ; 
			}else if(n == 0) { 
				socket_connected_flag = 0 ; //protejer con mutex 
			}
			buffer[n]=0x00 ; 
			printf("escribi %d bytes.:%s\r\n",n,buffer);	
		}	
	printf("end program after while \r\n") ; 
}






void* serial_service_uart(void *pv) { 
	uint8_t response_open  = serial_open(1,115200) ; 
	int rx_value = 0 ; 
	char buff[10]  ; 
	while(!response_open) ///! consultar sobre este caso 
	{ 		
    	rx_value = serial_receive(buff,10) ; 
		if (rx_value >0){ 
			printf("buffer rx: %s\r\n", buff) ; 
			tcp_server_transmit(buff) ; 
			memset(buff,'\0',10)  ;  ///! clean buffer 
		}else if (rx_value == 0 ){ 
			response_open = DISCONNECT_SERIAL_SERVICE ; 
		}
		sleep(1) ; 
	}
	serial_close() ; 

	printf("end thread of serial manage") ; 
}