#include "stdafx.h"
#include "resource.h"

_NT_BEGIN
#include "../winZ/window.h"
#include "../winZ/wic.h"
#include "CM.h"
#include "CertServer.h"
#include "util.h"
#include "pfx.h"
#include "BthDlg.h"

extern volatile const UCHAR guz;

//struct __declspec(uuid("9e16888d-720b-4e80-8861-b965ebb7f216")) GUID_BTH_RFCOMM_FIREWALL_DEVICE_INTERFACE;

class NewScDlg : public BthDlg
{
	HANDLE_SDP _hRecord = 0;
	PFX_CONTEXT* _ctx;
	PfxSocket* _pSocket = 0;
	LONG _dwFlags = 7;
	ULONG _nStage = 0;
	WCHAR _defChar;

	enum { fBth, fPin, fName, fListen };

	void ShowPass(HWND hwnd, WCHAR c)
	{
		SendMessage(hwnd, EM_SETPASSWORDCHAR, c, 0);
		InvalidateRect(hwnd, 0, TRUE);
	}

	void ShowPass(HWND hwndDlg, BOOL b)
	{
		WCHAR c = b ? 0 : _defChar;
		ShowPass(GetDlgItem(hwndDlg, IDC_EDIT4), c);
		ShowPass(GetDlgItem(hwndDlg, IDC_EDIT3), c);
	}

	virtual void OnPortRemoved(BTH_RADIO* port)
	{
		if (PfxSocket* pSocket = _pSocket)
		{
			if (pSocket->get_id() == port->_id)
			{
				if (HANDLE_SDP hRecord = _hRecord)
				{
					UnregisterService(port->_hDevice, hRecord);
					_hRecord = 0;
				}

				pSocket->Close();
				pSocket->Release();
				_pSocket = 0;
			}
		}
	}
	
	virtual void OnAnyBthExist(HWND hwndDlg, BOOL b)
	{
		UpdateUI(hwndDlg, fBth, b);
	}
		
	virtual ULONG GetComboId()
	{
		return IDC_COMBO1;
	}

	void UpdateUI(HWND hwndDlg, LONG f, BOOL b)
	{
		if (b)
		{
			_bittestandreset(&_dwFlags, f);

			if (_dwFlags)
			{
				return;
			}
		}
		else
		{
			if (_bittestandset(&_dwFlags, f))
			{
				return ;
			}
		}

		EnableWindow(GetDlgItem(hwndDlg, IDOK), b);
	}

	HRESULT OnInitDialog(HWND hwndDlg)
	{
		SendDlgItemMessage(hwndDlg, IDC_EDIT1, EM_SETCUEBANNER, FALSE, (LPARAM)L"Card Name");
		SendDlgItemMessage(hwndDlg, IDC_EDIT3, EM_SETCUEBANNER, FALSE, (LPARAM)L"Enter PIN");
		SendDlgItemMessage(hwndDlg, IDC_EDIT4, EM_SETCUEBANNER, FALSE, (LPARAM)L"Confirm PIN");
		_defChar = (WCHAR)SendMessage(GetDlgItem(hwndDlg, IDC_EDIT3), EM_GETPASSWORDCHAR, 0, 0);

		return BthDlg::OnInitDialog(hwndDlg);
	}

