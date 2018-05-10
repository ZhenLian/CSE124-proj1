#include <stdio.h>
#include <string.h>
#include <strings.h>
#include "client.h"
 
int main(int argc, char* argv[])
{
    /* Parse the command line arguments */
    if (argc != 5) {
        fprintf(stderr,
                "Usage: %s server-ip server-port filepath arguments \n",
                argv[0]);
        return 1;
    }
    char * server_ip_str = argv[1];
    char * server_port_str = argv[2];
    char * fp = argv[3];
    char * argstr = argv[4];

    return invoke_client(server_ip_str, server_port_str, fp, argstr);
}
