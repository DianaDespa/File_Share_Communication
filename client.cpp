// Despa Diana Alexandra 321CA

#include "helpers.h"

using namespace std;

// Tabela de dispersie clienti, avand ca si cheie numele, iar ca valoare
// informatiile despre un client aflate in urma comenzii getfile: IP si port.
unordered_map<string, Client *> clients;

// Vector cu fisierele partajate de client.
vector<string> shared_files;

// Descriptori pentru socketi si fisiere.
int sockfd_server, sockfd_clients, newsockfd, to_fd, from_fd, 
	logfd, readfd, writefd;
fd_set read_fds;	// multimea de citire folosita in select()
fd_set tmp_fds;		// multime folosita temporar
int fdmax;			// valoare maxima file descriptor din multimea read_fds

// Flag-uri care indica daca clientul trimite un fisier sau primeste un fisier.
bool receiving_file = false, sending_file = false;

// Variabile auxiliare pentru separarea unui sir de caractere in cuvinte.
char * tmp_buffer, * token;
string path, my_name, folder, command_begin = "> ";

// Verifica daca un fisier este partajat de client.
// Intoarce pozitia lui in vectorul de fisiere daca se afla in vector,
// altfel -1.
int is_shared(char * file) {
	for (size_t i = 0; i < shared_files.size(); ++i) {
		if (shared_files[i].compare(file) == 0) {
			return i;
		}
	}
	return -1;
}

// Intoarce numarul de cuvinte din msg.
int word_count(char * msg) {
	char * word = strtok(msg, " ");
	int word_count = 0;
	while (word != NULL) {
		word = strtok(NULL, " ");
		++word_count;
	}
	return word_count;
}

// Afiseaza mesajul msg in consola si il scrie in fisierul de log.
void print_log(string msg) {
	printf("%s\n", msg.c_str());
	write(logfd, msg.c_str(), strlen(msg.c_str()));
	write(logfd, "\n", 1);
}

// Inchide toate conexiunile pe care asculta.
void close_set_sockets() {
	for (int j = 0; j <= fdmax; ++j) {
		if (FD_ISSET(j, &read_fds) && j != STDIN_FILENO &&
			j != sockfd_server && j != sockfd_clients) {
			close(j);
			FD_CLR(j, &read_fds);
		}
	}
}

// Inchide toate conexiunile pe care asculta, inclusiv fisierele din/in care 
// se citeste/scrie si determina terminarea programului.
void quit(int code) {
	close_set_sockets();
	close(sockfd_server);
	close(sockfd_clients);
	close(from_fd);
	close(readfd);
	close(writefd);
	close(logfd);
	exit(code);
}

// Adauga un fisierul file la vectorul de fisiere partajate si dimensiunea lui
// la comanda pentru server.
int sharefile(char * buffer, char * file, char * command) {
	struct stat buf;
	
	// Trebuie sa verific daca fisierul exista in folder.
	strcpy(command, buffer);
	
	path = folder;
	path += "/";
	path += file;
	
	if (stat(path.c_str(), &buf) < 0) {
		print_log(FILE_NOT_FOUND);
		return -1;
	}
	
	shared_files.push_back(file);
	strcat(command, " ");
	strcat(command, to_string(buf.st_size).c_str());
	return 0;
}


// Daca fisierul file este partajat, il sterg din vector. 
void unsharefile(char * buffer, char * file, char * command) {
	strcpy(command, buffer);
	
	int index = is_shared(file);
	if (index != -1) {
		shared_files.erase(shared_files.begin() + index);
	}
}

