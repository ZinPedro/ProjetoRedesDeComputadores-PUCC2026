#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/platform.h"

#define PORT 8080
#define BUFFER_SIZE 1024

typedef struct //dados compartilhados pela threads
{
    socket_t client_fd;

    int conectado;

    int pediu_quit;

    mutex_t mutex;

}DadosCliente;

void definir_conectado(DadosCliente*cliente, int valor)
{
    bloquear_mutex(&cliente-> mutex);

    cliente -> conectado = valor;
    liberar_mutex(&cliente -> mutex);
}

int verificar_conectado(DadosCliente*cliente)
{
    int conectado;

    bloquear_mutex(&cliente -> mutex);
    conectado = cliente -> conectado;
    liberar_mutex(&cliente -> mutex);

    return conectado;
}

THREAD_FUNC(receber_mensagens)
{
    DadosCliente*cliente = (DadosCliente *)arg;

    char buffer[BUFFER_SIZE];

    socket_io_t bytes_recebidos;

    while(verificar_conectado(cliente))
    {
        bytes_recebidos = recv(cliente -> client_fd, buffer, BUFFER_SIZE -1, 0);

        if(bytes_recebidos == PLATFORM_SOCKET_ERRO)
        {
            mostrar_erro_socket("erro ao receber a mensagem");

            definir_conectado(cliente, 0);

            break;
        }

        if(bytes_recebidos == 0)
        {
            printf("Servidor esta desconectado\n");

            definir_conectado(cliente, 0 );

            break;
        }

        buffer[bytes_recebidos] ='\0';

        printf("%s",buffer);

        fflush(stdout);
    }

    return THREAD_RETURN;
}

THREAD_FUNC(enviar_mensagens)
{
    DadosCliente*cliente = (DadosCliente*)arg;

    char mensagem[BUFFER_SIZE];

    while(verificar_conectado(cliente))
    {
        fflush(stdout);

        if(fgets(mensagem, BUFFER_SIZE, stdin) == NULL)
        {
            definir_conectado(cliente, 0);

            break;
        }

        mensagem[strcspn(mensagem, "\r\n")] = '\0';

        if(strlen(mensagem) == 0)
        {
            continue;
        }

        if(send(cliente -> client_fd, mensagem, (int)strlen(mensagem), 0) == PLATFORM_SOCKET_ERRO)
        {
            mostrar_erro_socket("erro ao enviar a mensagem");

            definir_conectado(cliente, 0);

            break;
        }

        if(strcmp(mensagem, ":quit") == 0)
        {
            cliente->pediu_quit = 1;

            break;
        }
    }

    return THREAD_RETURN;
}

int main()
{
        socket_t clientSocket;
        struct sockaddr_in serverAddr = {0};
        thread_t thread_recebimento;
        thread_t thread_envio;
        DadosCliente cliente;

        printf("Cliente iniciado!\n");

        //inicializa o winsock no Windows
        if (!iniciar_sockets()) {
            return 1;
        }

        clientSocket = socket(AF_INET, SOCK_STREAM, 0);

        if (clientSocket == PLATFORM_SOCKET_INVALIDO)
        {
            mostrar_erro_socket("Erro ao criar socket");
            finalizar_sockets(); //encerra o winsock que inicializamos no começo

            return 1;
        }

        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(PORT);
        serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");

        if (connect(clientSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) == PLATFORM_SOCKET_ERRO)
        {
            mostrar_erro_socket("Erro ao conectar ao servidor");
            fechar_socket(clientSocket); //fecha o socket se a conexão falhar
            finalizar_sockets(); //encerra o winsock que inicializamos no começo

            return 1;
        }

       printf("conectado ao servidor\n");

       cliente.client_fd = clientSocket;
       cliente.conectado =1;
       cliente.pediu_quit = 0;

       if(!iniciar_mutex(&cliente.mutex))
       {
        printf("erro para iniciar o mutex\n");

        fechar_socket(clientSocket);
        finalizar_sockets();

        return 1;
       }

       if(!criar_thread(&thread_recebimento, receber_mensagens, &cliente))
       {
        printf("erro ao criar thread de recebimento\n");

        destruir_mutex(&cliente.mutex);
        fechar_socket(clientSocket);
        finalizar_sockets();

        return 1;
       }

       if(!criar_thread(&thread_envio, enviar_mensagens, &cliente))
       {
            printf("erro ao criar thread de envio\n");

            definir_conectado(&cliente, 0);
            desligar_socket(clientSocket);
            aguardar_thread(thread_recebimento);
            destruir_mutex(&cliente.mutex);
            fechar_socket(clientSocket);
            finalizar_sockets();

            return 1;
       }

    aguardar_thread(thread_envio);

    if(!cliente.pediu_quit)               // só acontece às vezes
    {
        definir_conectado(&cliente, 0);
        desligar_socket(clientSocket);
    }
        
       aguardar_thread(thread_recebimento);
       aguardar_thread(thread_recebimento);
       destruir_mutex(&cliente.mutex);
       fechar_socket(clientSocket); //fecha o socket após a conexão
       finalizar_sockets(); //encerra o winsock que inicializamos no começo

       printf("Cliente encerrado\n");

    return 0;
}