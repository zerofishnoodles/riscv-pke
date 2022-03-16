/*
 * contains the implementation of all syscalls.
 */

#include <stdint.h>
#include <errno.h>

#include "util/types.h"
#include "syscall.h"
#include "string.h"
#include "process.h"
#include "util/functions.h"
#include "pmm.h"
#include "vmm.h"
#include "sched.h"

#include "spike_interface/spike_utils.h"

//
// implement the SYS_user_print syscall
//
ssize_t sys_user_print(const char* buf, size_t n) {
  //buf is an address in user space on user stack,
  //so we have to transfer it into phisical address (kernel is running in direct mapping).
  assert( current );
  char* pa = (char*)user_va_to_pa((pagetable_t)(current->pagetable), (void*)buf);
  sprint(pa);
  return 0;
}

//
// implement the SYS_user_exit syscall
//
ssize_t sys_user_exit(uint64 code) {
  sprint("User exit with code:%d.\n", code);
  // in lab3 now, we should reclaim the current process, and reschedule.
  free_process( current );
  schedule();
  return 0;
}

//
// maybe, the simplest implementation of malloc in the world ...
//
uint64 sys_user_allocate_page() {
  void* pa = alloc_page();
  uint64 va = g_ufree_page;
  g_ufree_page += PGSIZE;
  user_vm_map((pagetable_t)current->pagetable, va, PGSIZE, (uint64)pa,
         prot_to_type(PROT_WRITE | PROT_READ, 1));

  return va;
}

//
// reclaim a page, indicated by "va".
//
uint64 sys_user_free_page(uint64 va) {
  user_vm_unmap((pagetable_t)current->pagetable, va, PGSIZE, 1);
  return 0;
}

//
// kerenl entry point of naive_fork
//
ssize_t sys_user_fork() {
  sprint("User call fork.\n");
  return do_fork( current );
}

//
// kerenl entry point of yield
//
ssize_t sys_user_yield() {
  // TODO (lab3_2): implment the syscall of yield.
  // hint: the functionality of yield is to give up the processor. therefore,
  // we should set the status of currently running process to READY, insert it in 
  // the rear of ready queue, and finally, schedule a READY process to run.
  current->status = READY;
  insert_to_ready_queue(current);
  schedule();

  return 0;
}

// global variable
#define NSEM 10
static uint64 g_total_sem = 0;
semaphore sems[NSEM];
ssize_t sys_user_sem_new(int sem_value){
  sems[g_total_sem].value = sem_value;
  sems[g_total_sem].wait_list = NULL;
  sems[g_total_sem].sem_id = g_total_sem;
  g_total_sem++;
  return sems[g_total_sem-1].sem_id;
}

// the PV op is towards sem_id!!!
ssize_t sys_user_sem_P(int sem_id){
  // sprint("in P:sem_id:%d, value:%d, pid:%d\n", sem_id, sems[sem_id].value, current->pid);
  sems[sem_id].value--;
  // sprint("in P:sem_id:%d, value:%d\n", sem_id, sems[sem_id].value);
  if(sems[sem_id].value < 0){
    current->status = BLOCKED;
    process *temp = sems[sem_id].wait_list;
    if(temp == NULL) sems[sem_id].wait_list = current;
    else{
      sems[sem_id].wait_list = current;
      sems[sem_id].wait_list->queue_next = temp;
    }
    schedule();
  }
  return 0;
}

ssize_t sys_user_sem_V(int sem_id) {
  // no need to schedule: time slice arrive/ P op
  // sprint("in V:sem_id:%d, value:%d, current pid:%d\n", sem_id, sems[sem_id].value, current->pid);
  sems[sem_id].value++;
  // sprint("in V: sem_id:%d, value:%d\n", sem_id, sems[sem_id].value);
  // if(sems[sem_id].wait_list!=NULL) sprint("wait list:%d\n", sems[sem_id].wait_list->pid);
  if(sems[sem_id].value <= 0) {
    process *wait_p = sems[sem_id].wait_list;
    if(wait_p != NULL){
      insert_to_ready_queue(wait_p);
      sems[sem_id].wait_list = wait_p->queue_next;
    }else{
      panic("there is no waitint process!\n");
    }
  }
  // insert_to_ready_queue(current);
  return 0;
}

//
// [a0]: the syscall number; [a1] ... [a7]: arguments to the syscalls.
// returns the code of success, (e.g., 0 means success, fail for otherwise)
//
long do_syscall(long a0, long a1, long a2, long a3, long a4, long a5, long a6, long a7) {
  switch (a0) {
    case SYS_user_print:
      return sys_user_print((const char*)a1, a2);
    case SYS_user_exit:
      return sys_user_exit(a1);
    case SYS_user_allocate_page:
      return sys_user_allocate_page();
    case SYS_user_free_page:
      return sys_user_free_page(a1);
    case SYS_user_fork:
      return sys_user_fork();
    case SYS_user_yield:
      return sys_user_yield();
    case SYS_user_sem_new:
      return sys_user_sem_new(a1);
    case SYS_user_sem_P:
      return sys_user_sem_P(a1);
    case SYS_user_sem_V:
      return sys_user_sem_V(a1);
    default:
      panic("Unknown syscall %ld \n", a0);
  }
}