// Realizeaza toate actiunile necesare pentru indeplinirea comenzii getfile.
int getfile(char * file, char * client) {
	unordered_map<string, Client *>::const_iterator it = clients.find(client);
	
	// Daca nu se afla in vector clientul de la care doresc un fisier, adica
	// daca nu s-a dat comanda infoclient pentru el la un moment anterior,
	// afisez eroarea -1.
	if (it == clients.end()) {
		print_log(CONNECTION_ERR);
		return -1;
	}
	
	// Formez o comanda cu structura: getfile <nume_fisier> pentru client.
	char command[BUFLEN];
	char buffer[BUFLEN];
	
	strcpy(command, "getfile ");
	strcat(command, file);
	
	if (it->second->socket == -1) {
		// Creez socket pentru comunicarea cu celalalt client
		from_fd = socket(AF_INET, SOCK_STREAM, 0);
		
		if (from_fd < 0) {
			return -1;
		}

		struct sockaddr_in transf_addr;
		transf_addr.sin_family = AF_INET;
		transf_addr.sin_port = htons(it->second->port);
		inet_aton(it->second->ip, &transf_addr.sin_addr);

		// Conectez cei doi clienti.
		
		if (connect(from_fd, (struct sockaddr*) &transf_addr, 
					sizeof(transf_addr)) < 0) {
			print_log(CONNECTION_ERR);
			close(from_fd);
			return -1;
		}
		
		it->second->socket = from_fd;
	} else {
		from_fd = it->second->socket;
	}
	
	// Trimit comanda si primesc raspunsul.
	if (send(from_fd, command, strlen(command), 0) <= 0 ||
		recv(from_fd, buffer, sizeof(buffer), 0) <= 0) {
		print_log(CONNECTION_ERR);
		it->second->socket = -1;
		return -1;
	}
	
	// Verific ca nu am primit eroare.
	if (buffer[0] == '-') {
		print_log(buffer);
		return -1;
	}
	
	struct stat buf;
	path = folder;
	path += "/";
	path += file;
	
	// Verific daca fisierul este duplicat
	if (stat(path.c_str(), &buf) >= 0) {
		printf("%s", DUPLICATE_FILE);
		write(logfd, DUPLICATE_FILE,
				strlen(DUPLICATE_FILE));
		printf("Suprascriere? y/n?\n");
		char raspuns[10];
		fgets(raspuns, 10, stdin);
		if ((strcmp(raspuns, "y\n") != 0)){
			write(logfd, "Comanda anulata\n\n",
					strlen("Comanda anulata\n\n"));
			return 0;
		}
		write(logfd, "Suprascris\n\n",
			strlen("Suprascris\n\n"));
	}
	
	// Transferul poate incepe.
	receiving_file = true;
	writefd = open(path.c_str(), O_CREAT|O_TRUNC|O_WRONLY);
	
	return 0;
}

