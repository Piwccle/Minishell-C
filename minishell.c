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

void unMand(tline *mandato);
void vMand(tline *mandato);
void cd(tline *mandato);

int main(void)
{
	tline *mandato;
	char longitud[1024];
	char pwd[1024];
	getcwd(pwd, 1024);
	tline *ArrayMandatos[20];
	int error = dup(2);
	int salida = dup(1);
	int entrada = dup(0);
	signal(SIGINT, SIG_IGN);
	signal(SIGQUIT, SIG_IGN);
	printf("msh> ");
	while (fgets(longitud, 1024, stdin))
	{
		mandato = tokenize(longitud);
		if (mandato == NULL)
			continue;
		if (mandato->redirect_output != NULL)
		{
			int Fichero_de_salida = open(mandato->redirect_output, O_CREAT | O_RDWR, 0666);
			dup2(Fichero_de_salida, 1);
		}
		if (mandato->redirect_input != NULL)
		{
			int Fichero_de_entrada;
			printf("La entrada se ha redireccionado a : %s\n", mandato->redirect_input);
			Fichero_de_entrada = open(mandato->redirect_input, O_RDONLY);
			if (Fichero_de_entrada != -1)
			{
				dup2(Fichero_de_entrada, 0);
			}
			else
			{
				fprintf(stderr, "%s: Error encontrado : %s.\n", mandato->redirect_input, strerror(errno));
			}
		}
		if (mandato->redirect_error != NULL)
		{
			int Fichero_de_error = open(mandato->redirect_error, O_CREAT | O_RDWR, 0666);
			dup2(Fichero_de_error, 2);
		}
		if (mandato->background)
		{
			printf("Este comando se ejecutará en background");
			for (int x = 0; x < 20; x++)
			{
				if (ArrayMandatos[x] == 0)
				{
					ArrayMandatos[x] = mandato;
					break;
				}
			}
		}
		if (mandato->ncommands == 1)
		{
			if (strcmp(mandato->commands[0].argv[0], "cd") == 0)
				cd(mandato);
			if (strcmp(mandato->commands[0].argv[0], "exit") == 0)
				exit(0);
		}
	}
	return 0;
}

void cd(tline *mandato)
{
	if (mandato->commands[0].argc == 1)
	{
		char *Home = getenv("HOME");
		if (Home == NULL)
			fprintf(stderr, "Error: $HOME variable no existe.\n");
		else
			chdir(Home);
	}
	else
	{
		if (chdir(mandato->commands[0].argv[1]) != 0)
			fprintf(stderr, "Error: No se puede cambiar a %s.\n", mandato->commands[0].argv[1]);
	}
}
