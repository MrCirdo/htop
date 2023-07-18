#ifndef HEADER_BacktraceScreen
#define HEADER_BacktraceScreen
/*
htop - BacktraceSceen.h
(C) 2023 Htop Dev Team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include <stddef.h>

#include "InfoScreen.h"
#include "Object.h"
#include "Process.h"

#define MAX_FRAME 256
#define MAX_BACKTRACE 1024

typedef struct Frame_ {
   size_t nb;
   size_t address;
   char* function;
   char* path;
} Frame;

typedef struct Backtrace_ {
   pid_t pid;
   pid_t tgid;
   Frame frames[MAX_FRAME];
   size_t nbFrames;
} Backtrace;

typedef struct BacktraceScreen_ {
   InfoScreen super;
   Backtrace* backtraces;
   size_t nbBacktraces;
} BacktraceScreen;

extern const InfoScreenClass BacktraceScreen_class;

BacktraceScreen* BacktraceScreen_new(const Process* process);
void BacktraceScreen_delete(Object* cast);

#endif
