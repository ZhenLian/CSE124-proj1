#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <libgen.h>

#include "client.h"


ssize_t sendall(int sockfd, const char *buffer, size_t len) {
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




int invoke_client(char * server_ip_str, char * server_port_str,
				  char * filepath, char * arguments)
{
    // create a socket
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        printf("\n Cannot create the socket! \n");
        return -1;
    }
    //  connect 
    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    if(inet_pton(AF_INET, server_ip_str, &serv_addr.sin_addr)<=0) {
        printf("\n inet_pton error occured \n");
        return 1;
    }
    serv_addr.sin_port = htons(atoi(server_port_str));
    // (struct sockaddr *)&serv_addr is so important that 
    // it first get the adress of serv_addr (which is * sockaddr_in)
    // then convert it to struct sockaddr * (specific type to generic type)
    if(connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
       printf("Error : Connect Failed \n");
       return 1;
    }
    else {
        printf("Connection to server successful\n");
    }


    sendHeader(sockfd, filepath, arguments);
    


    char buf[BUFSIZE];
    memset(buf, '\0', sizeof(buf));
    receiveall(sockfd, buf);
    printf("%s\n", buf);

    // get the content length 
    char *pch = strstr(buf, "Content-Length");
    if(!pch) {
        printf("Cannot find Content-Length\n");
        return -1;
    }
    char *pch2 = strstr(pch, ":");
    // avoid white spaces
    pch2 += 2;
    char *pch3 = strstr(pch2, "\r\n");
    char conlength[10];
    memcpy(conlength, pch2, pch3 - pch2);
    int length = atoi(conlength);
    printf("%d\n", length);

    // receive the content and store it locally
    char content[BUFSIZE];
    memset(content, '\0', sizeof(content));
    receiveall(sockfd, content);
    printf("%s\n", content);
    FILE *output_stream = fopen(filepath, "w");
    fwrite(content, sizeof(char), length, output_stream); 
    fclose(output_stream);

    close(sockfd);
	return 0;  // 0 on success, non-zero on error
}


void sendHeader(int sockfd, char * filepath, char * arguments) {
    // send initial line
    char * initline;
    const char *tmp1 = "GET ";
    const char *tmp2 = " HTTP/1.1\r\n";
    initline = (char *)malloc(strlen(tmp1) + strlen(filepath) + strlen(tmp2) + 1);
    strcpy(initline, tmp1);
    strcat(initline, filepath);
    strcat(initline, tmp2);
    ssize_t sendBytes = sendall(sockfd, initline, strlen(initline));
    if(sendBytes < 0 || sendBytes != strlen(initline)) {
        printf("send() failed\n");
        return;
    }

    printf("Initial line sent successful\n");

    // send the parameters
    char * kvpair_start = arguments;
    while(kvpair_start) {
        // printf("copy starts\n");
        char *kvpair_end = strstr(kvpair_start, "&");
        if (kvpair_end == NULL) {
            // printf("this is LL\r\n");
            char *kvpair = (char *)malloc(strlen(kvpair_start) + 5);
            char *keyend = strstr(kvpair_start, "=");
            //copy key
            memcpy(kvpair, kvpair_start, keyend - kvpair_start);
            // printf("copy key successful\n");
            const char* tmp3 = ": ";
            memcpy(kvpair + (keyend - kvpair_start), tmp3, strlen(tmp3));
            //copy value
            memcpy(kvpair + (keyend - kvpair_start) + strlen(tmp3), keyend + 1, strlen(keyend + 1));
            const char* tmp4 = "\r\n";
            memcpy(kvpair + (keyend - kvpair_start) + strlen(tmp3) + strlen(keyend + 1), tmp4, strlen(tmp4));
            //send message

            printf("%s", kvpair);

            ssize_t sendBytes = sendall(sockfd, kvpair, strlen(kvpair));
            if(sendBytes < 0 || sendBytes != strlen(kvpair)) {
                printf("send() failed\n");
                return;
            }
            break;
        }
        char *kvpair = (char *)malloc(kvpair_end - kvpair_start + 5);
        char *keyend = strstr(kvpair_start, "=");
        //copy key
        memcpy(kvpair, kvpair_start, keyend - kvpair_start);
        // printf("copy key successful\n");
        const char* tmp3 = ": ";
        memcpy(kvpair + (keyend - kvpair_start), tmp3, strlen(tmp3));
        //copy value
        memcpy(kvpair + (keyend - kvpair_start) + strlen(tmp3), keyend + 1, kvpair_end - (keyend + 1));
        const char* tmp4 = "\r\n";
        memcpy(kvpair + (keyend - kvpair_start) + strlen(tmp3) + (kvpair_end - (keyend + 1)), tmp4, strlen(tmp4));
        //send message

        printf("%s", kvpair);

        ssize_t sendBytes = sendall(sockfd, kvpair, strlen(kvpair));
        if(sendBytes < 0 || sendBytes != strlen(kvpair)) {
            printf("send() failed\n");
            return;
        }
        kvpair_start = kvpair_end + 1;
    }

    const char* tmp4 = "\r\n";
    sendBytes = sendall(sockfd, tmp4, strlen(tmp4));
    if(sendBytes < 0 || sendBytes != strlen(tmp4)) {
        printf("send() failed\n");
        return;
    }

    printf("sending finished!\r\n");
}