// write.c
#include<stdio.h>
#include <sys/ipc.h> 
#include <sys/shm.h> 
#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdlib.h>

#define SHMEM_SIZE 1024 
#define DATA_SEQ SHMEM_SIZE - 4
#define VOTE_SEQ SHMEM_SIZE - 8
#define PARTITION 128		// data + result
#define VOTE_RESULT (128 - sizeof(int))// format: (int: # of error bytes)
// data partition
// shmem[1020]: global seq_no for data input
// shmem[1016]: global seq_no for vote result
// shmem[0-PARTITION-1]:   0-th local data (data + result)
// shmem[1*PARTITION-2*PARTITION-1]: 1-th local data (data + result)
// shmem[2*PARTITION-3*PARTITION-1]: 2-th local data (data + result)
// shmem[3*PARTITION-4*PARTITION-1]: 3-th local data (data + result)
// shmem[4*PARTITION-5*PARTITION-1]: 4-th local data (data + result)
// shmem[5*PARTITION-6*PARTITION-1]: vote result

// format of data to be voted
// struct {
//   int seq_no;
//   int size; 
//   char data[size];
// }
//
// format of data voted
//   int seq_no;
//   data[size];
//
//
#define N 1024*4
char ref_data[N];
int main(int argc, char const *argv[]) {
//    int key = ftok(".", 34);
    char *arr;
    char *data[5];
    int  *dsize[5];
    int  *result[5];
    int  result_map[5][5];
    int  vote_matrix[5];
    char * cptr;

    if (argc != 2) {
        printf("Usage: test <id>\n");
        exit(0);
    }
    for (int i = 0; i < 5; i++)
            vote_matrix[i] = 0;

    //int shmid = shmget(key,sizeof(int)*5,0666|IPC_CREAT);
    int shmid = shmget(1234,1024,0666|IPC_CREAT);

    arr = (char *)shmat(shmid, NULL, 0);
    int no_cores = atoi(argv[1]);
    int seq_no = 11;
    int no_majority = (no_cores / 2) + (no_cores % 2 == 0 ? 2 : 1);
    int * vote_seq = (int *)&arr[VOTE_SEQ];
    int * data_seq = (int *)&arr[DATA_SEQ];
    * vote_seq = seq_no-1;
    * data_seq = seq_no;
    for (int i = 0; i < no_cores; i++) {
        arr[i*PARTITION] = seq_no-1;
        data[i] = &arr[PARTITION*i+sizeof(int)*2];
        dsize[i] = (int *) &arr[PARTITION*i+sizeof(int)];
        result[i] = (int *) &arr[PARTITION*i+VOTE_RESULT];
    }
    while(1) {
        for (int i = 0; i < no_cores; i++) {
            while(*(int*)&arr[i*PARTITION] != seq_no) usleep(5);
            *result[i] = 0;
        }
#ifdef DEBUG1
        printf("voter: seq_no: %d got all from (%d) clients\n", seq_no, no_cores);
#endif
        // vote
        int data_size = *dsize[0];	// FIXIT: itself should be voted
//        printf("Voter: size(%d), data[0](%d)\n", data_size, data[2][0]);
#ifdef DEBUG
        for (int i = 0; i < no_cores; i++) {
            char * ptr = data[i];
            printf("Voter: Core %d: ", i);
            for (int j = 0; j < data_size; j++) {
                printf("(%d,%d) ", i, ptr[j]);
            }
            printf("\n");
        }
#endif
	// vote and make a reference data
        bool unrecoverable_error = false;
        for (int i = 0; i < data_size; i++) {
            int vote_count = 0;
            for (int j = 0; j < no_cores; j++) {
                for (int k = 0; k < no_cores; k++) 	// actual vote vote_matrix
                    if (data[j][i] == data[k][i]) vote_count++;
                if (vote_count >= no_majority) {
                    ref_data[i] = data[j][i];
                    break;
                } 
            }
            if (vote_count < no_majority) {
                unrecoverable_error = true;
                break;
            }
        }
#ifdef DEBUG
        printf("Ref data: ");
        for (int i = 0; i < data_size; i++) 
            printf("%d ", ref_data[i]);
        printf("\n");
#endif
	// compare and get result
        if (unrecoverable_error) {
            printf("Unrecoverable error: ");
            for (int j = 0; j < no_cores; j++) {
                (*result[j]) = -1;
            } 
        } else { // copy ref_data back
#ifdef DEBUG
            printf("Copy ref_data back\n");
#endif
            cptr = (char *) &arr[PARTITION*5];
            for (int i = 0; i < data_size; i++) {
                cptr[i] = ref_data[i];
            } 
            for (int j = 0; j < no_cores; j++) {
                (*result[j]) = 0;
                for (int i = 0; i < data_size; i++)
                    if (cptr[i] != data[j][i]) (*result[j])++;
#ifdef DEBUG
                printf("Core %d: # of errors = %d\n", *result[j]);
#endif
            }
        }
                
#ifdef DEBUG
       printf("Voter: move seq_no");
#endif
        * vote_seq = seq_no;
        seq_no++;
        * data_seq = seq_no;
        usleep(5);
    }
    shmdt((void *) arr);
    return 0;
}
