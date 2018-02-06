/* Trampolines from google's retpoline paper, with the interface required
 * by GCC 7.3.0's -mindirect-branch=thunk-extern.
 */

#include <sys/asm_linkage.h>

ENTRY(__x86_indirect_thunk)
        call 2f
1:
        pause
        lfence
        jmp 1b
2:
        lea 8(%rsp), %rsp
        ret
SET_SIZE(__x86_indirect_thunk)

#define INDIRECT_REGISTER_THUNK(reg) \
ENTRY(__x86_indirect_thunk_/**/reg); \
        call 2f;                     \
1:                                   \
        pause;                       \
        lfence;                      \
        jmp 1b;                      \
2:                                   \
        movq %/**/reg, (%rsp);       \
        ret;                         \
SET_SIZE(__x86_indirect_thunk_/**/reg)

INDIRECT_REGISTER_THUNK(rax)
INDIRECT_REGISTER_THUNK(rdx)
INDIRECT_REGISTER_THUNK(rcx)
INDIRECT_REGISTER_THUNK(rbx)
INDIRECT_REGISTER_THUNK(rsi)
INDIRECT_REGISTER_THUNK(rdi)
INDIRECT_REGISTER_THUNK(rbp)
INDIRECT_REGISTER_THUNK(r8)
INDIRECT_REGISTER_THUNK(r9)
INDIRECT_REGISTER_THUNK(r10)
INDIRECT_REGISTER_THUNK(r11)
INDIRECT_REGISTER_THUNK(r12)
INDIRECT_REGISTER_THUNK(r13)
INDIRECT_REGISTER_THUNK(r14)
INDIRECT_REGISTER_THUNK(r15)        

        
        
        
        
