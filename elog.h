#pragma once

class ELog
{
	HWND _hwnd = 0;
public:
	void Set(HWND hwnd) { _hwnd = hwnd; }

	ELog& operator << (PCWSTR pcsz);
	ELog& operator () (PCWSTR format, ...);
	ELog& operator << (HRESULT hr);

};