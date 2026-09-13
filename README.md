# Chat Multiusuário Cliente-Servidor TCP — Redes de Computadores

Projeto acadêmico da disciplina de Redes de Computadores — Engenharia da Computação, PUC Campinas.

## Equipe

- Eloise Dos Santos Ruiz
- Lucas Leal Ibrahim
- Pedro Henrique Coan Zin

## Sobre o projeto

Sala de bate-papo cliente-servidor implementada em C utilizando sockets TCP, onde vários usuários conectados simultaneamente trocam mensagens públicas entre si.

O servidor escuta na porta `8080` e atende múltiplos clientes ao mesmo tempo, cada um tratado de forma independente. A quantidade máxima de clientes simultâneos é definida por parâmetro na linha de comando.

A aplicação utiliza threads para permitir comunicação bidirecional assíncrona e possui uma camada de compatibilidade entre Windows e Linux, responsável por abstrair diferenças de sockets, threads, mutex e outras funções dependentes do sistema operacional.

## Funcionalidades

- Comunicação cliente-servidor utilizando TCP na porta `8080`
- Múltiplos clientes conectados simultaneamente, tratados de forma independente
- Limite de clientes configurável por linha de comando
- Vaga liberada automaticamente quando um cliente desconecta
- Recusa de conexão com mensagem informativa quando o limite é atingido
- Mensagens públicas retransmitidas a todos os usuários conectados
- Eco de confirmação para quem enviou a mensagem
- Nome de usuário configurável, com padrão `IP:porta`
- Envio periódico de data e hora pelo servidor, a cada 1 minuto
- Uso de threads para envio e recebimento simultâneo
- Uso de mutex para controle de dados compartilhados
- Compatibilidade com Windows e Linux

## Arquitetura

### Servidor

A thread principal permanece em `accept()`. A cada conexão aceita, cria uma thread de trabalho dedicada àquele cliente e volta imediatamente a aguardar novas conexões.

Cada cliente possui duas threads:

- **Thread 1** — lê continuamente do socket e grava os comandos recebidos na área de memória compartilhada daquele cliente.
- **Thread 2** — varre periodicamente a memória compartilhada, executa a ação pendente e envia os resultados pela rede. Também é responsável pelo envio da data e hora a cada minuto.

Os clientes conectados são mantidos em uma lista protegida por mutex, que permite retransmitir mensagens a todos e controlar o limite de conexões.

### Cliente

- **Thread 1** — lê os comandos digitados pelo usuário e envia ao servidor.
- **Thread 2** — recebe dados do servidor e imprime na tela.

## Funcionamento

Ao conectar, o cliente recebe a confirmação:

```text
18:33: CONECTADO!!
```

Caso o limite de clientes já tenha sido atingido, recebe no lugar:

```text
Servidor cheio. Tente novamente mais tarde.
```

e a conexão é encerrada.

Durante a sessão, o servidor envia periodicamente a data e hora atual, mesmo que ninguém esteja conversando:

```text
22/08/2026 18:33
```

## Comandos

### Enviar mensagem

Qualquer texto que não comece com `:` é tratado como mensagem pública.

```text
oiii
```

Quem enviou recebe o eco:

```text
Voce digitou: oiii
```

Os demais usuários conectados recebem a mensagem formatada com nome e horário:

```text
Lucas (18:35): oiii
```

### Alterar nome

```text
:nome Antonio
```

Enquanto o nome não for definido, o usuário é identificado automaticamente pelo seu endereço, no formato `IP:porta`:

```text
127.0.0.1:54312 (18:36): oiii
```

### Desconectar

```text
:quit
```

O cliente encerra a execução e o servidor libera a vaga para uma nova conexão.

## Estrutura do projeto

```text
ProjetoRedesDeComputadores-PUCC2026/
│
├── include/
│   ├── platform.h
│   ├── protocol.h
│   └── utils.h
│
├── src/
│   ├── cliente.c
│   ├── server.c
│   ├── protocol.c
│   └── utils.c
│
└── README.md
```

