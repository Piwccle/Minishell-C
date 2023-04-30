#include <stdio.h>
#include "parser.h"
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <dirent.h>
#include <libgen.h>
#include <signal.h>

/* typedef struct
{
	int pid;
	int estado;
} job; */

//gcc -Wall -Wextra shell1.c libparser_64.a -o shell1 -static

void unMand(tline *mandato);
void vMand(tline *mandato);
void cd(tline *mandato);

int main(void)
{

	//Declaración de variables y llamada del struct tline junto con las variables del directorio actual
	tline *mandato;
	char longitud[1024];
	char pwd[1024];
	getcwd(pwd, 1024);
	tline *ArrayMandatos[20];
	//Descriptores entrada y salida estandar
	int error = dup(2);	  //Tambien se podría haber utilizado  el dup así dup(fileno(sterr))
	int salida = dup(1);  //Tambien se podría haber utilizado  el dup así dup(fileno(stout))
	int entrada = dup(0); //Tambien se podría haber utilizado  el dup así dup(fileno(stin))

	//Ignorar señales de salida y pausa
	signal(SIGINT, SIG_IGN);
	signal(SIGQUIT, SIG_IGN);

	//Prompt
	printf("msh> ");

	//Bucle en el que se ejecutará la minishell
	while (fgets(longitud, 1024, stdin))
	{

		mandato = tokenize(longitud);
		if (mandato == NULL)
			continue;

		//Comprobamos si existe redireccion de salida
		if (mandato->redirect_output != NULL)
		{
			//Intentamos abrir el archivo y si no existe lo creamos
			int Fichero_de_salida = open(mandato->redirect_output, O_CREAT | O_RDWR, 0666);
			dup2(Fichero_de_salida, 1);
		}

		//Comprobamos si existe redireccion de entrada
		if (mandato->redirect_input != NULL)
		{
			int Fichero_de_entrada;
			printf("La entrada se ha redireccionado a : %s\n", mandato->redirect_input);
			Fichero_de_entrada = open(mandato->redirect_input, O_RDONLY);
			//Comprobamos si ha podido o no acceder al fichero
			if (Fichero_de_entrada != -1)
			{
				dup2(Fichero_de_entrada, 0); //redirijimos la entrada
			}
			else
			{
				fprintf(stderr, "%s: Error encontrado : %s.\n", mandato->redirect_input, strerror(errno));
			}
		}

		//Comprobamos si existe redireccion de errores
		if (mandato->redirect_error != NULL)
		{
			//Intentamos abrir el archivo y si no existe lo creamos
			int Fichero_de_error = open(mandato->redirect_error, O_CREAT | O_RDWR, 0666);
			dup2(Fichero_de_error, 2);
		}

		if (mandato->background)	//Comprobaremos si el mandato escrito está en background
		{
			printf("Este comando se ejecutará en background");

			for (int x = 0; x < ArrayMandatos[20]->ncommands; x++) //Ejecutaremos un bucle for con tamaño de 0 al tamaño de los array
			{
				while (ArrayMandatos[x] == 0)//E insertaremos en la posicion que sea 0 del array
				{
					ArrayMandatos[x] = mandato;//Inserción del mandato en el array
				}
			}
		}

		//Comprobamos que solo hay un mandado y si este es "cd" , "exit" o "jobs".
		if (mandato->ncommands == 1)
		{

			if (strcmp(mandato->commands[0].argv[0], "cd") == 0)
				cd(mandato);

			if (strcmp(mandato->commands[0].argv[0], "exit") == 0)
				exit(0);

			/* if (strcmp(mandato->commands[0].argv[0], "jobs") == 0)
			{
				if (ArrayMandatos[20]->commands == 0) //No imprimará por pantalla si no hay nada y saldrá un mensaje de error
				{
					printf("No se encuentra nada en segundo plano");
				}
				else
				{
					for (int i = 0; i < ArrayMandatos[20]->ncommands; i++)	//Volveremos a iterar por un bucle con tamaño 0 a tamaño del array
					{
						int cont;  //Creamos contador
						cont++;	  //Para que no empiece en 0 le incrementamos 1 asi empezará siempre en 1
						printf("[%d] + Running %s\n", cont, ArrayMandatos[i]->commands->filename);	//Imprimirá por pantalla el como el comando jobs lo hace
					}
				}
			} */
			//Aclaración: No funciona  el jobs. Hemos intentado implementarlo de la manera que hemos podido y de la manera más lógica

			if ((strcmp(mandato->commands[0].argv[0], "cd") != 0) &&
				(strcmp(mandato->commands[0].argv[0], "exit") != 0) &&
				/* (strcmp(mandato->commands[0].argv[0], "jobs") != 0) */)
				unMand(mandato);
		}

		//Si hay varios mandatos, llama a la funcion "vMand"
		if (mandato->ncommands > 1)
			vMand(mandato);

		//Devolver el valor original a la entrada, salida y salida de errores estándar(Si este ha sido modificado)
		if (mandato->redirect_input != NULL)
			dup2(entrada, 0);
		if (mandato->redirect_output != NULL)
			dup2(salida, 1);
		if (mandato->redirect_error != NULL)
			dup2(error, 2);

		//Actualizar directorio actual antes de mostrarlo en el prompt
		getcwd(pwd, 1024);
		//Volver a mostrar el prompt
		printf("msh> ");
	}
	return 0;
}

