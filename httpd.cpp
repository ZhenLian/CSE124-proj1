#include <iostream>
#include "httpd.h"
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <map>
#include <fstream>
#include <ctime>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string>
#include <vector>
#include <sstream>
#include <libgen.h>
#include <unordered_set>
#include <limits.h>
#include "httpd.h"

using namespace std;



int currentThread = 0;
struct ThreadArgs {
	int clntSock;
	string doc_root;
	string clntIP;
};



ssize_t sendall(int sockfd, const char *buffer, ssize_t len) {
    ssize_t ret, sent = 0;
    while(sent < len) {
        ret = send(sockfd, buffer + sent, len - sent, 0);
        if(ret < 0) {
            return -1;
        }
        sent += ret;
    }
    return sent;
}

ssize_t receiveall(int sockfd, char *buffer) {
	ssize_t rec = 0;
	const int bufsize = 8 * 1024; // 8k
    memset(buffer, '\0', strlen(buffer));
    const char *delim = "\r\n\r\n";
    // receiving the message
    while (strstr(buffer, delim) == NULL) {
	    int numBytes = recv(sockfd, buffer + rec, bufsize - 1 - rec, 0);
	    if(numBytes < 0) {
	        printf("Error: Could not receive filename\n");
	        return -1;
	    }
	    rec += numBytes;
	}
	return rec;
}

