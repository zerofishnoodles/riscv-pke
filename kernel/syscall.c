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

#include "spike_interface/spike_utils.h"

bool is_free_page(mem_block *tar, vm_area_struct *heap){
  if(tar->size == PGSIZE) {
    user_vm_unmap(current->pagetable, heap->vm_start+tar->vmoff, PGSIZE, 1);
    if(tar->pre) {
      tar->pre->next = NULL;
      heap->vm_end -= PGSIZE;
    }else{
      heap->mb = NULL;
      heap->vm_end = heap->vm_start;
    }
    return TRUE;
  }else{
    return FALSE;
  }
}

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
  // in lab1, PKE considers only one app (one process). 
  // therefore, shutdown the system when the app calls exit()
  shutdown(code);
}

//
// maybe, the simplest implementation of malloc in the world ...

//
uint64 sys_user_allocate_page(uint64 n) {
  // void* pa = alloc_page();
  // uint64 va = g_ufree_page;
  // g_ufree_page += PGSIZE;
  // user_vm_map((pagetable_t)current->pagetable, va, PGSIZE, (uint64)pa,
  //        prot_to_type(PROT_WRITE | PROT_READ, 1));

  // return va;

// first , round up the n and add the mem block size
// second, find the heap memory space and then mb
// third, if mb doesn't exist, init mb and return the address after mapping
// if mb exist, then find the available mb
// if avail mb exist, do the mapping and return the address
// if avail mb doesn;t exist, allocate a new page and return the address after the mapping

  // round up the bytes and add the mem block size
  n = ROUNDUP(n, 8) + sizeof(mem_block);
  if(n > PGSIZE) panic("the system is yet to realize allocating space larger than pagesize\n");
  // sprint("here\n");
  // find the heap memory space
  mm_struct *mcb = current->mcb;
  vm_area_struct *mmap = mcb->mmap;
  vm_area_struct *heap=NULL, *cur_vm_area = mmap;
  if(mcb->mmap_cache->vm_type == VM_HEAP) heap = mcb->mmap_cache;
  else{
    while(cur_vm_area){
      if(cur_vm_area->vm_type == VM_HEAP){
        heap = cur_vm_area;
      }
      cur_vm_area = cur_vm_area->vm_next;
    }
  }
  if(heap == NULL)  panic("the heap area doesn't exit!\n");
  // sprint("here\n");
  // check the memory block of the heap
  mem_block *mb = heap->mb;

  // init mb
  if(mb == NULL){
    // apply for a new page
    void *pa = alloc_page();
    uint64 va = g_ufree_page;
    g_ufree_page += PGSIZE;
    user_vm_map((pagetable_t)current->pagetable, va, PGSIZE, (uint64)pa, prot_to_type(PROT_WRITE | PROT_READ, 1));

    // update heap control block
    heap->vm_start = va;
    heap->vm_end = heap->vm_end+PGSIZE;

    // alert: in the s mode, the pagetable is the kernel pagetable, different from the user pagetable
    // when doing the memory operation, the va should be mapped in the kernel pagetable or just use the phsical address!
    mb = (mem_block *)pa;
    mb->size = n;
    mb->flags = MB_ALLOCED;
    mb->pre = NULL;
    mb->data = (uint64*)(&(mb->data))+1;
    mb->vmoff = 0;

    mb->next = (mem_block*)((char*)mb+n);
    mb->next->pre = mb;
    mb->next->flags = MB_EMPTY;
    mb->next->next = NULL;
    mb->next->size = PGSIZE-mb->size;
    mb->next->data = (uint64*)(&(mb->next->data))+1;
    mb->next->vmoff = mb->size;

    heap->mb = mb;

    uint64 rva = heap->vm_start+(uint64)mb->vmoff+sizeof(mem_block);
    // sprint("first:%x\n", mb);
    // sprint("%lld\n", n);
    // sprint("seconde:%x\n", mb->next);
    
    // user_vm_map((pagetable_t)current->pagetable, rva, n-sizeof(mem_block), (uint64)(pa+sizeof(mem_block)), prot_to_type(PROT_WRITE | PROT_READ, 1));
    return rva;
  }
  // sprint("here\n");
  // mb exist, find the available mb
  mem_block *cur_mb = mb;
  mem_block *avail_mb = NULL;
  while(cur_mb->next){ // for the sake of the allocate a new page
    if(cur_mb->size > n && cur_mb->flags == MB_EMPTY) {
      avail_mb = cur_mb;
    }
    cur_mb = cur_mb->next;
  }
  // for the sake of the allocate a new page
  if(cur_mb->size > n && cur_mb->flags == MB_EMPTY) {
      avail_mb = cur_mb;
  }
  // sprint("here\n");
  // avail_mb exist
  if(avail_mb != NULL) {
    avail_mb->flags = MB_ALLOCED;
      // sprint("here\n");
    avail_mb->next = avail_mb+n;
    avail_mb->next->size = avail_mb->size-n;
    avail_mb->next->pre = avail_mb;
    avail_mb->next->next = avail_mb->next;
    avail_mb->next->flags = MB_EMPTY;
    avail_mb->next->data = (uint64*)(&(avail_mb->next->data))+1;
    avail_mb->size = n;
    avail_mb->next->vmoff = avail_mb->vmoff+avail_mb->size;
    uint64 rva = heap->vm_start+avail_mb->vmoff+sizeof(mem_block);
    // user_vm_map((pagetable_t)current->pagetable, rva, n-sizeof(mem_block), (uint64)(avail_mb->data), 
    //             prot_to_type(PROT_WRITE | PROT_READ, 1));
      // sprint("here\n");
    // sprint("%lld\n", n);
    // sprint("second: %x\n", avail_mb);
    // sprint("third :%x\n", avail_mb->next);
    return rva;

  }else{ 
    void *pa = alloc_page();
    uint64 va = g_ufree_page;
    g_ufree_page += PGSIZE;
    user_vm_map((pagetable_t)current->pagetable, va, PGSIZE, (uint64)pa, prot_to_type(PROT_WRITE | PROT_READ, 1));

    // update heap control block
    heap->vm_end = heap->vm_end+PGSIZE;
    avail_mb = (mem_block*)pa;

    avail_mb->size = n;
    avail_mb->flags = MB_ALLOCED;
    avail_mb->pre = cur_mb;
    avail_mb->data = (uint64*)(&(avail_mb->data))+1;
    avail_mb->vmoff = heap->vm_end-PGSIZE;

    avail_mb->next = avail_mb+n;
    avail_mb->next->pre = avail_mb;
    avail_mb->next->flags = MB_EMPTY;
    avail_mb->next->next = NULL;
    avail_mb->next->size = PGSIZE-avail_mb->size;
    avail_mb->next->data = (uint64*)(&(avail_mb->next->data))+1;
    avail_mb->next->vmoff = avail_mb->vmoff+avail_mb->size;

    uint64 rva = heap->vm_start+avail_mb->vmoff+sizeof(mem_block);

    // user_vm_map((pagetable_t)current->pagetable, rva, n-sizeof(mem_block), (uint64)(pa+sizeof(mem_block)), prot_to_type(PROT_WRITE | PROT_READ, 1));

    return rva;
  }
}

