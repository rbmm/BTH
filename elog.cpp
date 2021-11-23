#include "stdafx.h"

_NT_BEGIN

#include "elog.h"

ELog& ELog::operator << (PCWSTR pcsz)
{
	if (HWND hwnd = _hwnd)
	{
		SendMessageW(hwnd, EM_SETSEL, MAXLONG, MAXLONG);
		SendMessageW(hwnd, EM_REPLACESEL, FALSE, (LPARAM)pcsz);
	}
	else
	{
		OutputDebugStringW(pcsz);
	}

	return *this;
}

ELog& ELog::operator () (PCWSTR format, ...)
{
	va_list args;
	va_start(args, format);
	int cch = 0;
	PWSTR buf = 0;

	while (0 < (cch = _vsnwprintf(buf, cch, format, args)))
	{
		if (buf)
		{
			operator <<(buf);
			break;
		}

		buf = (PWSTR)alloca(++cch * sizeof(WCHAR));
	}
	return *this;
}

ELog& ELog::operator << (HRESULT hr)
{
	HMODULE hmod = 0;
	ULONG dwFlags = FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_IGNORE_INSERTS|FORMAT_MESSAGE_FROM_SYSTEM;
	WCHAR fmt[] = L"// [%u]\r\n";

	if (hr & FACILITY_NT_BIT)
	{
		hr &= ~FACILITY_NT_BIT;
		dwFlags = FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_IGNORE_INSERTS|FORMAT_MESSAGE_FROM_HMODULE;
		static HMODULE hmod_nt = 0;
		if (!hmod_nt)
		{
			if (!(hmod = GetModuleHandleW(L"ntdll")))
			{
				return *this;
			}
			hmod_nt = hmod;
		}
		hmod = hmod_nt;
		fmt[5] = 'X';
	}

	operator()(fmt, hr);

	PWSTR buf;
	if (FormatMessageW(dwFlags, hmod, hr, 0, (PWSTR)&buf, 0, 0))
	{
		operator <<(buf);
		LocalFree(buf);
	}
	return *this;
}

_NT_END