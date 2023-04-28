#include <stddef.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>
#include <semaphore.h>
#include <fcntl.h>
#include <stdarg.h>
#include <sys/wait.h>
#include <time.h>

#define EXIT_SUCCESS 0


#define CLERK_S "clerk_semaphore"
#define CLERK_DONE_S "clerk_done_semaphore"
#define CUSTOMER_DONE_S "customer_done_semaphore"
#define CUSTOMER_S "customer_semaphor"
#define SHARED_LOCK_S "shared_lock_semaphor"

// Macro for checking if value is within range
#define check_range(max_range, value) ((max_range < value) ? 1 : 0)

// Using unsigned for args, there could be possibility of underflow
// but considering the most significant bit in unsigned int is much greater than
// highest accepted value the program will still exit according to its designed
// behavior
typedef struct {
  unsigned n_customers;
  unsigned n_clerks;
  unsigned timeout_ms;
  unsigned break_ms;
  unsigned working_ms;
} args_t;

typedef struct shared_memory{
  sem_t clerk_sem;
  sem_t clerk_done_sem;
  sem_t customer_sem;
  sem_t customer_done_sem;
  sem_t shared_lock;
  sem_t qe_sem[3];
  
  FILE *fp;
  bool is_open;
  unsigned action_id;
  unsigned pending_customers;
  int in_queue[3];
} shared_t;

