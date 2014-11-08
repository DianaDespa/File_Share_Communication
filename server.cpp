// Despa Diana Alexandra 321CA

#include "helpers.h"

using namespace std;

// Lista clienti ordonata dupa timpul la care s-au conectat
vector<pair<string, int> > in_order_clients;
// Tabela de dispersie clienti, avand ca si cheie numele, iar ca valoare
// informatiile specifice unui client: IP, port, data conectarii si fisiere
// partajate.
unordered_map<string, Client *> clients;

// Intoarce un string cu ora sistemului.
char * get_time() {
	time_t time_raw_format;
	struct tm * ptr_time;
	char * time_buffer = (char*)malloc(12);
	
	time(&time_raw_format);
	ptr_time = localtime(&time_raw_format);
	
	if(strftime(time_buffer, 12, "%I:%M:%S %p", ptr_time) == 0) {
		perror("Eroare formatare.");
		return NULL;
	}
	
	return time_buffer;
}

// Trimite mesajul msg pe socket-ul fd.
void error(const char *msg, int fd) {
	send(fd, msg, strlen(msg), 0);
}

// Afiseaza rezultatul comenzii status.
void status_command() {
	for (size_t i = 0; i < in_order_clients.size(); ++i) {
		printf("%s %s %d\n",in_order_clients[i].first.c_str(), 
							clients[in_order_clients[i].first]->ip,
							clients[in_order_clients[i].first]->port);
	}
}

// Intoarce primul cuvand din buffer.
string get_name(char const * buffer) {
	char * tmp_buffer = strdup(buffer);
	string name = strtok(tmp_buffer, " ");
	free(tmp_buffer);
	return name;
}

// Intoarce pozitia elementului corespunzator numarului elem in vectorul de
// perechi v, sau -1 daca el nu exista.
int elem_position(vector<pair<string, int> > v, int elem) {
	for (size_t i = 0; i < v.size(); ++i) {
		if (v[i].second == elem) {
			return i;
		}
	}
	return -1;
}

// Intoarce pozitia elementului corespunzator string-ului elem in vectorul de
// perechi v, sau -1 daca el nu exista.
template <class T>
int elem_position(vector<pair<string, T> > v, string elem) {
	for (size_t i = 0; i < v.size(); ++i) {
		if (elem.compare(v[i].first) == 0) {
			return i;
		}
	}
	return -1;
}

// Intoarce un sir de caractere reprezentand numarul de octeti number in format
// user-friendly.
string human_readable(unsigned long long number) {
	string size = "B";
	if (number > 1024) {
		number /= 1024;
		size = "KB";
		if (number > 1024) {
			number /= 1024;
			size = "MB";
			if (number > 1024) {
				number /= 1024;
				size = "GB";
		
			}
		}
	}
	return to_string(number) + size;
}

// Trimite raspunsul la comanda listclients.
void listcliets_command(int fd) {
	string output;
	for (size_t i = 0; i < in_order_clients.size(); ++i) {
		output += in_order_clients[i].first + "\n" ;
	}
	
	send(fd, output.c_str(), strlen(output.c_str()), 0);
}

// Trimite raspunsul la comanda infoclient.
void infoclient_command(char * client_name, int fd) {
	int index;
	if ((index = elem_position(in_order_clients, client_name)) == -1) {
		error(CLIENT_NOT_FOUND ,fd);
		
	} else {
		Client * client = clients[in_order_clients[index].first];
		string output = in_order_clients[index].first + " " + client->ip + " "
						+ to_string(client->port) + " " +
						client->timestamp + "\n";
		send(fd, output.c_str(), strlen(output.c_str()), 0);
	}
}

// Trimite raspunsul la comanda getshare.
void getshare_command(char * client_name, int fd) {
	int index;
	if ((index = elem_position(in_order_clients, client_name)) == -1) {
		error(CLIENT_NOT_FOUND ,fd);
		
	} else {
		Client * client = clients[in_order_clients[index].first];
		string output;
		
		for (size_t i = 0; i < client->shared_files.size(); ++i) {
			output += client->shared_files[i].first + " " +
						human_readable(client->shared_files[i].second) + "\n";
		}
		output += " ";
		send(fd, output.c_str(), strlen(output.c_str()), 0);
	}
}