int main(int argc, char* argv[]) {
	
	if (argc < 6) {
		fprintf(stderr,"Utilizare: %s <nume_client> <nume_director> <port_client> \
<ip_server> <port_server>\n", argv[0]);
		exit(1);
	}
	
	my_name = argv[1];
	folder = argv[2];
	
	int n, i, portno, clilen, len1, len2;
	
	struct sockaddr_in serv_addr, my_addr, cli_addr;
	struct timeval timeout, tmp_timeout;
	timeout.tv_sec = 1;
	timeout.tv_usec = 0;
	
	char buffer[BUFLEN],
			reply[BUFLEN],
			command[BUFLEN],
			bytes[MAX_SHARE];
	
	// Socket pentru conexiunea cu server-ul.
	sockfd_server = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd_server < 0) {
		perror("Error opening server socket");
		exit(1);
	}

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(atoi(argv[5]));
	inet_aton(argv[4], &serv_addr.sin_addr);
	
	// Conectez clientul la server.
	if (connect(sockfd_server, (struct sockaddr*) &serv_addr, 
				sizeof(serv_addr)) < 0) {
		perror("Error server connection");
		close(sockfd_server);
		exit(1);
	}
	
	logfd = open(strcat(argv[1], ".log"), O_CREAT|O_TRUNC|O_WRONLY);
	
	string first_msg = my_name + " " + argv[3];
	if (send(sockfd_server, first_msg.c_str(), strlen(first_msg.c_str()), 0) <= 0) {
		print_log(SOCKET_ERR);
		close(sockfd_server);
		exit(1);
	} else {
		n = recv(sockfd_server, buffer, sizeof(buffer), 0);
		if (n <= 0) {
			print_log(SOCKET_ERR);
			close(sockfd_server);
			exit(1);
		}
		else if (buffer[0] == '-') {
			print_log(buffer);
			close(sockfd_server);
			exit(1);
		}
	}
	
	// Socket care asculta pentru conexiuni cu alti clienti.
	sockfd_clients = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd_clients < 0) {
		perror("Error opening clients socket");
		close(sockfd_server);
		close(sockfd_clients);
		exit(1);
	}
	
	portno = atoi(argv[3]);
	
	memset((char *) &my_addr, 0, sizeof(serv_addr));
	my_addr.sin_family = AF_INET;
	my_addr.sin_addr.s_addr = INADDR_ANY;
	my_addr.sin_port = htons(portno);
	
	if (bind(sockfd_clients, (struct sockaddr *)&my_addr, sizeof(struct sockaddr)) < 0) {
		perror("Error binding");
		close(sockfd_server);
		close(sockfd_clients);
		exit(1);
	}
	
	listen(sockfd_clients, MAX_CLIENTS);
	
	// Golesc multimea de descriptori de citire(read_fds) si multimea tmp_fds.
	FD_ZERO(&read_fds);
	FD_ZERO(&tmp_fds);

	// Adaug sockfd_clients (socketul pe care se asculta conexiuni),
	// descriptorul pentru citire de la standard input si descriptorul pentru
	// comunicarea cu server-ul in multimea read_fds.
	FD_SET(sockfd_server, &read_fds);
	FD_SET(sockfd_clients, &read_fds);
	FD_SET(STDIN_FILENO, &read_fds);
	fdmax = sockfd_clients;
	
	while(1) {
		tmp_fds = read_fds;
		tmp_timeout = timeout;
		
		if ((n = select(fdmax + 1, &tmp_fds, NULL, NULL, &tmp_timeout)) == -1) {
			perror("Error in select");
			quit(1);
		}	
		
		// Daca a fost depasit timeout-ul si sunt in cursul trimiterii unui
		// fisier, citesc urmatorul fragment din fisier si il trimit.
		if (n == 0 && sending_file) {
			memset(bytes, 0, MAX_SHARE);
			len1 = read(readfd, bytes, MAX_SHARE);
			
			if (send(to_fd, bytes, len1, 0) <= 0) {
				close(readfd);
				close_set_sockets();
			}
			
			if (len1 == 0) {
				// S-a terminat citirea din fisier, adica au fost transmisi
				// toti octetii.
				close(readfd);
				sending_file = false;
				close_set_sockets();			
			}
		}
		
		// Daca a fost depasit timeout-ul si sunt in cursul primirii unui
		// fisier, ii citesc de pe socket si ii scriu in fisier.
		if (n == 0 && receiving_file) {
			// Am primit un fragment de fisier de la clientul sender.
			memset(bytes, 0, MAX_SHARE);
			len2 = recv(from_fd, bytes, sizeof(bytes), 0);
			
			write(writefd, bytes, len2);
			
			if (len2 <= 0) {
			// Celalalt client s-a deconectat sau transferul nu s-a realizat sau
			// transferul s-a incheiat.
			// => inchid fisierul transferat si conexiunea cu acel client.
				close(writefd);
				receiving_file = false;
				print_log("Succes\n");
				close_set_sockets();
			}
		}
		
		for(i = 0; i <= fdmax; ++i) {
			if (FD_ISSET(i, &tmp_fds)) {
				
				if (i == sockfd_clients) {
					// A venit ceva pe socketul inactiv (cel cu listen)
					clilen = sizeof(cli_addr);
					if ((newsockfd = accept(sockfd_clients, 
											(struct sockaddr *)&cli_addr,
											(socklen_t *)&clilen)) == -1) {
						perror("Error in accept");
						close(sockfd_clients);
						close(sockfd_server);
						exit(1);
						
					} else {
						// Adaug noul socket intors de accept() la multimea
						// descriptorilor de citire.
						FD_SET(newsockfd, &read_fds);
						if (newsockfd > fdmax) {
							fdmax = newsockfd;
						}
					}	
				}
				memset(buffer, 0, BUFLEN);
				memset(command, 0, BUFLEN);
				memset(reply, 0, BUFLEN);
				
				if (i == STDIN_FILENO) {
					// Primesc comanda de la utilizator.
					fgets(buffer, BUFLEN, stdin);
					if (buffer[0] == '\n')
						continue;
	
					buffer[strlen(buffer) - 1] = '\0';
					
					// Interpretez comanda.
					tmp_buffer = strdup(buffer);
					token = strtok(tmp_buffer, " "); // numele comenzii

					write(logfd, (command_begin + buffer + "\n").c_str(),
							strlen(buffer) + 3);
					
					if (strcmp(token, "listclients") == 0 ||
						strcmp(token, "infoclient") == 0 ||
						strcmp(token, "getshare") == 0 ||
						strcmp(token, "infofile") == 0) {
						strcpy(command, buffer);
						
					} else if (strcmp(token, "sharefile") == 0) {
						if (sharefile(buffer, strtok(NULL, " "), command) < 0)
							continue;
					
					} else if (strcmp(token, "unsharefile") == 0) {
						unsharefile(buffer, strtok(NULL, " "), command);
						
					} else if (strcmp(token, "getfile") == 0) {
						if (!receiving_file) { 	// nu sunt in cursul primirii 
												// unui fisier.
							getfile(strtok(NULL, " "), strtok(NULL, " "));
						}
						continue;
						
					} else if (strcmp(token, "quit") == 0) {
						quit(0);
						
					} else {
					// Comanda nu este recunoscuta, deci o ignor.
						printf("\n");
						write(logfd, "\n", 1);
						continue;
					}
					
					// Trimit comanda la server.
					if (send(sockfd_server, command, strlen(command), 0) <= 0) {
						print_log(SOCKET_ERR);
					}
					
					continue;
				}
				
				// Verific socketul pe care comunic cu server-ul.
				if (i == sockfd_server) {
					if ((n = recv(i, buffer, sizeof(buffer), 0)) < 0) {
						print_log(SOCKET_ERR);
					} else if (n == 0) {
						// Serverul a inchis conexiunea.
						quit(0);
						
					} else {
						// Verific daca am primit raspuns pentru comanda
						// infoclient, in functie de numarul de cuvinte al
						// mesajului si daca mesajul nu este de eroare.
						
						if (buffer[0] != '-' && word_count(strdup(buffer)) == 5) {
							tmp_buffer = strdup(buffer);
							char * client = strtok(tmp_buffer, " ");
							
							// Actualizez tabela cu clienti.
							clients[client] = new Client();
							clients[client]->ip = strtok(NULL, " ");
							clients[client]->port = atoi(strtok(NULL, " "));
						}
						print_log(buffer);
					}
					continue;
				}
				
				// Am primit cerere de partajare fisier de la alt client.
				if ((n = recv(i, buffer, sizeof(buffer), 0)) <= 0) {
				// Inchid si scot din multimea de citire socketul pentru 
				// clientul care a inchis conexiunea.
					close(i);
					FD_CLR(i, &read_fds);
					continue;
				}
				
				// Incep partajarea daca nu transmit un fisier altui client la
				// acest moment de timp.
				if (!sending_file) {
					token = strtok(buffer, " ");	//comanda getfile
					token = strtok(NULL, " ");		//numele fisierului
					
					// Verific daca partajeaza fisierul
					if (is_shared(token) != -1) {
						
						sending_file = true;
						path = folder;
						path += "/";
						path += token;
						readfd = open(path.c_str(), O_RDONLY);
						to_fd = i;
					
						// Trimit confirmarea comenzii valide.
						if (send(i, "ok\n", strlen("ok\n"), 0) <= 0) {
							close(i);
							FD_CLR(i, &read_fds);
							sending_file = false;
						}
					} else if (send(i, FILE_NOT_FOUND, strlen(FILE_NOT_FOUND), 0) <= 0) {
						close(i);
						FD_CLR(i, &read_fds);
					}
				} else if (send(i, CONNECTION_ERR, strlen(CONNECTION_ERR), 0) <= 0) {
					close(i);
					FD_CLR(i, &read_fds);
				}
			}
		}
	}
	
	return 0;
}
