#ifndef CLIENT_H
#define CLIENT_H

#define BUFSIZE 8192

int invoke_client(char * server_ip_str, char * server_port_str, char * filepath, char * arguments);
void sendHeader(int sockfd, char * filepath, char * arguments);
ssize_t sendall(int sockfd, const char *buffer, size_t len);
ssize_t receiveall(int sockfd, char *buffer);
#endif // CLIENT_H
