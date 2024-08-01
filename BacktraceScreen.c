/*
htop - BacktraceScreen.h
(C) 2023 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "BacktraceScreen.h"

#if defined(HAVE_BACKTRACE)

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

#include <ncursesw/curses.h>

#include "CRT.h"
#include "FunctionBar.h"
#include "ListItem.h"
#include "Macros.h"
#include "Object.h"
#include "Panel.h"
#include "Platform.h"
#include "Process.h"
#include "RichString.h"
#include "Vector.h"
#include "XUtils.h"

#define BACKTRACE_FRAME_HEADER_INDEX -1
#define BACKTRACE_FRAME_ERROR_ROW -2

#define BACKTRACE_PANEL_OPTIONS_NAME_DEMANGLE "Demangle"
#define BACKTRACE_PANEL_OPTIONS_NAME_RAW "Raw"

#define BACKTRACE_PANEL_OPTIONS_OBJECT_PATH_FULL "Full Path"
#define BACKTRACE_PANEL_OPTIONS_OBJECT_PATH_BASENAME "Basename"

static const char* const BacktraceScreenFunctions[] = {
   "Refresh",
#if defined(HAVE_DEMANGLING)
   BACKTRACE_PANEL_OPTIONS_NAME_RAW,
#endif
   BACKTRACE_PANEL_OPTIONS_OBJECT_PATH_FULL,
   "Done   ",
   NULL
};

static const char* const BacktraceScreenKeys[] = {
   "F1",
#if defined(HAVE_DEMANGLING)
   "F2",
#endif
   "F3",
   "Esc",
   NULL
};

static const int BacktraceScreenEvents[] = {
   KEY_F(1),
#if defined(HAVE_DEMANGLING)
   KEY_F(2),
#endif
   KEY_F(3),
   27,
};

static void BacktraceFrame_displayHeader(const BacktraceFrame* const frame, RichString* out) {
   const BacktracePanelPrintingHelper* const printingHelper = &frame->backtracePanel->printingHelper;
   assert(printingHelper);

   const BacktraceScreenDisplayOptions displayOptions = frame->backtracePanel->displayOptions;

   int maxFunctionNameLength = printingHelper->maxFunctionNameLength;
   if ((displayOptions& DEMANGLE_NAME_FUNCTION) == DEMANGLE_NAME_FUNCTION &&
      printingHelper->maxDemangledFunctionNameLength > 0) {
      maxFunctionNameLength = printingHelper->maxDemangledFunctionNameLength;
   }

   char* line = NULL;
   int len = xAsprintf(&line, "%-*s %-*s %-*s %-*s",
                       (int)printingHelper->maxNumberFrameLength, "#FR",
                       (int)printingHelper->maxAddressLength, "ADDRESS",
                       (int)maxFunctionNameLength, "NAME",
                       (int)printingHelper->maxObjectPathLength, "PATH");

   RichString_appendnAscii(out, CRT_colors[DEFAULT_COLOR], line, len);
   free(line);
}

static void Frame_display(const Object* super, RichString* out) {
   const BacktraceFrame* const frame = (const BacktraceFrame* const)super;
   assert(frame);

   if (frame->index == BACKTRACE_FRAME_HEADER_INDEX) {
      BacktraceFrame_displayHeader(frame, out);
      return;
   }

   const BacktracePanelPrintingHelper* const printingHelper = &frame->backtracePanel->printingHelper;
   assert(printingHelper);

   const BacktraceScreenDisplayOptions displayOptions = frame->backtracePanel->displayOptions;

   char* functionName = frame->functionName;
   int maxFunctionNameLength = printingHelper->maxFunctionNameLength;
   if ((displayOptions& DEMANGLE_NAME_FUNCTION) == DEMANGLE_NAME_FUNCTION && printingHelper->maxDemangledFunctionNameLength > 0) {
      maxFunctionNameLength = printingHelper->maxDemangledFunctionNameLength;
      if (frame->demangleFunctionName) {
         functionName = frame->demangleFunctionName;
      }
   }

   xAsprintf(&functionName, "%s+0x%lx", functionName, frame->offset);

   char *fullObjectPath = frame->objectPath;
   char *objectPath = NULL;
   if ((displayOptions& SHOW_FULL_PATH_OBJECT) == SHOW_FULL_PATH_OBJECT) {
      size_t n = 0;
      char ** splitted_by_slash = String_split(fullObjectPath, '/', &n);
      objectPath = xStrdup(splitted_by_slash[n - 1]);
      String_freeArray(splitted_by_slash);
   }

   char* line = NULL;
   int len = xAsprintf(&line, "%-*d 0x%-*lx %-*s %-*s",
                       (int)printingHelper->maxNumberFrameLength, frame->index,
                       (int)printingHelper->maxAddressLength - (int)strlen("0x"), frame->address,
                       (int)maxFunctionNameLength, functionName,
                       (int)printingHelper->maxObjectPathLength, objectPath ? objectPath : fullObjectPath);

   RichString_appendnAscii(out, CRT_colors[DEFAULT_COLOR], line, len);

   if (objectPath) {
      free(objectPath);
   }

   if (functionName) {
      free(functionName);
   }

   free(line);
}

static int Frame_compare(const void* object1, const void* object2) {
   const BacktraceFrame* frame1 = (const BacktraceFrame*)object1;
   const BacktraceFrame* frame2 = (const BacktraceFrame*)object2;
   return String_eq(frame1->functionName, frame2->functionName);
}

static void Frame_delete(Object* object) {
   BacktraceFrame* this = (BacktraceFrame*)object;
   if (this->functionName) {
      free(this->functionName);
   }

   if (this->demangleFunctionName) {
      free(this->demangleFunctionName);
   }

   if (this->objectPath) {
      free(this->objectPath);
   }

   free(this);
}

static void BacktracePanel_setError(BacktracePanel* this, char* error) {
   assert(error != NULL);
   assert(this->error == false);

   Panel* super = &this->super;
   Panel_prune(super);

   Vector* lines = this->super.items;
   Vector_delete(lines);
   this->super.items = Vector_new(Class(ListItem), true, DEFAULT_SIZE);
   Panel_set(super, 0, (Object*)ListItem_new(error, 0));
}

static void BacktracePanel_makePrintingHelper(const BacktracePanel* const this, BacktracePanelPrintingHelper *const printingHelper){
   Vector* lines = this->super.items;
   for (int i = 0; i < Vector_size(lines); i++) {
      BacktraceFrame* frame = (BacktraceFrame*)Vector_get(lines, i);
      frame->backtracePanel = this;

      size_t digitOfOffsetFrame = strlen("+0x") + countDigit(frame->offset, 16);
      if (frame->demangleFunctionName) {
         size_t demangledFunctionNameLength = strlen(frame->demangleFunctionName) + digitOfOffsetFrame;
         if (printingHelper->maxDemangledFunctionNameLength < demangledFunctionNameLength) {
            printingHelper->maxDemangledFunctionNameLength = demangledFunctionNameLength;
         }
      }

      if (frame->functionName) {
         size_t functionNameLength = strlen(frame->functionName) + digitOfOffsetFrame;
         if (printingHelper->maxFunctionNameLength < functionNameLength) {
            printingHelper->maxFunctionNameLength = functionNameLength;
         }
      }

      if (frame->objectPath) {
         size_t objectPathLength = strlen(frame->objectPath);
         if (printingHelper->maxObjectPathLength < objectPathLength) {
            printingHelper->maxObjectPathLength = objectPathLength;
         }
      }

      size_t addressLength = strlen("0x") + countDigit(frame->address, 16);
      if (printingHelper->maxAddressLength < addressLength) {
         printingHelper->maxAddressLength = addressLength;
      }
   }

   int maxNumberFrameDigit = countDigit(Vector_size(lines), 10);
   printingHelper->maxNumberFrameLength = MAXIMUM(maxNumberFrameDigit, (int)strlen("#FR"));
}

static void BacktracePanel_populateFrames(BacktracePanel* this) {
   if (this->error) {
      return;
   }

   char* error = NULL;
   Vector* lines = this->super.items;

   BacktraceFrame* header =  BacktraceFrame_new();
   header->index = BACKTRACE_FRAME_HEADER_INDEX;
   Vector_add(lines, (Object*)header);

   this->super.needsRedraw = true;

   Platform_getBacktrace(lines, this->process, &error);
   if (error) {
      BacktracePanel_setError(this, error);
      free(error);
      return;
   }

   BacktracePanelPrintingHelper* printingHelper = &this->printingHelper;
   BacktracePanel_makePrintingHelper(this, printingHelper);
}

BacktracePanel* BacktracePanel_new(const Process* process) {
   BacktracePanel* this = AllocThis(BacktracePanel);
   this->process = process;
   this->error = false;
   this->printingHelper.maxObjectPathLength = 0;
   this->printingHelper.maxFunctionNameLength = 0;
   this->printingHelper.maxDemangledFunctionNameLength = 0;
   this->displayOptions = DEMANGLE_NAME_FUNCTION;

   Panel* super = (Panel*) this;
   Panel_init(super, 1, 1, 1, 1, Class(BacktraceFrame), true,
              FunctionBar_new(BacktraceScreenFunctions, BacktraceScreenKeys,
                              BacktraceScreenEvents));

   char* header = NULL;
   xAsprintf(& header, "Backtrace of '%s' (%d)", process->procComm, Process_getPid(process));
   Panel_setHeader(super, header);
   free(header);

   BacktracePanel_populateFrames(this);
   return this;
}

BacktraceFrame* BacktraceFrame_new(void) {
   BacktraceFrame* this = AllocThis(BacktraceFrame);
   this->index = 0;
   this->address = 0;
   this->offset = 0;
   this->functionName = NULL;
   this->demangleFunctionName = NULL;
   this->objectPath = NULL;
   this->isSignalFrame = false;
   return this;
}

static HandlerResult BacktracePanel_eventHandler(Panel* super, int ch) {
   BacktracePanel* this = (BacktracePanel*)super;
   BacktraceScreenDisplayOptions* displayOptions = &this->displayOptions;

   HandlerResult result = IGNORED;
   switch (ch) {
   case KEY_F(1):
      Panel_prune(super);
      BacktracePanel_populateFrames(this);
      break;

#if defined(HAVE_DEMANGLING)
   case KEY_F(2):
      if ((*displayOptions & DEMANGLE_NAME_FUNCTION) == DEMANGLE_NAME_FUNCTION) {
         *displayOptions &= ~DEMANGLE_NAME_FUNCTION;
         FunctionBar_setLabel(super->defaultBar, KEY_F(2), BACKTRACE_PANEL_OPTIONS_NAME_DEMANGLE);
      } else {
         *displayOptions |= DEMANGLE_NAME_FUNCTION;
         FunctionBar_setLabel(super->defaultBar, KEY_F(2), BACKTRACE_PANEL_OPTIONS_NAME_RAW);
      }
      this->super.needsRedraw = true;
      break;
#endif
   case 'p':
   case KEY_F(3):
         if ((*displayOptions & SHOW_FULL_PATH_OBJECT) == SHOW_FULL_PATH_OBJECT) {
            *displayOptions &= ~SHOW_FULL_PATH_OBJECT;
            FunctionBar_setLabel(super->defaultBar, KEY_F(3), BACKTRACE_PANEL_OPTIONS_OBJECT_PATH_FULL);
         } else {
            FunctionBar_setLabel(super->defaultBar, KEY_F(3), BACKTRACE_PANEL_OPTIONS_OBJECT_PATH_BASENAME);
            *displayOptions |= SHOW_FULL_PATH_OBJECT;
         }
      this->super.needsRedraw = true;
      break;
   }
   return result;
}

void BacktracePanel_delete(Object* object) {
   Panel_delete(object);
}

const PanelClass BacktracePanel_class = {
   .super = {
      .extends = Class(Panel),
      .delete = BacktracePanel_delete,
   },
   .eventHandler = BacktracePanel_eventHandler,
};

const ObjectClass BacktraceFrame_class = {
   .extends = Class(Object),
   .compare = Frame_compare,
   .delete = Frame_delete,
   .display = Frame_display,
};
#endif
