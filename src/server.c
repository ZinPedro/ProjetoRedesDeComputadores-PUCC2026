#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//inclui o arquivo de cabeçalho da plataforma para compatibilidade entre Windows e Linux
#include "../include/platform.h"

#include "../include/protocol.h"

#include <limits.h> 
#include <errno.h>

#define PORT 8080
#define BUFFER_SIZE 1024
#define INTERVALO_TESTE 60

//estrutura para armazenar os dados do cliente (numero do cliente e status de conexão)
typedef struct {
    socket_t client_fd;
    int conectado;
    mutex_t mutex;      //protege a variável "conectado" para evitar condições de corrida entre threads
    mutex_t mutex_envio; //protege send() (envio de mensagem) para esse ciente
    shared_data_t protocolo; // aqui é a variavel que vai ter acesso a mem compartilhada para gravar o que o usuario digitou
} DadosCliente;

//estrutura para lista de clientes conectados
typedef struct {
    DadosCliente **clientes;
    int quantidade;
    int limite;
    mutex_t mutex; 
}ListaClientes;

//estrutura para passar o contexto do cliente para as threads (junta as informações do cliente e da lista de clientes)
typedef struct {
    DadosCliente *cliente;
    ListaClientes *lista;
} ContextoCliente;

//função para definir o status de conexão do cliente de forma thread-safe
void definir_conectado(DadosCliente *cliente, int valor){
    bloquear_mutex(&cliente->mutex);
    cliente->conectado = valor;
    liberar_mutex(&cliente->mutex);
}

//função para verificar o status de conexão do cliente de forma thread-safe
int verificar_conectado(DadosCliente *cliente){
    int conectado;
    bloquear_mutex(&cliente->mutex);
    conectado = cliente->conectado;
    liberar_mutex(&cliente->mutex);
    return conectado;
}

int enviar_para_cliente(DadosCliente *cliente, const char *mensagem){
    bloquear_mutex(&cliente->mutex_envio); //bloqueia o mutex de envio do cliente para evitar que outras threads enviem mensagens ao mesmo tempo
    
    int tamanho = (int)strlen(mensagem);
    int total_enviado = 0;

    //garante que toda a mensagem seja enviada, mesmo que o send() não consiga enviar tudo de uma vez
    while(total_enviado < tamanho){
        // configuração do send (socket, mensagem, tamanho da mensagem, flags)
        socket_io_t enviados = send(cliente->client_fd, mensagem + total_enviado, tamanho - total_enviado, 0);
        if(enviados == PLATFORM_SOCKET_ERRO || enviados == 0) {
            liberar_mutex(&cliente->mutex_envio);
            return 0;
        }
        total_enviado += enviados;
    }
    liberar_mutex(&cliente->mutex_envio);
    return 1;
}

//função para utilização da primeira thread, responsável por receber os dados do cliente
THREAD_FUNC(receber_dados) {

    DadosCliente *cliente = (DadosCliente *)arg;
    //buffer para armazenar os dados recebidos do cliente
    char buffer[BUFFER_SIZE];
    //variável para armazenar o número de bytes recebidos
    socket_io_t bytes_recebidos;    

    //receber mensagem do cliente
    //configuração do recv (socket, buffer, tamanho do buffer, flags)
    while(1){
        bytes_recebidos = recv(cliente->client_fd, buffer, BUFFER_SIZE - 1, 0);

        if(bytes_recebidos == PLATFORM_SOCKET_ERRO) {
            definir_conectado(cliente, 0); //marca o cliente como desconectado
            mostrar_erro_socket("Erro ao receber mensagem");
            break;
        }

        if(bytes_recebidos == 0) {
            definir_conectado(cliente, 0); //marca o cliente como desconectado
            printf("Cliente desconectado.\n");
            break;
        }

        //verifica o tamanho da mensagem recebida e adiciona o terminador de string
        buffer[bytes_recebidos] = '\0'; // Adiciona o terminador de string
        parse_input(buffer, &cliente->protocolo);
        
    }
    
    return THREAD_RETURN;
}

