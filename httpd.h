#ifndef HTTPD_H
#define HTTPD_H

#include <string>
using namespace std;

#define MAX_THREADS 5
#define BUFSIZE 8192

void *ThreadMain(void *threadArgs);
void HandleTCPClient(int clntSocket, const string& doc_root);
string getClientIP(const struct sockaddr *address);
char * manipulateHeader(const char * header, int clntSocket, const string& doc_root);
void sendSuccessInfo(char * file_name, int clntSocket);

#endif // HTTPD_H
