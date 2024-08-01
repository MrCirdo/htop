#ifndef HEADER_BacktraceScreen
#define HEADER_BacktraceScreen
/*
htop - BacktraceScreen.h
(C) 2023 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include <stddef.h>
#include <stdbool.h>

#include "Object.h"
#include "Panel.h"
#include "Process.h"

typedef struct BacktracePanelPrintingHelper_ {
   size_t maxAddressLength;
   size_t maxDemangledFunctionNameLength;
   size_t maxFunctionNameLength;
   size_t maxNumberFrameLength;
   size_t maxObjectPathLength;
} BacktracePanelPrintingHelper;

typedef enum BacktraceScreenDisplayOptions_ {
   NO_OPTION = 0,
   DEMANGLE_NAME_FUNCTION = 1 << 0,
   SHOW_FULL_PATH_OBJECT = 1 << 1,
} BacktraceScreenDisplayOptions;

typedef struct BacktracePanel_ {
   Panel super;
   const Process* process;
   bool error;
   BacktracePanelPrintingHelper printingHelper;
   BacktraceScreenDisplayOptions displayOptions;
} BacktracePanel;

typedef struct BacktraceFrame_ {
   Object super;

   int index;
   size_t address;
   size_t offset;
   char* functionName;
   char* demangleFunctionName;
   char* objectPath;
   bool isSignalFrame;

   const BacktracePanel* backtracePanel;
} BacktraceFrame;

BacktracePanel* BacktracePanel_new(const Process* process);
void BacktracePanel_delete(Object* object);
BacktraceFrame* BacktraceFrame_new(void);

extern const PanelClass BacktracePanel_class;
extern const ObjectClass BacktraceFrame_class;

#endif

