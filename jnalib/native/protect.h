/* Copyright (c) 2007 Timothy Wall, All Rights Reserved
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * <p/>
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.  
 */
#ifndef PROTECT_H
#define PROTECT_H

// Native memory access protection
// 
// Enable or disable by setting the 'protect' variable.
//
// Example usage:
// void my_function() {
//   int variable_decls;
//   PROTECT_START();
//   // do some dangerous stuff here
//   PROTECT_END();
// }
//
// The PROTECT_START() macro must immediately follow any variable declarations 
//
// The w32 implementation is courtesy of Ranjit Mathew
// http://gcc.gnu.org/ml/java/2003-03/msg00243.html
static int protect;

#ifdef _WIN32
#include <excpt.h>

typedef struct _exc_rec {
  EXCEPTION_REGISTRATION ex_reg;
  void* exc_handling_addr;
} exc_rec;

static EXCEPTION_DISPOSITION __cdecl
exc_handler(struct _EXCEPTION_RECORD* exception_record,
            void *establisher_frame,
            struct _CONTEXT *context_record,
            void* dispatcher_context) {
  exc_rec* xer = (exc_rec *)establisher_frame;

  /* Unwind from the called function assuming the standard 
   * function prologue.
   */
  context_record->Esp = context_record->Ebp;
  context_record->Ebp = *((DWORD *)context_record->Esp);
  context_record->Esp = context_record->Esp - 8;

  /* Restart execution at the handler within the caller */
  context_record->Eip = (DWORD )(xer->exc_handling_addr);

  /* Tell Windows to restart the "faulting" instruction. */
  return ExceptionContinueExecution;
}

#define PROTECTED_START() \
  exc_rec er; \
  if (protect) { \
    er.exc_handling_addr = &&_exc_caught; \
    er.ex_reg.handler = exc_handler; \
    asm volatile ("movl %%fs:0, %0" : "=r" (er.ex_reg.prev)); \
    asm volatile ("movl %0, %%fs:0" : : "r" (&er)); \
  }

// The initial conditional is required to ensure GCC doesn't consider
// _exc_caught to be unreachable
#define PROTECTED_END(ONERR) do { \
  if (!protect || er.exc_handling_addr != 0) \
    goto _remove_handler; \
 _exc_caught: \
  ONERR; \
 _remove_handler: \
  if (protect) { asm volatile ("movl %0, %%fs:0" : : "r" (er.ex_reg.prev)); } \
} while(0)

#else // _WIN32
// Most other platforms support signals
// Catch both SIGSEGV and SIGBUS
#include <signal.h>
#include <setjmp.h>
static jmp_buf context;
static volatile int _error;
static void _exc_handler(int sig) {
  if (sig == SIGSEGV || sig == SIGBUS) {
    longjmp(context, sig);
  }
}

#define PROTECTED_START() \
  void* _old_segv_handler; \
  void* _old_bus_handler; \
  int _error = 0; \
  if (protect) { \
    _old_segv_handler = signal(SIGSEGV, _exc_handler); \
    _old_bus_handler = signal(SIGBUS, _exc_handler); \
    if ((_error = setjmp(context) != 0)) { \
      goto _exc_caught; \
    } \
  }

#define PROTECTED_END(ONERR) do { \
  if (!_error) \
    goto _remove_handler; \
 _exc_caught: \
  ONERR; \
 _remove_handler: \
  if (protect) { \
    signal(SIGSEGV, _old_segv_handler); \
    signal(SIGBUS, _old_bus_handler); \
  } \
} while(0)
#endif

#endif // PROTECT_H
