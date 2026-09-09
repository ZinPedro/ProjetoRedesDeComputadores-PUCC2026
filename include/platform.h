#ifndef PLATFORM_H
#define PLATFORM_H

#include <time.h>
#include <stdio.h>

//Blibliotecas para Windows
#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <windows.h>

    typedef SOCKET socket_t;
    typedef int socket_io_t; // Definir socklen_t como int no Windows
    typedef int sock_len_t; // Definir socklen_t como int no Windows

    //threads
    typedef HANDLE thread_t;
    typedef CRITICAL_SECTION mutex_t;

    #define PLATFORM_SOCKET_INVALIDO INVALID_SOCKET
    #define PLATFORM_SOCKET_ERRO SOCKET_ERROR
    #define PLATFORM_SHUT_RDWR SD_BOTH

#else
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <pthread.h>

    typedef int socket_t;
    typedef ssize_t socket_io_t; // Definir socklen_t como int no Windows
    typedef socklen_t sock_len_t; // Definir socklen_t como int no Windows


    typedef pthread_t thread_t;
    typedef pthread_mutex_t mutex_t;

    #define PLATFORM_SOCKET_INVALIDO (-1)
    #define PLATFORM_SOCKET_ERRO (-1)
    #define PLATFORM_SHUT_RDWR SHUT_RDWR

#endif

//thread para criar uma thread de acordo com o SO  
#ifdef _WIN32

    #define THREAD_FUNC(nome) DWORD WINAPI nome(LPVOID arg)
    #define THREAD_RETURN 0

    typedef DWORD (WINAPI *thread_func_t)(LPVOID);

#else

    #define THREAD_FUNC(nome) void *nome(void *arg)
    #define THREAD_RETURN NULL

    typedef void *(*thread_func_t)(void *);

#endif

//inixializa o socket de acordo com o SO
static inline int iniciar_sockets(void){
    #ifdef _WIN32
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            printf("Erro ao iniciar o Winsock\n");
            return 0;
        }
    #endif
    return 1;
}

//finaliza o socket de acordo com o SO
static inline void finalizar_sockets(void){
    #ifdef _WIN32
        WSACleanup();
    #endif
}

//fecha o socket de acordo com o SO
static inline void fechar_socket(socket_t socket_fd){
    #ifdef _WIN32
        closesocket(socket_fd);
    #else
        close(socket_fd);
    #endif
}

//interrompe leitura e escrita no socket
static inline void desligar_socket(socket_t socket_fd){
        shutdown(socket_fd, PLATFORM_SHUT_RDWR);
}

//função para mostrar erro de socket de acordo com o SO
static inline void mostrar_erro_socket(const char *mensagem){
    #ifdef _WIN32
        fprintf(stderr, "%s. Código Winsock: %d\n", mensagem, WSAGetLastError());
    #else
        perror(mensagem);
    #endif
}

//função para dormir por um número de segundos de acordo com o SO
static inline void dormir_segundos(unsigned int segundos){
    #ifdef _WIN32
        Sleep(segundos * 1000);
    #else
        sleep(segundos);
    #endif
}

//função para obter o horário local de acordo com o SO
static inline int obter_horario_local(const time_t *tempo, struct tm *resultado){
    #ifdef _WIN32
        return localtime_s(resultado, tempo) == 0;
    #else
        return localtime_r(tempo, resultado) != NULL;
    #endif
}

 // Configuração do socket para reutilizar o endereço para evitar o erro "Address already in use" ao reiniciar o servidor rapidamente

static inline int configurar_reuso_endereco(socket_t socket_fd) {
    int opt = 1;

    #ifdef _WIN32
        return setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt)) != SOCKET_ERROR;
    #else  
        return setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) != -1;
    #endif
}

//funções para inicializar um mutex de acordo com o SO
static inline int iniciar_mutex(mutex_t *mutex) {
    #ifdef _WIN32
        InitializeCriticalSection(mutex);
        return 1; // Sucesso
    #else
        return pthread_mutex_init(mutex, NULL) == 0; // Retorna 1 se sucesso, 0 se falha
    #endif
}

static inline void bloquear_mutex(mutex_t *mutex) {
    #ifdef _WIN32
        EnterCriticalSection(mutex);
    #else
        pthread_mutex_lock(mutex);
    #endif
}

static inline void liberar_mutex(mutex_t *mutex) {
    #ifdef _WIN32
        LeaveCriticalSection(mutex);
    #else
        pthread_mutex_unlock(mutex);
    #endif
}

static inline void destruir_mutex(mutex_t *mutex) {
    #ifdef _WIN32
        DeleteCriticalSection(mutex);
    #else
        pthread_mutex_destroy(mutex);
    #endif
}

//função para criar uma thread de acordo com o SO
static inline int criar_thread(thread_t *thread, thread_func_t funcao, void *arg) {
    #ifdef _WIN32
        *thread = CreateThread(NULL, 0, funcao, arg, 0, NULL);
        return *thread != NULL; // Retorna 1 se sucesso, 0 se falha
    #else
        return pthread_create(thread, NULL, funcao, arg) == 0; // Retorna 1 se sucesso, 0 se falha
    #endif
}

static inline int aguardar_thread(thread_t thread) {
    #ifdef _WIN32
        DWORD resultado = WaitForSingleObject(thread, INFINITE);
        CloseHandle(thread);
        return resultado == WAIT_OBJECT_0; // Sucesso
    #else
        return pthread_join(thread, NULL) == 0; // Retorna 1 se sucesso, 0 se falha
    #endif
}

static inline void dormir_milisegundos(unsigned int milisegundos){
    #ifdef _WIN32
        Sleep(milissegundos);
    #else
        struct timespec tempo;
        tempo.tv_sec = milisegundos / 1000;
        tempo.tv_nsec = (long)(milisegundos % 1000) * 1000000L;

        nanosleep(&tempo, NULL);
    #endif
}

// Essa funcao é para que quando o servidor aceite varios clientes, ele nao vai precisar ficar esperando cada thread terminar para ele aceitar um novo cliente, ele so "libera" essa thread desse liente e ja vai aceitar o proximo cliente

static inline void desanexar_thread(thread_t thread)
{
    #ifdef _WIN32
        CloseHandle(thread);
    #else
        pthread_detach(thread);
    #endif
    
}

#endif // PLATFORM_H
