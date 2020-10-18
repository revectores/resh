# <center>Interpretation of Pipe Implementation</center>

<center>Lingyang Zeng, 10185102236</center>

<center>School of Computer Science and Technology, East China Normal University</center>



##### # Introduction

As definition, the pipe use the output of one process as the input of another process. This can be achieved by the redirection of input and output.

Three key functions should be added to support pipe:

- Pipe Parser, which devides the input that contains pipe into two commands.
- Chain Execution. For the non-pipe command, only one shell process and one child process that execute command are required, while for the pipe chain, two or even multiple commands(that is, processes) should be executed sequentially.
- I/O Redirection. Connect the input and output of two process by applying system call `pipe()`.

To support input with multiple pipes like

```shell
ls | cat | wc -l
```

that is, a "pipe chain", the core idea of the implementation is that: only parse the first pipe into two parts, and consider the latter one (might include more pipes) as a normal input from user and processed by a forked shell, that is, we create a "child shell", by which the pipes will be parsed sequentially without recognizing the existence of multiple pipes explicitly, and far more naturally, of course.







##### # Pipe Parser

Before dividing input into seperate parameters, we search the pipe symbol `|` in command and split the input into two parts, that part behinds the first pipe symbol is assigned to `pipe_out_cmd`, which will be used as the `input` of next command:

```c
void pipe_parser(char* input){
    pipe_mode = 0;
    char* chr_ptr = strchr(input, '|');
    if (chr_ptr) {
        pipe_mode = 1;
        *chr_ptr = '\0';
        chr_ptr ++;
        strcpy(pipe_out_cmd, chr_ptr);
    }
}
```



##### # Chain Execution

In `pipe_mode`(that is, the input is recognized as including pipe symbol), we execute `fork()` inside the main loop of command processing:

```c
if (pipe_mode) {
    pipe(pipe_fd);
    output_fd = pipe_fd[1];
}

route();

if (pipe_mode) {
    int rc = fork();

    if (rc == -1) {
        err_msg();
    } else if (rc > 0) {
        close(pipe_fd[0]);
        close(pipe_fd[1]);
        wait(NULL);
    } else if (rc == 0) {
        close(pipe_fd[1]);
        strcpy(input, pipe_out_cmd);
        dup2(pipe_fd[0], STDIN_FILENO);
        is_pipe_out = 1;
        continue;
    }
}

if (is_pipe_out) _exit(0);
```

Since the file writing has been completed in the command execution function `route()`(which will be disscussed later in I/O redirection), the parent process shall close both the descriptors, especially the write descriptor `pipe_fd[1]`, otherwise the end of pipe will not finish reading. Then we `wait(NULL)` until entire pipe finished execution.

For the child process we close the write descriptor `pipe_fd[1]` as well in case of writing incorrectly. The split out `pipe_out_cmd` will be used as its input, and will be parse again as if it is received from the normal input from user.

After preprocessing in child process has completed, it should jump to start of main loop to parse and execute the new input. We create a flag variable `is_pipe_out` to represent that the new input comes from the end of pipe, which is used in two places:

- Short-circuit the `fgets` in main loop.

    ```c
    while (is_pipe_out || ((r = fgets(input, 512, file_ptr)) != NULL))
    ```

- Exit when the command is finished.

    ```c
    if (is_pipe_out) _exit(0);
    ```



##### # I/O Redirection

We should redirect the output of the pipe head and the input of pipe end. We should take care of this in three places:

1. [Output] Before the command execution. If `pipe_mode` is recognized, a pipe should be created by invoking `pipe()` system call. The file descriptor variable `output_fd`, which is used for output of command execution(which set to `STDOUT_FILENO` by default), should be assigned as the pipe writer.

    ```c
    int pipe_fd[2] = {0, 0};
    
    if (pipe_mode) {
        pipe(pipe_fd);
        output_fd = pipe_fd[1];
    }
    ```

2. [Output] Before child process fork and exec. Different from (1), it is impossible to control the output file descriptor of `exec` program. Hence the only approach is to redirect the `STDOUT_FILENO` into that file by duplicating the `output_fd`.

    ```c
    } else if (rc == 0){
        if (redirect_mode){
            dup2(output_fd, STDOUT_FILENO);
        }
        if (pipe_mode){
            dup2(output_fd, STDOUT_FILENO);
        }
        execvp(params[0], params);
        err_msg();
        _exit(0);
    }
    ```

3. [Input] We redirect `STDIN_FILENO` into `pipe_fd[0]`, which also make sure those child processes that `exec()` to other programs read from pipe. Since there is no extra standard input is required for pipe tail, this can be done as soon as the    creation of child shell.

    ```c
    } else if (rc == 0) {
        close(pipe_fd[1]);
        strcpy(input, pipe_out_cmd);
        dup2(pipe_fd[0], STDIN_FILENO);
        is_pipe_out = 1;
    }
    ```

