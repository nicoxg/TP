#include <stdio.h>
#include <string.h>
#include <errno.h>

int main(void) {
    // Longitudes arbitrarias, comando de maximo 3 digitos
    char linea[256], comando[4], clave[128], valor[128];

    // Condicion: fgets devuelve NULL en el EOF
    while (fgets(linea, sizeof(linea), stdin) != NULL) {
	// Parseo de dos palabras, el resto de la linea es el valor. Se trunca para evitar overflow
        int camposOk = sscanf(linea, "%3s %127s %127[^\n]", comando, clave, valor);

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
	    // Puntero al archivo, modo escritura (si ya existe lo sobrescribe)
            FILE *f = fopen(clave, "w");
	    
	    // Check funciones estandar
	    if (f == NULL) {
		perror("ERROR: fopen");
		continue;
	    }
	
	    if (fputs(valor, f) == EOF) {
		perror("ERROR: fputs");
		fclose(f);
		continue;
	    }

            fclose(f);
            printf("OK\n");

        } else if (strcmp(comando, "GET") == 0) {
            FILE *f = fopen(clave, "r");
            if (f == NULL) {
                printf("NOTFOUND\n");
            } else {
		if (fgets(valor, sizeof(valor), f) == NULL) {
		    fprintf(stderr, "ERROR: fgets");
		    fclose(f);
		    continue;
                }
		fclose(f);
		printf("OK\n%s\n", valor);
	    }
	    
        } else if (strcmp(comando, "DEL") == 0) {
	    // ENOENT, el archivo no existe
	    if (remove(clave) != 0 && errno != ENOENT) {
		perror("ERROR: remove");
		continue;
	    }
	    printf("OK\n");

        } else {
	    fprintf(stderr, "ERROR: comando desconocido\n");
	}
    }
}