	void Start(HWND hwndDlg)
	{
		if (_pSocket)
		{
			return ;
		}

		if (BTH_RADIO* port = GetSelectedPort())
		{
			PFX_CONTEXT* ctx = _ctx;

			NTSTATUS status = ctx->InitUserUuid(hwndDlg, IDC_EDIT2);

			if (0 > status)
			{
				ShowErrorBox(hwndDlg, HRESULT_FROM_NT(status), L"Invalid PIN");
				return;
			}

			if (ctx->Init(hwndDlg))
			{
				HRESULT hr = StartServer(hwndDlg, port);

				if (0 > hr)
				{
					ShowErrorBox(hwndDlg, hr, L"Fail Start Server", MB_ICONHAND);
				}
				else
				{
					port->EnableScan();

					EnableWindow(GetDlgItem(hwndDlg, IDC_EDIT1), FALSE);
					EnableWindow(GetDlgItem(hwndDlg, IDC_EDIT2), FALSE);
					EnableWindow(GetDlgItem(hwndDlg, IDC_COMBO1), FALSE);
					EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON1), TRUE);
					UpdateUI(hwndDlg, fListen, FALSE);
				}
			}
		}
	}

	HRESULT StartServer(HWND hwndDlg, _In_ BTH_RADIO* port, CSocketObject* pAddress)
	{
		DbgPrint("%s\n", __FUNCTION__);

		SOCKADDR_BTH asi = { AF_BTH, port->_btAddr, {}, BT_PORT_ANY };

		HRESULT dwError = pAddress->CreateAddress((PSOCKADDR)&asi, sizeof(asi), BTHPROTO_RFCOMM);

		if (NOERROR == dwError)
		{
			SOCKET_ADDRESS LocalAddr = { (PSOCKADDR)&asi, sizeof(asi) };

			if (NOERROR == (dwError = pAddress->GetLocalAddr(&LocalAddr)))
			{
				if (PfxSocket* p = new PfxSocket(hwndDlg, port->_id, pAddress, _ctx))
				{
					if (NOERROR == (dwError = p->Create(0, AF_BTH, BTHPROTO_RFCOMM)))
					{
						dwError = p->Listen();

						if (NOERROR == dwError)
						{
							if (0 <= (dwError = RegisterService(port->_hDevice, _ctx, (UCHAR)asi.port, &_hRecord)))
							{
								DbgPrint("[SDP: %I64x]\n", _hRecord);

								_pSocket = p;

								port->EnableScan();

								return S_OK;
							}

							DbgPrint("RegisterService=%x\n", dwError);

							dwError |= FACILITY_NT_BIT;
						}

						p->Close();
					}

					p->Release();
				}
			}
		}

		return HRESULT_FROM_WIN32(dwError);
	}

	HRESULT StartServer(_In_ HWND hwndDlg, _In_ BTH_RADIO* port)
	{
		HRESULT status = HRESULT_FROM_NT(STATUS_NO_MEMORY);

		if (CSocketObject* pAddress = new CSocketObject)
		{
			status = StartServer(hwndDlg, port, pAddress);

			pAddress->Release();
		}

		return status;
	}

	void Stop()
	{
		if (PfxSocket* pSocket = _pSocket)
		{
			if (HANDLE_SDP hRecord = _hRecord)
			{
				if (BTH_RADIO* port = getByRadioId(pSocket->get_id()))
				{
					UnregisterService(port->_hDevice, hRecord);
					_hRecord = 0;
				}
			}

			pSocket->Close();
			pSocket->Release();
			_pSocket = 0;
		}
	}

	virtual INT_PTR DialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
	{
		switch (uMsg)
		{
		case WM_DESTROY:
			BthDlg::OnDestroy();
			break;

		case WM_INITDIALOG:
			if (lParam = OnInitDialog(hwndDlg))
			{
				EndDialog(hwndDlg, lParam);
			}
			return FALSE;

		case WM_DEVICECHANGE:
			OnInterfaceChange(hwndDlg, wParam, (PDEV_BROADCAST_DEVICEINTERFACE)lParam);
			return 0;

		case WM_COMMAND:
			switch (wParam)
			{
			case IDCANCEL:
				EndDialog(hwndDlg, STATUS_CANCELLED);
				break;
			case MAKEWPARAM(IDC_EDIT1, EN_CHANGE):
				UpdateUI(hwndDlg, fName, (ULONG)GetWindowTextLength((HWND)lParam) - 1 < 63);
				break;
			case MAKEWPARAM(IDC_EDIT2, EN_CHANGE):
				UpdateUI(hwndDlg, fPin, GetWindowTextLength((HWND)lParam) == 6);
				break;
			
			case MAKEWPARAM(IDC_COMBO1, CBN_SELCHANGE):
				SetSelected(ComboBox_GetCurSel((HWND)lParam));
				break;

			case IDOK:
				Start(hwndDlg);
				break;
			case IDC_BUTTON1:
				Stop();
				break;

			case MAKEWPARAM(IDC_CHECK1, BN_CLICKED):
				ShowPass(hwndDlg, SendMessage((HWND)lParam, BM_GETCHECK, 0, 0) == BST_CHECKED);
				break;
			}
			return 0;

		case WM_SRV_CLOSED:
			EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON1), FALSE);
			EnableWindow(GetDlgItem(hwndDlg, IDC_EDIT1), TRUE);
			EnableWindow(GetDlgItem(hwndDlg, IDC_EDIT2), TRUE);
			if (GetBthCount()) 
			{
				EnableWindow(GetDlgItem(hwndDlg, IDC_COMBO1), TRUE);
				UpdateUI(hwndDlg, fListen, TRUE);
			}
			else
			{
				_bittestandreset(&_dwFlags, fListen);
			}
			break;

		case WM_DONE:
			MessageBox(hwndDlg, 0, L"certificate received !", MB_ICONINFORMATION);
			EndDialog(hwndDlg, IDOK);
			break;
		}

		return ZDlg::DialogProc(hwndDlg, uMsg, wParam, lParam);
	}

public:

	NewScDlg(PFX_CONTEXT* ctx) : _ctx(ctx){}
};

void ImportCert(HWND hwndDlg)
{
	if (PFX_CONTEXT* ctx = new PFX_CONTEXT)
	{
		HRESULT hr = ctx->OpenFolder();
		if (0 > hr)
		{
			ShowErrorBox(0, hr, L"Can not create ScCert folder");
		}
		else
		{
			NewScDlg dlg(ctx);

			dlg.DoModal((HINSTANCE)&__ImageBase, MAKEINTRESOURCEW(IDD_DIALOG3), hwndDlg, 0);

			__nop();
		}
		ctx->Release();
	}
}

_NT_END