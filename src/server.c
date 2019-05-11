#include "../utils/server.h"

struct client_list *client_list;

int main(int argc, char *argv[]) {
	int sockfd, newsockfd, n;
	socklen_t clilen;
	pthread_t clientThread, syncThread;
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
	
	if(responseThread.payloadCommand) {
      if(pthread_create(&clientThread, NULL, client_thread, &newsockfd)) {
        printf("ERROR creating thread\n");
        return -1;
      }
	} else {
		if(pthread_create(&syncThread, NULL, sync_thread_sv, &newsockfd)) {
        	printf("ERROR creating sync thread\n");
        	return -1;
      	}
	}



	// /* write in the socket */ 
	// struct packet connected;
	// connected.payloadCommand = 1;
	// connected.type = RESP;
	// n = write(newsockfd, &connected, sizeof(struct packet));
	// close(newsockfd);

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

void *client_thread (void *socket) {
  int byteCount, connected;
  int *client_socket = (int*)socket;
  char username[USER_MAX_NAME];
  struct client client;
  struct packet clientThread;

  // lê os dados de um cliente
  byteCount = read(*client_socket, &clientThread, sizeof(struct packet));
  strcpy(username, clientThread._payload);
  printf("username %s \n", username);
  
  // erro de leitura
  if (byteCount < 1)
    printf("ERROR reading from socket\n");

  // inicializa estrutura do client
  if (initializeClient(*client_socket, username, &client) > 0)
  {
	// avisamos cliente que conseguiu conectar  
	struct packet conSucc;
	conSucc.payloadCommand = 1;
	conSucc.type = RESP;
	byteCount = write(*client_socket, &conSucc, sizeof(struct packet));
      if (byteCount < 0) {
		printf("ERROR sending connected message 1\n");
	  }
    
	printf("%s connected!\n", username);
  }
  else {
    // avisa cliente que não conseguimos conectar
	struct packet conSucc;
	conSucc.payloadCommand = 0;
	conSucc.type = RESP;
	byteCount = write(*client_socket, &conSucc, sizeof(struct packet));
	if (byteCount < 0) {
		printf("ERROR sending connected message 2\n");
	}
    return NULL;
  }

//   listen_client(*client_socket, username); TODO
}

int initializeClient(int client_socket, char *username, struct client *client) {
  struct client_list *client_node;
  struct stat sb;
  int i;

  // não encontrou na lista ---- NEW CLIENT
  if (!findNode(username, client_list, &client_node)) {
    client->devices[0] = client_socket;
    client->devices[1] = DEVICE_FREE;
    strcpy(client->username, username);

    for(i = 0; i < FILE_MAX; i++) {
      client->file_info[i].size = -1;
    }
    client->logged = 1;

    // insere cliente na lista de client
    insertList(&client_list, *client);
  }
  // encontrou CLIENT na lista, atualiza device
  else {
    if(client_node->client.devices[0] == DEVICE_FREE) {
      client_node->client.devices[0] = client_socket;
    } else if (client_node->client.devices[1] == DEVICE_FREE) {
      client_node->client.devices[1] = client_socket;
    }
    // caso em que cliente já está conectado em 2 dipostivos
    else return -1;
  }

  if (stat(username, &sb) == 0 && S_ISDIR(sb.st_mode)) {
    // usuário já tem o diretório com o seu nome
	 printf("User %s Already Have Dir\n", username);
  } else {
    if (mkdir(username, 0777) < 0) {
      // erro
      if (errno != EEXIST)
        printf("ERROR creating directory\n");
    }
    // diretório não existe
    else {
      printf("Creating %s directory...\n", username);
    }
  }

  return 1;
}

int findNode(char *username, struct client_list *client_list, struct client_list **client_node) {
	struct client_list *client_list_aux = client_list;
	while(client_list_aux != NULL) {
		if (strcmp(username, client_list_aux->client.username) == 0) {
			*client_node = client_list_aux;
			return 1;
		}
		else
			client_list_aux = client_list_aux->next;
	}
	return 0;
}

void *sync_thread_sv(void *socket) {
  int byteCount, connected;
  int *client_socket = (int*)socket;
  char username[USER_MAX_NAME];
  struct packet clientThread;

  // lê os dados de um cliente
  byteCount = read(*client_socket, &clientThread, sizeof(struct packet));
  strcpy(username, clientThread._payload);

  // erro de leitura
  if (byteCount < 1)
    printf("ERROR reading from socket\n");

   listen_sync(*client_socket, username);
}

void listen_sync(int client_socket, char *username) {
  int byteCount, command;
  struct packet responseThread;

  do {	  	
	/* read from the socket */
	byteCount = read(client_socket, &responseThread, sizeof(struct packet));
	if (byteCount < 0) {
		printf("ERROR reading from socket");
	} 

      switch (responseThread.payloadCommand) {
        case UPLOAD: printf("RECEIVE UPLOAD CMD"); break;
        case DOWNLOADALL: printf("RECEIVE DOWNLOADALL CMD"); break;
        case DELETE: printf("RECEIVE DOWNLOADALL CMD"); break;
        case EXIT: ;break;
        default: break;
      }
  } while(responseThread.payloadCommand != EXIT);
}