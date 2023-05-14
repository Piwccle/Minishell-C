#include "stdio.h"
#include "parser.h"
#include "sys/types.h"
#include "sys/wait.h"
#include "unistd.h"
#include "stdlib.h"
#include "errno.h"
#include "string.h"
#include "fcntl.h"
#include "dirent.h"
#include "libgen.h"
#include "signal.h"

//gcc -Wall -Wextra myshell.c libparser.a -o myshell -static
//Compilar

//struct para permitir implementar el jobs
typedef struct _job{
	pid_t pidProcesoBackground;
	tline* comandoBackground;
	char* lineaComando;
	struct _job *punteroSiguienteNodo;
	//char* salida;
}job;

//Funciones para hacer mas modular el main y no tener un main que se encargue de todo
void ejecutarComando(tline *comando);
void ejecutarComandos(tline *comando);

//Funciones cd, fg y jobs que se piden en el enunciado
void cd(tline *comando);
void fg();
void jobs();

//Funciones privadas para manejar el struct job (necesarias sino no se puede implementar fg y jobs)
void addJob(pid_t pgid, tline *comando);
void borrarJob();

job *comandosBackground; //Puntero a la lista enlazada de jobs
int nComandosB = 0;

int main(int argc, char* argv[]) {

	if(argc != 1){
		fprintf(stderr, "Error en el numero de argumentos, uso: %s\n", argv[0]);
		return 1;
	}
	//Declaramos variables para la posible ejecucion de cualquier comando ya sean 1 o varios, con 
	//posibles redirecciones y posible ejecucion en background
	tline *comando;
	int stdEntrada = dup(0);
	int stdSalida = dup(1);
	int stdError = dup(2);	//Esto es necesario para guardar el valor inicial de stdin, stdout y stderr

	
	//Buffer donde queda el stdin 
	char buffer[1024];

	//Ignorar señales CTRL+C y CTRL+'\' 
	signal(SIGQUIT, SIG_IGN);
	signal(SIGINT, SIG_IGN);
	

	//Imprimimos el primer prompt por pantalla y al final del while el que vendria despues
	printf("msh > ");

	
	while (fgets(buffer, 1024, stdin)) { 

		//tokenizamos el "comando" para convertirlo en un tline y poder ejecutarlo con todos sus argumentos
		comando = tokenize(buffer);
		if (comando == NULL) continue; //Si por lo que sea el comando no se ha tokenizado bien, volvemos al principio

		if (comando->redirect_input != NULL) { //Redireccion de entrada
			int fdEntrada;
			fdEntrada = open(comando->redirect_input, O_RDONLY);

			if (fdEntrada == -1){ //Comprobamos si ha podido o no acceder al fichero
				fprintf(stderr, "Error %s.\n", strerror(errno));
				continue;
			}
			else dup2(fdEntrada, 0); //Si no ha habido problemas sobreescribimos en la entrada estandar fdEntrada
			
		}

		if (comando->redirect_output != NULL) { //Redireccion de salida
			
			int fdSalida = open(comando->redirect_output, O_CREAT | O_RDWR, 0666); //Abrimos o creamos el archivo donde se quiere redirigir
			dup2(fdSalida, 1); //Como no puede fallar, porque si no existe se crea sobreescribimos en la salida estandar fdSalida
		}

		if (comando->redirect_error != NULL) { //Redireccion de errores

			
			int fdError = open(comando->redirect_error, O_CREAT | O_RDWR, 0666); //Abrimos o creamos el archivo donde se quiere redirigir el error
			dup2(fdError, 2); //Redirigimos la salida estandar de error a ese fdError
		}

		if (comando->ncommands == 1) { //Ejecucion cuando hay un solo comando
			if (strcmp(comando->commands[0].argv[0], "cd") == 0) cd(comando); //Si es el cd
			else if (strcmp(comando->commands[0].argv[0], "exit") == 0) exit(0); //Si es el exit
			else if (strcmp(comando->commands[0].argv[0], "jobs") == 0) jobs(); //Si es el jobs
			else if (strcmp(comando->commands[0].argv[0], "fg") == 0) fg(); //Si es el fg
			else ejecutarComando(comando); //En caso de que no sea ninguno de los evaluados previamente llamamos a la funcion que ejecuta un solo comando, con todos sus argumentos, para hacer el main mas visual
		} else { //Cuando hay varios comandos
			ejecutarComandos(comando); //Si hay varios comandos en lo devuelto por el buffer llamamos a nuestra funcion que ejecuta varios comandos, para hacer el main mas visual
		}

		//Debemos devolver las salidas o entradas estandar a su correspondiente valor, en caso de haberlo modificado, para que el siguiente comando que reciba el prompt no se vea alterado
		if (comando->redirect_input != NULL) dup2(stdEntrada, 0);
		if (comando->redirect_output != NULL) dup2(stdSalida, 1);
		if (comando->redirect_error != NULL) dup2(stdError, 2);

		//if(comando->background != 1)
		printf("msh > ");
	}
	return 0;
}


