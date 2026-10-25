#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <sys/wait.h>

#include <string.h>
#include <ctype.h>

#include <fcntl.h>

int split_command(char *line, char *comv[]);
char *expand_one_argument(const char *arg);
void expand_variables(int comc, char *comv[]);

struct BackgroundProcess {
    pid_t pid;
    char command[100];
};

struct BackgroundList {
    struct BackgroundProcess jobs[100];
    int count;
};

struct ShellState {
    struct BackgroundList bg_list;
};

struct Builtin {
    char *name;
    void (*funct)(int, char *[], struct ShellState*);
};


void builtin_cd(int comc, char *comv[], struct ShellState *state);
void builtin_pwd(int comc, char *comv[], struct ShellState *state);
void builtin_echo(int comc, char *comv[], struct ShellState *state);
void builtin_export(int comc, char *comv[], struct ShellState *state);
void builtin_sbg(int comc, char *comv[], struct ShellState *state);
void builtin_history(int comc, char *comv[], struct ShellState *state);

void bg_add(struct BackgroundList *list, pid_t pid, const char *command);
void bg_remove(struct BackgroundList *list, pid_t pid);


struct Builtin builtin_table[] = {
    {"cd", builtin_cd},
    {"pwd", builtin_pwd},
    {"echo", builtin_echo},
    {"export", builtin_export},
    {"sbg", builtin_sbg},
    {"history", builtin_history},
};


