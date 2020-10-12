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
char* redirect_to;


void decrease_cur_proc(){
    cur_proc --;
}


void err_msg(){
    char error_message[30] = "An error has occurred\n";
    write(STDERR_FILENO, error_message, strlen(error_message));
    // fflush(NULL);
}


int add_space(char raw_input[]){
    char input[512];
    int raw_cur = 0;
    int cur = 0;
    char c;

    while ((c = raw_input[raw_cur++])){
        if (c == '>') {
            input[cur++] = ' ';
            input[cur++] = c;
            input[cur++] = ' ';
        } else {
            input[cur++] = c;
        }
    }

    strcpy(raw_input, input);
    return 0;
}


int parser(char* input){
    /* split input into params list */
    int param_index = 0;
    background_mode = 0;
    redirection_mode = 0;

    char* param;
    params[param_index++] = strtok(input, " ");
    while ((param = (strtok(NULL, " ")))){
        params[param_index++] = param;
    }

    if (param_index >= 2 && strcmp(params[param_index-1], "&") == 0){
        background_mode = 1;
        
    }

    if (param_index >= 3 && strcmp(params[param_index-2], ">") == 0){
        redirection_mode = 1;
        redirect_to = params[param_index-1];
        param_index -= 2;
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
        // printf("%s\n", input);
        int err = add_space(input);
        // printf("%s\n", input);
        // continue;

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
