#include "protocol.h"
#include <string.h>
#include <stdio.h>

int protocol_init(shared_data_t *shared_data)
{
    if(shared_data == NULL) //vai verificar se a memória compartilhada é valida
    {
        return 0;
    }

    shared_data -> nome_usuario[0] = '\0';
    shared_data -> inicio = 0;
    shared_data -> fim = 0;
    shared_data -> quantidade = 0;

    if(!iniciar_mutex(&shared_data -> lock))
    {
        return 0;
    }

    return 1;
}

void protocol_destroy(shared_data_t * shared_data)
{
    if(shared_data == NULL)
    {
        return;
    }

    destruir_mutex(&shared_data -> lock); // vai destruir o mutex que pertence ao cliente 
}

void parse_input(const char *linha, shared_data_t *shared_data) 
{

    tipo_acao_t acao;
    char conteudo [MAX_MSG];

    //indentifica a ação recebida
    if(strncmp(linha, ":nome ", 6) == 0) // cada if aqui checa ara ver qual acao foi executada (olha se foi nome, desconectar ou enviar mensagem)
    {
        acao = ACAO_MUDAR_NOME; // passo a acao mudar nome para a acao na mem compartilhada para o servidor saber qual é a acao

        strncpy (conteudo, linha + 6, MAX_MSG - 1); // copia o nome
        conteudo [MAX_MSG - 1] = '\0';  // parar no \0 que é onde acabou a string
    }
    else if (strncmp(linha, ":quit", 5) == 0) 
    {
        acao = ACAO_DESCONECTAR;
        conteudo[0] = '\0';
    } 
    else 
    {
        acao = ACAO_ENVIAR_MSG;
        strncpy(conteudo, linha, MAX_MSG - 1);
        conteudo[MAX_MSG - 1] = '\0';
    }

    bloquear_mutex(&shared_data -> lock); // Tranco o mutex porque vamos escrever na mem compartilhada

    //verifica fila cheia
    if(shared_data->quantidade >= MAX_FILA){
        fprintf(stderr, "Fila de acoes cheia\n");
        liberar_mutex(&shared_data -> lock);
        return;
    }

    shared_data->fila[shared_data->fim].acao = acao; //adciona ação na fila

    strncpy(shared_data->fila[shared_data->fim].conteudo, conteudo, MAX_MSG - 1); //adciona conteudo da ação na fila

    shared_data->fila[shared_data->fim].conteudo[MAX_MSG - 1] = '\0'; //adciona sinalizador de fim da string 
    shared_data->fim = (shared_data->fim + 1) % MAX_FILA;   //altera posição do fim da fila
    shared_data->quantidade++; //registra ação no numero de ações

    liberar_mutex(&shared_data -> lock); // destranca o mutex
}

tipo_acao_t process_shared_data(shared_data_t *shared_data, char *eco, int tam_eco, char *broadcast, int tam_broadcast)
{
    item_acao_t item;
    char horario[TAM_HORARIO];

    if (tam_eco > 0) eco[0] = '\0';
    if (tam_broadcast > 0) broadcast[0] = '\0';

    bloquear_mutex(&shared_data->lock);

    if (shared_data->quantidade == 0) {
        liberar_mutex(&shared_data->lock);
        return ACAO_NENHUMA;
    }

    item = shared_data->fila[shared_data->inicio];
    shared_data->inicio = (shared_data->inicio + 1) % MAX_FILA;
    shared_data->quantidade--;

    if (item.acao == ACAO_MUDAR_NOME) {
        strncpy(shared_data->nome_usuario, item.conteudo, MAX_NOME - 1);
        shared_data->nome_usuario[MAX_NOME - 1] = '\0';
    } 
    else if (item.acao == ACAO_ENVIAR_MSG) {
        hora_atual(horario, sizeof(horario));
        snprintf(eco, tam_eco, "Voce digitou: %s\n", item.conteudo);
        snprintf(broadcast, tam_broadcast, "%s (%s): %s\n", shared_data->nome_usuario, horario, item.conteudo);
    }

    liberar_mutex(&shared_data->lock);

    return item.acao;
}

