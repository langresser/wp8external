#pragma once

#include <windows.h>

#ifndef CREATE_SUSPENDED
#define CREATE_SUSPENDED 0x00000004
#endif

HANDLE WINAPI CreateThread(_In_opt_ LPSECURITY_ATTRIBUTES unusedThreadAttributes, _In_ SIZE_T unusedStackSize, _In_ LPTHREAD_START_ROUTINE lpStartAddress, _In_opt_ LPVOID lpParameter, _In_ DWORD dwCreationFlags, _Out_opt_ LPDWORD unusedThreadId);
DWORD WINAPI ResumeThread(_In_ HANDLE hThread);
BOOL WINAPI SetThreadPriority(_In_ HANDLE hThread, _In_ int nPriority);

VOID WINAPI Sleep(_In_ DWORD dwMilliseconds);
VOID WINAPI InitializeCriticalSection(_Out_ LPCRITICAL_SECTION lpCriticalSection);

DWORD WINAPI TlsAlloc();
BOOL WINAPI TlsFree(_In_ DWORD dwTlsIndex);
LPVOID WINAPI TlsGetValue(_In_ DWORD dwTlsIndex);
BOOL WINAPI TlsSetValue(_In_ DWORD dwTlsIndex, _In_opt_ LPVOID lpTlsValue);

void WINAPI TlsShutdown();