//função para utilização da segunda thread, responsável por enviar periodicamente mensagens para o cliente
THREAD_FUNC(enviar_periodicamente) {

    DadosCliente *cliente = (DadosCliente *)arg;

    char mensagem[128];

    time_t ultimo_envio_horario = time(NULL);

    while(verificar_conectado(cliente)){ 


        //saí quando o cliente estiver desconectado
        if(!verificar_conectado(cliente)){
            break;
        }

    char resposta[BUFFER_SIZE];
    tipo_acao_t acao = process_shared_data(&cliente->protocolo, resposta, sizeof(resposta)); // processa qual acao foi executada

    if (acao == ACAO_DESCONECTAR)  // se foi desconectar, desconecta
    {
        definir_conectado(cliente, 0);
        desligar_socket(cliente->client_fd);
        break;
    }

    if (strlen(resposta) > 0) 
    {
        if (!enviar_para_cliente(cliente, resposta)) {
            mostrar_erro_socket("Erro ao enviar resposta do protocolo");
            definir_conectado(cliente, 0);
            desligar_socket(cliente->client_fd);
            break;
        }
    }

        time_t agora = time(NULL);

        //verifica 1 minuto desde de o ultimo envio de mensagem 
        if(agora - ultimo_envio_horario >= INTERVALO_TESTE){
            struct tm horario;

            if(!obter_horario_local(&agora, &horario)) {
                fprintf(stderr, "Erro ao obter horário local.\n");
                continue;
            }
            
            strftime(mensagem, sizeof(mensagem), "%d/%m/%Y %H:%M\n", &horario);

            if(!enviar_para_cliente(cliente, mensagem)){
                mostrar_erro_socket("Erro ao enviar mensagem periodica");
                definir_conectado(cliente, 0); //marca o cliente como desconectado
                desligar_socket(cliente->client_fd); //fecha o socket do cliente para interromper a thread de recebimento
                break;

            }
            ultimo_envio_horario = agora;
        }
        dormir_milisegundos(100);
    }


    return THREAD_RETURN;
}

socket_t criar_servidor (void){
    socket_t server_fd;

    struct sockaddr_in server_addr = {0}; //informações servidor

    //Criação do socket (IPv4, TCP, protocolo padrão)
    server_fd = socket(AF_INET, SOCK_STREAM, 0);


    if (server_fd == PLATFORM_SOCKET_INVALIDO) {
        mostrar_erro_socket("Erro ao criar o socket");

        return PLATFORM_SOCKET_INVALIDO;
    }

    printf("Socket criado com sucesso.\n");

    if(!configurar_reuso_endereco(server_fd)){
        mostrar_erro_socket("Erro ao configurar reuso de endereço (setsockopt)");
        
        fechar_socket(server_fd);

        return PLATFORM_SOCKET_INVALIDO;
    }

    //configura endereço do servidor (IPv4, qualquer endereço, porta definida)
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    //configuração da bind (socket, endereço, tamanho do endereço)
    if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == PLATFORM_SOCKET_ERRO) {
        mostrar_erro_socket("Erro ao fazer bind");
        fechar_socket(server_fd);
        
        return PLATFORM_SOCKET_INVALIDO;
    }

    printf("Bind realizado com sucesso na porta %d.\n", PORT);

        //configuração do listen (socket, tamanho da fila de conexões)
    if(listen(server_fd, 1) == PLATFORM_SOCKET_ERRO) {
        mostrar_erro_socket("Erro no listen");
        fechar_socket(server_fd);
        
        return PLATFORM_SOCKET_INVALIDO;
    }
    return server_fd;
}

int enviar_mensagem_conexao(DadosCliente *cliente){

    //vetor para armazenar a hora e mensagem de conexao (11:23: CONECTADO!!)
    char mensagem[64];

    //obter a hora atual
    time_t agora = time(NULL);
    struct tm horario;

    if(!obter_horario_local(&agora, &horario)){
        fprintf(stderr, "Erro ao obter horario local.\n");

        return 0;
    }

    //Montar mensagem
    strftime(mensagem, sizeof(mensagem), "%H:%M: CONECTADO!!\n", &horario);

    //enviar mensagem para o cliente
    printf("Enviando ao Cliente: %s", mensagem);

    //envia a mensagem de conexão para o cliente, se falhar, retorna 0
    if(!enviar_para_cliente(cliente, mensagem)){
        fprintf(stderr, "Erro ao enviar mensagem de conexao para o cliente.\n");
        return 0;
    }
    return 1;
}

