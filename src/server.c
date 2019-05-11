#include "../utils/config.h"
#include "../utils/server.h"
#define ENOUGH 100

int main(int argc, char *argv[])
{
	int sockfd, newsockfd, n;
	socklen_t clilen;
	struct sockaddr_in serv_addr, cli_addr;

	char buffer[256];

	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) 
        printf("ERROR opening socket");
	
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(PORT);
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	bzero(&(serv_addr.sin_zero), 8);     
    
	if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) 
		printf("ERROR on binding");
	
	listen(sockfd, 5);
	
	clilen = sizeof(struct sockaddr_in);
	if ((newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen)) == -1) 
		printf("ERROR on accept");
	
	struct packet response;
	bzero(&response, sizeof(struct packet));
	
	/* read from the socket */
	n = read(newsockfd, &response, sizeof(struct packet));
	if (n < 0) 
		printf("ERROR reading from socket");
	printf("Here is the message: %s\n", response._payload);


	/* write in the socket */ 
	struct packet connected;

	strcpy(connected._payload, "1");
	connected.type = RESP;


	n = write(newsockfd, &connected, sizeof(struct packet));
	if (n < 0) 
		printf("ERROR writing to socket");

	close(newsockfd);
	close(sockfd);
	return 0; 
}

void initializeServerConnection() {



}