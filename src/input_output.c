/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   input_output.c                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: vjan-nie <vjan-nie@student.42madrid.com    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/08/25 13:50:20 by vjan-nie          #+#    #+#             */
/*   Updated: 2025/09/10 12:09:46 by vjan-nie         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "minishell.h"

/* 
IMPORTANTE: Al parecer, si bloque de comando tiene varias redirecciones,
no utiliza procesos hijos por separado, sino que se ejecuta en un
solo proceso, escribiendo a la vez en varios FDs!
En shells reales, cada comando con sus redirecciones se ejecuta en un proceso.
Hace falta implementar o testear múltiples redirecciones en un mismo
bloque de comandos
 */
int redirect_in(t_command *command_list, int in_fd)
{
	t_redir *temp;
	int 	new_fd;

	temp = command_list->redirs;
	new_fd = in_fd;
	while (temp)
	{
		if (temp->type == REDIR_IN)
			new_fd = get_inputfile_fd(temp->file);
		else if (temp->type == HEREDOC)
			new_fd = get_heredoc_fd(temp->file);
		if (new_fd == -1)
			return (-1);
		temp = temp->next;
	}
	return (new_fd);
}

int redirect_out(t_command *command_list, int out_fd)
{
	t_redir *temp;
	int 	new_fd;

	temp = command_list->redirs;
	new_fd = out_fd;
	while (temp)
	{
		if (temp->type == REDIR_OUT)
			new_fd = get_outputfile_fd(temp->file);
		else if (temp->type == APPEND)
			new_fd = get_append_fd(temp->file);
		if (new_fd == -1)
			return (-1);
		temp = temp->next;
	}
	return (new_fd);
}

int	get_inputfile_fd(char *infile)
{
	int	fd;

	fd = open(infile, O_RDONLY);
	if (fd < 0)
		return (perror(infile), -1);
	return (fd);
}

int	get_outputfile_fd(char *outfile)
{
	int	fd;

	fd = open(outfile, O_CREAT | O_WRONLY | O_TRUNC, 0644);//abrir outfile con opción de reescritura (truncate)
	if (fd < 0)
		return (perror(outfile), -1);
	return (fd);
}

int	get_append_fd(char *outfile)
{
	int	fd;

	fd = open(outfile, O_CREAT | O_WRONLY | O_APPEND, 0644);//abrir outfile con opción de append
	if (fd < 0)
		return (perror(outfile), -1);
	return (fd);
}

static void	heredoc_child_process(char *limiter, int *pipe_fd)
{
	char	*line;

	signal(SIGINT, SIG_DFL); // restaurar comportamiento default (terminar con Ctrl+C)
	close(pipe_fd[0]); // cerramos lectura en el hijo
	while (1)
	{
		write(1, "heredoc> ", 9);
		line = get_next_line(0);
		if (!line)
			exit(EXIT_SUCCESS); // EOF o Ctrl+D
		if (strncmp(line, limiter, strlen(limiter)) == 0 && line[strlen(limiter)] == '\n')
		{
			free(line);
			break;
		}
		if (write(pipe_fd[1], line, strlen(line)) == -1)
		{
			perror("write to pipe");
			free(line);
			exit(EXIT_FAILURE);
		}
		free(line);
	}
	close(pipe_fd[1]);
	exit(EXIT_SUCCESS);
}

/* HERE_DOC!!:
Si presionas Ctrl+C mientras estás escribiendo en el heredoc, lo correcto es que el shell:
Interrumpa solo el heredoc.
NO termine todo el minishell.
Regrese al prompt de comandos, como hace bash/zsh.

Solución: usar fork()
Separar el heredoc en un proceso hijo permite:
Que el hijo maneje la lectura.
Que si se presiona Ctrl+C, sólo se mate el hijo, no el minishell.
El padre puede detectar la interrupción y actuar (por ejemplo, no ejecutar el comando).
Lagunas teóricas: no entiendo muy bien cómo funcionan estas señales!
 */
int	get_heredoc_fd(char *limiter)
{
	int		pipe_fd[2];
	pid_t	pid;
	int		status;

	if (pipe(pipe_fd) == -1)//creamos pipe
		return (perror("here_doc pipe"), -1);
	pid = fork();
	if (pid == -1)
		return (perror("fork"), -1);
	if (pid == 0) // HIJO:
		heredoc_child_process(limiter, pipe_fd);
	signal(SIGINT, SIG_IGN); // ignorar SIGINT en el padre durante el heredoc
	close(pipe_fd[1]); // cerramos escritura en el padre
	waitpid(pid, &status, 0); // esperamos que termine el hijo
	signal(SIGINT, SIG_DFL); // restauramos manejo de SIGINT
	if (WIFSIGNALED(status) && WTERMSIG(status) == SIGINT)// El heredoc fue interrumpido con Ctrl+C
		return (close(pipe_fd[0]), -1);
	return (pipe_fd[0]); // devuelve el fd de lectura con el contenido
}
