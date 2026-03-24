#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

#include <syslog.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>

#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/timerfd.h>
#include <sys/wait.h>

#define SCHED_BASH_FILE "/data/data/com.termux/files/usr/bin/bash"
#define SCHED_ECHO_FILE "/data/data/com.termux/files/usr/bin/echo"
#define NULL_DEV_FILE "/dev/null"

static volatile sig_atomic_t keep_running = 1;
static volatile sig_atomic_t restart = 0;

static int   _exec(char *, char **, int *);
static void  validate_n_parse_args(const char **, unsigned short *);
static int   create_daemon(void);
static void  run_exec(char **, char **);
static void  handler(int);
static int   set_signal_handler(void);
static void  time_wait(struct itimerspec *, int, ssize_t *);
static void  reset_signal_action(struct sigaction *);
static int   attach_stdio_to_null(void);
static int   _pipe2(int[2]);

static const char* PROG_NAME = "scheduler";
static char sf_path[PATH_MAX];

int main(int args, char **argv) {
	unsigned short s = 0;
	ssize_t dum_buf;

	if(args == 3) {
		validate_n_parse_args((const char**)argv, &s);

		char *bash_args[] = { "bash", argv[2], NULL };
		char *echo_args[] = { "echo", "-ne", "\a", NULL };

		getcwd(sf_path, sizeof(sf_path)); // this is for the script file since we chdir
		strcat(sf_path, "/");

		if(create_daemon() < 0) {
			fprintf(stderr, "Failure in creating daemon: %s\n", strerror(errno));
			return EXIT_FAILURE;
		}
		/*
		 * Hoping it would contribute.
		 */
		if(setpriority(PRIO_PROCESS, 0, -20) < 0) syslog(LOG_WARNING, "%i: Unable to set priority to -10.", errno);
		if(set_signal_handler() < 0) {
			syslog(LOG_ERR, "Failed to set signal handler!");
			closelog();
			return EXIT_FAILURE;
		}
		if(chdir(sf_path) < 0) syslog(LOG_WARNING, "%s: Unable to set current working directory!", sf_path);

		int tfd = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC);
		struct itimerspec ts = {
			.it_value.tv_sec = s * 60,
			.it_value.tv_nsec = 0,
			.it_interval.tv_sec = 0,
			.it_interval.tv_nsec = 0
		};

		while(keep_running) {
			if(restart) {
				syslog(LOG_INFO, "Restarted! Re-executing script file...");
				restart = 0;

				// main work after restart
				run_exec(bash_args, echo_args);
			}
			else {
				// main work
				run_exec(bash_args, echo_args);
			}
			time_wait(&ts, tfd, &dum_buf);
		} // while loop
		syslog(LOG_INFO, "Daemon terminated!");
		closelog();
		return EXIT_SUCCESS;
	}
	printf("Usage: %s [minutes] [script_file]\n", argv[0]);
	return EXIT_FAILURE;
}

static int _pipe2(int pfd[2]) {
	if(pipe(pfd) == 0) {
		fcntl(pfd[0], F_SETFD, FD_CLOEXEC);
		fcntl(pfd[1], F_SETFD, FD_CLOEXEC);
		fcntl(pfd[0], F_SETFL, O_NONBLOCK);
		fcntl(pfd[1], F_SETFL, O_NONBLOCK);
		return 0;
	}
	return -1;
}

static int _exec(char *file, char **args, int *status) {
	int pipefd[2];
	int execv_ret_p = 0;
	pid_t pid;

	// qhy pipe i dunno i just wanted to address execv()'s error then
	// skipped interval.
	if(_pipe2(pipefd) < 0) {
		syslog(LOG_ERR, "_exec(): %i: Unable to create pipe!", errno);
		return 1;
	}
	pid = fork(); // to make sure pipe is created and dup by forking
	// only child steps here
	if(pid == 0 && execv(file, args) == -1) {
		int execv_ret = 1;
		// leave it as it is even tho it has 0_CLOEXEC flags
		close(pipefd[0]); // child doesnt receive
		write(pipefd[1], &execv_ret, sizeof(execv_ret)); // send ret stat to calling parent
		close(pipefd[1]); // close aftwr use
		syslog(LOG_ERR, "_exec(): execv(): %i: Problem occurred!", errno);
		_exit(EXIT_FAILURE); // exit child if exec fails
	}
	// end of child
	if(pid < 0) {
		syslog(LOG_ERR, "_exec(): fork(): %i: Fork failure!", errno);
		close(pipefd[0]);
		close(pipefd[1]);
		return -1;
	}
	if(pid > 0) {
		close(pipefd[1]); // parent doesnt write
		waitpid(pid, status, 0);
		if(read(pipefd[0], &execv_ret_p, sizeof(execv_ret_p)) == -1) {
			execv_ret_p = 1;
			syslog(LOG_WARNING, "%i: Unexpected error!", errno);
		}
		close(pipefd[0]);
	}
	return execv_ret_p;
}

static void time_wait(struct itimerspec *spec, int tfd, ssize_t *dum_buf) {
	timerfd_settime(tfd, 0, spec, NULL);
	read(tfd, dum_buf, sizeof(dum_buf));
}

