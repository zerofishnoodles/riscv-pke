#ifndef _PROC_H_
#define _PROC_H_

#include "riscv.h"

#define VM_HEAP 1

#define MB_EMPTY 0
#define MB_ALLOCED 1

typedef struct mem_block
{
  struct mem_block *pre;
  struct mem_block *next; 
  uint64 size;
  uint64 flags;  // 0 is free memory block, 1 is used memory block
  uint64 vmoff;  // the offset to the vm_start
  uint64 *data;  // the data is just behind the mem block for the convenience 
}mem_block;

typedef struct vm_area_struct {
  //指向vm_mm
	struct mm_struct *vm_mm;	/* The address space we belong to. */
	//该虚拟内存空间的首地址
  uint64 vm_start;		/* Our start address within vm_mm. */
	//该虚拟内存空间的尾地址
  uint64 vm_end;		/* The first byte after our end address within vm_mm. */
  // memory block in the virtual memory space
  mem_block* mb;

  //VMA链表的下一个成员
	/* linked list of VM areas per task, sorted by address */
	struct vm_area_struct *vm_next;

	//保存VMA标志位
  uint64 vm_flags;		/* Flags, listed below. */  // unused in this lab

  uint64 vm_type;  // heap, stack. etc
} vm_area_struct;



typedef struct mm_struct {
  vm_area_struct *mmap; /* 指向虚拟区间（VMA）链表 */
  vm_area_struct *mmap_cache; /* 指向最近找到的虚拟区间*/
  pagetable_t page_dir; /*指向进程的页目录*/
  uint32 mm_count; /* 对"struct mm_struct"有多少引用*/
  int map_count; /* 虚拟区间的个数*/
}mm_struct;



typedef struct trapframe {
  // space to store context (all common registers)
  /* offset:0   */ riscv_regs regs;

  // process's "user kernel" stack
  /* offset:248 */ uint64 kernel_sp;
  // pointer to smode_trap_handler
  /* offset:256 */ uint64 kernel_trap;
  // saved user process counter
  /* offset:264 */ uint64 epc;

  //kernel page table
  /* offset:272 */ uint64 kernel_satp;
}trapframe;

// the extremely simple definition of process, used for begining labs of PKE
typedef struct process {
  // pointing to the stack used in trap handling.
  uint64 kstack;
  // user page table
  pagetable_t pagetable;
  // trapframe storing the context of a (User mode) process.
  trapframe* trapframe;
  // memory control block
  mm_struct *mcb;
}process;

// switch to run user app
void switch_to(process*);

// current running process
extern process* current;
// virtual address of our simple heap
extern uint64 g_ufree_page;

#endif
