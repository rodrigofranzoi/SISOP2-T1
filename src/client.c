#include <netdb.h> 
#include "../utils/config.h"
#include "../utils/client.h"


char userid[USER_MAX_NAME];
char *host;
int port;

int main(int argc, char *argv[])
{
    int sockfd, n;
    struct sockaddr_in serv_addr;
    struct hostent *server;
	
    char buffer[256];
	
    //Verifica os parametros e configura o host
    if(initHost(argv, argc) == -1) exit(0);

    

    connect_server(host, port)
    // if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) 
    //     printf("ERROR opening socket\n");
    
	// serv_addr.sin_family = AF_INET;     
	// serv_addr.sin_port = htons(PORT);    
	// serv_addr.sin_addr = *((struct in_addr *)server->h_addr);
	// bzero(&(serv_addr.sin_zero), 8);     
	
    
	// if (connect(sockfd,(struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0) 
    //     printf("ERROR connecting\n");

    // printf("Enter the message: ");
    // bzero(buffer, 256);
    // fgets(buffer, 256, stdin);
    
	// /* write in the socket */
	// n = write(sockfd, buffer, strlen(buffer));
    // if (n < 0) 
	// 	printf("ERROR writing to socket\n");

    // bzero(buffer,256);
	
	// /* read from the socket */
    // n = read(sockfd, buffer, 256);
    // if (n < 0) 
	// 	printf("ERROR reading from socket\n");

    // printf("%s\n",buffer);
    
	// close(sockfd);
    return 0;
}

int initHost(char *argv[], int argc) {
    int resp = 1;
    
    if (argc < 3) {
		printf("Insufficient arguments: user, hostname or port \n");
		return -1;
    }

    // primeiro argumento nome do usuário
	if (strlen(argv[1]) <= USER_MAX_NAME) {
        strcpy(userid, argv[1]);
    } else {
        printf("User Name Is Too long \n");
        return -1;
    };

    //segundo arumento hostname do server	
	host = malloc(sizeof(argv[2]));
	strcpy(host, argv[2]);


	// terceiro argumento porta
    port = atoi(argv[3]);

    return resp;
}

int connect_server (char *host, int port) {
	int socketByteSize, connected;
	struct sockaddr_in server_addr;
	struct hostent *server;
	int client_thread = 1;
	char buffer[256];


    // Inicializa Server
	server = gethostbyname(host);
	if (server == NULL) {
        printf("ERROR, no such host\n");
        return -1;
    }

	// Inicializa Socket
	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1){
		printf("ERROR opening socket\n");
		return -1;
	}

	// Inicializa Addr
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port);
	server_addr.sin_addr = *((struct in_addr *)server->h_addr);
	bzero(&(server_addr.sin_zero), 8);

	
    // Testa Socket
	if (connect(sockfd,(struct sockaddr *) &server_addr,sizeof(server_addr)) < 0) {
  		printf("ERROR connecting\n");
		return -1;
	}

	write(sockfd, &client_thread, sizeof(client_thread));

	// envia userid para o servidor
    socketByteSize = write(sockfd, userid, sizeof(userid));
    if (socketByteSize < 0) {
		printf("ERROR sending userid to server\n");
		return -1;
	}

	socketByteSize = read(sockfd, &connected, sizeof(int));

	if (socketByteSize < 0){
		printf("ERROR receiving connected message\n");
		return -1;
	} else if (connected == 1) {
		printf("connected\n");
		return 1;
	} else {
		printf("You already have two devices connected\n");
		return -1;
	}
}