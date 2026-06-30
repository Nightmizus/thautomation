#include "debugger.h"

#include <cstdio>
#include <tchar.h>
#include <psapi.h>
#include <strsafe.h>
#include <vector>
#include <atlstr.h>  
#include "winmanip.h"

extern PROCESS_INFORMATION pi;

static void PrintExceptionModule(void* address)
{
	HMODULE modules[1024];
	DWORD needed = 0;
	if (!EnumProcessModules(pi.hProcess, modules, sizeof(modules), &needed))
		return;

	for (DWORD i = 0; i < needed / sizeof(HMODULE); ++i)
	{
		MODULEINFO info{};
		if (!GetModuleInformation(pi.hProcess, modules[i], &info, sizeof(info)))
			continue;

		auto base = reinterpret_cast<uintptr_t>(info.lpBaseOfDll);
		auto end = base + info.SizeOfImage;
		auto target = reinterpret_cast<uintptr_t>(address);
		if (target < base || target >= end)
			continue;

		TCHAR modulePath[MAX_PATH]{};
		GetModuleFileNameEx(pi.hProcess, modules[i], modulePath, MAX_PATH);
		_tprintf(_T(" in %s+0x%Ix"), modulePath, target - base);
		return;
	}
}

DWORD OnLoadDllDebugEvent(const LPDEBUG_EVENT DebugEv)
{
	//if (DebugEv->u.LoadDll.lpImageName)
	//{
	//	LPTSTR path = GetFilePathFromHandle(DebugEv->u.LoadDll.hFile);
	//	LPTSTR name = GetFileName(path);
	//	// printf("DLL Load: %s\n", name);
	//	delete[] path;
	//	delete[] name;
	//}

	return DBG_CONTINUE;
}

DWORD OnOutputDebugStringEvent(const LPDEBUG_EVENT debug_event)
{
	CStringW strEventMessage;
	OUTPUT_DEBUG_STRING_INFO & DebugString = debug_event->u.DebugString;
	WCHAR *msg = new WCHAR[DebugString.nDebugStringLength];

	ReadProcessMemory(pi.hProcess, DebugString.lpDebugStringData, msg, DebugString.nDebugStringLength, NULL);

	if (DebugString.fUnicode)
		strEventMessage = msg;
	else
		strEventMessage = (char*)msg;
	_tprintf("%ws", (LPCWSTR)strEventMessage);
	delete[]msg;
	return DBG_CONTINUE;
}

void EnterDebugLoop(const LPDEBUG_EVENT DebugEv)
{
	bool continue_dbg = true;
	DWORD dwContinueStatus = DBG_CONTINUE;
	while(continue_dbg)
	{
		WaitForDebugEvent(DebugEv, INFINITE);
		switch (DebugEv->dwDebugEventCode)
		{
		case EXCEPTION_DEBUG_EVENT:
			dwContinueStatus = DBG_EXCEPTION_NOT_HANDLED;
			// Process the exception code. When handling 
			// exceptions, remember to set the continuation 
			// status parameter (dwContinueStatus). This value 
			// is used by the ContinueDebugEvent function. 

			switch (DebugEv->u.Exception.ExceptionRecord.ExceptionCode)
			{
			case EXCEPTION_ACCESS_VIOLATION:
				// First chance: Pass this on to the system. 
				// Last chance: Display an appropriate error. 
				if (!DebugEv->u.Exception.dwFirstChance)
				{
					void* addr = DebugEv->u.Exception.ExceptionRecord.ExceptionAddress;
					printf("EXCEPTION_ACCESS_VIOLATION at %p", addr);
					PrintExceptionModule(addr);
					printf("\n");
				}
				break;

			case EXCEPTION_BREAKPOINT:
				// First chance: Display the current 
				// instruction and register values. 
				dwContinueStatus = DBG_CONTINUE;
				break;

			case EXCEPTION_DATATYPE_MISALIGNMENT:
				// First chance: Pass this on to the system. 
				// Last chance: Display an appropriate error. 
				if (!DebugEv->u.Exception.dwFirstChance)
					printf("EXCEPTION_DATATYPE_MISALIGNMENT\n");
				break;

			case EXCEPTION_SINGLE_STEP:
				// First chance: Update the display of the 
				// current instruction and register values. 
				dwContinueStatus = DBG_CONTINUE;
				break;

			case DBG_CONTROL_C:
				// First chance: Pass this on to the system. 
				// Last chance: Display an appropriate error. 
				if (!DebugEv->u.Exception.dwFirstChance)
					printf("DBG_CONTROL_C\n");
				break;

			default:
				// Handle other exceptions. 
				if (!DebugEv->u.Exception.dwFirstChance)
					printf("EXCEPTION_UNKNOWN\n");
				break;
			}

			break;

		case LOAD_DLL_DEBUG_EVENT:
			dwContinueStatus = OnLoadDllDebugEvent(DebugEv);
			break;
		case OUTPUT_DEBUG_STRING_EVENT:
			// Display the output debugging string. 
			dwContinueStatus = OnOutputDebugStringEvent(DebugEv);
			break;
		case EXIT_PROCESS_DEBUG_EVENT:
			continue_dbg = false;

		}
		ContinueDebugEvent(DebugEv->dwProcessId,
			DebugEv->dwThreadId,
			dwContinueStatus);
	}
}
