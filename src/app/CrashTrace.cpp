#include "CrashTrace.h"

#include <windows.h>

#include <dbghelp.h>

#include <cstdio>
#include <cstdlib>

namespace crashtrace {

namespace {

LONG WINAPI onCrash(EXCEPTION_POINTERS* info)
{
    HANDLE process = GetCurrentProcess();
    HANDLE thread = GetCurrentThread();
    SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);
    SymInitialize(process, nullptr, TRUE);
    std::fprintf(stderr, "\n*** Plantage : exception 0x%08lX à %p (thread %lu)\n", info->ExceptionRecord->ExceptionCode,
                 info->ExceptionRecord->ExceptionAddress, GetCurrentThreadId());
    CONTEXT ctx = *info->ContextRecord;
    STACKFRAME64 frame{};
    frame.AddrPC.Offset = ctx.Rip;
    frame.AddrPC.Mode = AddrModeFlat;
    frame.AddrFrame.Offset = ctx.Rbp;
    frame.AddrFrame.Mode = AddrModeFlat;
    frame.AddrStack.Offset = ctx.Rsp;
    frame.AddrStack.Mode = AddrModeFlat;
    char buffer[sizeof(SYMBOL_INFO) + 512]{};
    auto* symbol = reinterpret_cast<SYMBOL_INFO*>(buffer);
    for (int i = 0; i < 48; ++i) {
        if (!StackWalk64(IMAGE_FILE_MACHINE_AMD64, process, thread, &frame, &ctx, nullptr, SymFunctionTableAccess64,
                         SymGetModuleBase64, nullptr) ||
            frame.AddrPC.Offset == 0)
            break;
        symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
        symbol->MaxNameLen = 511;
        DWORD64 disp = 0;
        IMAGEHLP_LINE64 line{};
        line.SizeOfStruct = sizeof(line);
        DWORD lineDisp = 0;
        const bool hasSym = SymFromAddr(process, frame.AddrPC.Offset, &disp, symbol);
        const bool hasLine = SymGetLineFromAddr64(process, frame.AddrPC.Offset, &lineDisp, &line);
        std::fprintf(stderr, "  #%02d %s", i, hasSym ? symbol->Name : "?");
        if (hasLine)
            std::fprintf(stderr, "  (%s:%lu)", line.FileName, line.LineNumber);
        std::fprintf(stderr, "\n");
    }
    std::fflush(stderr);
    return EXCEPTION_EXECUTE_HANDLER;
}

} // namespace

void install()
{
    if (GetEnvironmentVariableA("PICAZOO_CRASH_TRACE", nullptr, 0) > 0)
        SetUnhandledExceptionFilter(onCrash);
}

} // namespace crashtrace