void ejecutarComando(tline *comando) {
	pid_t pidComando;
	int status;

	pidComando = fork(); //Creamos el proceso que ejecutara el comando
	if (pidComando < 0) { 
		fprintf(stderr, "Error al crear el proceso hijo.\n"); 
		exit(1);
	} else if (pidComando == 0) { //Si somos el proceso hijo ejecutamos el comando
		
		if (comando->background == 1) { //Definimos como se ha de comportar el proceso hijo frente a CTRL+C y CTRL+\ en funcion de si esta en background o no y su posterior ejecucion
			
			setpgid(0, 0); //Con esto asignamos el pid con el PGID, ambos 0
			if (execvp(comando->commands[0].argv[0], comando->commands[0].argv) < 0) {
                fprintf(stderr, "%s: No se encuentra el comando\n", comando->commands[0].argv[0]); //Si execvp retorna significa que ha ocurrido un error
                exit(1); //liberar proceso en caso de que no se pueda
            }
		}else {
			signal(SIGINT, SIG_DFL);
			signal(SIGQUIT, SIG_DFL);
			execvp(comando->commands[0].argv[0], comando->commands[0].argv); //execvp ejecuta el comando con sus argumentos como sabemos que solo hay un comando por ejecutar accedemos a la posicion [0]
			fprintf(stderr, "%s: No se encuentra el comando\n", comando->commands[0].argv[0]); //Si execvp retorna significa que ha ocurrido un error
			exit(1);
		}
	} else {
		//Esperar a que el hijo termine y comprobar el estado/exit
		if(comando->background != 1){
			wait (&status);
			if (WIFEXITED(status) != 0)
				if (WEXITSTATUS(status) != 0)
					printf("El comando no se ejecutó correctamente\n");
		}else{
			addJob(pidComando, comando); //Añadimos una tarea con nuesta funcion
			printf("[%d] %d\n", nComandosB, pidComando); //Imprimimos algo parecido a lo que imprime la terminal

			setpgid(pidComando, 0); //Asignamos el pid del comando al PGID 0
            tcsetpgrp(STDIN_FILENO, getpgrp()); //Pasamos la terminal a foreground para dejar en segundo plano el comando que queremos ejecutar y retornamos a la shell para que se siga ejecutando
			return;
		}
	}
}

