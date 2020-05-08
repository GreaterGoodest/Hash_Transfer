#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/sendfile.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#define BUFFER 4096
#define DEFAULT 2345
#define MAX_HASH 128

int server_connect(char *address, int port);

int send_algo(int socketfd, char *algo);

int transfer_file(int socketfd, char *filename, char *hash);

void print_help();

int main(int argc, char **argv){
    int ret = EXIT_SUCCESS;
    char *hash = NULL;
    int next_arg = 1;

    if (argc > 1 && strcmp(argv[next_arg], "-h") == 0){
        print_help();
        goto cleanup;
    } 

    if (argc < 4){
        puts("Not enough arguments provided");
        print_help();
        ret = EXIT_FAILURE;
        goto cleanup;
    }

    int port = 0;
    if (strcmp(argv[next_arg], "-p") == 0){
        port = atoi(argv[++next_arg]);
        next_arg++;
    }

    char *address = argv[next_arg];
    char *hash_algo = argv[++next_arg];

    int connection = server_connect(address, port);

    if (next_arg >= argc){ //Edge case: user provides port but no files
        puts("No files provided");
        print_help();
        ret = EXIT_FAILURE;
        if (ret > 0){
            perror("Unable to send file");
            goto cleanup;
        }
        goto cleanup;
    }

    ret = send_algo(connection, hash_algo);

    char *curr_file = argv[++next_arg];
    hash = calloc(sizeof(char), MAX_HASH+1);

    uint32_t file_count = argc - next_arg;
    write(connection, &file_count, sizeof(file_count));

    while(next_arg < argc){
        curr_file = argv[next_arg];
        ret = transfer_file(connection, curr_file, hash);
        if (ret > 0){
           printf("Unable to transfer: %s\n", curr_file); 
           goto cleanup;
        }
        printf("  %s  %s\n", hash, curr_file);
        next_arg++; 
    }

cleanup:
    if (hash)
        free(hash);
    return ret;
}

int server_connect(char *address, int port){
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);

    if (sockfd < 0){
        perror("Failed to create socket!");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in server;
    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;

    server.sin_port = (port) ? htons(port) : htons(DEFAULT); //set to default if no port
    if (inet_pton(AF_INET, address, &server.sin_addr) < 0){
        perror("IP address format incorrect");
        exit(EXIT_FAILURE);
    }

    if (port)
        printf("Connecting to %s:%d\n", address, port);
    else
        printf("Connecting to %s:%d\n", address, DEFAULT);
    if (connect(sockfd, (const struct sockaddr *) &server, sizeof(server)) < 0){
        perror("Failed to connect to server");
        exit(EXIT_FAILURE);
    }

    return sockfd;
}

int send_algo(int socketfd, char *algo){
    uint32_t algo_len = strlen(algo);
    write(socketfd, &algo_len, sizeof(algo_len));
    write(socketfd, algo, strlen(algo));
    return 0;
}

int transfer_file(int socketfd, char *filename, char *hash){
    if(access(filename, R_OK) == -1){
        perror("Unable to read file");
        return EXIT_FAILURE;
    }

    int file = open(filename, O_RDONLY);
    if (file == -1){
        perror("Failed to open file");
        return EXIT_FAILURE;
    }

    struct stat file_stat; 
    if (fstat(file, &file_stat) < 0){
        perror("Unable to read file");
        return EXIT_FAILURE;
    }


    
    int sent = 0;
    off_t offset = 0;
    uint32_t to_transfer = file_stat.st_size;
    write(socketfd, &to_transfer, sizeof(to_transfer));
    while (((sent = sendfile(socketfd, file, &offset, BUFFER))>0) && (to_transfer > 0)){
        to_transfer -= sent;
    }

    read(socketfd, hash, MAX_HASH);

    return 0;
}

void print_help(){
    puts("Usage: ./hashclient [-p port] <ip> <hash algorithm> <file> [additional files]");
}