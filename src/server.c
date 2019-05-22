#include "../utils/server.h"

struct client_list *client_list;

int main(int argc, char *argv[]) {
	int sockfd, newsockfd, n;
	socklen_t clilen;
	pthread_t clientThread, syncThread, syncServerClient;
	struct sockaddr_in serv_addr, cli_addr;
	struct socketPacket socketPacket;

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
			printf("ERROR reading from socket 1");
		}

		printf("Receive Initial Command %d\n", responseThread.payloadCommand);

		if(responseThread.payloadCommand == 1) {
			if(LOG_DEBUG)	printf("[LOG] - Creating Client CMD Thread\n");
				if(pthread_create(&clientThread, NULL, client_thread, &newsockfd)) {
					printf("ERROR creating thread\n");
					return -1;
				}
		} else if (responseThread.payloadCommand == 0) {
		if(LOG_DEBUG)	printf("[LOG] - Creating Client Server Sync Thread\n");
			socketPacket.socket = newsockfd;
			socketPacket.packet = responseThread;
			if(pthread_create(&syncThread, NULL, sync_thread_sv, &socketPacket)) {
				printf("ERROR creating sync thread\n");
				return -1;
			}
		} else if(responseThread.payloadCommand == 2){
		if(LOG_DEBUG)	printf("[LOG] - Creating Server Client Sync Thread\n");
			sync_thread_ServerClient(&newsockfd);
		}
	}
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
                  client.file_info[i].st = st;
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
	printf("[log] Initialized Client Thread\n");
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
    printf("ERROR reading from socket 2\n");

  // inicializa estrutura do client
  if (initializeClient(*client_socket, username, &client) > 0)
  {
	// avisamos cliente que conseguiu conectar
	printf("sending sucess...\n" );
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
  printf("sending error...\n" );
	struct packet conSucc;
	conSucc.payloadCommand = 0;
	conSucc.type = RESP;
	byteCount = write(*client_socket, &conSucc, sizeof(struct packet));
	if (byteCount < 0) {
		printf("ERROR sending connected message 2\n");
	}
    return NULL;
  }

  listen_client(*client_socket, username);
}

void listen_client(int client_socket, char *username) {
  int byteCount, command;
	struct packet clientRequest;

  do {
			bzero(&clientRequest, sizeof(clientRequest));
      byteCount = read(client_socket, &clientRequest, sizeof(struct packet));

			printf("COMMAND CLIENT: %d\n", clientRequest.payloadCommand);
      switch (clientRequest.payloadCommand) {
        case LIST: send_file_info(client_socket, username);break;
        case DOWNLOAD: send_file(clientRequest._payload, client_socket, username); break;
        case UPLOAD: receive_file(clientRequest, client_socket, username); break;
        case EXIT: close_client_connection(client_socket, username); break;
   //     default: printf("ERROR invalid command\n");
      }
  } while(clientRequest.payloadCommand != EXIT);
}

struct file_info getFileInfo(char *username, char*filename){
	struct client_list *client_node;
	struct file_info file;
	//insere socket do client no nodo para depois fazer a comunicacao
	if (findNode(username, client_list, &client_node)){
			for(int i = 0; i < FILE_MAX; i++){
				if(strcmp(client_node->client.file_info[i].name, filename)){
					return client_node->client.file_info[i];
				}
			}
	} else {
		if(LOG_DEBUG) printf("[LOG] - Client Node Not Found.\n");
	}

	return file;
}

