#ifndef PROTOCOL_H
#define PROTOCOL_H

#include "platform.h"

#define MAX_NOME 64
#define MAX_MSG 512
#define MAX_FILA 32

// Criando uma enumeracao das acoes predefinidas que o usuario pode fazer 

typedef enum {

    ACAO_NENHUMA,
    ACAO_MUDAR_NOME,
    ACAO_ENVIAR_MSG,
    ACAO_DESCONECTAR,

} tipo_acao_t;


//estrutura para guardar ações na fila
typedef struct {
    tipo_acao_t acao;
    char conteudo[MAX_MSG];
} item_acao_t;

// Estrutura para a memória compartilhada entre as threads thread 1 (gravar/escrever) e a thread 2 (ler/processar) do servidor

typedef struct mem_compartilhada {

    char nome_usuario[MAX_NOME]; // nome do usuario para aparecer na mensagem
    item_acao_t fila[MAX_FILA]; // fila de ações pendentes
    int inicio; //proxima ação a ser processada
    int fim;    //lugar para onde vai uma nova ação
    int quantidade; //quantidade de ações na fila
    mutex_t lock; // mutex na fase 1 era global ou seja todos clientes iam compartilhar do mesmo mutex, dai se um cliente ta usando todos outros tem que esperar, agora cada cliente tem seu mutex proprio acabando com essa espera

} shared_data_t;


// funcao que vai receber a info do socket ve qual acao é, de quem é, qual msg,  grava o resultado na mem compartlhada

int protocol_init(shared_data_t *shared_data); // ela inicia a cada cliente especifico agora (antes so tinha um cliente entao nao precisava disso) (retorna 1 se foi sucesso e 0 se falhou)

void protocol_destroy(shared_data_t *shared_data); // essa funcao destroi (limpa) os mutex e dados de um cliente especifico

void parse_input(const char *linha, shared_data_t *shared_data); 

// funcao para o servidor processar o que estiver pendente, escreve o texto formatado para imprimir, retorna 1 se tiver processado algo, 0 se nao processou

tipo_acao_t process_shared_data(shared_data_t *shared_data, char *saida, int tam_saida);

#endif