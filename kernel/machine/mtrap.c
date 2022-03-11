#include "kernel/riscv.h"
#include "kernel/process.h"
#include "spike_interface/spike_utils.h"

static void handle_instruction_access_fault() { panic("Instruction access fault!"); }

static void handle_load_access_fault() { panic("Load access fault!"); }

static void handle_store_access_fault() { panic("Store/AMO access fault!"); }

static void handle_illegal_instruction() { panic("Illegal instruction!"); }

static void handle_misaligned_load() { panic("Misaligned Load!"); }

static void handle_misaligned_store() { panic("Misaligned AMO!"); }

static void handle_timer() {
  int cpuid = 0;
  // setup the timer fired at next time (TIMER_INTERVAL from now)
  *(uint64*)CLINT_MTIMECMP(cpuid) = *(uint64*)CLINT_MTIMECMP(cpuid) + TIMER_INTERVAL;

  // setup a soft interrupt in sip (S-mode Interrupt Pending) to be handled in S-mode
  write_csr(sip, SIP_SSIP);
}

// this function is going to print the code address which trigger the exception(machine exception)
void print_debug_info() {
  uint64 line_number = 0;
  char *file_path = NULL;
  char *file_name = NULL;

  // get wrong code address
  uint64 wrong_addr = read_csr(mepc);

  // get debug info
  for(uint32 i = 0;i<current->line_ind;i++) {
    if(current->line[i].addr == wrong_addr) {
      line_number = current->line[i].line;
      uint64 filename_ndx = current->line[i].file;
      file_name = current->file[filename_ndx].file;
      uint64 filepath_ndx = current->file[filename_ndx].dir;
      file_path = current->dir[filepath_ndx];
    }
  }

  sprint("%s%s%lld", file_path, file_name, line_number);
  
}

//
// handle_mtrap calls cooresponding functions to handle an exception of a given type.
//
void handle_mtrap() {
  print_debug_info();
  uint64 mcause = read_csr(mcause);
  switch (mcause) {
    case CAUSE_MTIMER:
      handle_timer();
      break;
    case CAUSE_FETCH_ACCESS:
      handle_instruction_access_fault();
      break;
    case CAUSE_LOAD_ACCESS:
      handle_load_access_fault();
    case CAUSE_STORE_ACCESS:
      handle_store_access_fault();
      break;
    case CAUSE_ILLEGAL_INSTRUCTION:
      // TODO (lab1_2): call handle_illegal_instruction to implement illegal instruction
      // interception, and finish lab1_2.
      handle_illegal_instruction();

      break;
    case CAUSE_MISALIGNED_LOAD:
      handle_misaligned_load();
      break;
    case CAUSE_MISALIGNED_STORE:
      handle_misaligned_store();
      break;

    default:
      sprint("machine trap(): unexpected mscause %p\n", mcause);
      sprint("            mepc=%p mtval=%p\n", read_csr(mepc), read_csr(mtval));
      panic( "unexpected exception happened in M-mode.\n" );
      break;
  }
}

