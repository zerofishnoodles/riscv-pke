/*
 * Supervisor-mode startup codes
 */

#include "riscv.h"
#include "string.h"
#include "elf.h"
#include "process.h"
#include "pmm.h"
#include "vmm.h"
#include "memlayout.h"
#include "spike_interface/spike_utils.h"

process user_app;

//
// trap_sec_start points to the beginning of S-mode trap segment (i.e., the entry point of
// S-mode trap vector).
//
extern char trap_sec_start[];

//
// turn on paging.
//
void enable_paging() {
  // write the pointer to kernel page (table) directory into the CSR of "satp".
  write_csr(satp, MAKE_SATP(g_kernel_pagetable));

  // refresh tlb to invalidate its content.
  flush_tlb();
}

//
// load the elf, and construct a "process" (with only a trapframe).
// load_bincode_from_host_elf is defined in elf.c
//
void load_user_program(process *proc) {
  sprint("User application is loading.\n");
  proc->trapframe = (trapframe *)alloc_page();  //trapframe
  memset(proc->trapframe, 0, sizeof(trapframe));

  //user pagetable
  proc->pagetable = (pagetable_t)alloc_page();
  memset((void *)proc->pagetable, 0, PGSIZE);

  // user memory control block
  proc->mcb = (mm_struct*)alloc_page();
  memset(proc->mcb, 0, sizeof(mm_struct));
  proc->mcb->mm_count = 1; // suppose there is only one process using this mcb
  proc->mcb->page_dir = proc->pagetable;

  // user virtual memory area, suppose the VMA linked list is smaller than one page.
  proc->mcb->mmap = (vm_area_struct*)alloc_page();
  memset(proc->mcb->mmap, 0, sizeof(vm_area_struct));

  // suppose the first and the only vm_area is just the heap for the convenience
  vm_area_struct *heap = proc->mcb->mmap;
  heap->vm_start = g_ufree_page;
  heap->vm_type = VM_HEAP;
  heap->vm_end = g_ufree_page;
  heap->vm_mm = proc->mcb;
  heap->vm_next = NULL;
  proc->mcb->map_count = 1;  // it is not a good idea!
  proc->mcb->mmap_cache = heap;

  proc->kstack = (uint64)alloc_page() + PGSIZE;   //user kernel stack top
  uint64 user_stack = (uint64)alloc_page();       //phisical address of user stack bottom
  proc->trapframe->regs.sp = USER_STACK_TOP;  //virtual address of user stack top

  sprint("user frame 0x%lx, user stack 0x%lx, user kstack 0x%lx \n", proc->trapframe,
         proc->trapframe->regs.sp, proc->kstack);

  load_bincode_from_host_elf(proc);

  // map user stack in userspace
  user_vm_map((pagetable_t)proc->pagetable, USER_STACK_TOP - PGSIZE, PGSIZE, user_stack,
         prot_to_type(PROT_WRITE | PROT_READ, 1));

  // map trapframe in user space (direct mapping as in kernel space).
  user_vm_map((pagetable_t)proc->pagetable, (uint64)proc->trapframe, PGSIZE, (uint64)proc->trapframe,
         prot_to_type(PROT_WRITE | PROT_READ, 0));

  // map S-mode trap vector section in user space (direct mapping as in kernel space)
  // we assume that the size of usertrap.S is smaller than a page.
  user_vm_map((pagetable_t)proc->pagetable, (uint64)trap_sec_start, PGSIZE, (uint64)trap_sec_start,
         prot_to_type(PROT_READ | PROT_EXEC, 0));

//   // map mcb in user space (direct mapping)
//   user_vm_map((pagetable_t)proc->pagetable, (uint64)proc->mcb, PGSIZE, (uint64)proc->mcb,
//          prot_to_type(PROT_WRITE | PROT_READ, 0));

//   // map VMA linked list in user space(direct mapping)
//   user_vm_map((pagetable_t)proc->pagetable, (uint64)proc->mcb->mmap, PGSIZE, (uint64)proc->mcb->mmap, 
//          prot_to_type(PROT_WRITE | PROT_READ, 0));
}

//
// s_start: S-mode entry point of PKE OS kernel.
//
int s_start(void) {
  sprint("Enter supervisor mode...\n");
  // in the beginning, we use Bare mode (direct) memory mapping as in lab1,
  // but now switch to paging mode in lab2.
  write_csr(satp, 0);

  // init phisical memory manager
  pmm_init();

  // build the kernel page table
  kern_vm_init();

  // now, switch to paging mode by turning on paging (SV39)
  enable_paging();
  sprint("kernel page table is on \n");

  // the application code (elf) is first loaded into memory, and then put into execution
  load_user_program(&user_app);

  sprint("Switch to user mode...\n");
  switch_to(&user_app);

  return 0;
}
