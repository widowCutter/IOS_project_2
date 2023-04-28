#include "proj2.h"
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

pid_t child_pid, wpid;
int status = 0;

sem_t *clerk_sem;
sem_t *clerk_done_sem;
sem_t *customer_sem;
sem_t *customer_done_sem;
sem_t *shared_lock;

int parse_arguments(int argc, char *argv[], args_t *args) {
  if (argc != 6) {
    return -1;
  }
  if (!sscanf(argv[1], "%u", &args->n_customers))
    return -1;
  if (!sscanf(argv[2], "%u", &args->n_clerks))
    return -1;
  if (!sscanf(argv[3], "%u", &args->timeout_ms))
    return -1;
  if (!sscanf(argv[4], "%u", &args->break_ms))
    return -1;
  if (!sscanf(argv[5], "%u", &args->working_ms))
    return -1;

  if (check_range(10000, args->timeout_ms) ||
      check_range(100, args->break_ms) ||
      check_range(10000, args->working_ms)) {
    return -1;
  }
  return 0;
}

void error_exit(int exit_code, char *err_message) {
  fprintf(stderr, "%s\n", err_message);
  exit(exit_code);
}

shared_t *create_shared_memory(size_t size) {
  int protection = PROT_READ | PROT_WRITE;

  // Visible to process and its children but not accessible by other processess
  int visibility = MAP_SHARED | MAP_ANONYMOUS;
  shared_t *mem = mmap(NULL, size, protection, visibility, -1, 0);

  mem->fp = mmap(NULL, sizeof(FILE *), protection, visibility, -1, 0);

  return mem;
}
void destroy_semaphores() {
  sem_destroy(clerk_sem);
  sem_destroy(clerk_done_sem);
  sem_destroy(customer_sem);
  sem_destroy(customer_done_sem);
  sem_destroy(shared_lock);
}

void init_semaphores(shared_t *mem) {

  short failed = 0;
  if (sem_init(&mem->clerk_sem, 1, 1) == -1)
    failed = 1;
  if (sem_init(&mem->clerk_done_sem, 1, 1) == -1)
    failed = 1;
  if (sem_init(&mem->customer_sem, 1, 1) == -1)
    failed = 1;
  if (sem_init(&mem->customer_done_sem, 1, 1) == -1)
    failed = 1;
  if (sem_init(&mem->shared_lock, 1, 1) == -1)
    failed = 1;
  if (sem_init(&mem->qe_sem[0], 1, 0) == -1)
    failed = 1;
  if (sem_init(&mem->qe_sem[1], 1, 0) == -1)
    failed = 1;
  if (sem_init(&mem->qe_sem[2], 1, 0) == -1)
    failed = 1;

  if (failed == 1)
    error_exit(5, "Failed to create semaphores\n");
}

int log_msg(shared_t *mem, bool log_action, bool is_mem_locked, const char *fmt, ...) {

  if (!is_mem_locked) {
    sem_wait(&mem->shared_lock);
  }
  va_list args;

  if (log_action) {
    fprintf(stdout, "%u", mem->action_id);
  }
  va_start(args, fmt);
  vfprintf(stdout, fmt, args);

  va_start(args, fmt);
  if (log_action) {
    fprintf(mem->fp, "%u", mem->action_id);
    mem->action_id++;
  }
  vfprintf(mem->fp, fmt, args);

  fflush(stdout);
  fflush(mem->fp);
  if (!is_mem_locked) {
     sem_post(&mem->shared_lock);
  }

  return EXIT_SUCCESS;
}


void wait_in_queue(unsigned queue, shared_t *mem){
  sem_wait(&mem->shared_lock);
  mem->in_queue[queue]++;
  sem_post(&mem->shared_lock);
  sem_wait(&mem->qe_sem[queue]);
}


void clerk(unsigned id, shared_t *mem, unsigned break_ms) {
  log_msg(mem, true, false,": U %u: started\n", id);
  srand(id * time(NULL));
  while (true) {
    sem_wait(&mem->shared_lock);
    int sum = 0;
    for (unsigned i = 0; i < sizeof(mem->in_queue); i++) {
      sum += mem->in_queue[i];
    }
    if (!sum) {
      if (!mem->is_open) {
        log_msg(mem, true, true,": U %u: going home\n", id);
        sem_post(&mem->shared_lock);
        return;
      }
      else {
        log_msg(mem, true, true,": U %u: taking break\n", id);
        sem_post(&mem->shared_lock);
        usleep(rand() % (break_ms + 1) * 1000);
        log_msg(mem, true, false, ": U %u: break finished\n", id);
      }
    }
    else {
      unsigned que;
      while (mem->in_queue[(que = rand() % 3)] == 0);
      sem_post(&mem->qe_sem[que]);
      mem->in_queue[que]--;
      log_msg(mem, true, true, ": U %u: serving a service of type %u\n", id, que);
      sem_post(&mem->shared_lock);
      usleep(rand() % 11 * 1000);
      log_msg(mem, true, false, ": U %u: service finished\n", id, que);
    }
  }
}

void customer(unsigned id, shared_t *mem, unsigned timeout) {
  log_msg(mem, true, false,": Z %u: started\n", id);
  srand(id * time(NULL));
  usleep(rand() % (timeout * 1000));
  sem_wait(&mem->shared_lock);
  if (!mem->is_open) {
    log_msg(mem, true, true, ": Z %u: going home\n", id);
    sem_post(&mem->shared_lock);
    return;
  }
  unsigned que = rand() % 3;
  log_msg(mem, true, true, ": Z %u: entering office for a service %u\n", id, que );
  sem_post(&mem->shared_lock);
  wait_in_queue(que, mem);
  log_msg(mem, false, false, "Z %u: called by office worker\n", id);
  usleep(rand() % 11 * 1000);
  log_msg(mem, true, false,": Z %u: going home\n", id);
}

int main(int argc, char *argv[]) {
  args_t args;
  if (0 > parse_arguments(argc, argv, &args)) {
    error_exit(1, "Wrong arguments");
  }

  shared_t *mem = create_shared_memory(sizeof(shared_t));
  mem->action_id = 1;
  mem->is_open = true;

  mem->pending_customers = args.n_customers;

  init_semaphores(mem);

  if (!(mem->fp = fopen("./proj2.out", "w"))) {
    exit(10);
  }

  for (unsigned i = 1; i <= args.n_customers + args.n_clerks; i++) {
    switch ((child_pid = fork())) {
    // PID 0: Child process
    case 0:
      if (i <= args.n_customers) {
        customer(i, mem, args.timeout_ms);
        return 0;
      } else if (i > args.n_customers) {
        clerk(i - args.n_customers, mem, args.break_ms);
        return 0;
      }
      break;
    case -1:
      error_exit(2, "fork failed?");
    default:
      break;
    }
  }
  usleep(args.working_ms * 1000);
  sem_wait(&mem->shared_lock);
  mem->is_open = false;
  log_msg(mem, true, true,": closing\n");
  sem_post(&mem->shared_lock);
  while ((wpid = wait(&status)) > 0)
    ;

  return EXIT_SUCCESS;
}