int inicializar_cliente(DadosCliente *cliente, socket_t client_fd, struct sockaddr_in *client_addr){
    
    //registro do cliente em uma estrutura de dados para controle de conexão
    cliente->client_fd = client_fd;
    cliente->conectado = 1; //marca o cliente como conectado

    //inicializa o mutex do cliente
    if(!iniciar_mutex(&cliente->mutex)) {
        fprintf(stderr, "Erro ao inicializar mutex para o cliente.\n");
        return 0; // continua para aceitar novas conexões mesmo que uma falhe
    }

    //inicializa o mutex de envio do cliente        
    if(!iniciar_mutex(&cliente->mutex_envio)) {
        fprintf(stderr, "Erro ao inicializar mutex de envio para o cliente.\n");
        destruir_mutex(&cliente->mutex);
        return 0; // continua para aceitar novas conexões mesmo que uma falhe
    }

    //inicializa o protocolo do cliente 
    if(!protocol_init(&cliente->protocolo)){
        fprintf(stderr, "Erro ao inicializar protocolo para o cliente.\n");
        destruir_mutex(&cliente->mutex);
        destruir_mutex(&cliente->mutex_envio);
        return 0; // continua para aceitar novas conexões mesmo que uma falhe
    }

    //nome padrao do usuario como "IP:porta"
    snprintf(cliente->protocolo.nome_usuario, MAX_NOME, "%s:%d", inet_ntoa(client_addr->sin_addr), ntohs(client_addr->sin_port));  // define o nome padrao do usuario como "IP:porta"

    return 1;
}

DadosCliente* criar_cliente(socket_t client_fd, struct sockaddr_in *client_addr){
    DadosCliente *cliente = (DadosCliente *)malloc(sizeof(DadosCliente));
    if(cliente == NULL){
        fprintf(stderr, "Erro ao alocar memoria para o cliente.\n");
        return NULL;
    }

    //inicializa o cliente, se falhar, fecha o socket e continua para aceitar novas conexões
    if(!inicializar_cliente(cliente, client_fd, client_addr)){
        free(cliente);
        return NULL;
    }

    return cliente;
}

void limpar_cliente(DadosCliente *cliente){

    if(cliente == NULL){
        return;
    }
    protocol_destroy(&cliente->protocolo);
    destruir_mutex(&cliente->mutex);
    destruir_mutex(&cliente->mutex_envio);
    fechar_socket(cliente->client_fd);
    free(cliente);
}

int executar_threads_cliente(DadosCliente *cliente){
    //variável para armazenar as threads de recebimento e envio periodico
    thread_t thread_recebimento;
    thread_t thread_envio_periodico;

    //criação da thread de recebimento
    if(!criar_thread(&thread_recebimento, receber_dados, cliente)){
        fprintf(stderr, "Erro ao criar thread de recebimento.\n");
        return 0; //continua para aceitar novas conexões mesmo que uma falhe
    }

    //criação da thread de envio periodico
    if(!criar_thread(&thread_envio_periodico, enviar_periodicamente, cliente)){
        fprintf(stderr, "Erro ao criar thread de envio periodico.\n");
        definir_conectado(cliente, 0); //marca o cliente como desconectado
        desligar_socket(cliente->client_fd); //fecha o socket do cliente para interromper a thread de recebimento
        aguardar_thread(thread_recebimento); //espera a thread de recebimento terminar
        return 0; //continua para aceitar novas conexões mesmo que uma falhe
    }

    aguardar_thread(thread_recebimento); //espera a thread de recebimento terminar
    aguardar_thread(thread_envio_periodico); //espera a thread de envio periodico
    
    return 1; 
}

int atender_clientes(DadosCliente *cliente){

            //envia mensagem de conexão para o cliente
        if(!enviar_mensagem_conexao(cliente)){
            return 0; //continua para aceitar novas conexões mesmo que uma falhe    
        }

        if(!executar_threads_cliente(cliente)){
            return 0; //continua para aceitar novas conexões mesmo que uma falhe    
        }  
        return 1;
}