int main(){
    char *USER = getenv("USER");
    char *HOME = getenv("HOME");
    char HOST[100];
    char CWD[100];
    char display[100];
    gethostname(HOST, sizeof(HOST));
    char prompt[300];

    char *comv[100];
    int comc = 0;

    int fdin = 0;
    struct ShellState state = {0};

    using_history();

    while(1){
        getcwd(CWD, sizeof(CWD));
        fdin = 0;

        pid_t finished_pid;
        while ((finished_pid = waitpid(-1, NULL, WNOHANG)) > 0) {
            bg_remove(&state.bg_list, finished_pid);
        }

        if (strncmp(CWD, HOME, strlen(HOME)) == 0){
            sprintf(display, "~%s", CWD + strlen(HOME));
        } else {
            sprintf(display, "%s", CWD);
        }
        sprintf(prompt, "%s@%s:%s $ ", USER, HOST, display);
        char *line = readline(prompt);

        if (line == NULL){
            break;
        } else {
            if (line[0] != '\0') {
                add_history(line);
            } else {
                free(line);
                continue;
            }
            if (strcmp(line, "exit") == 0){
                free(line);
                break;
            } else if (strcmp(line, "") == 0){
                free(line);
            } else {
                comc = split_command(line, comv);
                int stop = (strcmp(comv[comc-1], "&") == 0) ? 1 : 0;

                int redirect = 0;
                int redirect_index = 0;
                for(int i=0;i<comc;i++){
                    if(strcmp(comv[i], ">") == 0){
                        redirect = 1;
                        redirect_index = i;
                        break;
                    }
                }
                char *rdfile;
                if (stop) {
                    comv[comc-1] = NULL;
                    comc--;
                }
                if (redirect) {
                    rdfile = comv[redirect_index + 1];
                    for(int i=redirect_index;i + 2 <= comc;i++){
                        comv[i] = comv[i+2];
                    }
                    comc-=2;
                }
                expand_variables(comc, comv);

                for(int i=0;i<6;i++){
                    if(strcmp(comv[0], builtin_table[i].name) == 0){
                        builtin_table[i].funct(comc, comv, &state);
                        fdin = 1;
                        break;
                    }
                }
                
                if (fdin == 0){
                    int x = fork();
                    if (x == 0){
                        if (redirect) {
                            int fd = open(rdfile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                            if (fd == -1) {
                                perror("open");
                                exit(1);
                            }
                            dup2(fd, STDOUT_FILENO);
                            close(fd);
                        }
                        execvp(comv[0], comv);
                        printf("execvp: No Such File or Directory\n");
                        return 0;
                    }
                    if (!stop) {
                        waitpid(x, NULL, 0);
                    } else {
                        printf("[Process running in background with PID %d]\n", x);
                        bg_add(&state.bg_list, x, comv[0]);
                    }
                }
                for(int i=0;i<comc;i++){
                    free(comv[i]);
                }
                free(line);
            }
        }
    }

    return 0;
}

int split_command(char *line, char *comv[]){
    int comc = 0;
    int read = 0;
    int write = 0;
    int in_quote = 0;
    int token_started = 0;

    while(line[read] != '\0'){
        if (line[read] == '\"'){
            if(token_started == 0){
                comv[comc++] = &line[write];
                token_started = 1;
            }
            in_quote = (in_quote == 1) ? 0 : 1;
            read++;
        } else if (line[read] == ' ' && in_quote == 0){
            if (token_started == 1){
                line[write++] = '\0';
                token_started = 0;
            }
            read++;
        } else {
            if (token_started == 0){
                comv[comc++] = &line[write]; //set : 剛偵測到字元
                token_started = 1;
            }
            line[write] = line[read];
            write++;
            read++;
        }
    }
    if (token_started == 1){
        line[write] = '\0';
        token_started = 0;
    }
    comv[comc] = NULL;
    return comc;
}

char *expand_one_argument(const char *arg){
    int read = 0;
    int write = 0;
    int length = 0;

    char *home = getenv("HOME");

    // 第一遍：計算展開後需要多少空間
    while(arg[read] != '\0'){

        if(read == 0 && arg[read] == '~'){
            if(home != NULL){
                length += strlen(home);
            }
            read++;
        } else if(arg[read] == '$' &&
                (isalnum((unsigned char)arg[read + 1]) ||
                 arg[read + 1] == '_')){

            int start = read + 1;
            int end = start;

            while(isalnum((unsigned char)arg[end]) ||
                  arg[end] == '_'){
                end++;
            }

            int name_len = end - start;

            char *name = malloc(name_len + 1);

            if(name == NULL){
                return NULL;
            }

            memcpy(name, &arg[start], name_len);
            name[name_len] = '\0';

            char *value = getenv(name);

            if(value != NULL){
                length += strlen(value);
            }

            free(name);

            read = end;
        } else{
            length++;
            read++;
        }
    }

    // +1 留給 '\0'
    char *result = malloc(length + 1);

    if(result == NULL){
        return NULL;
    }


    // 第二遍：真正產生展開後的字串
    read = 0;

    while(arg[read] != '\0'){

        if(read == 0 && arg[read] == '~'){
            if(home != NULL){
                for(int j = 0; home[j] != '\0'; j++){
                    result[write++] = home[j];
                }
            }

            read++;
        } else if(arg[read] == '$' &&
                (isalnum((unsigned char)arg[read + 1]) ||
                 arg[read + 1] == '_')){

            int start = read + 1;
            int end = start;

            while(isalnum((unsigned char)arg[end]) ||
                  arg[end] == '_'){
                end++;
            }

            int name_len = end - start;

            char *name = malloc(name_len + 1);

            if(name == NULL){
                free(result);
                return NULL;
            }

            memcpy(name, &arg[start], name_len);
            name[name_len] = '\0';

            char *value = getenv(name);

            if(value != NULL){
                for(int j = 0; value[j] != '\0'; j++){
                    result[write++] = value[j];
                }
            }

            free(name);

            read = end;
        } else{
            result[write++] = arg[read++];
        }
    }

    result[write] = '\0';

    return result;
}

void expand_variables(int comc, char *comv[]){
    for(int i = 0; i < comc; i++){
        char *expanded = expand_one_argument(comv[i]);

        if(expanded == NULL){
            // 前面已成功配置的先釋放
            for(int j = 0; j < i; j++){
                free(comv[j]);
            }
        }

        comv[i] = expanded;
    }
}


void builtin_cd(int comc, char *comv[], struct ShellState *state){
    chdir(comv[1]);
}

void builtin_pwd(int comc, char *comv[],struct ShellState *state){
    char cwd[100];
    printf("%s\n", getcwd(cwd, sizeof(cwd)));
}

void builtin_echo(int comc, char *comv[],struct ShellState *state){
    for(int i=1;i<comc;i++){
        if(comv[i] != NULL){
            printf("%s ", comv[i]);
        }
    }
    printf("\n");
}

void builtin_export(int comc, char *comv[], struct ShellState *state){
    char *equal = strchr(comv[1], '=');
    
    *equal = '\0';
    char *name = comv[1];
    char *content = equal + 1;

    setenv(name, content, 1);
}

void builtin_sbg(int comc, char *comv[], struct ShellState *state) {
    if (state->bg_list.count != 0){
        printf("Background Processes :\n");
        for(int i=0;i<state->bg_list.count;i++){
            printf("[%d] PID: %d, Command: %s\n", i+1, state->bg_list.jobs[i].pid, state->bg_list.jobs[i].command);
        }
    } else {
        printf("No background process running.\n");
    }
}

void builtin_history(int comc, char *comv[], struct ShellState *state){
    HIST_ENTRY **history = history_list();

    if(history == NULL){
        return;
    }

    for(int i = 0; history[i] != NULL; i++){
        printf("%d:  %s\n", i + 1, history[i]->line);
    }
}

void bg_add(struct BackgroundList *list, pid_t pid, const char *command) {
    if (list->count < 99) {
        sprintf(list->jobs[list->count].command, "%s", command);
        list->jobs[list->count].pid = pid;
        list->count++;
    }
}

void bg_remove(struct BackgroundList *list, pid_t pid) {
    int fd = 0;
    int index = 0;
    for (int i=0;i<list->count;i++){
        if (pid == list->jobs[i].pid)  {
            index = i;
            fd = 1;
            break;
        }
    }
    if (fd) {
        for (int i=index;i<list->count;i++){
            list->jobs[i] = list->jobs[i+1];
        }
        list->count--;
    } else {
        fprintf(stderr, "bg_remove failed : PID %d is not found!\n", pid);
    }
}

