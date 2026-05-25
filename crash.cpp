#include <iostream>
#include <pthread.h>
#include "yaxmalloc.h"

void *thread_routine(void *vargp){
   for(int i = 0; i < 1000000; i++){
    void *ptr = mm_malloc(16);
    mm_free(ptr);
   }
    return NULL;
}
int main() {
    pthread_t tid[4];
    mem_init_sandbox();
    mm_init();
    for(int i = 0; i < 4; i ++){
        pthread_create(&tid[i],NULL,thread_routine,NULL);
    }
    for(int i = 0; i < 4; i ++){
        pthread_join(tid[i],NULL);
    }
    
    std::cout << "[Phase 3] System survived. Four threads verified.\n";
    return 0;
}