int inicializar_lista_clientes(ListaClientes *lista, int limite){
    lista -> clientes = calloc(limite, sizeof(DadosCliente *)); //calloc = malloc mas inicializado com todas as posições com 0, para evitar lixo de memória

    // verifica se a alocação de memória foi bem sucedida
    if(lista -> clientes == NULL){
        fprintf(stderr, "Erro ao alocar memoria para a lista de clientes.\n");
        return 0;
    }

    //inicializa a quantidade de clientes conectados como 0 e o limite de clientes conectados como o valor passado por parâmetro
    lista -> quantidade = 0;
    lista -> limite = limite;

    //inicializa o mutex da lista de clientes
    if(iniciar_mutex(&lista -> mutex) == 0){
        fprintf(stderr, "Erro ao inicializar mutex para a lista de clientes.\n");
        free(lista -> clientes);
        lista->clientes = NULL;
        return 0;
    }
    return 1; 
}

void destruir_lista_clientes(ListaClientes *lista){

    //libera a memória alocada para a lista de clientes
    free(lista -> clientes);
    lista -> clientes = NULL;

    //destrói o mutex da lista de clientes
    destruir_mutex(&lista -> mutex);
}

int adicionar_cliente(ListaClientes *lista, DadosCliente *cliente){
    bloquear_mutex(&lista -> mutex); //bloqueia o mutex da lista de clientes para evitar que outras threads acessem a lista enquanto estamos adicionando um cliente

    //verifica se a lista de clientes está cheia
    if(lista -> quantidade >= lista -> limite){
        liberar_mutex(&lista -> mutex);
        return 0;
    }

    //procura posição livre na lista de clientes
    for(int i = 0; i < lista -> limite; i++){
        if(lista -> clientes[i] == NULL){
            lista -> clientes[i] = cliente;
            lista -> quantidade++;
            liberar_mutex(&lista -> mutex);
            return 1;
        }
    }

    //segurança: quantidade indica vaga mas nenhuma posição (NULL) foi encontrada, então não adiciona o cliente
    liberar_mutex(&lista -> mutex);
    return 0;
}

void remover_cliente(ListaClientes *lista, DadosCliente *cliente){
    bloquear_mutex(&lista -> mutex); //bloqueia o mutex da lista de clientes para evitar que outras threads acessem a lista enquanto estamos removendo um cliente
    
    //procura o cliente na lista de clientes
    for(int i = 0; i < lista -> limite; i++){
        if(lista -> clientes[i] == cliente){ //se achar o cliente, remove da lista
            lista -> clientes[i] = NULL;
            lista -> quantidade--;
            break;
        }
    }

    liberar_mutex(&lista -> mutex);
}

int obter_limite_clientes(int argc, char *argv[]){
    if (argc != 2) {
        fprintf(stderr, "Uso: %s <limite_clientes>\n", argv[0]);
        return -1;
    }

    char *fim;
    errno = 0; // Reset errno before calling strtol
    long valor = strtol(argv[1], &fim, 10); //transforma "3" em 3

    if (errno == ERANGE || argv[1][0] == '\0' || *fim != '\0' || valor <= 0 || valor > INT_MAX) { //verifica se o argumento passado é valido
        fprintf(stderr, "Erro: limite_clientes deve ser um número inteiro positivo.\n");
        return -1;
    }
    return (int)valor;
}

THREAD_FUNC(gerenciar_cliente){
    ContextoCliente *contexto = (ContextoCliente *)arg; //trata ponteiro como contexto do cliente
    DadosCliente *cliente = contexto->cliente;
    ListaClientes *lista = contexto->lista;

    free(contexto); //libera a memória alocada para o contexto do cliente
    atender_clientes(cliente); //atende o cliente, se falhar, fecha o socket e continua para aceitar novas conexões
    remover_cliente(lista, cliente); //remove o cliente da lista de clientes
    limpar_cliente(cliente); //limpa o cliente
    return THREAD_RETURN;
}

void enviar_mensagem_servidor_cheio(socket_t client_fd){
    const char *mensagem = "Servidor cheio. Tente novamente mais tarde.\n";
    if(send(client_fd, mensagem, (int)strlen(mensagem), 0) == PLATFORM_SOCKET_ERRO){
        mostrar_erro_socket("Erro ao enviar mensagem de servidor cheio");
    }
}

