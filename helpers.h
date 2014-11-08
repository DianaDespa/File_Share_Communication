// Despa Diana Alexandra 321CA

#ifndef _HELPERS_H
#define _HELPERS_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <string.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <time.h>

#define CLIENT_NOT_FOUND "-1 : Client inexistent\n"
#define FILE_NOT_FOUND "-2 : Fisier inexistent\n"
#define DUPLICATE_FILE "-3 : Exista fisier local cu acelasi nume\n"
#define CONNECTION_ERR "-4 : Eroare la conectare\n"
#define SOCKET_ERR "-5 : Eroare la citire de/scriere pe socket\n"

#define MAX_CLIENTS	20
#define MAX_SHARE 1024
#define BUFLEN 256
#define TIMEOUT 1000

// Structura reprezentativa pentru un client.
struct Client {
	char * ip;
	int port;
	int socket = -1;
	char * timestamp;
	std::vector<std::pair<std::string, unsigned long long> > shared_files;
	
	~Client() {
		free(timestamp);
		free(ip);
	}
};

#endif