// Trimite raspunsul la comanda infofile.
void infofile_command(char * file_name, int fd) {
	Client * client;
	bool flag = false;
	string output;
	
	for (size_t i = 0; i < in_order_clients.size(); ++i) {
		client = clients[in_order_clients[i].first];
		for (size_t j = 0; j < client->shared_files.size(); ++j) {
			if (client->shared_files[j].first.compare(file_name) == 0) {
				flag = true;
				output += in_order_clients[i].first + " " + file_name + " " +
						human_readable(client->shared_files[j].second) + "\n";
			}
		}
	}
	
	if (!flag) {
		error(FILE_NOT_FOUND, fd);
	} else {
		send(fd, output.c_str(), strlen(output.c_str()), 0);
	}
}

// Trimite raspunsul la comanda sharefile.
void sharefile_command(char * file_name, unsigned long long size, int fd){
	int index_client = elem_position(in_order_clients, fd);
	
	pair<string, unsigned long long> new_file(file_name, size);
	Client * client = clients[in_order_clients[index_client].first];
	
	int index_file = elem_position(client->shared_files, file_name);
	if (index_file == -1) {
		client->shared_files.push_back(new_file);
	} else {
		client->shared_files[index_file] = new_file;
	}
	send(fd, "Succes\n", strlen("Succes\n"), 0);
}

// Trimite raspunsul la comanda sharefile.
void unsharefile_command(char * file_name, int fd){
	int index_client = elem_position(in_order_clients, fd);
	Client * client = clients[in_order_clients[index_client].first];
	
	int index_file = elem_position(client->shared_files, file_name);
	if (index_file == -1) {
		error(FILE_NOT_FOUND, fd);
	} else {
		client->shared_files.erase(client->shared_files.begin() + index_file);
		send(fd, "Succes\n", strlen("Succes\n"), 0);
	}
}