int main(int argc, char *argv[]){
    //obtem limite passado no argumento da linha de comando, se for invalido, encerra o programa
    int limite_clientes = obter_limite_clientes(argc, argv);
    if(limite_clientes <= 0){
        return 1;
    }

    
    socket_t client_fd;

    //informações do servidor e do cliente
    
    struct sockaddr_in client_addr = {0};
    sock_len_t client_addr_len = sizeof(client_addr); //tamanho do endereço do cliente

    //inicializa o winsock no Windows
    if(!iniciar_sockets()) {
        return 1;
    }

    //inicializa a lista de clientes, se falhar, encerra o programa
    ListaClientes lista_clientes;
    if(!inicializar_lista_clientes(&lista_clientes, limite_clientes)) {
        finalizar_sockets();
        return 1;
    }

    socket_t server_fd = criar_servidor();

    if(server_fd == PLATFORM_SOCKET_INVALIDO){
        destruir_lista_clientes(&lista_clientes); //destrói a lista de clientes caso o servidor não consiga ser criado
        finalizar_sockets(); //encerra o winsock que inicializamos no começo
        return 1;
    }

    //loop para aceitar multiplas conexões de clientes, uma por vez
    while(1){

        //resetar o tamanho do endereço do cliente antes de cada accept
        client_addr_len = sizeof(client_addr);

        printf("Servidor aguardando conexoes na porta %d...\n", PORT);

        //configuração do accept (socket, endereço do cliente, tamanho do endereço do cliente)
        client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_addr_len);

        if (client_fd == PLATFORM_SOCKET_INVALIDO) {
            mostrar_erro_socket("Erro no accept");
            continue; //continua para aceitar novas conexões mesmo que uma falhe
        }   

        printf("Conexao aceita de %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));


        DadosCliente *cliente = criar_cliente(client_fd,&client_addr); //estrutura para armazenar os dados do cliente (numero do cliente e status de conexão)

        
        if(cliente == NULL){
            fechar_socket(client_fd); //fecha o socket do cliente caso a criação da estrutura falhe
            continue; //continua para aceitar novas conexões mesmo que uma falhe
        }

        if(!adicionar_cliente(&lista_clientes, cliente)){
            fprintf(stderr, "Limite de clientes atingido.\n");
            enviar_mensagem_servidor_cheio(cliente->client_fd);
            limpar_cliente(cliente); //limpa o cliente caso a adição na lista falhe
            continue; //continua para aceitar novas conexões mesmo que uma falhe
        }

        ContextoCliente *contexto = (ContextoCliente *)malloc(sizeof(ContextoCliente)); //aloca memória para o contexto do cliente
        if(contexto == NULL){
            fprintf(stderr, "Erro ao alocar memoria para o contexto do cliente.\n");
            remover_cliente(&lista_clientes, cliente); //remove o cliente da lista de clientes caso a alocação de memória falhe
            limpar_cliente(cliente); //limpa o cliente caso a alocação de memória falhe
            continue; //continua para aceitar novas conexões mesmo que uma falhe
        }

        contexto->cliente = cliente; //atribui o cliente ao contexto
        contexto->lista = &lista_clientes; //atribui a lista de clientes ao contexto

        thread_t thread_gerente;

        if(!criar_thread(&thread_gerente, gerenciar_cliente, contexto)){
            fprintf(stderr, "Erro ao criar thread para gerenciar o cliente.\n");
            free(contexto); //libera a memória alocada para o contexto do cliente caso a criação da thread falhe
            remover_cliente(&lista_clientes, cliente); //remove o cliente da lista de clientes caso a criação da thread falhe
            limpar_cliente(cliente); //limpa o cliente caso a criação da thread falhe
            continue; //continua para aceitar novas conexões mesmo que uma falhe
        }

        desanexar_thread(thread_gerente); //desanexa a thread para que ela seja liberada automaticamente quando terminar

        
    }

    //fechar o socket do servidor, saida do loop ainda nao implementada
    fechar_socket(server_fd);
    finalizar_sockets(); //encerra o winsock que inicializamos no começo

    return 0;
}