//Función que se encarga de ejecutar un solo comando de 0 a n argumentos
void unMand(tline *mandato)
{
	pid_t pid;
	int Estado;

	//Se crea el hijo
	pid = fork();

	//Comprobación de errores
	if (pid < 0)
	{
		fprintf(stderr, "Fallo al crear el hijo. %s\n", strerror(errno));
		exit(1);
	}

	//Nos metemos en el codigo que ejecuta el hijo
	if (pid == 0)
	{
		//Comprobamos las  señales
		if (mandato->background == 1)
		{
			signal(SIGINT, SIG_IGN);
			signal(SIGQUIT, SIG_IGN);
		}

		else
		{
			signal(SIGINT, SIG_DFL);
			signal(SIGQUIT, SIG_DFL);
		}

		//Se ejecuta el comando que ha sido tecleado por el usuario
		execvp(mandato->commands[0].argv[0], mandato->commands[0].argv);
		//Si no se ha ejucutado correctamente el comando "execvp" muestra un mensaje de error
		fprintf(stderr, "%s: El comando no existe.\n", mandato->commands[0].argv[0]);
		exit(1);
	}
	//Nos metemos en el codigo que ejecuta el padre
	else
	{
		//Esperar a que el hijo termine y comprobar el estado/exit
		wait(&Estado);
		if (WIFEXITED(Estado) != 0)
			if (WEXITSTATUS(Estado) != 0)
				printf("El comando ha fallado\n");
	}
}

