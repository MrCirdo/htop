/*
htop - Action.c
(C) 2015 Hisham H. Muhammad
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "BacktraceScreen.h"

#if (defined(HTOP_LINUX) && defined(HAVE_LIBUNWIND_PTRACE))

#include <sys/wait.h>

#include "CRT.h"
#include "Object.h"
#include "Panel.h"
#include "Process.h"
#include "RichString.h"
#include "XUtils.h"
#include "errno.h"

#include <libunwind-ptrace.h>
#include <sys/ptrace.h>

#define MAX_FRAME 256

static const char* const BacktraceScreenFunctions[] = {"Update ", "Done   ", NULL};

static const char* const BacktraceScreenKeys[] = {"F3", "F4", "Esc"};

static const int BacktraceScreenEvents[] = {KEY_F(3), KEY_F(4), 27};

static void Frame_display(const Object* super, RichString* out) {
   const Frame* const frame = (const Frame*)super;
   if (frame->isError) {
      RichString_appendAscii(out, CRT_colors[DEFAULT_COLOR], frame->error);
     return;
   }

   char bufferNumberOfFrame[16] = {'\0'};
   int len = snprintf(bufferNumberOfFrame, sizeof(bufferNumberOfFrame), "#%-3d ", frame->index);
   RichString_appendnAscii(out, CRT_colors[DYNAMIC_GREEN], bufferNumberOfFrame, len);

   char bufferAddress[32] = {'\0'};
   len = snprintf(bufferAddress, sizeof(bufferAddress), "0x%016zx ", frame->address);
   RichString_appendnAscii(out, CRT_colors[DYNAMIC_YELLOW], bufferAddress, len);

   RichString_appendAscii(out, CRT_colors[DEFAULT_COLOR], frame->functionName);
   if (frame->isSignalFrame) {
      RichString_appendAscii(out, CRT_colors[DYNAMIC_RED], " signal frame");
   }

   char bufferFrameOffset[16] = {'\0'};
   len = snprintf(bufferFrameOffset, sizeof(bufferFrameOffset), "+%zu", frame->offset);
   RichString_appendAscii(out, CRT_colors[DYNAMIC_BLUE], bufferFrameOffset);
}

static void BacktracePanel_getFrames(BacktracePanel* this) {
   Panel* super = (Panel*) this;

   unw_addr_space_t addrSpace = unw_create_addr_space(&_UPT_accessors, 0);
   if (!addrSpace) {
      xAsprintf(&this->error, "Unable to init libunwind.");
      return;
   }

   const pid_t pid = Process_getPid(this->process);

   if (pid == 0) {
      xAsprintf(&this->error, "Unable to get the pid");
      goto addr_space_error;
   }

   if (ptrace(PTRACE_ATTACH, pid, 0, 0) == -1) {
      xAsprintf(&this->error, "ptrace: %s", strerror(errno));
      goto addr_space_error;
   }
   wait(NULL);

   struct UPT_info* context = _UPT_create(pid);
   if (!context) {
      xAsprintf(&this->error, "Unable to init backtrace panel.");
      goto ptrace_error;
   }

   unw_cursor_t cursor;
   int ret = unw_init_remote(&cursor, addrSpace, context);
   if (ret < 0) {
      xAsprintf(&this->error, "libunwind cursor: ret=%d", ret);
      goto context_error;
   }

   int index = 0;
   do {
      char procName[256] = "?";
      unw_word_t offset;
      unw_word_t pc;

      if (unw_get_proc_name(&cursor, procName, sizeof(procName), &offset) == 0) {
         ret = unw_get_reg(&cursor, UNW_REG_IP, &pc);
         if (ret < 0) {
            xAsprintf(&this->error, "unable to get register rip : %d", ret);
            break;
         }

         Frame* frame = Frame_new();
         frame->index = index;
         frame->address = pc;
         frame->offset = offset;
         frame->isSignalFrame = unw_is_signal_frame(&cursor);
         xAsprintf(&frame->functionName, "%s", procName);
         Panel_add(super, (Object*)frame);
      }
      index++;
   } while (unw_step(&cursor) > 0 && index < MAX_FRAME);

context_error:
   _UPT_destroy(context);

ptrace_error:
   ptrace(PTRACE_DETACH, pid, 0, 0);

addr_space_error:
   unw_destroy_addr_space(addrSpace);
}

BacktracePanel* BacktracePanel_new(const Process* process) {
   BacktracePanel* this = CallocThis(BacktracePanel);
   this->process = process;

   Panel* super = (Panel*) this;
   Panel_init(super, 1, 1, 1, 1, Class(Frame), true, FunctionBar_new(BacktraceScreenFunctions, BacktraceScreenKeys, BacktraceScreenEvents));
   BacktracePanel_getFrames(this);
   if (this->error) {
      Panel_prune(super);

      Frame *errorFrame = Frame_new();
      errorFrame->error = xStrdup(this->error);
      errorFrame->isError = true;
      Panel_add(super, (Object *)errorFrame);
   }

   char *header = NULL;
   xAsprintf(&header, "Backtrace of '%s' (%d)", process->procComm, Process_getPid(process));
   Panel_setHeader(super, header);
   free(header);
   return this;
}

Frame* Frame_new(void) {
   Frame* this = CallocThis(Frame);
   return this;
}

static int Frame_compare(const void* object1, const void* object2) {
   const Frame* frame1 = (const Frame*)object1;
   const Frame* frame2 = (const Frame*)object2;
   return String_eq(frame1->functionName, frame2->functionName);
}

static void Frame_delete(Object* object) {
   Frame* this = (Frame*)object;
   if (this->functionName) {
      free(this->functionName);
   }

   if (this->isError && this->error) {
      free(this->error);
   }

   free(this);
}

void BacktracePanel_delete(Object* object) {
   BacktracePanel* this = (BacktracePanel*)object;
   if (this->error) {
      free(this->error);
   }
   Panel_delete(object);
}

const PanelClass BacktracePanel_class = {
   .super = {
      .extends = Class(Panel),
      .delete = BacktracePanel_delete,
   },
};

const ObjectClass Frame_class = {
   .extends = Class(Object),
   .compare = Frame_compare,
   .delete = Frame_delete,
   .display = Frame_display,
};

#endif /* HTOP_LINUX && HAVE_LIBUNWIND_PTRACE */
