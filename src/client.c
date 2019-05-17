#include <netdb.h> 
#include <pwd.h>
#include <sys/inotify.h>
#include "../utils/config.h"
#include "../utils/client.h"
#include "../utils/utils.h"


char username[USER_MAX_NAME];
char *host;
int port;
char directory[FILENAME_MAX_SIZE + 50];
int sockfd = -1, sync_socket = -1;
int notifyfd;
int watchfd;

int main(int argc, char *argv[]) {

    struct sockaddr_in serv_addr;
    struct hostent *server;
	
    char buffer[256];
    
    //Verifica os parametros e configura o host
    if(initHost(argv, argc) == -1) exit(0);

    if ((connect_server(host, port)) > 0) {
		// sincroniza diretório do servidor com o do cliente
		  sync_client_first();
		  

		// espera por um comando de usuário
		 client_interface();
    }


    return 0;
}

void refreshSocket(){
	close_connection();
	sockfd = -1;
	connect_server(host, port);
	sync_socket = -1;
	create_sync_sock();
}

int initHost(char *argv[], int argc) {
    int resp = 1;
    
    if (argc < 3) {
		printf("Insufficient arguments: user, hostname or port \n");
		return -1;
    }

    // primeiro argumento nome do usuário
	if (strlen(argv[1]) <= USER_MAX_NAME) {
        strcpy(username, argv[1]);
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

	int socketByteSize;
	struct sockaddr_in server_addr;
	struct hostent *server;
	char buffer[256];

    packet connected;

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

    packet threadSetter;
    threadSetter.type = RESP;
    threadSetter.payloadCommand = SHOULD_CREATE_THREAD;
	write(sockfd, &threadSetter, sizeof(struct packet));

    packet userPacket;
    userPacket.type = RESP;
    strcpy(userPacket._payload, username);

	//envia username para o servidor
    socketByteSize = write(sockfd, &userPacket, sizeof(struct packet));
    if (socketByteSize < 0) {
 	    printf("ERROR sending username to server\n");
        return -1;
    }

	socketByteSize = read(sockfd, &connected, sizeof(struct packet));

    if(connected.type ==  RESP){
        printf("respo payload %d\n", connected.payloadCommand);
        if (socketByteSize < 0){
            printf("ERROR receiving connected message\n");
            return -1;
        } else if (connected.payloadCommand) {
            printf("connected\n");
            return 1;
        } else {
            printf("You already have two devices connected\n");
            return -1;
        }
    } else return -1;
}

void sync_client_first() {
	char *homedir;
	char fileName[FILE_MAX + 10] = "sync_dir_";
	pthread_t syn_th;

	if ((homedir = getenv("HOME")) == NULL) {
        printf("entrei aqui e n sei pq");
        homedir = getpwuid(getuid())->pw_dir;
    }
	// nome do arquivo
	strcat(fileName, username);

	// forma o path do arquivo
	strcpy(directory, homedir);
	strcat(directory, "/");
	strcat(directory, fileName);

	if (mkdir(directory, 0777) < 0) {
		// erro
		if (errno != EEXIST)
			printf("ERROR creating directory\n");
	}
	// diretório não existe
	else {
		printf("Creating %s directory in your home\n", fileName);
	}

	initializeNotifyDescription();

	//cria thread para sincronização
	if(pthread_create(&syn_th, NULL, sync_thread, NULL)) {
		printf("ERROR creating thread\n");
	}
}

void createMainDir(){
	char *homedir;
	char fileName[FILE_MAX + 10] = "sync_dir_";

	if ((homedir = getenv("HOME")) == NULL) {
        printf("entrei aqui e n sei pq");
        homedir = getpwuid(getuid())->pw_dir;
    }
	// nome do arquivo
	strcat(fileName, username);

	// forma o path do arquivo
	strcpy(directory, homedir);
	strcat(directory, "/");
	strcat(directory, fileName);

	if (mkdir(directory, 0777) < 0) {
		// erro
		if (errno != EEXIST)
			printf("ERROR creating directory\n");
	}
	// diretório não existe
	else {
		printf("Creating %s directory in your home\n", fileName);
	}
}

void handleGetSyncDirClient(){
	createMainDir();
	get_all_files();
}

void *sync_thread() {
	int length, i = 0;
    char buffer[BUF_LEN];
	char path[200];

	create_sync_sock();
	// get_all_files();

	while(1) {
	  length = read( notifyfd, buffer, BUF_LEN );

	  if ( length < 0 ) {
	    perror( "read" );
	  }

	  while ( i < length ) {
	    struct inotify_event *event = ( struct inotify_event * ) &buffer[ i ];
	    if ( event->len ) {
				if ( event->mask & IN_CLOSE_WRITE || event->mask & IN_CREATE || event->mask & IN_MOVED_TO) {
					strcpy(path, directory);
					strcat(path, "/");
					strcat(path, event->name);
					if(exists(path) && (event->name[0] != '.')) {
						upload_file(path, sync_socket);
					}
				}
				else if (event->mask & IN_DELETE || event->mask & IN_MOVED_FROM) {
					strcpy(path, directory);
					strcat(path, "/");
					strcat(path, event->name);
					if(event->name[0] != '.') {
						delete_file_request(path, sync_socket);
					}
				}
	    }
	    i += EVENT_SIZE + event->len;
  	}
		i = 0;

		sleep(10);
	}
}

void initializeNotifyDescription() {
	notifyfd = inotify_init();

	watchfd = inotify_add_watch(notifyfd, directory, IN_CLOSE_WRITE | IN_CREATE | IN_DELETE | IN_MOVED_FROM | IN_MOVED_TO);
}

pthread_mutex_t file_mutex2;
pthread_mutex_t file_mutex3;
void get_all_files() {
	printf("GET_ALL_FILLES\n");
	int byteCount, bytesLeft, fileSize, fileNum, i;
	FILE* ptrfile;
	char dataBuffer[KBYTE], file[FILENAME_MAX_SIZE], path[KBYTE];

	// copia nome do arquivo e comando para enviar para o servidor
	printf("DOWNLOAD REQUEST\n");
    packet threadRequest;
    threadRequest.type = CMD;
    threadRequest.payloadCommand = DOWNLOADALL;
	byteCount = write(sync_socket, &threadRequest, sizeof(struct packet));
	
	if (byteCount < 0)
		printf("Error sending DOWNLOAD message to server\n");

	printf("GET NUMB OF FILES\n");
	packet connection;
	printf("WAITING FILENUM\n");
	byteCount = read(sync_socket, &connection, sizeof(struct packet));
	if (byteCount < 0)
		printf("Error FILENUM \n");

	fileNum = connection.payloadCommand;
	printf("FILEnum: %d\n", fileNum);
	printf("START FOR\n");
	for(i = 0; i < fileNum; i++) {
		// lê nome do arquivo do servidor
		printf("FILE: %d\n", i);
		packet connectionFile;
		printf("WAITING READ");
		byteCount = read(sync_socket, &connectionFile, sizeof(struct packet));
		if (byteCount < 0) {
			printf("Error receiving filename\n");
		}
		strcpy(file, connectionFile._payload);

		strcpy(path, directory);
		strcat(path, "/");
		strcat(path, file);

		printf("RECEBEU: %s\n", file);
		// pthread_mutex_lock(&file_mutex2);
		get_file(file, 1);
		// pthread_mutex_unlock(&file_mutex2);
		bzero(file, sizeof(file));
		// usleep(1000);
	}
	// pthread_mutex_lock(&file_mutex2);
	// refreshSocket();
	// pthread_mutex_unlock(&file_mutex2);
}

int create_sync_sock() {
	int byteCount, connected;
	struct sockaddr_in server_addr;
	struct hostent *server;
	int client_thread = 0;
	char buffer[256];

	server = gethostbyname(host);

	if (server == NULL)
	{
  	return -1;
  }

	// tenta abrir o socket
	if ((sync_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1)
	{
		return -1;
	}

	// inicializa server_addr
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port);
	server_addr.sin_addr = *((struct in_addr *)server->h_addr);

	bzero(&(server_addr.sin_zero), 8);

	// tenta conectar ao socket
	if (connect(sync_socket,(struct sockaddr *) &server_addr,sizeof(server_addr)) < 0){
			printf("TENTANDO CONECTAR COM SYNC SOCKET");
		  return -1;
	}

    packet threadSetter;
    threadSetter.type = RESP;
    threadSetter.payloadCommand = 0;
	byteCount = write(sync_socket, &threadSetter, sizeof(struct packet));
	if (byteCount < 0) {
 	    printf("ERROR sending 0 to zerverr\n");
        return -1;
    }
	

    packet userPacket;
    userPacket.type = RESP;
    strcpy(userPacket._payload, username);
	//envia username para o servidor
    byteCount = write(sync_socket, &userPacket, sizeof(struct packet));
    if (byteCount < 0) {
 	    printf("ERROR sending username to server\n");
        return -1;
    }


}

// função que extrai o nome do arquivo a partir de um pathname
void getFilename(char *pathname, char *filename) {
	char *filenameAux;

	filenameAux = strtok(pathname, "/");
	strcpy(filename, filenameAux);
	while(filenameAux != NULL) {
		strcpy(filename, filenameAux);
		filenameAux = strtok(NULL, "/");
	}
}

time_t getFileModifiedTime(char *path) {
    struct stat attr;
    if (stat(path, &attr) == 0)
    {
        return attr.st_mtime;
    }
    return 0;
}

int exists(const char *fname) {
    FILE *file;
    if (file = fopen(fname, "rb"))
    {
        fclose(file);
        return 1;
    }
    return 0;
}

void delete_file_request(char* file, int socket) {
	int byteCount;

    packet threadRequest;
    threadRequest.type = CMD;
    threadRequest.payloadCommand = DELETE;

	getFilename(file, threadRequest._payload);

	byteCount = write(socket, &threadRequest, sizeof(struct packet));

	if (byteCount < 0)
		printf("ERROR sending delete file request\n");
}

pthread_mutex_t file_mutex;
void upload_file(char *file, int socket) {
	printf("ENTROU DENTRO DO UPLOAD\n");
	int byteCount, fileSize;
	FILE* ptrfile;
	char dataBuffer[KBYTE];
    packet threadRequest;		
	if (ptrfile = fopen(file, "rb")) {
			getFilename(file, threadRequest._payload);
            threadRequest.type = RESP;
            threadRequest.payloadCommand = UPLOAD;
            threadRequest.length = getFileSize(ptrfile);
			printf("ENIVOU TAMANHO\n");
            write(socket, &threadRequest, sizeof(struct packet));
			printf("recebeu response TAMANHO\n");


			if (threadRequest.length == 0) {
				fclose(ptrfile);
				return;
			}
		int cont =0;
		printf("ANTE DO WILE\n");
			while(!feof(ptrfile)) {
				// pthread_mutex_lock(&file_mutex);
					printf("pack %d\n\n", ++cont);
					packet fileRequestBuff;
					fread(dataBuffer, sizeof(dataBuffer), 1, ptrfile);
					fileRequestBuff.type = DATA;
					memcpy(&fileRequestBuff._payload, &dataBuffer, sizeof(dataBuffer));
					fileRequestBuff.length = sizeof(dataBuffer);
					fileRequestBuff.payloadCommand = 9;
					byteCount = write(socket, &fileRequestBuff, sizeof(struct packet));
					if(byteCount < 0){
						printf("ERROR sending file\n");	
					}
					bzero(dataBuffer, sizeof(dataBuffer));
					// pthread_mutex_unlock(&file_mutex);
					// usleep(1000);			
			}
			fclose(ptrfile);

			if (socket != sync_socket)
				printf("the file has been uploaded\n");
	}
	// arquivo não existe
	else {
		printf("ERROR this file doesn't exist\n\n");
	}	
}

int getFileSize(FILE *ptrfile) {
	int size;

	fseek(ptrfile, 0L, SEEK_END);
	size = ftell(ptrfile);
	rewind(ptrfile);

	return size;
}

void client_interface() {
	int command = 0;
	char request[200], file[200];

	printf("\nCommands:\nupload <path/filename.ext>\ndownload <filename.ext>\nlist_server\nget_sync_dir\nexit\nlist_client\ndelete <filename.ext>\n");
	do {
		printf("\ntype your command: ");
		fgets(request, sizeof(request), stdin);
		command = commandRequest(request, file);

		// verifica requisição do cliente
		switch (command)
		{
			case LIST: show_files(); break;
			case EXIT: close_connection(); break;
			case SYNC: handleGetSyncDirClient(); break;
			case DOWNLOAD: get_file(file, 0); break;
		    case UPLOAD: upload_file(file, sockfd); break;
			case LIST_CLIENT: list_client(); break;
			case DELETE: delete_file(file); break;
			default: printf("ERROR invalid command\n");
		}
	}while(command != EXIT);
}

void list_client(){
	DIR *d, *userDir;
	struct dirent *dir, *userDirent;
	int i = 0;
	FILE *file_d;
	struct stat st;
	char folder[FILE_MAX], path[200], initial[200];

	strcpy(initial, directory);
    strcat(initial, "/");
	printf("diretorio: %s\n", initial);

  	d = opendir(initial);
  	if (d){
        while((userDirent = readdir(d)) != NULL) {
              //testa se e um arquivo regular e se esta no root dir
			if(userDirent->d_type == DT_REG && strcmp(userDirent->d_name,".")!=0 && strcmp(userDirent->d_name,"..")!=0) {
                	strcpy(path, initial);
                	strcat(path, userDirent->d_name);			
                 	stat(path, &st);
					printf("------------\n");
					printf("file name %s\n", userDirent->d_name) ;
					printf("file Size %ld\n", st.st_size);
					printf("file Last Modified: %s", ctime(&st.st_mtime));
					printf("file Access Time: %s", ctime(&st.st_atime));
					printf("file Creation: %s", ctime(&st.st_ctime));

					i++;
           	} 
        }
    }
		closedir(d);
}

void get_file(char *file, int shouldSaveOnMainDir) {
	int byteCount, bytesLeft, fileSize;
	struct client_request clientRequest;
	FILE* ptrfile;
	char dataBuffer[KBYTE];

	// copia nome do arquivo e comando para enviar para o servidor
	packet clientCMDRequest;
	clientCMDRequest.type = CMD;
	clientCMDRequest.payloadCommand = DOWNLOAD;
	strcpy(clientCMDRequest._payload, file);

	byteCount = write(sockfd, &clientCMDRequest, sizeof(struct packet));
	if (byteCount < 0)
		printf("Error sending DOWNLOAD message to server\n");


	// lê estrutura do arquivo que será lido do servidor
	packet clientCMDSize;
	byteCount = read(sockfd, &clientCMDSize, sizeof(struct packet));
	if (byteCount < 0){
		printf("Error receiving filesize\n");
	}
	fileSize = clientCMDSize.length;

	if (fileSize < 0) {
		printf("The file doesn't exist\n\n\n");
		return;
	}

	char fileDirector[200];
	bzero(fileDirector, sizeof(fileDirector));
	if(shouldSaveOnMainDir){
		strcpy(fileDirector, directory);
		strcat(fileDirector, "/");
		strcat(fileDirector, file);
	
	} else {
		strcpy(fileDirector, file);
	}


	// cria arquivo no diretório do cliente
	ptrfile = fopen(fileDirector, "wb");

	// número de bytes que faltam ser lidos
	bytesLeft = fileSize;
	int cont = 0;
	while(bytesLeft > 0) {
		// lê 1kbyte de dados do arquivo do servidor
		bzero(dataBuffer, KBYTE);
		cont++;
		packet clientCMDData;
		byteCount = read(sockfd, &clientCMDData, sizeof(struct packet));
		//strcpy(dataBuffer, clientCMDData._payload); memcpy
		//printf("BUFFER %d - VALOR LIDO\n", cont);
		//printf("SIZE: %ld", sizeof(clientCMDData._payload));
		//printf("LENGHT: %d\n", clientCMDData.length);
		// escreve no arquivo do cliente os bytes lidos do servidor
		if(bytesLeft > KBYTE)
		{
			byteCount = fwrite(clientCMDData._payload, KBYTE, 1, ptrfile);
		}
		else
		{
			fwrite(clientCMDData._payload, bytesLeft, 1, ptrfile);
		}
		// decrementa os bytes lidos
		bytesLeft -= KBYTE;
		usleep(1000);
	}

	fclose(ptrfile);
	printf("File %s has been downloaded\n\n", file);
}

int commandRequest(char *request, char *file) {
	char *requestAux, *fileAux;
	int strLen;

	strLen = strlen(request);
	if ((strLen > 0) && (request[strLen-1] == '\n')) {
		  request[strLen-1] = '\0';
	}

	if (!strcmp(request, commands[LIST]))
		return LIST;
	else if (!strcmp(request, commands[EXIT]))
		return EXIT;
	else if (!strcmp(request, commands[SYNC]))
		return SYNC;
	else if (!strcmp(request, commands[LIST_CLIENT]))
		return LIST_CLIENT;	

	requestAux = strtok(request, " ");
	fileAux = strtok(NULL, "\n");
	if (fileAux != NULL)
		strcpy(file, fileAux);
	else
		return -1;

	if (file != NULL){
		if (!strcmp(requestAux, commands[DOWNLOAD]))
			return DOWNLOAD;
		else if (!strcmp(requestAux, commands[UPLOAD]))
			return UPLOAD;
		else if (!strcmp(requestAux, commands[DELETE]))
			return DELETE;	
	}
	else
		return -1;
}
pthread_mutex_t lock;
void show_files() {
	int byteCount, fileNum, i;
	struct file_info file_info;
	struct stat st;

	pthread_mutex_lock(&lock);
	packet clientCMDRequest;
	clientCMDRequest.type = CMD;
	clientCMDRequest.payloadCommand = LIST;
	byteCount = write(sockfd, &clientCMDRequest, sizeof(struct packet));
	if (byteCount < 0)
		printf("Error sending LIST message to server\n");

	// lê número de arquivos existentes no diretório
	packet clientCMDFileSizes;
	byteCount = read(sockfd, &clientCMDFileSizes, sizeof(struct packet));
		if (byteCount < 0)
		printf("Error READING LIST message to server\n");
	usleep(1000);	
	fileNum = clientCMDFileSizes.payloadCommand;
	printf("FILE SIZE RECEIVED: %d", clientCMDFileSizes.payloadCommand);
	printf("Count Files: %d \n", fileNum);

	if (clientCMDFileSizes.payloadCommand == 0){
		printf("Empty directory\n\n\n");
		return;
	}

	for (i = 0; i < fileNum; i++) {
		byteCount = read(sockfd, &file_info, sizeof(file_info));
		printf("\nFile: %s \nLast modified: %sLast Access: %sLast Created: %ssize: %d\n", file_info.name, file_info.last_modified, ctime(&file_info.st.st_atime), ctime(&file_info.st.st_ctime), file_info.size);
	}
	pthread_mutex_unlock(&lock);
	// refreshSocket();
}

void close_connection() {
	int byteCount;

	packet clientCMDRequest;
	clientCMDRequest.type = CMD;
	clientCMDRequest.payloadCommand = EXIT;

	//Disconect ASYNC
	byteCount = write(sockfd, &clientCMDRequest, sizeof(struct packet));
	if (byteCount < 0)
		printf("Error sending LIST message to server\n");
	
	//Disconect SYNC
	byteCount = write(sync_socket, &clientCMDRequest, sizeof(struct packet));
	if (byteCount < 0)
		printf("Error sending LIST message to server\n");

	close(sockfd);
	printf("Connection with server has been closed\n");
}

void delete_file(char* file) {
	char *homedir;
	char folder[FILE_MAX + 50];

	// constroi caminho do home do usuário
	strcpy(folder, directory);
	strcat(folder, "/");
	strcat(folder, file);

    int status = remove(folder);
 
  	if (status == 0){
    	printf("%s file deleted successfully.\n", file);
	} else {
    	printf("Unable to delete the file\n");
    	perror("Following error occurred");
  	}
}