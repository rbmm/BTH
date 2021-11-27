#include "stdafx.h"
#include "resource.h"

_NT_BEGIN
#include "../asio/io.h"
#include "../winZ/window.h"
#include "l2cap.h"

class L2Dlg : public ZDlg
{
	L2capSocket* _p = 0;

	void OnOk(HWND hwndDlg)
	{
		WCHAR sz[32], *c;
		if (GetDlgItemTextW(hwndDlg, IDC_EDIT1, sz, _countof(sz)))
		{
			if (BTH_ADDR btAddr = _wcstoui64(sz, &c, 16))
			{
				if (*c == ':')
				{
					ULONG Psm = wcstoul(c + 1, &c, 16);
					if (Psm && Psm < 0x10000 && !*c)
					{

						if (L2capSocket* p = new L2capSocket(hwndDlg, "test L2"))
						{
							if (0 <= p->Create())
							{
								if (0 <= p->Connect(btAddr, (USHORT)Psm))
								{
									_p = p;
									return ;
								}
							}
							p->Release();
						}
					}
				}
			}
		}
	}

	void CloseSocket()
	{
		if (L2capSocket* p = _p)
		{
			p->Close();
			p->Release();
			_p = 0;
		}
	}

	void Send(HWND hwndDlg)
	{
		WCHAR sz[32], *c;
		if (ULONG cch = GetDlgItemTextW(hwndDlg, IDC_EDIT2, sz, _countof(sz)))
		{
			ULONG len;

			if (*sz == '[' && (len = wcstoul(sz + 1, &c, 16)) && len <= MAXUSHORT && *c==']' && !c[1])
			{
				if (CDataPacket* packet = new(len) CDataPacket)
				{
					memset(packet->getData(), '*', len);
					packet->setDataSize(len);
					_p->Send(packet);
					packet->Release();
				}
			}
			else
			{
				_p->Send(sz, cch*sizeof(WCHAR));
			}
		}
	}

	void Recv()
	{
		if (CDataPacket* packet = new(0x2000) CDataPacket)
		{
			_p->Recv(packet);
			packet->Release();
		}
	}

	virtual INT_PTR DialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
	{
		switch (uMsg)
		{
		case WM_CONNECT:
			if (lParam < 0)
			{
		case WM_DISCONNECT:
			CloseSocket();
			}
			break;
		case WM_DESTROY:
			CloseSocket();
			break;
		case WM_COMMAND:
			switch (wParam)
			{
			case IDC_BUTTON1:
				CloseSocket();
				break;
			case IDC_BUTTON2:
				if (_p) Send(hwndDlg);
				break;
			case IDC_BUTTON3:
				if (_p) Recv();
				break;

			case IDCANCEL:
				EndDialog(hwndDlg, 0);
				break;
			case IDOK:
				if (!_p) OnOk(hwndDlg);
				break;
			}
			break;
		}
		return ZDlg::DialogProc(hwndDlg, uMsg, wParam, lParam);
	}
};

void L2Test()
{
	L2Dlg dlg;
	dlg.DoModal((HINSTANCE)&__ImageBase, MAKEINTRESOURCEW(IDD_DIALOG2), 0, 0);
}

_NT_END