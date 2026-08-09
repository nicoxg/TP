#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>

// Longitudes arbitrarias, comando de maximo 3 digitos
#define MAX_LINEA   4096
#define MAX_COMANDO 4
#define MAX_CLAVE   2047 // 2046 + 1, caso borde
#define MAX_VALOR   2047
#define PORT        5000    

// Señal 1 definida por el usuario, SIGUSR1
static volatile sig_atomic_t signal_sigusr1 = 0;

static void handler_sigusr1(int signum) {
    (void)signum;
    signal_sigusr1 = 1;
}

// Si fgets no llego a leer un '\n' y todavia no es el EOF, la linea era mas
// larga que el buffer, descarta lo que no entra
static void descartar_resto(FILE *in) {
    int c;
    while ((c = fgetc(in)) != '\n' && c != EOF)
        ;
}

static void registro_handler_sigusr1(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa)); // Campos sin uso en 0
    sa.sa_handler = handler_sigusr1; // Puntero al handler
    sa.sa_flags = 0; // Sin flags, incluida SA_RESTART
    // No se bloquean otras señales mientras se ejecuta el handler
    sigemptyset(&sa.sa_mask);

    // Registro de la estructura sa a la señal SIGUSR1
    if (sigaction(SIGUSR1, &sa, NULL) == -1) {
        perror("ERROR: sigaction");
        exit(1);
    }
}

// Parseo de comando, clave y valor. Devuelve la cantidad de campos reconocidos
static int parse_comando(const char *linea, char *comando, char *clave, char *valor) {
    int camposOk = sscanf(linea, "%3s %2046s %2046[^\n]", comando, clave, valor);
 
    // Si sscanf llego a llenar el buffer, es posible que hubo un truncamiento
    if (camposOk >= 2 && strlen(clave) == MAX_CLAVE - 1) {
        fprintf(stderr, "ERROR: la clave supera el tamaño maximo, entrada truncada\n");
        return -1;
    }
    if (camposOk >= 3 && strlen(valor) == MAX_VALOR - 1) {
        fprintf(stderr, "ERROR: el valor supera el tamaño maximo, entrada truncada\n");
        return -1;
    }
    return camposOk;
}

// Se agrega el puntero out, apunta a la estructura FILE creada por fdopen
static void cmd_set(const char *clave, const char *valor, FILE *out) {
    // Puntero al archivo, modo escritura (si ya existe lo sobrescribe)
    FILE *f = fopen(clave, "w");

    // Check funciones estandar
    if (f == NULL) {
        perror("ERROR: fopen");
        return;
    }

    if (fputs(valor, f) == EOF) {
        perror("ERROR: fputs");
        fclose(f);
        return;
    }

    fclose(f);
    fprintf(out, "OK\n");
    fflush(out); // Fuerza a vaciar el buffer en el momento
}

static void cmd_get(const char *clave, char *valor, size_t cap_valor, FILE *out) {
    FILE *f = fopen(clave, "r");
    if (f == NULL) {
        // ENOENT: el archivo no existe
        // Se informa si el error es por otras causas
        if (errno == ENOENT) {
            fprintf(out, "NOTFOUND\n");
        } else {
            perror("ERROR: fopen");
        }
        return;
    }

    // Comprobacion ante el NULL de fgets
    if (fgets(valor, (int)cap_valor, f) == NULL) {
        if (feof(f)) {
            valor[0] = '\0'; // Archivo vacio, no es un error
        } else {
            fprintf(stderr, "ERROR: fgets\n");
            fclose(f);
            return;
        }
    }

    fclose(f);
    fprintf(out, "OK\n%s\n", valor);
    fflush(out);
}

static void cmd_del(const char *clave, FILE *out) {
    // ENOENT, el archivo no existe
    if (remove(clave) != 0 && errno != ENOENT) {
        perror("ERROR: remove");
        return;
    }

    fprintf(out, "OK\n");
    fflush(out);
}

