#include <stdio.h>
#include <string.h>
#include <errno.h>

// Longitudes arbitrarias, comando de maximo 3 digitos
#define MAX_LINEA 4096
#define MAX_COMANDO 4
#define MAX_CLAVE 2046
#define MAX_VALOR 2046

// Si fgets no llego a leer un '\n' y todavia no es el EOF, la linea era mas
// larga que el buffer, descarta lo que no entra
static void descartar_resto(void) {
    int c;
    while ((c = getchar()) != '\n' && c != EOF)
        ;
}

// Parseo de comando, clave y valor. Devuelve la cantidad de campos reconocidos
static int parse_comando(const char *linea, char *comando, char *clave, char *valor) {
    int camposOk = sscanf(linea, "%3s %2045s %2045s[^\n]", comando, clave, valor);
 
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


static void cmd_set(const char *clave, const char *valor) {
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
    printf("OK\n");
}

static void cmd_get(const char *clave, char *valor, size_t cap_valor) {
    FILE *f = fopen(clave, "r");
    if (f == NULL) {
        // ENOENT: el archivo no existe
        // Se informa si el error es por otras causas
        if (errno == ENOENT) {
            printf("NOTFOUND\n");
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
    printf("OK\n%s\n", valor);
}

static void cmd_del(const char *clave) {
    // ENOENT, el archivo no existe
    if (remove(clave) != 0 && errno != ENOENT) {
        perror("ERROR: remove");
        return;
    }
    printf("OK\n");
}

int main(void) {
    char linea[MAX_LINEA], comando[MAX_COMANDO], clave[MAX_CLAVE], valor[MAX_VALOR];

    // Condicion: fgets devuelve NULL en el EOF
    while (fgets(linea, sizeof(linea), stdin) != NULL) {
        // Si no se encuentra '\n' y no es la ultima linea del archivo, se
        // supero el tamaño del buffer
        if (strchr(linea, '\n') == NULL && !feof(stdin)) {
            fprintf(stderr, "ERROR: la linea supera el tamaño maximo, entrada truncada\n");
            descartar_resto();
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
            cmd_set(clave, valor);
        } else if (strcmp(comando, "GET") == 0) {
            cmd_get(clave, valor, sizeof(valor));
        } else if (strcmp(comando, "DEL") == 0) {
            cmd_del(clave);
        } else {
            fprintf(stderr, "ERROR: comando desconocido\n");
        }
    }
}
