#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <string.h>

#define MAX_REQ 64536
#define MAX_CONN 1000

struct targs {
	pthread_t tid;
	int cfd;
	struct sockaddr_in caddr;
};
typedef struct targs targs;

targs tclients[MAX_CONN + 3];
int num_clients = 0; // Contador simples de clientes

void init(targs *tclients, int n){
	int i;
	for (i = 0; i< MAX_CONN + 3; i++) {
		tclients[i].cfd = -1;
	}
}

void *handle_client(void *args) {
		int cfd = *(int *)args;
		
		int my_index = -1;
		for(int i=0; i<MAX_CONN; i++) {
			if(tclients[i].cfd == cfd) {
				my_index = i;
				break;
			}
		}

		int peer_index = (my_index == 0) ? 1 : 0;
    
		printf("Cliente %d conectado (%s:%d).\n", 
				my_index, inet_ntoa(tclients[my_index].caddr.sin_addr), 
				ntohs(tclients[my_index].caddr.sin_port));

		// Espera até que o outro cliente conecte (cfd deixe de ser -1)
		while(tclients[peer_index].cfd == -1) {
			usleep(100000); 
		}
		
		// recv
		int nr, ns;
		unsigned char requisicao[MAX_REQ];
		bzero(requisicao, MAX_REQ);
		nr = recv(cfd, requisicao, MAX_REQ, 0);
		if (nr < 0) {
			perror("erro no recv()");
			return NULL;
		}

		printf("Recebeu do cliente:%d): '%s'\n", my_index, requisicao);

		// send
		char resposta[256];
    	bzero(resposta, 256);

		// my_index 0 = cliente A, my_index 1 = cliente B
		char tipo = (my_index == 0) ? 'A' : 'B';

		sprintf(resposta, "%s %d %c", 
            inet_ntoa(tclients[peer_index].caddr.sin_addr),
            ntohs(tclients[peer_index].caddr.sin_port),
            tipo);

    	printf("Enviando para cliente %d (tipo %c) os dados do par: %s\n", my_index, tipo, resposta);
		
		ns = send(tclients[my_index].cfd, resposta, strlen(resposta), 0);
		if (ns < 0){
			perror("erro no send()");
			return NULL;
		}

		char resposta_par[256];
    	bzero(resposta_par, 256);
		char tipo_par = (peer_index == 0) ? 'A' : 'B';
		sprintf(resposta_par, "%s %d %c", 
            inet_ntoa(tclients[my_index].caddr.sin_addr),
            ntohs(tclients[my_index].caddr.sin_port),
            tipo_par);

    	printf("Enviando para cliente %d (tipo %c) os dados do par: %s\n", peer_index, tipo_par, resposta_par);
		
		ns = send(tclients[peer_index].cfd, resposta_par, strlen(resposta_par), 0);
		if (ns < 0){
			perror("erro no send()");
			return NULL;
		}

		//manter a conexão aberta 
		// close
		// close(tclients[cfd].cfd);
		// tclients[cfd].cfd = -1;

		while(1) {
			sleep(10);
		}

		return NULL;
}

int main(int argc, char **argv) {

	if (argc != 2){
		printf("Uso: %s <porta> \n", argv[0]);
		return 0;
	}

	init(tclients, MAX_CONN+3);

	struct sockaddr_in saddr;

	// socket
	int sl = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	// bind
	saddr.sin_family = AF_INET;
	saddr.sin_port = htons(atoi(argv[1]));
	saddr.sin_addr.s_addr = INADDR_ANY;

	if (bind(sl, (const struct sockaddr *)&saddr, sizeof(struct sockaddr_in)) < 0) {
		perror("erro no bind()");
		return -1;
	}

	if (listen(sl, 1000) < 0) {
		perror("erro no listen()");
		return -1;
	}

	// accept
	int cfd, addr_len;
	struct sockaddr_in caddr;
	addr_len = sizeof(struct sockaddr_in);

	while(1) {

		cfd = accept(sl, (struct sockaddr *)&caddr, (socklen_t *)&addr_len);
		if (cfd == 1) {
			perror("erro no accept()");
			return -1;
		}

		int idx = (num_clients % 2); // 0 ou 1

		tclients[idx].cfd = cfd;
		tclients[idx].caddr = caddr;
		int *arg = malloc(sizeof(int));
        *arg = cfd;

		pthread_create(&tclients[idx].tid, NULL, handle_client, (void*)arg);
		num_clients++;
	}
	
	for (int i = 0; i< MAX_CONN + 3; i++) {
		if (tclients[i].cfd == -1) continue;
		pthread_join(tclients[i].tid, NULL);
	}

	return 0;
}