int main(int argc, char* argv[]) {
	
	if (argc < 2) {
		fprintf(stderr,"Utilizare: %s <port_server>\n", argv[0]);
		exit(1);
	}
	
	int sockfd, newsockfd, portno, clilen;
	char buffer[BUFLEN];
	struct sockaddr_in serv_addr, cli_addr;
	int n, i, j;
	
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) {
		perror("Error opening socket");
		exit(1);
	}
	
	portno = atoi(argv[1]);
	
	memset((char *) &serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(portno);
	
	if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(struct sockaddr)) < 0) {
		perror("Error binding");
		close(sockfd);
		exit(1);
	}
	
	listen(sockfd, MAX_CLIENTS);
	
	fd_set read_fds;	// multimea de citire folosita in select()
	fd_set tmp_fds;		// multime folosita temporar
	int fdmax;			// valoare maxima file descriptor din multimea read_fds
	
	// Golesc multimea de descriptori de citire(read_fds) si multimea tmp_fds.
	FD_ZERO(&read_fds);
	FD_ZERO(&tmp_fds);

	// Adaug sockfd (socketul pe care se asculta conexiuni) si descriptorul
	// pentru citire de la standard input in multimea read_fds.
	FD_SET(sockfd, &read_fds);
	FD_SET(STDIN_FILENO, &read_fds);
	fdmax = sockfd;
	
	while(1) {
		tmp_fds = read_fds;
		if (select(fdmax + 1, &tmp_fds, NULL, NULL, NULL) == -1) {
			perror("Error in select");
			close(sockfd);
			exit(1);
		}
		
		for(i = 0; i <= fdmax; ++i) {
			if (FD_ISSET(i, &tmp_fds)) {
				memset(buffer, 0, BUFLEN);
				if (i == sockfd) {
					// A venit ceva pe socketul inactiv(cel cu listen)
					// => o noua conexiune => actiunea serverului: accept()
					clilen = sizeof(cli_addr);
					if ((newsockfd = accept(sockfd, 
											(struct sockaddr *)&cli_addr,
											(socklen_t *)&clilen)) == -1) {
						perror("Error in accept");
						close(sockfd);
						exit(1);
						
					} else 
					// Citesc primul mesaj de la client, cel cu numele si portul.
					if ((n = recv(newsockfd, buffer, sizeof(buffer), 0)) <= 0) {
						close(newsockfd);
					} else {
						// Verific daca exista un client cu acelasi nume
						// deja conectat.
						
						if (elem_position(in_order_clients, get_name(buffer)) == -1) {
							// Daca nu, adaug noul client la multimea clientilor.
							// Variabila buffer contine numele clientului si
							// portul pe care asculta cereri de conexiune.
							
							Client * client = new Client();
							
							client->ip = inet_ntoa(cli_addr.sin_addr);
							
							char * client_port = strtok(buffer, " ");
							client_port = strtok(NULL, " ");
							client->port = atoi(client_port);
							client->timestamp = get_time();
							
							string name = get_name(buffer);
							clients[name] = client;
							in_order_clients.push_back(make_pair(name, newsockfd));
							
							FD_SET(newsockfd, &read_fds);
							if (newsockfd > fdmax) {
								fdmax = newsockfd;
							}	
							
							//Trimit confirmarea conexiunii valide.							
							send(newsockfd, "ok\n", strlen("ok\n"), 0);
						
						} else {
							error(CONNECTION_ERR, newsockfd);
							close(newsockfd);
						}
					}
				} else {
					if (i == STDIN_FILENO) {
						// Am primit mesaj de la standard input.
						fgets(buffer, BUFLEN, stdin);
						buffer[strlen(buffer) - 1] = '\0';
						
						if (strcmp(buffer, "quit") == 0) {
							// Inchid conexiunile cu totii clientii
							for (j = 0; j <= fdmax; ++j) {
								if (FD_ISSET(j, &tmp_fds) && j != sockfd &&
									j != STDIN_FILENO) {
									close(j);
									FD_CLR(j, &read_fds);
								}
							}
							close(sockfd);
							return 0;
						
						} else if (strcmp(buffer, "status") == 0) {
							status_command();
						}
						continue;
					}
						
					// Am primit date pe unul din socketii cu care vorbesc
					// cu clientii => actiunea serverului: recv()
					if ((n = recv(i, buffer, sizeof(buffer), 0)) <= 0) {
						// Inchid si scot din multimea de citire socketul
						// pentru clientul care a inchis conexiunea.
						close(i);
						FD_CLR(i, &read_fds);
						// Sterg clientul din structurile server-ului.
						int index = elem_position(in_order_clients, i);
						clients.erase(in_order_clients[index].first);
						in_order_clients.erase(in_order_clients.begin() + index);
					
					} else {
						char * tmp_buffer = strdup(buffer);
						char * command = strtok(tmp_buffer, " ");
						char * param = strtok(NULL, " ");
						char * size;
						
						if (strcmp(command, "listclients") == 0) {
							listcliets_command(i);
						
						} else if (strcmp(command, "infoclient") == 0) {
							infoclient_command(param, i);
						
						} else if (strcmp(command, "getshare") == 0) {
							getshare_command(param, i);
						
						} else if (strcmp(command, "infofile") == 0) {
							infofile_command(param, i);
						
						} else if (strcmp(command, "sharefile") == 0) {
							size = strtok(NULL, " ");
							sharefile_command(param, atoll(size), i);
						
						} else if (strcmp(command, "unsharefile") == 0) {
							unsharefile_command(param, i);
						
						} else if (strcmp(command, "quit") == 0) {
							close(i);
							FD_CLR(i, &read_fds);
							int index = elem_position(in_order_clients, i);
							clients.erase(in_order_clients[index].first);
							in_order_clients.erase(in_order_clients.begin() + index);
						}
					}
				}
			}
		}
	}
	
	return 0;
}
