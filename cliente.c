#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>
#include <string.h>
#include <sys/select.h> 

#define MAX_MSG 256

int main(int argc, char **argv) {
    if (argc != 3) {
        printf("Usage: %s <dst_ip> <dst_port>\n", argv[0]);
        return 0;
    }

    // socket
    int cfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (cfd == -1) {
        perror("socket()");
        return -1;
    }

    struct sockaddr_in saddr;
    saddr.sin_port = htons(atoi(argv[2]));
    saddr.sin_family = AF_INET;
    saddr.sin_addr.s_addr = inet_addr(argv[1]);

    // connect
    if (connect(cfd, (struct sockaddr*) &saddr, sizeof(struct sockaddr_in)) == -1) {
        perror("connect()");
        return -1;
    }

    printf("Conectado! Pressione ENTER para solicitar conexão ao par OU aguarde...\n");

    fd_set readfds;
    char msg[MAX_MSG], resp[MAX_MSG];
    int max_fd = cfd; // O maior descritor é o socket (stdin é 0)

    while(1) {
        FD_ZERO(&readfds);
        FD_SET(0, &readfds);   // Adiciona STDIN (Teclado)
        FD_SET(cfd, &readfds); // Adiciona Socket (Servidor)

        // select bloqueia até que algo aconteça no teclado OU no socket
        int activity = select(max_fd + 1, &readfds, NULL, NULL, NULL);

        if ((activity < 0)) {
            perror("select error");
            break;
        }

        // se o usuário apertou enter
        if (FD_ISSET(0, &readfds)) {
            bzero(msg, MAX_MSG);
            fgets(msg, MAX_MSG, stdin); 

            // Envia para o servidor
            if (write(cfd, msg, strlen(msg)) == -1) {
                perror("write()");
                break;
            }
            printf("Solicitação enviada. Aguardando dados do par...\n");
        }

        // o usuário só recebe informações do par
        if (FD_ISSET(cfd, &readfds)) {
            bzero(resp, MAX_MSG);
            int nr = read(cfd, resp, MAX_MSG);
            
            if (nr <= 0) {
                printf("Servidor desconectou ou erro.\n");
                break;
            }
        
            printf("\n>>> DADOS RECEBIDOS DO SERVIDOR: %s\n", resp);
            
            break; 
        }
    }

    while(1) sleep(10);
    
    close(cfd);
    return 0;
}