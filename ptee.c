#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <unistd.h>

#include <errno.h>

struct ptee_command {
	char* command;
	int pipeout; // Forked pipe writer
	int pipein; // Forked pipe reader
	pid_t forkpid;
	struct ptee_command *next;
};

int spawn_ptee_command(struct ptee_command *cmd)
{
	int p[2];
	if (pipe(p)) {
		fprintf(stderr, "ptee: can't create IPC pipe.");
		exit(EXIT_FAILURE);
	}
	cmd->pipeout = p[0];
	cmd->pipein = p[1];

	if ( (cmd->forkpid = fork()) == 0)
	{

		close(STDIN_FILENO);
		// Make STDIN point to the pipe end point
		dup2(cmd->pipeout, STDIN_FILENO);

		close(STDOUT_FILENO);

		// Try to determine the user's current shell
		char *shellenv = getenv("SHELL");
		if (shellenv == NULL) {
			shellenv = "/bin/sh";
		}
		// Argyment list for execvp, NULL terminated
		char *args[] = {shellenv,  "-c", cmd->command, NULL };

		int r = execvp(shellenv, args);
		if (r) {
			exit(EXIT_FAILURE);
		}

		exit(0);
	}
	return 0;
}


void ptee_rw(struct ptee_command *first)
{
	const size_t count = 32 * 1024;
	void *buf = malloc(count); // 32KB buffer
	for (;;) {
		ssize_t res = read(STDIN_FILENO, buf, count);
		if (res <= 0) 
			break;
		struct ptee_command *n = first;
		struct ptee_command *last = first;

		if (n == NULL) {
			// Nothing left - we'll just quit now
			break;
		}

		while(n) {
			if (write(n->pipein, buf, res) <= 0) {
				// Error condition, remove the command node
				if (n == first) {
					first = n->next;
					continue;
				}
				last->next = n->next;
				close(n->pipein);
				free(n);
				n = last->next;
				continue;
			}
			last = n;
			n = n->next;
		}
	}

	if (first) {
		struct ptee_command *n = first->next;
		while(n) {
			close(n->pipein); // close our side of the pipe
			n = n->next;
		}
	}
	free(buf);
	return;

}

int main(int argc, char **argv)
{

	// Setup the command LL with our stdout as a virtual "process"
	struct ptee_command *first = malloc(sizeof(struct ptee_command));
	first->pipeout = STDOUT_FILENO;
	first->pipein = -1;
	first->command = NULL;
	first->next = NULL;

	// TODO: Options

	struct ptee_command *last = first;
	// Parse in extra shell command lines, and fork() off their progeny
	for (int i = 1; i < argc ; i++) {
		struct ptee_command *next = malloc(sizeof(struct ptee_command));
		next->command = argv[i];
		last->next = next;
		last = next;
		spawn_ptee_command(next);
	}

	ptee_rw(first);
	exit(0);
}
