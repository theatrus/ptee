/*
Copyright (c) 2009 Yann Ramin <atrus@stackworks.net>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/wait.h>
#include <signal.h>


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
		perror("ptee");
		exit(EXIT_FAILURE);
	}
	cmd->pipeout = p[0];
	cmd->pipein = p[1];

	if ( (cmd->forkpid = fork()) == 0)
	{
		close(cmd->pipein);
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
	} else {
		close(cmd->pipeout);
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

void child_wait(int signal, siginfo_t *info, void*v)
{
	int status;
	waitpid(info->si_pid, &status, 0);
}

int main(int argc, char **argv)
{
	struct sigaction act;
	act.sa_sigaction = child_wait;
	act.sa_flags = SA_SIGINFO | SA_NOCLDSTOP | SA_RESTART;
	sigaction(SIGCHLD, &act, NULL);
	struct sigaction ignore;
	ignore.sa_handler = SIG_IGN;
	sigaction(SIGPIPE, &ignore, NULL);

	// Setup the command LL with our stdout as a virtual "process"
	struct ptee_command *first = malloc(sizeof(struct ptee_command));
	first->pipein = STDOUT_FILENO;
	first->pipeout = -1;
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