void ejecutarComandos(tline *comando) {

	
	int nComandos = comando->ncommands; //Necesitamos saber cuandos comandos hay en total en la linea que acabamos de leer para crear los correspondientes pipes y relacionarlos entre si
	int pipes[nComandos - 1][2];//Array de pipes, siempre seran nComandos-1 pipes p.e (ls | head | wc tiene 2 pipes y con nComandos = 3)

	if(comando->background == 0){

		//Abrimos todos los pipes necesarios
		for (int i = 0; i < nComandos; i++)	pipe(pipes[i]);
		
		for (int i = 0; i < nComandos; i++)	{
			//Creamos un hijo
			pid_t pidComando = fork();
			if(pidComando < 0){
				fprintf(stderr, "Error al crear el hijo\n");
				exit(1);
			} else if (pidComando == 0 && i == 0) { //Para el primer comando hay una ejecucion diferente ya que no recibe nada de ningun otro comando

				//Cerramos el extremo de lectura del primer comando
				close(pipes[i][0]);
				//Redireccionamos la salida del comando al extremo de escritura del pipe
				dup2(pipes[i][1], 1);

				execvp(comando->commands[0].argv[0], comando->commands[0].argv);
				fprintf(stderr, "%s: No se encuentra el mandato\n", comando->commands[0].argv[0]); //Si execvp retorna significa que ha ocurrido un error
				exit(1);

			} else if (pidComando == 0 && i == nComandos-1) { //Para el ultimo comando hay otra ejecucion diferente ya que no tiene que enviar nada a otro comando

				//Cerramos el extremo de escritura del ultimo comando
				close(pipes[i-1][1]);

				//Cerramos todos los extremos de todos los comandos que no se utilicen porque podria haber interferencia
				for (int j = 0; j < i-1; j++) {
					close(pipes[j][1]);
					close(pipes[j][0]);
				}

				//Redireccionamos el extremo de lectura del ultimo comando a la salida estandar que hayamos definido previamente
				dup2(pipes[i-1][0], 0);

				execvp(comando->commands[i].argv[0], comando->commands[i].argv);
				fprintf(stderr, "%s: No se encuentra el comando.\n", comando->commands[i].argv[0]);//Si execvp retorna significa que ha ocurrido un error
				exit(1);

			} else if(pidComando == 0) { //Si somos cualquier otro comando intermedio

				//Cerramos el extremo de escritura del pipe (i-1) 
				//Cerramos el extremo de lectura de la  pipe (i) 
				close(pipes[i - 1][1]);
				close(pipes[i][0]);

				//Cerramos el resto de pipes de los demas comandos ya que solo necesitamos la anterior a nuestro comando y la de nuestro comando
				for (int j = 0; j < (nComandos - 1); j++) 
				{
					if (j != i && j != (i - 1)) //Esto asegura que no se cierren las que necesitamos
					{
						close(pipes[j][1]);
						close(pipes[j][0]);
					}
				}

				dup2(pipes[i - 1][0], 0); //Redireccionamos el extremo de lectura del pipe del comando anterior al nuestro con nuestra entrada estandar
				dup2(pipes[i][1], 1);	  //Redireccionamos el extremo de escritura de nuestro pipe con la salida estandar

				execvp(comando->commands[i].argv[0], comando->commands[i].argv);
				fprintf(stderr, "%s: No se encuentra el comando.\n", comando->commands[i].argv[0]);//Si execvp retorna significa que ha ocurrido un error
				exit(1);

			}
		}

	//Una vez finalizado cerramos todos los hijos
		for (int j = 0; j < (nComandos - 1); j++) {
			close(pipes[j][1]);
			close(pipes[j][0]);
		}

		//Esperamos a que terminen todos los hijos
		for (int i = 0; i < nComandos; i++) wait(NULL);



		
	} else {//BACKGROUND 
		//Para realizar la ejecucion de varios comandos en background es necesario empezar con el primer fork como si solo fuera un comando y despues hacer lo mismo qeu cuando era 1 comando
		for (int i = 0; i < nComandos; i++)	pipe(pipes[i]);

		pid_t pidComando = fork();
		if (pidComando == 0) { //Para el primer comando hay una ejecucion diferente ya que no recibe nada de ningun otro comando
			setpgid(0, 0);
			//Cerramos el extremo de lectura del primer comando
			close(pipes[0][0]);
			//Redireccionamos la salida del comando al extremo de escritura del pipe
			dup2(pipes[0][1], 1);
			
			if(execvp(comando->commands[0].argv[0], comando->commands[0].argv)< 0){
				fprintf(stderr, "%s: No se encuentra el mandato\n", comando->commands[0].argv[0]); //Si execvp retorna significa que ha ocurrido un error
				exit(1);
			}
		}
		else {
			addJob(pidComando, comando);
			printf("[%d] %d\n", nComandosB, pidComando);
			setpgid(pidComando, 0);
            tcsetpgrp(STDIN_FILENO, getpgrp());
		}

		for (int i = 1; i < nComandos; i++)	{
			//Creamos un hijo
			pid_t pidComando = fork();
			if (pidComando == 0 && i == nComandos-1) { //Para el ultimo comando hay otra ejecucion diferente ya que no tiene que enviar nada a otro comando

				//Cerramos el extremo de escritura del ultimo comando
				close(pipes[i-1][1]);

				//Cerramos todos los extremos de todos los comandos que no se utilicen porque podria haber interferencia
				for (int j = 0; j < i-1; j++) {
					close(pipes[j][1]);
					close(pipes[j][0]);
				}

				//Redireccionamos el extremo de lectura del ultimo comando a la salida estandar que hayamos definido previamente
				dup2(pipes[i-1][0], 0);

				execvp(comando->commands[i].argv[0], comando->commands[i].argv);
				fprintf(stderr, "%s: No se encuentra el comando.\n", comando->commands[i].argv[0]);//Si execvp retorna significa que ha ocurrido un error
				exit(1);

			} else if(pidComando == 0) { //Si somos cualquier otro comando intermedio

				//Cerramos el extremo de escritura del pipe (i-1) 
				//Cerramos el extremo de lectura de la  pipe (i) 
				close(pipes[i - 1][1]);
				close(pipes[i][0]);

				//Cerramos el resto de pipes de los demas comandos ya que solo necesitamos la anterior a nuestro comando y la de nuestro comando
				for (int j = 0; j < (nComandos - 1); j++) 
				{
					if (j != i && j != (i - 1)) //Esto asegura que no se cierren las que necesitamos
					{
						close(pipes[j][1]);
						close(pipes[j][0]);
					}
				}

				dup2(pipes[i - 1][0], 0); //Redireccionamos el extremo de lectura del pipe del comando anterior al nuestro con nuestra entrada estandar
				dup2(pipes[i][1], 1);	  //Redireccionamos el extremo de escritura de nuestro pipe con la salida estandar

				execvp(comando->commands[i].argv[0], comando->commands[i].argv);
				fprintf(stderr, "%s: No se encuentra el comando.\n", comando->commands[i].argv[0]);//Si execvp retorna significa que ha ocurrido un error
				exit(1);

			}
		}

		//Una vez finalizado cerramos todos los hijos
		for (int j = 0; j < (nComandos - 1); j++) {
			close(pipes[j][1]);
			close(pipes[j][0]);
		}

	}
}

