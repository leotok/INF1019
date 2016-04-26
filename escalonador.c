#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/shm.h>
#include <string.h>
#include "lista.h"

#define TIME_SHARE = 0.5

#define CHAVE_INFO_FLAG 8752
#define CHAVE_TIPO 8753
#define CHAVE_NUM_TICKETS 8754
#define CHAVE_PRIORIDADE 8755
#define CHAVE_PATH 8756
#define CHAVE_END 8757

No *listaPrioridade = NULL;
No *listaRoundRobin = NULL;
No *listaLoteria = NULL;


void insereProcesso(char *path, int tipo, int prioridade, int numBilhetes) {
	
	pid_t pid = 0;

	// Cria processo filho que será substituido por programa em 'path'
	if((pid = fork()) < 0) {
		printf("Erro ao criar processo filho.\n");
		exit(1);
	}
	else if(pid == 0) {
		if(execv(path, NULL) < 0) {
			printf("Erro ao executar o programa %s.\n", path);
		}
	}
	else if(pid > 0) {
		kill(pid, SIGSTOP);
		printf("Escalonador criou processo filho %s.\n", path);
	}


	if (tipo == 0) {
		listaRoundRobin = insereNo(listaRoundRobin, pid, path, tipo, prioridade, numBilhetes);
	}
	else if (tipo == 1) {

		// Procura no com prioridade menor (ex:  prioridade 3 < prioridade 2) e insere antes dele
		printf("tipo Prioridade, inserindo na listaPrioridade...\n");
		No * p = listaPrioridade;
		No * ant = NULL;
		No * novo;

		if (p != NULL) {
			while(p->prioridade <= prioridade){
				p = p->prox;
			}
			printf("achou no\n");
			ant = p->ant;	
		}
		
		novo = insereNo(p, pid, path, tipo, prioridade, numBilhetes);
		novo->ant = ant;
	}
	else if (tipo == 2) {
		listaLoteria = insereNo(listaLoteria, pid, path, tipo, prioridade, numBilhetes);
	}
}

void rodaProcessoPrioridade() {

}

pid_t retiraPID (int tipo) {

	if (tipo == 0) {
		return retiraNo(&listaRoundRobin);
	}
	else if (tipo == 1) {
		return retiraNo(&listaPrioridade);
	}
	else if (tipo == 2) {
		return retiraNo(&listaLoteria);
	}
	return -1;
}

void realocaProcesso (int tipo) {

	if (tipo == 0) {
		No *processoRealocado = realocaNo(&listaRoundRobin);
	}
	else if (tipo == 1) {
		retiraNo(&listaPrioridade);
	}
	else if (tipo == 2) {
		retiraNo(&listaLoteria);
	}
	else {
		return;
	}
}

void liberaEscalonador() {
	liberaLista(listaRoundRobin);
	liberaLista(listaPrioridade);
	liberaLista(listaLoteria);
}

int main() {

	int segmentoEnd, segmentoNovaInfoFlag, segmentoPath, segmentoNumTickets, segmentoPrioridade, segmentoTipo;
	char *path; //[15];
	int *end, *numTickets, *prioridade, *tipo, *novaInfoFlag;

	segmentoNovaInfoFlag = shmget(CHAVE_INFO_FLAG, sizeof(int), S_IRUSR | S_IWUSR );
	if( segmentoNovaInfoFlag < 0 ) { 
		printf(" erro ao criar segmento de novainfoflag\n");
		exit(1);
	}
	novaInfoFlag = (int *) shmat(segmentoNovaInfoFlag, 0, 0);

	segmentoEnd = shmget(CHAVE_END, sizeof(int), S_IRUSR | S_IWUSR );
	if( segmentoEnd < 0 ) { 
		printf(" erro ao criar segmento de end\n");
		exit(1);
	}
	end = (int *) shmat(segmentoEnd, 0, 0);


	segmentoPrioridade = shmget(CHAVE_PRIORIDADE, sizeof(int), S_IRUSR | S_IWUSR );
	if( segmentoPrioridade < 0 ) { 
		printf(" erro ao criar segmento de prioridade\n");
		exit(1);
	}
	prioridade = (int *) shmat(segmentoPrioridade, 0, 0);

	segmentoTipo = shmget(CHAVE_TIPO, sizeof(int), S_IRUSR | S_IWUSR );
	if( segmentoTipo < 0 ) { 
		printf(" erro ao criar segmento de tipo\n");
		exit(1);
	}
	tipo = (int *) shmat(segmentoTipo, 0, 0);

	segmentoNumTickets = shmget(CHAVE_NUM_TICKETS, sizeof(int), S_IRUSR | S_IWUSR );
	if( segmentoNumTickets < 0 ) { 
		printf(" erro ao criar segmento de numtickets\n");
		exit(1);
	}
	numTickets = (int *) shmat(segmentoNumTickets, 0, 0);

	segmentoPath = shmget(CHAVE_PATH, sizeof(char) * 15, S_IRUSR | S_IWUSR );
	if( segmentoPath < 0 ) { 
		printf(" erro ao criar segmento de path\n");
		exit(1);
	}
	path = (char *) shmat(segmentoPath, 0, 0);
	
	// aqui vamos ter q ficar rodando esperando informacoes novas a partir do flag q a main disparar
	// quando ler coisas nova insere em alguma lista
	// se nao le nada novo, continua escalonando todos os processos até eles terminarem

	while(1) {

		
		if(*novaInfoFlag == 1){
			No *processoRodando = listaPrioridade;
			printf("path: %s, tipo: %d, numTickets: %d, prioridade: %d \n", path, *tipo, *numTickets, * prioridade );			
			*novaInfoFlag = 0;
			insereProcesso(path, *tipo, *prioridade, *numTickets);
			rodaProcessoPrioridade();
		}
		else if (*end == 1 && listaPrioridade == NULL && listaLoteria == NULL && listaRoundRobin == NULL) {
			break;
		}

	}

	printf("Escalonador terminou de executar todos programas.\n");
	// libera memoria compartilhada do processo
	shmdt(tipo);
	shmdt(prioridade);
	shmdt(numTickets);
	shmdt(novaInfoFlag);
	shmdt(path);

	// libera memoria compartilhada
	shmctl(segmentoPath, IPC_RMID, 0);
	shmctl(segmentoTipo, IPC_RMID, 0);
	shmctl(segmentoPrioridade, IPC_RMID, 0);
	shmctl(segmentoNumTickets, IPC_RMID, 0);
	shmctl(segmentoNovaInfoFlag, IPC_RMID, 0);

	liberaEscalonador();
	
	return 0;
}
