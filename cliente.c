#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>
#include <string.h>
#include <sys/select.h>
#include <pthread.h> 

#define MAX_MSG 256

void *listen_udp(void *arg) {
        int sock = *(int*)arg;
        char buffer[MAX_MSG];
        struct sockaddr_in sender;
        socklen_t len = sizeof(sender);
    
        printf("[UDP-Thread] Ouvindo porta UDP...\n");
        while(1) {
            int n = recvfrom(sock, buffer, MAX_MSG, 0, (struct sockaddr*)&sender, &len);
            if (n > 0) {
                buffer[n] = '\0';
                // Se receber isso, O HOLE PUNCHING FUNCIONOU!
                printf("\n\n>>> SUCESSO! MENSAGEM UDP RECEBIDA DE (%s:%d): %s\n", 
                       inet_ntoa(sender.sin_addr), ntohs(sender.sin_port), buffer);
            }
        }
        return NULL;
    }

int main(int argc, char **argv) {
    if (argc != 3) {
        printf("Usage: %s <dst_ip> <dst_port>\n", argv[0]);
        return 0;
    }

    // socket tcp
    int cfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (cfd == -1) {
        perror("socket()");
        return -1;
    }

    struct sockaddr_in saddr;
    saddr.sin_port = htons(atoi(argv[2]));
    saddr.sin_family = AF_INET;
    saddr.sin_addr.s_addr = inet_addr(argv[1]);

    struct sockaddr_in my_local_addr;
    socklen_t len_local = sizeof(my_local_addr);

    // connect
    if (connect(cfd, (struct sockaddr*) &saddr, sizeof(struct sockaddr_in)) == -1) {
        perror("connect()");
        return -1;
    }

    getsockname(cfd, (struct sockaddr*)&my_local_addr, &len_local);
    int my_tcp_port = ntohs(my_local_addr.sin_port);
    printf("Minha porta TCP Local: %d\n", my_tcp_port);
    printf("Conectado! Pressione ENTER para solicitar conexão ao par OU aguarde...\n");

    fd_set readfds;
    char msg[MAX_MSG], resp[MAX_MSG];
    int max_fd = cfd; // O maior descritor é o socket (stdin é 0)

    // Variáveis para guardar os dados do PAR
    char peer_ip[32];
    int peer_tcp_port = 0;
    int tem_dados = 0;

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
            sscanf(resp, "%s %d", peer_ip, &peer_tcp_port);
            tem_dados = 1;
            printf("\n>>> DADOS RECEBIDOS DO SERVIDOR: %s\n", resp);
            
            break; 
        }
    }

    if (!tem_dados) return 0;

    int peer_udp_port = peer_tcp_port + 1;

    printf("Par TCP (NAT): %s:%d\n", peer_ip, peer_tcp_port);
    printf("Alvo UDP (NAT): %s:%d (Tentativa Incremental +1)\n", peer_ip, peer_udp_port);

    // Cria Socket UDP
    int udp_sock = socket(AF_INET, SOCK_DGRAM, 0);

    int yes = 1;
    if (setsockopt(udp_sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1) {
        perror("setsockopt");
    }
    
    struct sockaddr_in my_udp_addr;
    my_udp_addr.sin_family = AF_INET;
    my_udp_addr.sin_addr.s_addr = INADDR_ANY; // Qualquer IP da máquina
    my_udp_addr.sin_port = htons(my_tcp_port + 1); // A Mágica do Incremental

    if (bind(udp_sock, (struct sockaddr*)&my_udp_addr, sizeof(my_udp_addr)) < 0) {
        perror("Erro ao fazer bind na porta incremental UDP");
    }

    struct sockaddr_in peer_addr;
    peer_addr.sin_family = AF_INET;
    peer_addr.sin_port = htons(peer_udp_port);
    peer_addr.sin_addr.s_addr = inet_addr(peer_ip);

    // Inicia thread para ouvir (para não bloquear o envio)
    pthread_t tid;
    pthread_create(&tid, NULL, listen_udp, &udp_sock);

    char udp_msg[] = "";
    
    printf("Enviando pacotes UDP para furar o NAT...\n");
    for(int i=0; i<20; i++) {
        sendto(udp_sock, udp_msg, strlen(udp_msg), 0, 
               (struct sockaddr*)&peer_addr, sizeof(peer_addr));
        
        printf("Pacote UDP %d enviado para %s:%d\n", i+1, peer_ip, peer_udp_port);
        sleep(1); // Espera 1 seg entre tentativas
    }

    printf("Fim do envio. Aguardando respostas...\n");
    pthread_join(tid, NULL);

    while(1) sleep(10);
    close(udp_sock);
    close(cfd);
    return 0;
}