//read.c 
#include<stdio.h>
#include <sys/ipc.h> 
#include <sys/shm.h> 
#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>

#define PARTITION 128
#define SHMEM_SIZE 1024 
#define DATA_SEQ SHMEM_SIZE - 4
#define VOTE_SEQ SHMEM_SIZE - 8
#define VOTE_RESULT (PARTITION - sizeof(int)) // format: (int: # of error bytes)

_Thread_local static char * arr;
_Thread_local static int shmid;

extern void __ft_vote(void * , const int32_t );
extern void __ft_init(int );
extern void __ft_exit(int );
extern void __ft_dummy(void *, int32_t, int32_t);

void (*__ft_voter)() = __ft_vote;
void (*__ft_votel)() = __ft_vote;
void (*__ft_votenow)() = __ft_vote;
void (*__ft_auto_votel)() = __ft_vote;
void (*__ft_auto_voter)() = __ft_vote;
void (*__ft_auto_votenow)() = __ft_vote;

void (*ft_init)() = __ft_init;
void (*ft_exit)() = __ft_exit;

void __ft_auto_start(void * varaddr, const int32_t varsize) {
};
void __ft_auto_end(void * varaddr, const int32_t varsize) {
};
void __ft_dummy(void * varaddr, int32_t v1, int32_t v2) {
};

static int vote_init() {
	int key = ftok(".", 34);
	
	shmid = shmget(1234,1024,0666/*|IPC_EXCL*/);

	arr = shmat(shmid, NULL, 0);
#if 0
	int i;
	for(i=0; i<5; i++) 
        printf("%d\n", arr[i] );

        while(1) ;
    shmdt((void *) arr);
    shmctl(shmid, IPC_RMID, NULL);
	return 0;
#endif
	return 0;
}

_Thread_local static int nmr_id;
_Thread_local static int seq_no;
void __ft_init(int id) {
	nmr_id = id;
	vote_init();
	seq_no = arr[VOTE_SEQ]+1;
}

void __ft_exit(int id) {
    shmdt((void *) arr);
    shmctl(shmid, IPC_RMID, NULL);
//    return 0;
}

static inline void __ft_copy(void * varaddr, int size) {
    char * data = varaddr;
    // seq_no, size, data
    for (int i = 0; i < size; i++)
        arr[PARTITION*nmr_id + 2*sizeof(int) + i] = data[i];
//    printf("core(%d), data[0] = %d\n", nmr_id, data[0]);
    * (int *) &arr[PARTITION*nmr_id + sizeof(int)] = size;
    * (int *) & arr[PARTITION*nmr_id] = seq_no;
}

void __ft_vote(void * varaddr, const int32_t varsize) {
   void * tptr;
   tptr = (void *) varaddr;

   int * data_seq = (int *)&arr[DATA_SEQ];
   while (* data_seq != seq_no) {
      usleep(50);
   }
   // data write
   __ft_copy(tptr, varsize);

   usleep(50);

   // result read
//   printf("vote: arr[VOTE_SEQ] = %d, seq_no = %d\n", arr[VOTE_SEQ], seq_no);
   int * vote_seq = (int *)&arr[VOTE_SEQ];
   while (* vote_seq != seq_no) {
      usleep(50);
   }
   int no_errors = * (int *)&arr[nmr_id * PARTITION + VOTE_RESULT];
   if (no_errors > 0) {
      printf("Core %d: %d errors in %d bytes, Recovered\n", nmr_id, no_errors, varsize);
      char * resbuf = &arr[5*PARTITION];
      char * dataptr = (char *)tptr;
      for (int i = 0; i < varsize; i++) {
         dataptr[i] = arr[5 * PARTITION + i];
      }
   } else if (no_errors < 0) {
      printf("Core %d: unrecoverable error. Exit\n", nmr_id);
      exit(0);
   } else {
//      printf("%d: no errors\n", nmr_id);
   }
   seq_no++;
   
//   printf("vote: %s, varaddr (%p), size(%d), indirection_depth(%d)\n", strptr, tptr, varsize, ptr_depth);
}
