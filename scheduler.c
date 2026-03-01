#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
//#include <string.h>

static void validate_n_parse_args(const char**, unsigned short*);

int main(int args, char** argv) {
	unsigned short s = 0;
	//char* cmd;
	//              source ... &> /dev/null
	size_t cmd_len = 22;

	if(args == 3) {
		validate_n_parse_args((const char**)argv, &s);
		cmd_len += strlen(argv[2]);
		//if((cmd = malloc(cmd_len)) == NULL) perror("Unable to allocate memory!");
		//memset(cmd, 0, cmd_len); // to make buf has null byte for string
		char cmd[cmd_len];
		snprintf(cmd, cmd_len, "%s %s %s", "source", argv[2], "&> /dev/null");
		while(1) {
			system(cmd);
			system("echo -n \a"); // beeps per defined minute(s) (SEE README.txt for explanation!)
			sleep(s * 60);
		}
	}
	printf("Usage: %s [minutes] [script_file]\n", argv[0]);
	return 1;
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