void send_file(char *file, int socket, char *username) {
	int byteCount, bytesLeft, fileSize;
	FILE* ptrfile;
	char dataBuffer[KBYTE], path[KBYTE];
	struct file_info file_info;

	file_info = getFileInfo(username, file);

	pthread_mutex_lock(&file_info.file_mutex);

	printf("FILE NAME%s\n", file);
  strcpy(path, username);
  strcat(path, "/");
  strcat(path, file);
  if (ptrfile = fopen(path, "rb")) {
      fileSize = getFileSize(ptrfile);

			packet clientCMDSize;
			clientCMDSize.length = fileSize;
    	byteCount = write(socket, &clientCMDSize, sizeof(struct packet));

      while(!feof(ptrfile)) {
				packet clientCMDData;
					fread(&dataBuffer, sizeof(dataBuffer), 1, ptrfile);
					memcpy(&clientCMDData._payload, &dataBuffer, sizeof(dataBuffer));
					clientCMDData.length = sizeof(dataBuffer);
          byteCount = write(socket, &clientCMDData, sizeof(struct packet));
          if(byteCount < 0){
					  printf("ERROR sending file\n");
					}
					bzero(dataBuffer, sizeof(dataBuffer));
					usleep(100);
      }
      fclose(ptrfile);
  }
  // arquivo não existe
  else {
    fileSize = -1;
		packet clientCMDSize;
		clientCMDSize.length = fileSize;
    byteCount = write(socket, &clientCMDSize, sizeof(struct packet));
  }
	pthread_mutex_unlock(&file_info.file_mutex);
}


void close_client_connection(int socket, char *username) {
  struct client_list *client_node;
	int i, fileNum = 0;

  printf("Disconnecting %s\n", username);

	if (findNode(username, client_list, &client_node))
	{
    if(client_node->client.devices[0] == DEVICE_FREE){
      client_node->client.devices[1] = DEVICE_FREE;
			client_node->client.syncSocket[1] = DEVICE_FREE;
      client_node->client.logged = 0;
    }
    else if (client_node->client.devices[1] == DEVICE_FREE) {
      client_node->client.devices[0] = DEVICE_FREE;
			client_node->client.syncSocket[0] = DEVICE_FREE;
      client_node->client.logged = 0;
    }
    else if (client_node->client.devices[0] == socket) {
			client_node->client.devices[0] = DEVICE_FREE;
			client_node->client.syncSocket[0] = DEVICE_FREE;
		}
    else {
			client_node->client.devices[1] = DEVICE_FREE;
			client_node->client.syncSocket[1] = DEVICE_FREE;
		}
  }
}

