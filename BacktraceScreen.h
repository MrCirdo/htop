#ifndef HEADER_BacktraceScreen
#define HEADER_BacktraceScreen
/*
htop - BacktraceScreen.h
(C) 2023 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#if defined(HAVE_BACKTRACE)
#include <stddef.h>
#include <stdbool.h>

#include "Object.h"
#include "Panel.h"
#include "Process.h"

typedef struct BacktracePanel_ {
   Panel super;
   const Process* process;
   bool error;
} BacktracePanel;

typedef struct BacktraceFrame_ {
   Object super;
   int index;
   size_t address;
   size_t offset;
   char* functionName;
#if defined(HAVE_DEMANGLING)
   char* demangleFunctionName;
#endif
   char* objectPath;
   bool isSignalFrame;
} BacktraceFrame;

BacktracePanel* BacktracePanel_new(const Process* process);
void BacktracePanel_delete(Object* object);
BacktraceFrame* BacktraceFrame_new(void);

extern const PanelClass BacktracePanel_class;
extern const ObjectClass BacktraceFrame_class;

#endif
#endif
