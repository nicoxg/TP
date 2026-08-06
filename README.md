# TP1 - Sistemas Operativos de Proposito General
**Estudiante**: Nicolas Martinez

## Descripcion de la solucion

Para implementar el almacen clave-valor, cada clave se guarda como un archivo cuyo nombre es la clave y cuyo contenido es el valor. 
Los comandos se leen de stdin, uno por línea, y se procesan de manera continua hasta EOF (Ctrl + D).

Los comandos que se pueden usar en este programa son:

- SET <clave> <valor>: crea (o sobreescribe) el archivo <clave> con el contenido <valor> y retorna OK.
- GET <clave>: si el archivo existe, responde OK seguido de su contenido. En  caso contrario, retorna NOTFOUND.
- DEL <clave>: borra el archivo si existe, retorna OK aun si el archivo no existe.

### Manejo de errores

- Comando desconocido o comando SET sin clave/valor: se informa por stderr y se sigue recibiendo comandos, se considera error recuperable.
- Falla de una llamada de funciones estandar sobre un archivo (fopen, fputs, fgets): se informa la causa por stderr con perror y se espera a recibir el siguiente comando, error recuperable.
- Comando DEL, si remove falla porque el archivo no existe (errno == ENOENT): no se lo considera error, retorna OK.

## Compilacion y ejecucion
gcc TP1.c -o '<nombre>'

./'<nombre>'