void send_file_info(int socket, char *username) {
	printf("ENTROU DENTRO SEND FILE\n");
	struct client_list *client_node;
	struct client client;
	int i, fileNum = 0;

	if (findNode(username, client_list, &client_node)) {
		client = client_node->client;
		for (i = 0; i < FILE_MAX; i++) {
			if (client.file_info[i].size != -1)
				fileNum++;
		}

		printf("Encontrou %d arquivos\n", fileNum);

		struct packet clientListNum;
		clientListNum.type = DATA;
		clientListNum.payloadCommand = fileNum;
		write(socket, &clientListNum, sizeof(struct packet));

		printf("Enviou %d files\n", clientListNum.payloadCommand);

		for (i = 0; i < FILE_MAX; i++) {
			if (client.file_info[i].size != -1){
				printf("Enviando %s \n",client.file_info[i].name );
				write(socket, &client.file_info[i], sizeof(client.file_info[i]));
			}
			usleep(100);
		}
	}
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

		client->syncSocket[0] = -1;
		client->syncSocket[1] = -1;

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

void sync_thread_ServerClient(void *socket) {
	if(LOG_DEBUG) printf("[LOG] - Initializer Sync Server Thread\n");
	int start = 0;
  int byteCount, connected;
  int *client_socket = (int*)socket;
  char username[USER_MAX_NAME];
  struct packet clientThread;
	struct client_list *client_node;

  // lê os dados de um cliente
	if(LOG_DEBUG)	printf("[LOG] - Waiting to Read Name From Client\n");
  byteCount = read(*client_socket, &clientThread, sizeof(struct packet));
  strcpy(username, clientThread._payload);

	if(LOG_DEBUG) printf("[LOG] - Receive name %s from server--client thread\n", username);

  // erro de leitura
  if (byteCount < 1)
    printf("ERROR reading from socket 3\n");


	//insere socket do client no nodo para depois fazer a comunicacao
	if (findNode(username, client_list, &client_node)){
		if(LOG_DEBUG) printf("[LOG] - Inserted server--client socket to node\n");
		if(client_node->client.syncSocket[0] == 0 && client_node->client.syncSocket[1] == 0){
			client_node->client.syncSocket[0] = -1;
			client_node->client.syncSocket[1] = -1;
		}
		if(LOG_DEBUG) printf("[LOG] - SYNC 0: %d, SYNC 1 : %d\n", client_node->client.syncSocket[0], client_node->client.syncSocket[1]);
		if(client_node->client.syncSocket[0] == -1){
				if(LOG_DEBUG) printf("[LOG] - Sync To Socket 1\n");
				client_node->client.syncSocket[0] = *client_socket;
		} else if(client_node->client.syncSocket[1] == -1) {
				if(LOG_DEBUG) printf("[LOG] - Sync To Socket 2\n");
				client_node->client.syncSocket[1] = *client_socket;
		} else {
			if(LOG_DEBUG) printf("[LOG] - Sync To Socket 1\n");
		}
	} else {
		if(LOG_DEBUG) printf("[LOG] - Client Node Is Not Inicialized\n");
	}

   listen_syncServerClient(*client_socket, username);
}

void listen_syncServerClient(int client_socket, char *username) {
  int byteCount, command;
  struct packet clientThread;

	//comunica client que esta ok
	clientThread.type = DATA;
	clientThread.payloadCommand = 1;
	write(client_socket, &clientThread, sizeof(struct packet));
}


void *sync_thread_sv(void *socketPacket) {
	if(LOG_DEBUG)	printf("[LOG] - Initialized Sync Client Thread\n");
  int byteCount, connected;
	struct socketPacket *pkt = (struct socketPacket*) socketPacket;
  int client_socket = pkt->socket;
  char username[USER_MAX_NAME];
  struct packet clientThread;

  // lê os dados de um cliente
	printf("[LOG] - Waiting to Client Send Name\n");
  // byteCount = read(client_socket, &clientThread, sizeof(struct packet));
  strcpy(username, pkt->packet._payload);
	printf("[LOG] - Received Sync Client Thread Name: %s\n", username);

  // if (byteCount < 1)
  //   printf("ERROR reading from socket 4\n");


   listen_sync(client_socket, username);
}

void listen_sync(int client_socket, char *username) {
  int byteCount, command;
  struct packet responseThread;

  do {
		/* read from the socket */
		byteCount = read(client_socket, &responseThread, sizeof(struct packet));
		if (byteCount < 0) {
			printf("ERROR reading from socket 5");
		}
			printf("[LOG] - Receive SYNC CMD %d ", responseThread.payloadCommand);
      switch (responseThread.payloadCommand) {
        case UPLOAD: if(responseThread.length > 0) receive_file(responseThread, client_socket, username); break;
        case DOWNLOADALL: send_all_files(client_socket, username); break;
        case DELETE: delete_file(responseThread._payload, client_socket, username); break;
        case EXIT: ;break;
        // default: usleep(100); break;
      }
  } while(responseThread.payloadCommand != EXIT);
}


void signal_download2client(char *username, char *fileName, time_t *last_modified) {
	if(LOG_DEBUG) printf("[LOG] - Download Signal Function\n");
	struct client_list *client_node;
	int MAX_SOCKET = 2;
	int socket[MAX_SOCKET];
	time_t *mod_time = (time_t*)last_modified;

	if (findNode(username, client_list, &client_node)){
		if(LOG_DEBUG) printf("[LOG] - Found Client %s Node\n", client_node->client.username);
		for(int i = 0; i < MAX_SOCKET; i++){
			socket[i] = client_node->client.syncSocket[i];
		}
	}

	if(LOG_DEBUG) printf("[LOG] - Socket Signal Username: %s\n", username);
	if(LOG_DEBUG) printf("[LOG] - Socket Signal File: %s\n", fileName);
	if(LOG_DEBUG) printf("[LOG] - Socket Signal Last Modified %s", ctime(mod_time));

	for(int i = 0; i < MAX_SOCKET; i++){
		if(socket[i] != -1){
			int byteCount;
			struct packet clientThread;
			//signal client que esta ok
			clientThread.type = DATA;
			clientThread.payloadCommand = 1; // 1 CMD PARA SIGNAL DWONLOAD
			clientThread.lst_modified = *mod_time;
			strcpy(clientThread._payload, fileName);
			byteCount = write(socket[i], &clientThread, sizeof(struct packet));
			if(byteCount < 0){
				if(LOG_DEBUG) printf("[LOG] - Erro Sending Socket Signal\n");
			}
			printf("[LOG] - Socket Signal Sent\n");
		} else {
			if(LOG_DEBUG) printf("[LOG] - Socket2 Signal Not Found\n");
		}
	}
}

void signal_delete2client(char *username, char *fileName){
	if(LOG_DEBUG) printf("[LOG] - Delete Signal Function\n");
	struct client_list *client_node;
	int MAX_SOCKET = 2;
	int socket[MAX_SOCKET];

	if (findNode(username, client_list, &client_node)){
		if(LOG_DEBUG) printf("[LOG] - Found Client %s Node\n", client_node->client.username);
		for(int i = 0; i < MAX_SOCKET; i++){
			socket[i] = client_node->client.syncSocket[i];
		}
	}

	if(LOG_DEBUG) printf("[LOG] - Socket Signal Username: %s\n", username);
	if(LOG_DEBUG) printf("[LOG] - Socket Signal File: %s\n", fileName);

	for(int i = 0; i < MAX_SOCKET; i++){
		if(socket[i] != -1){
			int byteCount;
			struct packet clientThread;
			//signal client que esta ok
			clientThread.type = DATA;
			clientThread.payloadCommand = 2; // 1 CMD PARA SIGNAL DWONLOAD
			strcpy(clientThread._payload, fileName);
			byteCount = write(socket[i], &clientThread, sizeof(struct packet));
			if(byteCount < 0){
				if(LOG_DEBUG) printf("[LOG] - Erro Sending Socket Signal\n");
			}
			printf("[LOG] - Socket Signal Sent\n");
		} else {
			if(LOG_DEBUG) printf("[LOG] - Socket2 Signal Not Found\n");
		}
	}
}

//pthread_mutex_t t1, t2;
void send_all_files(int client_socket, char *username) {
	printf("CHEGOU DENTRO DO ALL FILES\n\n");
  int byteCount, bytesLeft, fileSize, fileNum=0, i;
  FILE* ptrfile;
  char dataBuffer[KBYTE], path[KBYTE];
  struct client_list *client_node;
	struct packet clientThread;

  if (findNode(username, client_list, &client_node)) {
    for(i = 0; i < FILE_MAX; i++) {
      if (client_node->client.file_info[i].size != -1){
				fileNum++;
			}
    }
  }
	printf("FILE COUNT: %d\n", fileNum);
	clientThread.type = DATA;
	clientThread.payloadCommand = fileNum;
  write(client_socket, &clientThread, sizeof(struct packet));

	int t = 0;

  for(i = 0; i < FILE_MAX; i++) {
		if (client_node->client.file_info[i].size != -1) {
      strcpy(path, username);
      strcat(path, "/");
      strcat(path, client_node->client.file_info[i].name);

			printf("sending file name: %s\n", client_node->client.file_info[i].name);

			struct packet clientPacketFile;
			clientPacketFile.type = DATA;
			strcpy(clientPacketFile._payload, client_node->client.file_info[i].name);
      write(client_socket, &clientPacketFile, sizeof(struct packet));
			if(byteCount < 0){
				printf("ERROR sending file NAME\n");
			}
			printf("ENVIOU file NAME\n");

    }
  }
}

int getFileSize(FILE *ptrfile) {
	int size;

	fseek(ptrfile, 0L, SEEK_END);
	size = ftell(ptrfile);
	rewind(ptrfile);

	return size;
}

pthread_mutex_t mutex;
void receive_file(struct packet responseThread, int socket, char*username) {
	if(LOG_DEBUG) printf("[LOG] - Receiving New File\n");
  int byteCount, bytesLeft, fileSize;
  FILE* ptrfile;
  char dataBuffer[KBYTE], path[200];
	char file[FILENAME_MAX_SIZE];
  struct file_info file_info;
  struct stat st;
  time_t now;

	pthread_mutex_lock(&file_info.file_mutex);

	fileSize = responseThread.length;
	strcpy(file, responseThread._payload);
  strcpy(path, username);
  strcat(path, "/");
  strcat(path, file);

	if(LOG_DEBUG) printf("[LOG] - Receiving New File Name: %s\n", file);
	if(LOG_DEBUG) printf("[LOG] - Receiving New File Lenght: %d\n", fileSize);
	if(LOG_DEBUG) printf("[LOG] - Receiving New File Lst Modified: %s\n", ctime(&responseThread.lst_modified));

  if (ptrfile = fopen(path, "wb")) {

      if (fileSize == 0) {
        fclose(ptrfile);

				printf("FILE SIZE IS == 0\n");

      	strcpy(file_info.name, file);
        strcpy(file_info.last_modified, ctime(&responseThread.lst_modified));
        file_info.lst_modified = responseThread.lst_modified;
        file_info.size = fileSize;
        stat(path, &st);
        file_info.st = st;

      	updateFileInfo(username, file_info);
        return;
      }

      bytesLeft = fileSize;
			int cont = 0;


				//pthread_mutex_lock(&mutex);
	    while(bytesLeft > 0) {

				bzero(dataBuffer, sizeof(dataBuffer));

				printf("WRITING TO FILE\n");
				packet fileRequestBuff;
    		bzero(&fileRequestBuff, sizeof(struct packet));
				byteCount = read(socket, &fileRequestBuff, sizeof(struct packet));
					printf("SOKET %d\n", ++cont);
					if(bytesLeft > KBYTE) {
						byteCount = fwrite(fileRequestBuff._payload, KBYTE, 1, ptrfile);
					} else {
						fwrite(fileRequestBuff._payload, bytesLeft, 1, ptrfile);
					}
					// decrementa os bytes lidos
					bytesLeft -= KBYTE;
					usleep(1000);
      }
			//pthread_mutex_unlock(&mutex);

			printf("closing file\n" );
      fclose(ptrfile);
      time (&now);


      stat(path, &st);
      file_info.st = st;
      strcpy(file_info.name, file);
      strcpy(file_info.last_modified, ctime(&responseThread.lst_modified));
      file_info.lst_modified = responseThread.lst_modified;
      file_info.size = fileSize;

      updateFileInfo(username, file_info);
			signal_download2client(username, file, &file_info.lst_modified);
			pthread_mutex_unlock(&file_info.file_mutex);
	}
}

void updateFileInfo(char *username, struct file_info file_info) {
  struct client_list *client_node;
  int i;

  if (findNode(username, client_list, &client_node)) {
    for(i = 0; i < FILE_MAX; i++)
      if(!strcmp(file_info.name, client_node->client.file_info[i].name)) {
          client_node->client.file_info[i] = file_info;
          return;
        }
    for(i = 0; i < FILE_MAX; i++) {
      if(client_node->client.file_info[i].size == -1) {
        client_node->client.file_info[i] = file_info;
        break;
      }
    }
  }
}

void delete_file(char *file, int socket, char *username) {
  int byteCount;
  FILE *ptrfile;
  char path[200];
  struct file_info file_info;

  strcpy(path, username);
  strcat(path, "/");
  strcat(path, file);

  if(remove(path) != 0) {
    printf("Error: unable to delete the %s file\n", file);
  }

  strcpy(file_info.name, file);
  file_info.size = -1;

  updateFileInfo(username, file_info);
	signal_delete2client(username, file);
}