- `cliente.c`: implementação do cliente TCP e das threads de envio e recebimento.
- `server.c`: servidor, gerenciamento de múltiplas conexões, lista de clientes e retransmissão de mensagens.
- `protocol.c` / `protocol.h`: interpretação dos comandos, memória compartilhada e formatação das mensagens.
- `utils.c` / `utils.h`: funções auxiliares, como formatação de horário.
- `platform.h`: camada de compatibilidade entre Windows e Linux.

## Tecnologias

C, TCP/IP, sockets, threads, mutex, Winsock, POSIX e pthreads.

---

# Como rodar

## Linux

### 1. Pré-requisitos

É necessário ter o GCC e as ferramentas básicas de compilação instalados.

No Ubuntu/Debian:

```bash
sudo apt update
sudo apt install build-essential
```

Verifique a instalação:

```bash
gcc --version
```

### 2. Compilar o servidor

Na pasta raiz do projeto:

```bash
gcc src/server.c src/protocol.c src/utils.c -Iinclude -o server -pthread -Wall -Wextra
```

### 3. Compilar o cliente

```bash
gcc src/cliente.c -Iinclude -o cliente -pthread -Wall -Wextra
```

### 4. Executar

O servidor exige o limite de clientes como argumento:

```bash
./server 3
```

Sem o argumento, ele informa o uso correto e encerra.

Em outros terminais, execute os clientes:

```bash
./cliente
```

O servidor deverá mostrar:

```text
Servidor aguardando conexoes na porta 8080...
```

---

## Windows

### 1. Pré-requisitos

É necessário ter um compilador GCC para Windows, como o MinGW-w64.

Uma forma de instalar é através do MSYS2.

No PowerShell:

```powershell
winget install -e --id MSYS2.MSYS2
```

Depois abra o aplicativo **MSYS2 UCRT64** e execute:

```bash
pacman -Syu
```

Se o terminal solicitar reinicialização, feche o MSYS2 UCRT64, abra novamente e execute:

```bash
pacman -S --needed mingw-w64-ucrt-x86_64-gcc
```

Verifique no MSYS2:

```bash
gcc --version
```

### 2. Disponibilizar o GCC no PowerShell

Para poder usar `gcc` diretamente no PowerShell, adicione esta pasta ao `Path` do Windows:

```text
C:\msys64\ucrt64\bin
```

Caminho no Windows:

**Pesquisar "Variáveis de ambiente" → Variáveis de Ambiente → Path → Editar → Novo**

Adicione:

```text
C:\msys64\ucrt64\bin
```

Feche e abra novamente o PowerShell e confirme:

```powershell
gcc --version
```

### 3. Compilar o servidor

No PowerShell, na pasta raiz do projeto:

```powershell
gcc src/server.c src/protocol.c src/utils.c -Iinclude -o server.exe -lws2_32 -Wall -Wextra
```

### 4. Compilar o cliente

```powershell
gcc src/cliente.c -Iinclude -o cliente.exe -lws2_32 -Wall -Wextra
```

### 5. Executar

PowerShell 1 — servidor, com o limite de clientes:

```powershell
.\server.exe 3
```

Demais PowerShells — clientes:

```powershell
.\cliente.exe
```

---

## Teste entre dois computadores

O cliente está configurado por padrão para:

```c
inet_addr("127.0.0.1")
```

Esse endereço funciona apenas quando cliente e servidor estão no mesmo computador.

Para utilizar dois computadores diferentes, descubra o IP da máquina que executará o servidor.

No Linux:

```bash
hostname -I
```

Exemplo:

```text
192.168.0.15
```

No `src/cliente.c`, altere:

```c
serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
```

para o IP do servidor:

```c
serverAddr.sin_addr.s_addr = inet_addr("192.168.0.15");
```

Depois recompile o cliente.

No Windows, com o servidor já executando, é possível testar a porta com:

```powershell
Test-NetConnection 192.168.0.15 -Port 8080
```

O resultado esperado é:

```text
TcpTestSucceeded : True
```

Os computadores podem estar conectados por Wi-Fi ou cabo, desde que estejam na mesma rede local e a comunicação entre dispositivos não esteja bloqueada pelo roteador ou firewall.