static void run_exec(char **pb, char **pe) {
	syslog(LOG_INFO, "Executing script file...");

	int status;
	int r = _exec(SCHED_BASH_FILE, pb, &status);

	switch(r) {
		case -1:
			// fork() error
			goto log_skip_msg;
		case  0:
			// handle child's exit status
			if(WIFEXITED(status)) {
				unsigned int s = WEXITSTATUS(status);

				syslog(LOG_INFO, "bash: Exited with a status code (%i)!", s);
				if(s > 0) {
					syslog(LOG_ERR, "bash: Unable to execute file %s! Check whether it exist or not!", pb[1]);
					goto log_skip_msg;
				}
			}
			else if(WIFSIGNALED(status)) {
				syslog(LOG_WARNING, "bash: Terminated unexpectedly with signal %i.", WTERMSIG(status));
				goto log_skip_msg;
			}

			// we will not handle echo's exit code since why tho?
			r = _exec(SCHED_ECHO_FILE, pe, &status);

			if(r < 0) goto log_skip_msg;
			if(r > 0) syslog(LOG_WARNING, "Could not ring a bell!");
			goto log_executed_msg;
		case  1:
			// execv() error
			goto log_skip_msg;
	}

	log_skip_msg:
	syslog(LOG_INFO, "Skipped.");
	return;

	log_executed_msg:
	syslog(LOG_INFO, "Script executed!");
}

static void reset_signal_action(struct sigaction *sa) {
	int n = 0;

	for(int i = 1; i < SIGRTMAX; i++) {
		if(i != SIGKILL && i != SIGSTOP && sigaction(i, sa, NULL) < 0) {
			syslog(LOG_WARNING, "Failure signal reset on SIGNAL(%i)!", i);
			n = 1;
		}
	}
	if(n) {
		syslog(LOG_INFO, "Terminating.");
		_exit(EXIT_FAILURE);
	}
}

static int set_signal_handler(void) {
	struct sigaction sa;

	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = SIG_DFL;
	sa.sa_flags = 0;
	sigemptyset(&sa.sa_mask);
	reset_signal_action(&sa);
	sa.sa_handler = handler;

	if(sigaction(SIGTERM, &sa, NULL) < 0 ||
	   sigaction(SIGINT , &sa, NULL) < 0 ||
	   sigaction(SIGHUP, &sa, NULL) < 0) return -1;
	return 0;
}

static void handler(int sig) {
	switch(sig) {
		case SIGTERM:
		case SIGINT:
			syslog(LOG_INFO, "Received process termination signal (%i). Terminating process...", sig);
			keep_running = 0;
		break;
		case SIGHUP:
			syslog(LOG_INFO, "Restarting...");
			restart = 1;
		break;
	}
}

static int create_daemon(/*int* fd_copy*/void) {
	pid_t pid = fork();

	if(pid < 0) return -1;
	if(pid > 0) _exit(0);  //parent process exit imm
	if(setsid() < 0) return -1;

	pid = fork(); // prevent reacquiring a tty

	if(pid < 0) return -1;
	if(pid > 0) _exit(0);

	umask(0);

	struct rlimit rl;

	if(getrlimit(RLIMIT_NOFILE, &rl) < 0) return -1;
	for(int i = 3; i < rl.rlim_cur; i++) close(i);

	if(attach_stdio_to_null() < 0) return -1;

	openlog(PROG_NAME, LOG_PID | LOG_NDELAY, LOG_DAEMON);
	syslog(LOG_INFO, "Daemon successfully created!");

	return 0;
}

static int attach_stdio_to_null(void) {
	int nullfd_rd = open(NULL_DEV_FILE, O_RDONLY);
	int nullfd_wr = open(NULL_DEV_FILE, O_WRONLY);

	if(nullfd_rd == -1 || nullfd_wr == -1 ||
	   dup2(nullfd_rd, STDIN_FILENO) == -1 ||
	   dup2(nullfd_wr, STDERR_FILENO) == -1) return -1;
	return 0;
}

static void validate_n_parse_args(const char** argv, unsigned short* seconds) {
	size_t i = 0;

	// validate for minute arg
	while(argv[1][i] != '\0') {
		if(!(argv[1][i] >= '0' && argv[1][i] <= '9')) {
			puts("Arg [minute] should be characters between (0 - 9)!");
			exit(1);
		}
		// convert char 0 - 9 as short int
		*seconds = *seconds * 10 + (argv[1][i] - '0');
		i++;
	}
	if(!(*seconds >= 1 && *seconds <= 1440)) {
		puts("Arg [minute] only supports 1 - 1440 minutes!");
		exit(1);
	}
	// i = 0;
	// vwlidate for script_file (checks if exist as well as
	// permission for executable!
	if(access(argv[2], F_OK) == -1) {
		puts("Arg [script_file] doesn't exist! Make sure you entered the right path or should exists!");
		exit(1);
	}
	if(access(argv[2], X_OK) == -1) {
		puts("Arg [script_file] doesn't have executable permission!");
		exit(1);
	}
}