//Función que se encarga de ejecutar minimo 2 comandos
void vMand(tline *mandato)
{
	//Almacenamos el numero de comandos introducidos por entrada estándar
	int numero_de_mandatos = mandato->ncommands;
	//Creamos el array donde se almacenarán las pipes
	int pipes[numero_de_mandatos - 1][2];

	//Creamos las pipes y las introducimos al array
	pipe(pipes[0]);

	//generamos el primer hijo
	int pid = fork();
	if (pid == 0)
	{
		//Comprobamos si el comando se ejecuta en segundo plano
		if (mandato->background == 1)
		{
			signal(SIGINT, SIG_IGN);
			signal(SIGQUIT, SIG_IGN);
		}

		else
		{
			signal(SIGINT, SIG_DFL);
			signal(SIGQUIT, SIG_DFL);
		}

		//Cerramos la  salida de las pipes de entrada
		close(pipes[0][0]);
		//Redireccionamos la  salida a la entrada de la pipe
		dup2(pipes[0][1], 1);

		//Se ejecuta el comando que ha sido tecleado por el usuario
		execvp(mandato->commands[0].argv[0], mandato->commands[0].argv);
		//Si no se ha ejucutado correctamente el comando "execvp" muestra un mensaje de error
		fprintf(stderr, "%s: El comando no existe.\n", mandato->commands[0].argv[0]);
		exit(1);
	}

	//Comprobamos que existen más de dos comandos
	if (numero_de_mandatos > 2)
	{
		//Creamos las pipes correspondientes a cada hijo
		for (int i = 1; i < (numero_de_mandatos - 1); i++)
		{
			pipe(pipes[i]);
		}

		//Hijos entre hijos
		for (int z = 1; z < (numero_de_mandatos - 1); z++)
		{
			//Creamos un hijo
			pid = fork();
			if (pid == 0)
			{
				//Comprobamos si el comando se ejecuta en segundo plano
				if (mandato->background == 1)
				{
					signal(SIGINT, SIG_IGN);
					signal(SIGQUIT, SIG_IGN);
				}

				else
				{
					signal(SIGINT, SIG_DFL);
					signal(SIGQUIT, SIG_DFL);
				}
				//Cerramos la  entrada de la  pipe (z-1) para que solo lea
				//Cerramos la  salida de la  pipe (z) para que solo escriba
				close(pipes[z - 1][1]);
				close(pipes[z][0]);

				//Cerramos todas las pipes que no se utilicen
				for (int y = 0; y < (numero_de_mandatos - 1); y++)
				{
					if (y != z && y != (z - 1))
					{
						close(pipes[y][1]);
						close(pipes[y][0]);
					}
				}

				dup2(pipes[z - 1][0], 0); //Redireccionamos su entrada a la salida de la pipe (z-1)
				dup2(pipes[z][1], 1);	  //Redireccionamos su salida a la entrada de la pipe z

				//Se ejecuta el comando que ha sido tecleado por el usuario
				execvp(mandato->commands[z].argv[0], mandato->commands[z].argv);
				//Si no se ha ejucutado correctamente el comando "execvp" muestra un mensaje de error
				fprintf(stderr, "%s: No se encuentra el mandato.\n", mandato->commands[z].argv[0]);
				exit(1);
			}
		}
	}

	//Creamos el ultimo hijo
	pid = fork();
	if (pid == 0)
	{
		//Comprobamos si el comando se ejecuta en segundo plano
		if (mandato->background == 1)
		{
			signal(SIGINT, SIG_IGN);
			signal(SIGQUIT, SIG_IGN);
		}

		else
		{
			signal(SIGINT, SIG_DFL);
			signal(SIGQUIT, SIG_DFL);
		}

		//Cerramos la  entrada de la  pipe (numero_de_mandatos-1) para que solo lea
		close(pipes[numero_de_mandatos - 2][1]);

		//Cerramos todas las pipes que no se utilicen
		for (int j = 0; j < (numero_de_mandatos - 2); j++)
		{
			close(pipes[j][1]);
			close(pipes[j][0]);
		}

		//Redireccionamos la salida del ultimo pipe a la entrada estándar
		dup2(pipes[numero_de_mandatos - 2][0], 0);

		//Se ejecuta el comando que ha sido tecleado por el usuario
		execvp(mandato->commands[numero_de_mandatos - 1].argv[0], mandato->commands[numero_de_mandatos - 1].argv);
		//Si no se ha ejucutado correctamente el comando "execvp" muestra un mensaje de error
		fprintf(stderr, "%s: No se encuentra el mandato.\n", mandato->commands[numero_de_mandatos - 1].argv[0]);
		exit(1);
	}

	//Una vez finalizado cerramos todos los hijos
	for (int j = 0; j < (numero_de_mandatos - 1); j++)
	{
		close(pipes[j][1]);
		close(pipes[j][0]);
	}

	//Esperamos a que terminen todos los hijos
	for (int i = 0; i < numero_de_mandatos; i++)
	{
		wait(NULL);
	}
}

//Función para ejecutar el mandato "cd"
void cd(tline *mandato)
{

	char pwd[1024];
	//Si no tiene ningun parámetro entonces vuelve e $HOME
	if (mandato->commands[0].argv[1] == NULL)
	{ //Comprobamos que no tiene argumentos

		chdir(getenv("HOME")); //Nos lleva a HOME
		//Mostramos por pantalla el directorio actual
		getcwd(pwd, 1024);
		printf("Se ha cambiado al directorio %s \n", pwd);
	}

	else if (mandato->commands[0].argc == 2)
	{
		int e = chdir(mandato->commands[0].argv[1]); //si tiene argumentos nos lleva al destino solicitado

		if (e == -1)
		{ //a menos que no exista

			fprintf(stderr, "El directorio actual no existe: %s \n", mandato->commands[0].argv[1]);
		}
		else
		{
			//Mostramos por pantalla el directorio actual
			getcwd(pwd, 1024);
			printf("Se ha cambiado al directorio %s \n", pwd);
		}
	}

	else
	{

		fprintf(stderr, "Error, hay mas de 1 argumento. Uso: cd [argumento1].\n");
	}
}