// Se crea, bindea y pone en escucha el socket del servidor
// Errores no recuperables, no se puede seguir si falla
static int crear_socket(void) {
    // FD del socket con direcciones IPv4, con argumento 0 el protocolo sera TCP
    int fd_socket = socket(AF_INET, SOCK_STREAM, 0);
    
    if (fd_socket == -1) {
        perror("ERROR: socket");
        exit(1);
    }

    // Variable con la información del socket
    struct sockaddr_in direccion;
    // Se ponen en 0 todos los campos ya que algunos no se van a usar
    memset(&direccion, 0 , sizeof(direccion));
    direccion.sin_family = AF_INET; // IPv4
    direccion.sin_addr.s_addr = INADDR_ANY; // Desde cualquier interfaz
    direccion.sin_port = htons(PORT); // Garantizar formato big endian

    // Syscall para asociar el socket con la direccion y puerto
    if (bind(fd_socket, (struct sockaddr *)&direccion, sizeof(direccion)) == -1) {
        perror("ERROR: bind");
        exit(1);
    }

    // Se pone el socket en modo escucha, 1 cliente a la vez
    if (listen(fd_socket, 1) == -1) {
        perror("ERROR: listen");
        exit(1);
    }

    return fd_socket;
}

static void atender_cliente(FILE *cliente) {
    char linea[MAX_LINEA], comando[MAX_COMANDO], clave[MAX_CLAVE], valor[MAX_VALOR];

    // Condicion: fgets devuelve NULL en el EOF (cliente cerro la conexion)
    while (fgets(linea, sizeof(linea), cliente) != NULL) {
        // Si no se encuentra '\n' y no es la ultima linea del archivo, se
        // supero el tamaño del buffer
        if (strchr(linea, '\n') == NULL && !feof(cliente)) {
            fprintf(stderr, "ERROR: la linea supera el tamaño maximo, entrada truncada\n");
            descartar_resto(cliente);
            continue;
        }

        // Parseo de comando y clave
        int camposOk = parse_comando(linea, comando, clave, valor);
        if (camposOk < 0)
            continue;

        if (camposOk < 2) {
            fprintf(stderr, "ERROR: cantidad incorrecta de arguemntos\n");
            continue;
        }

        // Se ingresaron al menos comando y clave
        if (strcmp(comando, "SET") == 0 && camposOk < 3) {
            fprintf(stderr, "ERROR: SET requiere clave y valor\n");
            continue;
        }

        if (strcmp(comando, "SET") == 0) {
            cmd_set(clave, valor, cliente);
        } else if (strcmp(comando, "GET") == 0) {
            cmd_get(clave, valor, sizeof(valor), cliente);
        } else if (strcmp(comando, "DEL") == 0) {
            cmd_del(clave, cliente);
        } else {
            fprintf(stderr, "ERROR: comando desconocido\n");
        }
    }
}

int main(void) {
    int fd_socket = crear_socket();
    registro_handler_sigusr1();

    printf("PID: %d\n", getpid());
    fflush(stdout); // Se fuerza la salida 

    while (1) {
        struct sockaddr_in direccion_cliente;
        socklen_t len_direccion = sizeof(direccion_cliente);

        // Syscall que bloquea el proceso hasta que se conecta un cliente
        int fd_cliente = accept(fd_socket, (struct sockaddr *)&direccion_cliente, &len_direccion);

        if (fd_cliente == -1) {
            // Si la señal SIGUSR1 llego mientras no habia ningun cliente se ignora
            if (errno == EINTR) {
                signal_sigusr1 = 0;
                continue;
            }

            perror("ERROR: accept");
            continue;
        }

        // Reseteo de la flag al aceptar un nuevo cliente
        signal_sigusr1 = 0;

        char ip_cliente[INET_ADDRSTRLEN];

        // Se hace una copia de la IP del cliente actual y se convierte a texto
        strncpy(ip_cliente, inet_ntoa(direccion_cliente.sin_addr), sizeof(ip_cliente));

        // Null terminator
        ip_cliente[sizeof(ip_cliente) - 1] = '\0';

        printf("Cliente conectado: %s\n", ip_cliente);
        fflush(stdout);

        // fdopen toma el FD del socket, crea la estructura FILE
        FILE *cliente = fdopen(fd_cliente, "r+");

        if (cliente == NULL) {
            perror("ERROR: fdopen");
            close(fd_cliente);
            continue;
        }

        atender_cliente(cliente);
        fclose(cliente);

        printf("Cliente desconectado: %s\n", ip_cliente);
        fflush(stdout);
    }
}