void start_httpd(unsigned short port, string doc_root) {
	cout << "Invoking server on port " << port << " using dir " << doc_root << endl;
	int listenfd = 0, connfd = 0;
	struct sockaddr_in serv_addr;

	//creating a socket
	listenfd = socket(AF_INET, SOCK_STREAM, 0);
	
	int enable = 1;
	if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
		cout << "setsockopt(SO_REUSEADDR) failed" << endl;
		return;
	}

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port = htons(port);

	//binding socket with the address and port
	if(::bind(listenfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
		cout << "bind failed" << endl;
		return;
	} 

	//listening
	if(::listen(listenfd, 10) < 0) {
		cout << "listen failed" << endl;
		return;
	}

	struct timeval timeout;
	timeout.tv_sec = 10;
	timeout.tv_usec = 0;
	while(1) {
		cout << "currentThread: " << currentThread << endl;
		cout << "max thread: " << MAX_THREADS << endl;
		while(currentThread >= MAX_THREADS) {
			sleep(1);
		}
		++currentThread;
		cout << "listening" << endl;

		struct sockaddr_storage clntAddr; // Client address
		// Set length of client address structure (in-out parameter)
		socklen_t clntAddrLen = sizeof(clntAddr);
		connfd = ::accept(listenfd, (struct sockaddr*) &clntAddr, &clntAddrLen); 
		printf("new connection accepted\n");
		string clntIP;
		if((clntIP = getClientIP((struct sockaddr *) &clntAddr)) == "") {
			cout << "invalid clntIP" << endl;
			continue;
		}
		cout << "client IP: " << clntIP << endl;
		if(connfd < 0) {
			printf("Error: accept() failed\n");
			continue;
		}

		printf("accept succeed with socket: %d\n", connfd);

		if(setsockopt(connfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0) {
			cout << "setsockopt failed" << endl;
		}
		cout << "setsockopt succeed" << endl;
		//create seperate memory for client argument
		struct ThreadArgs *threadArgs = new ThreadArgs();
		if(threadArgs == NULL) {
			cout << "malloc() failed" << endl;
		}
		threadArgs->clntSock = connfd;
		threadArgs->doc_root = doc_root;
		threadArgs->clntIP = clntIP;
		cout << "creatig new thread" << endl;
		pthread_t threadID;
		int returnValue = pthread_create(&threadID, NULL, ThreadMain, threadArgs);
		if(returnValue != 0) {
			cout << "pthread_create() failed" << endl;
		}	

	}
}


string getClientIP(const struct sockaddr *address) {
	char addrBuffer[INET6_ADDRSTRLEN];

	void *numericAddress = &((struct sockaddr_in *) address)->sin_addr;

	// Convert binary to printable address
	if (inet_ntop(address->sa_family, numericAddress, addrBuffer, sizeof(addrBuffer)) == NULL) {
	cout << "invalid address" << endl; // Unable to convert
		return "";
	}
	string s(addrBuffer);
	return s;
}

void *ThreadMain(void *threadArgs) {
	pthread_detach(pthread_self());

	int clntSock = ((struct ThreadArgs *) threadArgs)->clntSock;
	string doc_root = ((struct ThreadArgs *) threadArgs)->doc_root;
	string clntIP = ((struct ThreadArgs *) threadArgs)->clntIP;
	delete (struct ThreadArgs *)threadArgs;
	HandleTCPClient(clntSock, doc_root);
	return (NULL);
}

void HandleError(int code, int clntSocket) {
	if (code == 400) {
		const char * message = "The client sent a malformed or invalid request that the server doesn’t understand";
		sendall(clntSocket, message, strlen(message));
	}
}

void HandleTCPClient(int clntSocket, const string& doc_root) {
    char header[BUFSIZE];
    printf("Receiving Header...\n");
    receiveall(clntSocket, header);
    printf("Header received: \n%s", header);
    char * file_name = manipulateHeader(header, clntSocket, doc_root);
    sendSuccessInfo(file_name, clntSocket);
    close(clntSocket);
    sleep(1);
	--currentThread;
}

char * manipulateHeader(const char * header, int clntSocket, const string& doc_root) {
	// manipulate GET Method
    char *pch = strstr(header, " ");
    if(!pch) {
        printf("Error: Invalid filename\n");
        HandleError(400,clntSocket);
        return NULL;
    }
    char method[4];
    strncpy(method, header, 3);
    if (strcmp(method, "GET") != 0 && strcmp(method, "get") != 0) {
    	printf("Get parse error\n");
    	HandleError(400,clntSocket);
        return NULL;
    } 

    // manipulate URL
    pch++;
    char *pch2 = strstr(pch, " ");
    if (!pch2) {
		printf("Error: Invalid filename\n");
        HandleError(400,clntSocket);
        return NULL;
    }

    char *file_name = (char*)malloc(doc_root.length() + (pch2 - pch) + 1);
    memset(file_name, '\0', strlen(file_name));
    std::strcpy(file_name, doc_root.c_str());
    strncat(file_name, pch, pch2 - pch);
    printf("File path received: %s\n", file_name);
    pch2++;
    char *pch3 = strstr(pch2, "\r");
    if (!pch3) {
		printf("Error: Invalid filename\n");
        HandleError(400,clntSocket);
        return NULL;
    }
    char version[9];
    strncpy(version, pch2, 8);
    if (strcmp(version, "HTTP/1.1") != 0) {
    	// send error message
    	printf("Version parse error\n");
    	HandleError(400,clntSocket);
        return NULL;
    }


    // key value 
    map<string, string> kvmap;
    char * kvpair_start = pch3 + 2;
    // printf("kvpair starts:%s\n", kvpair_start);                   
    while(kvpair_start) {
    	char *kvpair_end = strstr(kvpair_start, "\r");
    	if (!kvpair_end || kvpair_end == kvpair_start) {
    		break;
    	}
    	char *kvpair = (char *)malloc(kvpair_end - kvpair_start + 1);
    	memcpy(kvpair, kvpair_start, kvpair_end - kvpair_start);
    	string kv_str(kvpair);
    	size_t mid = kv_str.find(':');
    	if (mid == string::npos) {
    		printf("Missing : \n");
    		HandleError(400,clntSocket);
        	return NULL;
    	}
    	string key = kv_str.substr(0, mid - 0);
    	string value = kv_str.substr(mid + 1, string::npos);
    	kvmap[key] = value;
    	// cout<< "Key: " << key << " " << "Value: " << value << endl;
    	// printf("Key: %s Value: %s \n", key, value);
    	kvpair_start = kvpair_end + 2;
    }
    if (kvmap.count("host") != 1 && kvmap.count("HOST") != 1) {
    	printf("no host error \n");
    	HandleError(400,clntSocket);
        return NULL;
    }

    return file_name;
}

void sendSuccessInfo(char * file_name, int clntSocket) {
	//send Initial Line
    const char* cpnt1 = "HTTP/1.1 200 ";
    const char* desc = "OK: The request was successful";
    const char* cpnt2 = "\r\n"; 
    char *intial_line = (char *)malloc(strlen(cpnt1) + strlen(desc) + strlen(cpnt2) + 1);
    strcpy(intial_line, cpnt1);
    strcat(intial_line, desc);
    strcat(intial_line, cpnt2);
    int numBytes = sendall(clntSocket, intial_line, strlen(cpnt1) + strlen(desc) + strlen(cpnt2));
    if (numBytes < 0) {
        printf("recv() failed \n");
        return;
    }
  
    // send metadata(key-value pairs)
    string file(file_name);
    //last modified information and size
    struct stat fileInfo;
	if (stat(file_name, &fileInfo) != 0) {  // Use stat( ) to get the info
	  printf("get file stats failed \n");
        return;
	}
	char date[50];
	strftime(date, 50, "Last-Modified: %A, %d %m %Y %H:%M:%S GMT", localtime(&fileInfo.st_mtime));
	printf("%s", date);
	int file_size = fileInfo.st_size;
	string file_type;
	if (file.substr(file.find_last_of(".")) == ".jpg") {
		file_type = "image/jpeg";
	} 
	else if (file.substr(file.find_last_of(".")) == ".png") {
		file_type = "image/png";
	} 
	else if (file.substr(file.find_last_of(".")) == ".html") {
		file_type = "text/html";
	} 
	else {
		printf("get file file type failed \n");
        return;
	}
	string metadata = "Server: 127.0.1\r\n" + string(date) + "\r\n" + "Content-Type: " + file_type 
		+ "\r\nContent-Length: " + to_string(file_size) + "\r\n" + "\r\n";
	numBytes = sendall(clntSocket, metadata.c_str(), metadata.length());
    if (numBytes < 0) {
        printf("recv() failed \n");
        return;
    }

    // send content
    printf("\nContents: \n");
    ifstream in_stream(file);
	string line;
	while (getline(in_stream, line, '\0')){
		numBytes = sendall(clntSocket, line.c_str(), line.length());
		cout<< "Line: " << line << " Bytes: " << numBytes << endl;
        if (numBytes < 0) {
            printf("recv() failed \n");
            return;
        }
	}
	// ending
	sendall(clntSocket, "\r\n\r\n", 4);
	in_stream.close();
}