//
// reclaim a page, indicated by "va".
//
uint64 sys_user_free_page(uint64 va) {
  // user_vm_unmap((pagetable_t)current->pagetable, va, PGSIZE, 1);
  // sprint("here\n");
  // only free the memory block
  // first, find the corresbonding mb according to the va
  uint64 pa = lookup_pa(current->pagetable, va);
  mem_block *tar = (mem_block*)(pa+((va-sizeof(mem_block))&((1<<PGSHIFT)-1)));

  // find the heap memory space
  mm_struct *mcb = current->mcb;
  vm_area_struct *mmap = mcb->mmap;
  vm_area_struct *heap=NULL, *cur_vm_area = mmap;
  if(mcb->mmap_cache->vm_type == VM_HEAP) heap = mcb->mmap_cache;
  else{
    while(cur_vm_area){
      if(cur_vm_area->vm_type == VM_HEAP){
        heap = cur_vm_area;
      }
      cur_vm_area = cur_vm_area->vm_next;
    }
  }
  if(heap == NULL)  panic("the heap area doesn't exit!\n");

  // update mb
  tar->flags = MB_EMPTY;
  // merge the empty space and if empty space is a pagesize, free the page
  if(tar->pre && tar->next && tar->pre->flags == MB_EMPTY && tar->next->flags == MB_ALLOCED) {
    tar->pre->size += tar->size;
    tar->pre->next = tar->next;
  }
  if(tar->pre && tar->next && tar->next->flags == MB_EMPTY && tar->pre->flags == MB_ALLOCED) {
    tar->size += tar->next->size;
    tar->next = tar->next->next;
  }
  if(tar->pre && tar->next && tar->pre->flags == MB_EMPTY && tar->next->flags == MB_EMPTY) {
    tar->pre->size = tar->pre->size + tar->size + tar->next->size;
    if(is_free_page(tar->pre, heap)) return 0;
    else tar->pre->next = tar->next->next;
  }

  if(tar->pre == NULL && tar->next->flags == MB_EMPTY) {
    tar->size = tar->size + tar->next->size;
    if(is_free_page(tar, heap)) return 0;
    else tar->next = tar->next->next;
  }
  if(tar->next == NULL && tar->pre->flags == MB_EMPTY) {
    tar->pre->size = tar->pre->size + tar->size;
    if(is_free_page(tar->pre, heap)) return 0;
    else tar->pre->next = tar->next;
  }
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
      return sys_user_allocate_page(a1);
    case SYS_user_free_page:
      return sys_user_free_page(a1);
    default:
      panic("Unknown syscall %ld \n", a0);
  }
}
