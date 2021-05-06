#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <stdio.h>
#include <strings.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

const int MAX_LINE = 80;

char **parse_command(char *line)
{
	char **args = malloc(sizeof(char *)*(MAX_LINE/2+1));
	for (int i = 0; i < (MAX_LINE/2+1); i++)
		args[i] = malloc(MAX_LINE);
	char * token = strtok(line, " ");
	int i;
	for(i = 0; token != NULL; i++) {
		strcpy(args[i],token);
		token = strtok(NULL, " ");
	}
	args[i] = NULL;
	return args;
}

enum {DEFAULT, NOWAIT, REDIN, REDOUT, PIPE};

void subs(char from, char to, char **str)
{
	char *pt = *str;
	while (*pt) {
		if (*pt == from)
			*pt = to;
		pt++;
	}
}

int pre_parse_command(char **str, char **aux)
{
	// FIXME: Tirar o /n do fim de aux
	char *end = strpbrk(*str, "&<>|\n");
	switch (*end) {
	case '\n':
		*end = '\0';
		aux = NULL;
		return DEFAULT;
	case '&':
		*end = '\0';
		aux = NULL;
		return NOWAIT;
	case '<':
		*end = '\0';
		*aux = end+2;
		subs('\n', '\0', aux);
		return REDIN;
	case '>':
		*end = '\0';
		*aux = end+2;
		subs('\n', '\0', aux);
		return REDOUT;
	case '|':
		*end = '\0';
		*aux = end+2;
		subs('\n', '\0', aux);
		return PIPE;
	}
	return -1;
}

int shell_pipe(char **arg1, char **arg2) {
	/*
	 * (1) create pipe
	 * (2) fork proccess
	 */
	int pfd[2];
	pid_t child_pid;
	if (pipe(pfd) < 0)
		return 1;
	int po = pfd[0]; // pipe out / read
	int pi = pfd[1]; // pipe in / write
	child_pid = fork();
	if (child_pid < 0) {
		return 1;
	} else if (child_pid == 0) {
	/*
	 * (3-child) close read end
	 * (4-child) dup2 pipe stdout
	 * (5-child) execpv args1
	 */
		close(po);
		dup2(pi, STDOUT_FILENO);
		if (execvp(arg1[0], arg1) < 0)
			return 1;
	} else {
	/*
	 * (3-paren) close write end
	 * (4-paren) dup2 pipe stdin
	 * (5-paren) wait child
	 * (6-paren) execpv args2
	 */
		close(pi);
		dup2(po, STDIN_FILENO);
		wait(NULL);
		if (execvp(arg2[0], arg2) < 0)
			return 1;
	}
	return 0;
}

int shell_fork(int mode, char **args, char *aux)
{
	pid_t child_pid = fork();
	if (child_pid < 0) {
		printf("Error! Fork failed and returned %d\n",
				child_pid);
		return 1;
	} else if (child_pid == 0) {
		if (mode == REDIN) {
			FILE *fd = fopen(aux, "r");
			dup2(fileno(fd), STDIN_FILENO);
		} else if (mode == REDOUT) {
			FILE *fd = fopen(aux, "w");
			dup2(fileno(fd), STDOUT_FILENO);
		} else if (mode == PIPE) {
			if (shell_pipe(args, parse_command(aux))) {
				printf("pipe failed\n");
				exit(1);
			}
			return 0;
		}

		if (execvp(args[0], args) < 0) {
			printf("coudln't run program %s\n", args[0]);
			exit(1);
		}
	} else {
		if (mode != NOWAIT) {
			wait(NULL);
			printf("child %d (%s) exited\n",
				child_pid, args[0]);
		}
		else { // NOWAIT
			printf("child %d (%s) running on bg\n",
				child_pid, args[0]);
		}
	}
	return 0;
}

int main(void)
{
	const char prompt[] = "xel> ";
	char *line = malloc(sizeof(char)*MAX_LINE);
	char *hist = NULL;

	while (1) {
		printf(prompt);
		fflush(stdout);

		fgets(line, MAX_LINE, stdin);

		if (strncmp(line, "exit", 4) == 0) {
			exit(0);
		} else if (strncmp(line,"!!",2)== 0){
			if (hist == NULL){
				printf("No commands in history.\n");
				continue;
			}
			else{
				printf("%s\n",hist);
				line = strdup(hist);
			}

		} else {
			if (hist) free(hist);
			hist = strdup(line);
		}

		char *aux;
		char **args;
		int mode;
		mode = pre_parse_command(&line, &aux);
		args = parse_command(line);
		shell_fork(mode, args, aux);
		free(args);
	}
}
