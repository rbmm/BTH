#include "stdafx.h"
#include "resource.h"

_NT_BEGIN
#include "../asio/io.h"
#include "../winZ/window.h"
#include "l2cap.h"
#include "msg.h"

void DumpBytes(const UCHAR* pb, ULONG cb);

class L2TestSocket : public L2capSocket
{
	PCSTR name;
	HWND _hwnd;

protected:

	virtual ~L2TestSocket()
	{
		DbgPrint("%s<%s>\n", __FUNCTION__, name);
	}

private:

	virtual BOOL OnConnect(NTSTATUS status, _BRB_L2CA_OPEN_CHANNEL* OpenChannel);

	virtual void OnDisconnect(NTSTATUS status);

	virtual void OnRecv(NTSTATUS status, _BRB_L2CA_ACL_TRANSFER* acl);

	virtual void OnSend(NTSTATUS status, _BRB_L2CA_ACL_TRANSFER* acl);

public:

	L2TestSocket(HWND hwnd, PCSTR name) : name(name), _hwnd(hwnd)
	{
		DbgPrint("%s<%s>\n", __FUNCTION__, name);
	}
};

void L2TestSocket::OnRecv(NTSTATUS status, _BRB_L2CA_ACL_TRANSFER* acl)
{
	DbgPrint("%s<%p>(s=%x %x/%x)\n", __FUNCTION__, this, status, acl->RemainingBufferSize, acl->BufferSize);
	DumpBytes((PUCHAR)acl->Buffer, acl->BufferSize);
	SendMessageW(_hwnd, WM_RECV, 0, 0);
}

void L2TestSocket::OnSend(NTSTATUS status, _BRB_L2CA_ACL_TRANSFER* acl)
{
	DbgPrint("%s<%p>(s=%x %x/%x)\n", __FUNCTION__, this, status, acl->RemainingBufferSize, acl->BufferSize);
}

void L2TestSocket::OnDisconnect(NTSTATUS status)
{
	DbgPrint("%s<%p>(%x)\n", __FUNCTION__, this, status);
	PostMessageW(_hwnd, WM_DISCONNECT, 0, 0);
}

BOOL L2TestSocket::OnConnect(NTSTATUS status, _BRB_L2CA_OPEN_CHANNEL* OpenChannel)
{
	DbgPrint("%s<%p>%p(%x %x %x)=%x\n", __FUNCTION__, this, OpenChannel->ChannelHandle,
		OpenChannel->Hdr.Status, OpenChannel->Hdr.BtStatus, OpenChannel->Response, status);

	PostMessageW(_hwnd, WM_CONNECT, 0, HRESULT_FROM_NT(status));

	return TRUE;
}

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
						L2capSocket* p;
						if (p = _p)
						{
							p->Connect(btAddr, (USHORT)Psm);
							return ;
						}

						if (p = new L2TestSocket(hwndDlg, "test L2"))
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

			if (*sz == '[' && (len = wcstoul(sz + 1, &c, 16)) && len <= 0x100000 && *c==']' && !c[1])
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

	virtual INT_PTR DialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
	{
		switch (uMsg)
		{
		case WM_DISCONNECT:
			DbgPrint("WM_DISCONNECT\n");
			break;
		case WM_CONNECT:
			if (lParam < 0)
			{
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
				if (_p) _p->Disconnect();
				break;

			case IDCANCEL:
				EndDialog(hwndDlg, 0);
				break;
			case IDOK:
				OnOk(hwndDlg);
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