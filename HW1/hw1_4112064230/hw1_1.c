#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

int MIN_PID = 500;
int MAX_PID = 999;

bool *pid_map;

int allocate_map(){
    pid_map = calloc((MAX_PID - MIN_PID + 1), sizeof(bool));
    if (pid_map == NULL){
        return -1;
    }
    return 1;
}

int allocate_pid(){
    for(int i=0;i<(MAX_PID - MIN_PID + 1);i++){
        if(pid_map[i] == 0){
            pid_map[i] = 1;
            return (MIN_PID + i);
        }
    }
    return -1;
}

void release_pid(int pid){
    if (pid > MAX_PID || pid <MIN_PID){
        perror("pid error");
        exit(1);
    }
    pid_map[pid - MIN_PID] = 0;
    printf("Release PID %d\n", pid);
}

int main(){

    if(allocate_map() == -1){
        perror("allocate_map fail");
        exit(1);
    }

    printf("Allocate: ");
    for(int i=0;i<4;i++){
        int pid = allocate_pid();
        if(pid == -1){
            printf("All pid is used");
        } else {
            printf("%d ", pid);
        }
    }
    printf("\n");

    release_pid(501);

    int pid = allocate_pid();
    if(pid == -1){
        printf("All pid is used");
    } else {
        printf("Allocate again: %d\n", pid);
    }

    free(pid_map);
    return 0;
}