---

# Possíveis problemas

## `Uso: ./server <limite_clientes>`

O servidor exige a quantidade máxima de clientes como argumento. Informe um número inteiro positivo:

```bash
./server 3
```

---

## `gcc` não é reconhecido no Windows

Mensagem semelhante a:

```text
gcc : O termo 'gcc' não é reconhecido...
```

Verifique primeiro:

```powershell
gcc --version
```

Se o GCC já estiver instalado através do MSYS2, confirme que esta pasta foi adicionada ao `Path`:

```text
C:\msys64\ucrt64\bin
```

Depois feche e abra novamente o PowerShell.

---

## `gcc: command not found` no Linux

Instale as ferramentas de compilação:

```bash
sudo apt update
sudo apt install build-essential
```

Depois verifique:

```bash
gcc --version
```

---

## Erro de referência indefinida a `hora_atual`

O `utils.c` precisa ser incluído na compilação do servidor. Confira se ele está na linha de comando:

```bash
gcc src/server.c src/protocol.c src/utils.c -Iinclude -o server -pthread -Wall -Wextra
```

---

## Windows bloqueia `cliente.exe` ou `server.exe`

Se o PowerShell informar que uma política de Controle de Aplicativo bloqueou o executável, tente:

```powershell
Unblock-File .\cliente.exe
```

ou:

```powershell
Unblock-File .\server.exe
```

Depois tente executar novamente.

Se apenas o cliente continuar bloqueado, ele pode ser recompilado com outro nome:

```powershell
gcc src/cliente.c -Iinclude -o cliente_teste.exe -lws2_32 -Wall -Wextra
.\cliente_teste.exe
```

Se a política continuar bloqueando o programa, a restrição é do próprio Windows ou da configuração administrativa da máquina.

---

## Porta 8080 não está acessível

No Linux, confirme se o servidor está escutando:

```bash
ss -ltn | grep 8080
```

Se o `ufw` estiver ativo:

```bash
sudo ufw status
```

Caso necessário, libere a porta TCP 8080:

```bash
sudo ufw allow 8080/tcp
```

No Windows, para testar um servidor remoto:

```powershell
Test-NetConnection IP_DO_SERVIDOR -Port 8080
```

---

## `Address already in use`

Esse erro normalmente indica que outro processo já está utilizando a porta `8080`.

No Linux:

```bash
ss -ltnp | grep 8080
```

No Windows:

```powershell
netstat -ano | findstr :8080
```

Feche o processo anterior antes de executar uma nova instância do servidor.

---

## Cliente não conecta ao servidor em outro computador

Verifique:

1. Se os dois computadores estão na mesma rede local.
2. Se o endereço IP configurado no `cliente.c` é o IP da máquina do servidor.
3. Se o servidor está executando antes do cliente.
4. Se a porta `8080` está liberada no firewall.
5. Se a rede Wi-Fi não possui isolamento de clientes ou rede de convidados.

---

# Testes realizados

Durante os testes foram validados:

- Compilação e execução em Linux
- Compilação e execução em Windows
- Comunicação entre computadores diferentes na mesma rede local
- Mensagem de conexão com horário
- Eco de confirmação ao remetente
- Retransmissão de mensagens entre múltiplos clientes, nos dois sentidos
- Nome padrão `IP:porta` quando não definido
- Alteração de nome com `:nome`
- Limite de clientes respeitado, com recusa informativa ao exceder
- Liberação da vaga após desconexão de um cliente
- Recebimento periódico de data e hora
- Desconexão com `:quit`
- Desconexão abrupta de um cliente sem afetar os demais
- Ausência de vazamento de memória verificada com Valgrind

## Nível do projeto

O projeto aplica conceitos fundamentais de Redes de Computadores e programação concorrente em C, incluindo sockets TCP, comunicação cliente-servidor com múltiplos clientes simultâneos, threads de trabalho, memória compartilhada, exclusão mútua com mutex e tratamento de diferenças entre sistemas operacionais.

A estrutura separa comunicação, protocolo e funcionalidades específicas de cada plataforma, facilitando a organização e a evolução do código.
