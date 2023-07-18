#include "BacktraceScreen.h"

#include <ctype.h>
#include <execinfo.h>
#include <stdio.h>
#include <string.h>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <libunwind-ptrace.h>

#include "FunctionBar.h"
#include "InfoScreen.h"
#include "Object.h"
#include "Panel.h"
#include "Process.h"
#include "Vector.h"
#include "XUtils.h"

static const char* const BackTraceScreenFunctions[] = {"Refresh", "Done", NULL};
static const char* const BackTraceScreenKeys[] = {"F3", "Esc"};
static const int BackTraceScreenEvents[] = {KEY_F(3), 27};

static void BacktraceScreen_getMaxWidth(size_t* maxFunctionWidth, size_t* maxPathWidth, Backtrace* backtraces, size_t maxBacktraces) {
   *maxFunctionWidth = 0;
   *maxPathWidth = 0;

   for (size_t i = 0; i < maxBacktraces; i++) {
      Backtrace* backtrace = &backtraces[i];
      for (size_t j = 0; j < backtrace->nbFrames; j++) {
         Frame* frame = &backtrace->frames[j];
         size_t lenFunction = strlen(frame->function);

         if (lenFunction > *maxFunctionWidth)
            *maxFunctionWidth = lenFunction;

         size_t lenPath = strlen(frame->path);
         if (lenPath > *maxPathWidth)
            *maxPathWidth = lenPath;
      }
   }
}

static void BacktraceScreen_freeFrame(Frame* frame) {
   free(frame->function);
   free(frame->path);
}

static void BacktraceScreen_freeBacktraces(Backtrace* backtraces) {
   if (!backtraces)
      return;

   Backtrace* backtraceArray = backtraces;
   for (size_t backtraceIndex = 0; backtraceIndex < MAX_BACKTRACE; backtraceIndex++) {
      Backtrace* currentBacktrace = &backtraces[backtraceIndex];
      for (size_t frameIndex = 0; frameIndex < currentBacktrace->nbFrames; frameIndex++) {
         Frame* currentFrame = &currentBacktrace->frames[frameIndex];
         BacktraceScreen_freeFrame(currentFrame);
      }
   }
   free(backtraceArray);
}

BacktraceScreen* BacktraceScreen_new(const Process* process) {
   BacktraceScreen* this = xCalloc(1, sizeof(BacktraceScreen));
   Object_setClass(this, Class(BacktraceScreen));
   FunctionBar* functionBar = FunctionBar_new(BackTraceScreenFunctions, BackTraceScreenKeys, BackTraceScreenEvents);
   CRT_disableDelay();
   return (BacktraceScreen*) InfoScreen_init(&this->super, process, functionBar, LINES - 2, "NB ADDRESS NAME PATH ");
}

void BacktraceScreen_delete(Object* cast) {
   BacktraceScreen* this = (BacktraceScreen*) cast;
   BacktraceScreen_freeBacktraces(this->backtraces);
   CRT_enableDelay();
   free(InfoScreen_done((InfoScreen*)this));
}

static void BacktraceScreen_draw(InfoScreen* this) {
   if (Process_isThread(this->process))
      InfoScreen_drawTitled(this, "Backtrace of thread %d - %s", this->process->pid, Process_getCommand(this->process));
   else
      InfoScreen_drawTitled(this, "Backtrace of process %d - %s", this->process->pid, Process_getCommand(this->process));
}

static void BacktraceScreen_print(BacktraceScreen* backtraceScreen) {
   const Process* process = backtraceScreen->super.process;
   bool isThread = Process_isThread(process);

   size_t maxFunctionWidth = 0;
   size_t maxPathWidth = 0;
   BacktraceScreen_getMaxWidth(&maxFunctionWidth, &maxPathWidth, backtraceScreen->backtraces, backtraceScreen->nbBacktraces);

   char* header = NULL;
   xAsprintf(&header, " NB ADDRESS             NAME %*s PATH", (int)(maxFunctionWidth - sizeof("NAME")), " ");
   Panel_setHeader(backtraceScreen->super.display, header);

   free(header);

   for (size_t i = 0; i < backtraceScreen->nbBacktraces; i++) {
      Backtrace* backtrace = &backtraceScreen->backtraces[i];
      char* tgidStr = NULL;
      if (!isThread)  {
         xAsprintf(&tgidStr, "TID: %d", backtrace->tgid);
         InfoScreen_addLine(&backtraceScreen->super, tgidStr);
      }

      if (isThread && backtrace->tgid != process->tgid) {
         continue;
      }

      for (size_t j = 0; j < backtrace->nbFrames; j++) {
         Frame* frame = &backtrace->frames[j];

         char* frameStr = NULL;
         xAsprintf(&frameStr, "  #%zu 0x%016zx %-*s %s", frame->nb, frame->address, (int)maxFunctionWidth, frame->function, frame->path);
         InfoScreen_addLine(&backtraceScreen->super, frameStr);
         free(frameStr);
      }
      free(tgidStr);
   }
}

static void BacktraceScreen_getBacktraceFromProcess(const Process *process, Backtrace *backtraces, size_t maxBacktraces) {
   pid_t pid = Process_isThread(process) ? process->tgid : process->pid;

   unw_addr_space_t addrSpace = unw_create_addr_space(&_UPT_accessors, 0);
   ptrace(PTRACE_ATTACH, pid, 0, 0);
   void *context = _UPT_create(pid);
   unw_cursor_t cursor;
   unw_init_remote(&cursor, addrSpace, context);

   do {
      unw_word_t offset = 0;

      char symbolName[4096];
      if (unw_get_proc_name(&cursor, symbolName, sizeof(symbolName), &offset) == 0) {

      }

   } while (unw_step(&cursor));


   _UPT_destroy(context);
   unw_destroy_addr_space(addrSpace);
   ptrace(PTRACE_DETACH, pid);
}

static void BacktraceScreen_scan(InfoScreen* super) {
   BacktraceScreen* this = (BacktraceScreen*)super;
   this->backtraces = xCalloc(MAX_BACKTRACE, sizeof(Backtrace));

   BacktraceScreen_print(this);
}

static bool BacktraceScreen_onKey(InfoScreen* super, int character) {
   BacktraceScreen* this = (BacktraceScreen*)super;
   switch (character) {
   case 'r':
   case KEY_F(3):
      BacktraceScreen_freeBacktraces(this->backtraces);
      Panel_prune(super->display);
      Vector_prune(super->lines);
      BacktraceScreen_scan(super);
      return true;
   }
   return false;
}

const InfoScreenClass BacktraceScreen_class = {
   .super = {
      .extends = Class(Object),
      .delete = BacktraceScreen_delete,
   },
   .scan = BacktraceScreen_scan,
   .draw = BacktraceScreen_draw,
   .onKey = BacktraceScreen_onKey,
};