void addJob(pid_t ppb, tline *c) {
    job *nuevoJob = malloc(sizeof(job));
    nuevoJob->pidProcesoBackground = ppb;
    nuevoJob->comandoBackground = c;
	nuevoJob->lineaComando = (char*)malloc(sizeof(char)* 100);
	strcpy(nuevoJob->lineaComando, c->commands[0].argv[0]);
	for(int i = 1; i<c->commands[0].argc; i++){
		char* args = (char*)malloc(sizeof(char)* 100);
		char* aux = " \0";
		strcat(args, aux);
		strcat(args, c->commands[0].argv[i]);
		strcat(nuevoJob->lineaComando, args); 
		free(args);
	}
    nuevoJob->punteroSiguienteNodo = comandosBackground;
    comandosBackground = nuevoJob;
	nComandosB++;
}

void borrarJob() {
    job *j = comandosBackground;
    comandosBackground = j->punteroSiguienteNodo;
	nComandosB--;
	free(j->lineaComando);
	free(j);
}


void cd(tline *comando)
{
	char directorio[128]; //Creamos string para saber donde nos encontramos en caso de que el cd se ejecute sin argumentos
	if (comando->commands[0].argc == 1) { //Comprobamos que no tenga argumentos para, en ese caso, ir a $HOME
		chdir(getenv("HOME")); //Siempre existe, no hay que comprobar error
		
		getcwd(directorio, 128); //Se obtiene la ruta absoulta
		printf("Nuevo directorio de trabajo: %s \n", directorio); 

	} else if (comando->commands[0].argc == 2) {
		int dirStatus = chdir(comando->commands[0].argv[1]); //si tiene argumentos nos lleva al destino solicitado
		if (dirStatus == -1) fprintf(stderr, "El directorio %s no existe\n", comando->commands[0].argv[1]); //Si da fallo
		else {
			getcwd(directorio, 128);
			printf("Nuevo directorio de trabajo: %s\n", directorio);
		}
	} 
	else fprintf(stderr, "Uso: %s ó %s argumento.\n", comando->commands[0].argv[0], comando->commands[0].argv[0]);
}


void fg() {
	if(nComandosB <= 0){
		fprintf(stderr,"Error al pasar a foreground, el comando no existe en background\n");
        return;
	}
    if (comandosBackground->comandoBackground == NULL) {
        fprintf(stderr,"Error al pasar a foreground, el comando no existe en background\n");
        return;
    }
	job* t = comandosBackground;

	pid_t ppb = comandosBackground->pidProcesoBackground;
	
	signal(SIGTTIN, SIG_IGN);
    signal(SIGTTOU, SIG_IGN); //Evitar interferencias con valores de entrada y salida

	printf("PID: %d \"%s\" puesto a FG\n", t->pidProcesoBackground, t->lineaComando);

    // Mover el proceso al foreground 
    tcsetpgrp(STDIN_FILENO, ppb);

	//Enviar señal para que termine
	kill(ppb, SIGCONT);

    // Esperar a que termine el proceso
    waitpid(ppb, NULL, WUNTRACED); //Podriamos almacenar estado pero no lo hemos visto necesario

    // Devolver el control de la terminal al shell
    tcsetpgrp(STDIN_FILENO, getpgrp());

	signal(SIGTTIN, SIG_DFL); //Devolver los valores a default
    signal(SIGTTOU, SIG_DFL);


    // Eliminar la información del trabajo de la lista de trabajos
	borrarJob();
	return;
}


void jobs() {
	int contador = 1;
    job *j;
    for(j = comandosBackground; j != NULL; contador++) { //Iterar sobre la lista que hemos creado
		printf("[%d]+ ", contador);
		if(contador == 1) printf("Running\t\t");
		else printf("Detenido\t\t");
        printf("%s %d\n", j->lineaComando, j->pidProcesoBackground);
		j = j->punteroSiguienteNodo;
    }
}
