#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>


char* params[100];

int cur_proc;

int batch_mode = 0;
int background_mode = 0;
int redirection_mode = 0;
int pipe_mode = 0;
char redirect_to[512];


void decrease_cur_proc(){
    cur_proc --;
}


void err_msg(){
    char error_message[30] = "An error has occurred\n";
    write(STDERR_FILENO, error_message, strlen(error_message));
    // fflush(NULL);
}


void redirect_parser(char* input){
    redirection_mode = 0;

    char* chr_ptr = strchr(input, '>');

    if (chr_ptr) {
        redirection_mode = 1;
        *chr_ptr = '\0';
        chr_ptr ++;
        while(*chr_ptr== ' ') chr_ptr++;
        strcpy(redirect_to, chr_ptr);
    }
}


/*
void pipe_parser(char* input){
    pipe_mode = 0;

    char* chr_ptr = strchr(input, '|');

    if (chr_ptr) {
        pipe_mode = 1;
        *chr_ptr = '\0';
        char_ptr ++;

    }
}
*/


int parser(char* input){
    /* split input into params list */
    int param_index = 0;
    background_mode = 0;

    redirect_parser(input);

    char* param;
    params[param_index++] = strtok(input, " ");
    while ((param = (strtok(NULL, " ")))){
        params[param_index++] = param;
    }

    if (param_index >= 2 && strcmp(params[param_index-1], "&") == 0){
        background_mode = 1;
        
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
    int fd = STDOUT_FILENO;

    if (redirection_mode){
        fd = open(redirect_to, O_WRONLY | O_CREAT | O_TRUNC);
        fchmod(fd, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    }

    write(fd, getcwd(cwd, sizeof(cwd)), strlen(cwd));
    write(fd, "\n", 1);
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
        while(cur_proc);
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
        perror("fork error");
    } else if (rc > 0) {
        if (!background_mode){
            wait(NULL);
        } else {

        }
    } else if (rc == 0){
        int fd = -1;
        if (redirection_mode){
            close(STDOUT_FILENO);
            fd = open(redirect_to, O_WRONLY | O_CREAT | O_TRUNC);
            fchmod(fd, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
        }

        execvp(params[0], params);
        err_msg();
        _exit(0);
    }

    return 0;
}


int main(int argc, const char** argv){
    signal(SIGCHLD, decrease_cur_proc);
    char input[512];

    batch_mode = argc - 1;
    FILE* file_ptr = (batch_mode ? fopen(argv[1], "r") : stdin);

    if (!batch_mode) write(STDOUT_FILENO, "mysh> ", 6);

    char* r;
    while ((r = fgets(input, 512, file_ptr)) != NULL){
        if (batch_mode) write(STDOUT_FILENO, input, strlen(input));

        strtok(input, "\n");
        int param_count = parser(input);

        /*int i = 0;
        while(params[i]!=NULL) puts(params[i++]);
        puts(params[i]);*/

        route();
        if (!batch_mode) write(STDOUT_FILENO, "mysh> ", 6);
        fflush(NULL);
    }

    fclose(file_ptr);
    return 0;
}
