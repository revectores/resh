#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <assert.h>
#include <sys/stat.h>


char* params[100];
int cur_proc;

int batch_mode = 0;

int background_mode = 0;

int redirect_mode = 0;
char redirect_to[512];

int pipe_mode = 0;
char pipe_out_cmd[512];
int pipe_fd[2] = {0, 0};
int output_fd = STDOUT_FILENO;
int is_pipe_out;



void decrease_cur_proc(){
    cur_proc --;
    // printf("cur_proc = %d\n", cur_proc);
}


void err_msg(){
    char error_message[30] = "An error has occurred\n";
    write(STDERR_FILENO, error_message, strlen(error_message));
}

void mysh_prompt(){
    write(STDOUT_FILENO, "mysh> ", 6);
}


int is_spaces(char* input){
    char* chr_ptr = input;
    while(*chr_ptr == ' ') chr_ptr++;
    return *chr_ptr == '\0';
}


void redirect_parser(char* input){
    redirect_mode = 0;

    char* chr_ptr = strchr(input, '>');

    if (chr_ptr) {
        redirect_mode = 1;
        *chr_ptr = '\0';
        chr_ptr ++;
        while(*chr_ptr== ' ') chr_ptr++;
        strcpy(redirect_to, chr_ptr);
    }
}


void pipe_parser(char* input){
    pipe_mode = 0;

    char* chr_ptr = strchr(input, '|');

    if (chr_ptr) {
        pipe_mode = 1;
        *chr_ptr = '\0';
        chr_ptr ++;
        strcpy(pipe_out_cmd, chr_ptr);
        // printf("pipe_out_cmd=%s\n", pipe_out_cmd);
    }
}


void background_parser(char* input){
	background_mode = 0;

	char* chr_ptr = input + (strlen(input) - 1);
	if (*chr_ptr == '&') { 
		background_mode = 1;
		*chr_ptr = '\0';
	}
}


void strip_tail_spaces(char* input){
	char* chr_ptr = input + (strlen(input) - 1);
	while(*chr_ptr == ' ') chr_ptr--;
	*(chr_ptr + 1) = '\0';
}


int parser(char* input){
    /* split input into params list */
    int param_index = 0;

    char* param;

    params[param_index++] = strtok(input, " ");
    while ((param = (strtok(NULL, " ")))){
        params[param_index++] = param;
    }

    // printf("param_index = %d\n", param_index);
    params[param_index] = NULL;
    return param_index;
}


int is_cmd(char* const command){
    return strcmp(params[0], command) == 0;
}


int pwd(){
    char cwd[80];
    write(output_fd, getcwd(cwd, sizeof(cwd)), strlen(cwd));
    write(output_fd, "\n", 1);
    return 0;
}


int cd(){
    if (params[1] == NULL) params[1] = getenv("HOME");
    chdir(params[1]);
    return 0;
}


int route(){
    /* The built-in commands */
    if (is_cmd("exit")){
        exit(0);
    } else if (is_cmd("wait")){
        // printf("cur_proc = %d\n", cur_proc);
        while(cur_proc) wait(NULL);
        // printf("complete waiting\n");
        return 0;
    } else if (is_cmd("pwd")) {
        pwd();
        return 0;
    } else if (is_cmd("cd")){
        cd();
        return 0;
    }

    /* fork to execute non built-in command */
    cur_proc ++;
    int rc = fork();

    if (rc == -1){
        err_msg();
    } else if (rc > 0) {
        if (!background_mode){
            waitpid(rc, NULL, 0);
        } else {

        }
    } else if (rc == 0){
        // printf("pipe_mode=%d\n", pipe_mode);
        if (redirect_mode){
            dup2(output_fd, STDOUT_FILENO);
        }
        if (pipe_mode){
            dup2(output_fd, STDOUT_FILENO);
        }
        execvp(params[0], params);
        // write(STDERR_FILENO, "output_here", strlen("output_here"));
        err_msg();
        _exit(0);
    }

    return 0;
}


int main(int argc, const char** argv){
    signal(SIGCHLD, decrease_cur_proc);
    char input[512];

    batch_mode = argc - 1;

    if (batch_mode){
        close(STDIN_FILENO);
        int input_fd = open(argv[1], O_RDONLY);
        assert(input_fd == STDIN_FILENO);
    }

    FILE* file_ptr = stdin;
    if (!batch_mode) mysh_prompt();

    char* r;

    is_pipe_out = 0;

    while (is_pipe_out || ((r = fgets(input, 512, file_ptr)) != NULL)){
        output_fd = STDOUT_FILENO;
        // printf("pid=%d\n", getpid());
        if (batch_mode) write(STDOUT_FILENO, input, strlen(input));

        strtok(input, "\n");
        if (input[0] == '\n' || is_spaces(input)){
            mysh_prompt();
            continue;
        }

		strip_tail_spaces(input);
		background_parser(input);
        pipe_parser(input);
        redirect_parser(input);
        int param_count = parser(input);

        assert(!(pipe_mode && redirect_mode));

        if (pipe_mode) {
            pipe(pipe_fd);
            output_fd = pipe_fd[1];
            // open(pipe_fd[1], O_WRONLY | O_TRUNC);
        }

        if (!pipe_mode && redirect_mode) {
            output_fd = open(redirect_to, O_WRONLY | O_CREAT | O_TRUNC);
            fchmod(output_fd, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
        }
        
        /*
        printf("param_index = %d\n", param_count);
        int i = 0;
        while(params[i]!=NULL) puts(params[i++]);
        puts(params[i]);
        */

        route();
        
        if (pipe_mode) {
        	cur_proc++;
            int rc = fork();

            if (rc == -1) {
                err_msg();
            } else if (rc > 0) {
                close(pipe_fd[0]);
                close(pipe_fd[1]);
                waitpid(rc, NULL, 0);
            } else if (rc == 0) {
                close(pipe_fd[1]);
                strcpy(input, pipe_out_cmd);
                dup2(pipe_fd[0], STDIN_FILENO);
                is_pipe_out = 1;
                continue;
            }
        }

        if (is_pipe_out) _exit(0);
        if (!batch_mode) mysh_prompt();
    }

    fclose(file_ptr);
    return 0;
}
