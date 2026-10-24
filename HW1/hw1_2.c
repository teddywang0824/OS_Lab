#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> // fork
#include <sys/wait.h>

#include <sys/mman.h>
#include <fcntl.h>

int main(){
    int m,n,p;
    printf("Please enter the size of matrices. A: m*n, B:nxp\n");
    scanf("%d %d %d", &m, &n, &p);
    

    // 向 OS 建立一個叫某個名字的 shared memory object
    int shm_fd;
    shm_fd = shm_open("/answer", O_CREAT|O_RDWR,  0666);

    if (shm_fd == -1){
        perror("shm_open failed");
        exit(1);
    }

    // 告訴 OS 要多少 bytes
    size_t size = m*p*sizeof(int);
    if (ftruncate(shm_fd, size) == -1){
        perror("ftruncate error");
        exit(1);
    }

    // 將這個空間的位址 mapping 進process的虛擬記憶體空間
    int *C;
    C = mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (C == MAP_FAILED) {
        perror("mmap failed");
        exit(1);
    } else {
        printf("Memory attached at %p\n", (void *)C);
    }

    int A[m][n];
    printf("Enter elements of matricx A:\n");
    for(int i=0;i<m;i++){
        for(int j=0;j<n;j++){
            int temp=0;
            scanf("%d", &temp);
            A[i][j] = temp;
        }
    }
    printf("Enter elements of matricx B:\n");
    int B[n][p];
    for(int i=0;i<n;i++){
        for(int j=0;j<p;j++){
            int temp=0;
            scanf("%d", &temp);
            B[i][j] = temp;
        }
    }   

    pid_t x;
    for(int i=0;i<m;i++){
        x = fork();
        if (x == 0){
            for(int j=0;j<p;j++){
                int sum = 0;
                for(int k=0;k<n;k++){
                    sum += A[i][k] * B[k][j];
                }
                C[i*p + j] = sum;
            }
            exit(0);
        } else if (x < 0){
            perror("fork failed");
            exit(1);
        }
    }

    for(int i=0;i<m;i++){
        wait(NULL);
    }
    printf("Result of MatrixA * MatrixB:\n");
    for(int i=0;i<m;i++){
        for(int j=0;j<p;j++){
            printf("%d ",C[i*p + j]);
        }
        printf("\n");
    }

    if(munmap((void *)C, size) == -1){
        perror("munmap failed");
        exit(1);
    }

    if (shm_unlink("/answer") == -1){
        perror("shm_unlink failed");
        exit(1);
    }

    return 0;
}