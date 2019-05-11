#include "../utils/server.h"

struct client_list *client_list;

int main(int argc, char *argv[]) {
	int sockfd, newsockfd, n;
	socklen_t clilen;
	struct sockaddr_in serv_addr, cli_addr;

	//Inicializando a lista de clients
	client_list = NULL;
	initializeClientList();

	// Comunicação
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
	while(1) {
	if ((newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen)) == -1){
		printf("ERROR on accept");
		return -1;
	} 
		
	
	struct packet responseThread;
	bzero(&responseThread, sizeof(struct packet));
	
	/* read from the socket */
	n = read(newsockfd, &responseThread, sizeof(struct packet));
	if (n < 0) {
		printf("ERROR reading from socket");
	} 
	
	if(responseThread.shouldCreateThread) {
		printf("payload: %s \n\n", responseThread._payload);
		printf("thread true \n");
		printf("thread true \n");
	} else {
		printf("payload: %s \n\n", responseThread._payload);
		printf("thread false \n");
		printf("thread false \n");
	}



	/* write in the socket */ 
	struct packet connected;
	strcpy(connected._payload, "1");
	connected.type = RESP;
	n = write(newsockfd, &connected, sizeof(struct packet));
	close(newsockfd);

	}
	close(sockfd);
	return 0; 
}

int initializeClientList() {

	struct client client;

	DIR *d, *userDir;
	struct dirent *dir, *userDirent;
	int i = 0;
	FILE *file_d;
	struct stat st;
	char folder[FILE_MAX], path[200];

  	d = opendir(".");
  	if (d){
    	while ((dir = readdir(d)) != NULL) {
		//testa se e um diretorio	
      	if (dir->d_type == DT_DIR && strcmp(dir->d_name,".")!=0 && strcmp(dir->d_name,"..")!=0) {
        	client.devices[0] = DEVICE_FREE;
          	client.devices[1] = DEVICE_FREE;
          	client.logged     = 0;

          	strcpy(client.username, dir->d_name);

          	userDir = opendir(dir->d_name);

          	strcpy(folder, dir->d_name);
          	strcat(folder, "/");

          	if (userDir) {
            	for(i = 0; i < FILE_MAX; i++) {
              	client.file_info[i].size = -1;
              	if (pthread_mutex_init(&client.file_info[i].file_mutex, NULL) != 0) {
                	printf("\n mutex init failed\n");
                  	return 1;
              	}
        	}
            i = 0;
            while((userDirent = readdir(userDir)) != NULL) {
              //testa se e um arquivo regular e se esta no root dir
			 	if(userDirent->d_type == DT_REG && strcmp(userDirent->d_name,".")!=0 && strcmp(userDirent->d_name,"..")!=0) {
                	strcpy(path, folder);
                	strcat(path, userDirent->d_name);

                 	stat(path, &st);

                 	strcpy(client.file_info[i].name, userDirent->d_name);
					client.file_info[i].size = st.st_size;
					client.file_info[i].lst_modified = st.st_mtime;

					strcpy(client.file_info[i].last_modified, ctime(&st.st_mtime));

					i++;
            	}
           	}
            insertList(&client_list, client);
          }
       }
	  }
    }
	closedir(d);
	return 0;
}

void insertList(struct client_list **client_list, struct client client) {
	struct client_list *client_node;
	struct client_list *client_list_aux = *client_list;

	client_node = malloc(sizeof(struct client_list));

	client_node->client = client;
	client_node->next = NULL;

	if (*client_list == NULL) {
		*client_list = client_node;
	}
	else {
		while(client_list_aux->next != NULL) {
			client_list_aux = client_list_aux->next;
		}
		client_list_aux->next = client_node;
	}
}