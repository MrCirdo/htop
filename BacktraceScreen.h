#ifndef HEADER_BacktraceScreen
#define HEADER_BacktraceScreen

#include "config.h" // IWYU pragma: keep

#if (defined(HTOP_LINUX) && defined(HAVE_LIBUNWIND_PTRACE))

#include "Panel.h"
#include "Process.h"
#include <stddef.h>

typedef struct BacktracePanel_ {
   Panel super;
   const Process* process;
   char* error;
} BacktracePanel;

typedef struct Frame_ {
   Object super;
   int index;
   size_t address;
   size_t offset;
   char* functionName;
   bool isSignalFrame;

   bool isError;
   char *error;
} Frame;

BacktracePanel* BacktracePanel_new(const Process* process);
void BacktracePanel_delete(Object* object);
Frame* Frame_new(void);

extern const PanelClass BacktracePanel_class;
extern const ObjectClass Frame_class;

#endif /* HTOP_LINUX && HAVE_LIBUNWIND_PTRACE */

#endif
