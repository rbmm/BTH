#include "stdafx.h"
#include "resource.h"

_NT_BEGIN

#include "../winZ/window.h"
#include "../winZ/wic.h"
#include "CM.h"
#include "msg.h"
#include "CertServer.h"
#include "util.h"
#include "pfx.h"
#include "elog.h"
#include "sdp.h"
#include "radio.h"
#include "BthDlg.h"
#include "BthRequest.h"
#include "ScSocket.h"

BOOL SC_Cntr::IsValid(ULONG cb)
{
	if (cb > sizeof(SC_Cntr) && Tag == scTag && subTag == bTag)
	{
		switch (algid)
		{
		case GIDS_RSA_1024_IDENTIFIER:
		case GIDS_RSA_2048_IDENTIFIER:
			if (ULONG kLen = PubKeyLength)
			{
				if (ULONG cLen = CertLength)
				{
					if (cb == FIELD_OFFSET(SC_Cntr, buf) + kLen + cLen)
					{
						UCHAR md5[MD5_HASH_SIZE];
						memcpy(md5, MD5, MD5_HASH_SIZE);
						RtlZeroMemory(MD5, sizeof(MD5));

						return 0 <= h_MD5(this, cb, MD5) && !memcmp(md5, MD5, MD5_HASH_SIZE);
					}
				}
			}
			break;
		}
	}

	return FALSE;
}

extern volatile const UCHAR guz;

class InsertScDlg : public BthDlg
{
	ELog log;
	BCRYPT_KEY_HANDLE _hKey = 0;
	ScSocket* _pSocket = 0;
	PBTH_DEVICE_INFO_LIST _pList = 0;
	LONG _dwInquireCount = 0;
	int _dwIndex;

	virtual void OnPortRemoved(BTH_RADIO* port)
	{
		if (ScSocket* pSocket = _pSocket)
		{
			if (pSocket->get_id() == port->_id)
			{
				pSocket->Close();
				pSocket->Release();
				_pSocket = 0;
			}
		}
	}

	virtual void OnAnyBthExist(HWND hwndDlg, BOOL b)
	{
		EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON1), b);
		EnableWindow(GetDlgItem(hwndDlg, IDOK), b && ComboBox_GetCount(GetDlgItem(hwndDlg, IDC_COMBO2)));
	}

	virtual ULONG GetComboId()
	{
		return IDC_COMBO3;
	}

	virtual void OnBthArrival(_In_ PCWSTR pszDeviceInterface)
	{
		log(L"++ %s\r\n", pszDeviceInterface);
	}

	virtual void OnBthRemoval(_In_ PCWSTR pszDeviceInterface)
	{
		log(L"-- %s\r\n", pszDeviceInterface);
	}

	virtual void OnNewRadio(_In_ BTH_RADIO* port, _In_ PCWSTR pszName)
	{
		log(L"++ [%I64X] %s\r\n", port->_btAddr, pszName);
	}

	// ++ inquire

	void LockInquire(HWND hwndDlg)
	{
		if (!_dwInquireCount++)
		{
			EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON1), FALSE);
			EnableWindow(GetDlgItem(hwndDlg, IDOK), FALSE);

			ComboBox_ResetContent(GetDlgItem(hwndDlg, IDC_COMBO2));

			if (PBTH_DEVICE_INFO_LIST pList = _pList)
			{
				LocalFree(pList);
				_pList = 0;
			}
			SendDlgItemMessageW(hwndDlg, IDC_PROGRESS1, PBM_SETMARQUEE, TRUE, 50);
			ShowWindow(GetDlgItem(hwndDlg, IDC_PROGRESS1), SW_SHOW);
		}
	}

	void UnlockInquire(HWND hwndDlg)
	{
		if (!--_dwInquireCount)
		{
			ShowWindow(GetDlgItem(hwndDlg, IDC_PROGRESS1), SW_HIDE);
			SendDlgItemMessageW(hwndDlg, IDC_PROGRESS1, PBM_SETMARQUEE, FALSE, 0);
			EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON1), TRUE);

			log(L"... Inquire\r\n");
		}
	}

	void RadioNotFound()
	{
		log << L"no bluetooth radio\r\n";
	}

	BTH_REQUEST* AllocateRequest(HWND hwndDlg, ULONG bthId, ULONG code, ULONG OutSize = 0)
	{
		if (BTH_REQUEST* irp = new(OutSize) BTH_REQUEST(hwndDlg, bthId, code))
		{
			LockInquire(hwndDlg);
			return irp;
		}

		return 0;
	}

	void StartInquire(HWND hwndDlg)
	{
		if (BTH_RADIO* port = GetSelectedPort())
		{
			if (BTH_REQUEST* irp = AllocateRequest(hwndDlg, port->_id, BTH_REQUEST::c_inq, sizeof(ULONG)))
			{
				BTH_DEVICE_INQUIRY hci_inc = { LAP_GIAC_VALUE, SDP_DEFAULT_INQUIRY_SECONDS / 2 };

				NTSTATUS status = NtDeviceIoControlFile(port->_hDevice, 0, 0, irp, irp, 
					IOCTL_BTH_DEVICE_INQUIRY, &hci_inc, sizeof(hci_inc), irp->Data, sizeof(ULONG));

				log(L"General Inquiry ... [%X]\r\n", status);

				irp->CheckStatus(status);

				//SendDlgItemMessageW(hwndDlg, IDC_PROGRESS1, PBM_SETMARQUEE, TRUE, 50);
			}
		}
	}

	void OnInquireEnd(HWND hwndDlg, BTH_REQUEST* irp)
	{
		NTSTATUS status = irp->Status;
		ULONG numOfDevices = *(PULONG)irp->Data;
		log(L"General Inquiry = %X [numOfDevices = %x] [%u ms]\r\n", status, numOfDevices, irp->GetIoTime());
		if (0 > status)
		{
__0:
			log << HRESULT_FROM_NT(status);
			return ;
		}

		ULONG bthId = irp->bthId;

		BTH_RADIO* port = getByRadioId(bthId);

		if (!port)
		{
			return RadioNotFound();
		}

		HANDLE hDevice = port->_hDevice;

		status = SyncIoctl(hDevice, IOCTL_BTH_GET_DEVICE_INFO, 0, 0, &numOfDevices, sizeof(numOfDevices));

		log(L"BTH_GET_DEVICE_INFO = %X [numOfDevices = %x]\r\n", status, numOfDevices);

		if (0 > status)
		{
			goto __0;
		}

		status = HRESULT_FROM_NT(STATUS_NOT_FOUND);

		if (numOfDevices)
		{
			ULONG size = FIELD_OFFSET(BTH_DEVICE_INFO_LIST, deviceList[numOfDevices]);

			status = STATUS_NO_MEMORY;

			if (PBTH_DEVICE_INFO_LIST p = (PBTH_DEVICE_INFO_LIST)LocalAlloc(0, size))
			{
				if (0 <= (status = SyncIoctl(hDevice, IOCTL_BTH_GET_DEVICE_INFO, 0, 0, p, size)))
				{
					status = STATUS_NOT_FOUND;

					if (numOfDevices = p->numOfDevices)
					{
						PBTH_DEVICE_INFO deviceList = p->deviceList;
						do 
						{
							ULONG flags = deviceList->flags;
							log(L"found: [%I64X] \"%S\" (%x %x)\r\n", 
								BDIF_ADDRESS & flags ? deviceList->address : 0, 
								BDIF_NAME & flags ? deviceList->name : "",
								BDIF_COD  & flags ? deviceList->classOfDevice : 0, 
								flags);

							if (BDIF_ADDRESS & flags)
							{
								status = 0;
								ConnectToSDP(hwndDlg, hDevice, bthId, deviceList->address);
							}

						} while (deviceList++, --numOfDevices);

						_pList = p;

						return ;
					}
				}

				LocalFree(p);
			}
		}

		if (0 > status)
		{
			goto __0;
		}
	}

	void ConnectToSDP(HWND hwndDlg, HANDLE hDevice, ULONG bthId, BTH_ADDR address)
	{
		if (BTH_REQUEST* irp = AllocateRequest(hwndDlg, bthId, BTH_REQUEST::c_cnt, sizeof(BTH_SDP_CONNECT)))
		{
			PBTH_SDP_CONNECT psc = (PBTH_SDP_CONNECT)irp->Data;
			psc->bthAddress = address;
			psc->fSdpConnect = 0;
			psc->hConnection = 0;
			psc->requestTimeout = SDP_REQUEST_TO_DEFAULT;

			irp->btAddr = address;

			NTSTATUS status = NtDeviceIoControlFile(hDevice, 0, 0, irp, irp, 
				IOCTL_BTH_SDP_CONNECT, psc, sizeof(BTH_SDP_CONNECT), psc, sizeof(BTH_SDP_CONNECT));

			log(L"SDP_CONNECT to [%I64X] ... [%X]\r\n", address, status);
			irp->CheckStatus(status);
		}
	}

	void OnSdpConnect(HWND hwndDlg, BTH_REQUEST* irp)
	{
		PBTH_SDP_CONNECT psc = (PBTH_SDP_CONNECT)irp->Data;
		NTSTATUS status = irp->Status;
		BTH_ADDR btAddr = irp->btAddr;
		log(L"SDP_CONNECT to [%I64X] = %X [%u ms]\r\n", btAddr, status, irp->GetIoTime());

		if (0 > status)
		{
__0:
			log << HRESULT_FROM_NT(status);
			return ;
		}

		ULONG bthId = irp->bthId;

		BTH_RADIO* port = getByRadioId(bthId);

		if (!port)
		{
			return RadioNotFound();
		}

		if (irp = AllocateRequest(hwndDlg, bthId, BTH_REQUEST::c_srh, FIELD_OFFSET(BTH_SDP_STREAM_RESPONSE, response[0x200])))
		{
			irp->hConnection = psc->hConnection;
			irp->btAddr = btAddr;

			BTH_SDP_SERVICE_ATTRIBUTE_SEARCH_REQUEST sss = { 
				psc->hConnection, 0, {{ {__uuidof(MyServiceClass)}, SDP_ST_UUID128}},
				{{SDP_ATTRIB_PROTOCOL_DESCRIPTOR_LIST, SDP_ATTRIB_PROTOCOL_DESCRIPTOR_LIST}}
			};

			status = NtDeviceIoControlFile(port->_hDevice, 0, 0, irp, irp, IOCTL_BTH_SDP_SERVICE_ATTRIBUTE_SEARCH, 
				&sss, sizeof(sss), irp->Data, FIELD_OFFSET(BTH_SDP_STREAM_RESPONSE, response[0x200]));

			log(L"SERVICE_SEARCH in [%I64X] ... [%X]\r\n", btAddr, status);
			irp->CheckStatus(status);
			if (0 > status)
			{
				goto __0;
			}
		}
	}

	void OnSdpSearch(HWND hwndDlg, BTH_REQUEST* irp)
	{
		NTSTATUS status = irp->Status;
		BTH_ADDR btAddr = irp->btAddr;
		log(L"SERVICE_SEARCH in [%I64X] = %X [%u ms]\r\n", btAddr, status, irp->GetIoTime());

		ULONG bthId = irp->bthId;
		BTH_RADIO* port = getByRadioId(bthId);

		if (!port)
		{
			return RadioNotFound();
		}

		SdpDisconnect(port->_hDevice, irp->hConnection);

		if (0 > status)
		{
			log << HRESULT_FROM_NT(status);
			return;
		}

		PBTH_SDP_STREAM_RESPONSE pr = (PBTH_SDP_STREAM_RESPONSE)irp->Data;
		PUCHAR resp = pr->response;

		PWSTR psz = 0;
		ULONG cch = 0;
		ULONG responseSize = pr->responseSize;
		while (CryptBinaryToStringW(resp, responseSize, CRYPT_STRING_HEX, psz, &cch))
		{
			if (psz)
			{
				log << psz;
				break;
			}

			psz = (PWSTR)alloca(cch * sizeof(WCHAR));
		}

		USHORT Port;

		//RFCOMM_PROTOCOL_UUID16 : UINT8
		if (GetProtocolValue(log, resp, responseSize, L2CAP_PROTOCOL_UUID16, sizeof(UINT16), &Port))
		{
			log(L"!! Found Device [%I64X:%X] !!\r\n", btAddr, Port);

			if (PBTH_DEVICE_INFO_LIST pList = _pList)
			{
				if (ULONG numOfDevices = pList->numOfDevices)
				{
					PBTH_DEVICE_INFO deviceList = pList->deviceList;

					do 
					{
						if (deviceList->address == btAddr)
						{
							deviceList->classOfDevice = Port;
							int i;
							WCHAR name[BTH_MAX_NAME_SIZE];
							if (deviceList->flags & BDIF_NAME)
							{
								i = MultiByteToWideChar(CP_UTF8, 0, deviceList->name, MAXULONG, name, _countof(name));
							}
							else
							{
								i = swprintf_s(name, _countof(name), L"[%I64X]", btAddr);
							}

							HWND hwndCB = GetDlgItem(hwndDlg, IDC_COMBO2);

							if (0 < i && 0 <= (i = ComboBox_AddString(hwndCB, name)))
							{
								ComboBox_SetItemData(hwndCB, i, (LPARAM)deviceList);
								ComboBox_SetCurSel(hwndCB, i);

								if (i == 0)
								{
									EnableWindow(hwndCB, TRUE);
									EnableWindow(GetDlgItem(hwndDlg, IDOK), TRUE);
								}
							}
							break;
						}
					} while (deviceList++, --numOfDevices);
				}
			}
		}
	}

	static void SdpDisconnect(HANDLE hDevice, ULONGLONG hConnection)
	{
		if (BTH_REQUEST* irp = new BTH_REQUEST(0, 0, BTH_REQUEST::c_dsc))
		{
			irp->CheckStatus(NtDeviceIoControlFile(hDevice, 0, 0, irp, irp, 
				IOCTL_BTH_SDP_DISCONNECT, &hConnection, sizeof(hConnection), 0, 0));
		}
	}

	// -- inquire
	HRESULT OpenFolder(_Out_ PHANDLE FileHandle)
	{
		HRESULT hr;

		UNICODE_STRING ObjectName {};
		ULONG Length = 0;
		while (Length = ExpandEnvironmentStringsW(L"\\??\\%APPDATA%\\ScCerts", ObjectName.Buffer, Length))
		{
			if (ObjectName.Buffer)
			{
				ObjectName.MaximumLength = (USHORT)(Length *= sizeof(WCHAR));
				ObjectName.Length = (USHORT)Length - sizeof(WCHAR);

				IO_STATUS_BLOCK iosb;
				OBJECT_ATTRIBUTES oa = { sizeof(oa), 0, &ObjectName };

				if (0 > (hr = NtOpenFile(FileHandle, FILE_LIST_DIRECTORY|SYNCHRONIZE, &oa, 
					&iosb, FILE_SHARE_VALID_FLAGS, FILE_DIRECTORY_FILE|FILE_SYNCHRONOUS_IO_NONALERT)))
				{
					return HRESULT_FROM_NT(hr);
				}

				return S_OK;
			}

			if (Length > MAXSHORT)
			{
				return HRESULT_FROM_NT(STATUS_NAME_TOO_LONG);
			}

			ObjectName.Buffer = (PWSTR)alloca(Length * sizeof(WCHAR));
		}

		return HRESULT_FROM_WIN32(GetLastError());
	}

	HRESULT EnumContainers(HWND hwndCB)
	{
		union {
			PVOID pv;
			PBYTE pb;
			PFILE_DIRECTORY_INFORMATION DirInfo;
		};

		HANDLE hFile;
		IO_STATUS_BLOCK iosb;
		UNICODE_STRING ObjectName;
		OBJECT_ATTRIBUTES oa = { sizeof(oa), 0, &ObjectName };
		int n = 0, i;

		enum { buf_size = 0x4000 };

		NTSTATUS status = OpenFolder(&oa.RootDirectory);

		if (0 > status)
		{
			return status;
		}

		status = STATUS_NO_MEMORY;

		if (PVOID buf = LocalAlloc(0, buf_size))
		{
			while (0 <= (status = NtQueryDirectoryFile(oa.RootDirectory, NULL, NULL, NULL, &iosb, 
				pv = buf, buf_size, FileDirectoryInformation, 0, NULL, FALSE)))
			{

				ULONG NextEntryOffset = 0;

				do 
				{
					pb += NextEntryOffset;

					ObjectName.Buffer = DirInfo->FileName;

					switch (ObjectName.Length = (USHORT)DirInfo->FileNameLength)
					{
					case 2*sizeof(WCHAR):
						if (ObjectName.Buffer[1] != '.') break;
					case sizeof(WCHAR):
						if (ObjectName.Buffer[0] == '.') continue;
					}

					ObjectName.MaximumLength = ObjectName.Length;

					if (!(DirInfo->FileAttributes & (FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT)) &&
						(ULONGLONG)(DirInfo->EndOfFile.QuadPart -= sizeof(SC_Cntr)) < 0x4000)
					{
						ULONG cch = ObjectName.Length / sizeof(WCHAR);
						UnFixBase64(ObjectName.Buffer, cch);

						CHAR utf8[128];
						WCHAR name[64];
						ULONG cb = _countof(utf8), cchName;
						if (CryptStringToBinaryW(ObjectName.Buffer, cch, CRYPT_STRING_BASE64, (PUCHAR)utf8, &cb, 0, 0) &&
							(cchName = MultiByteToWideChar(CP_UTF8, 0, utf8, cb, name, _countof(name) - 1)))
						{
							FixBase64(ObjectName.Buffer, cch);

							if (0 <= NtOpenFile(&hFile, FILE_GENERIC_READ, &oa, &iosb, FILE_SHARE_READ, 
								FILE_SYNCHRONOUS_IO_NONALERT|FILE_NON_DIRECTORY_FILE))
							{
								if (SC_Cntr* pkn = new(cb = DirInfo->EndOfFile.LowPart) SC_Cntr)
								{
									if (0 <= NtReadFile(hFile, 0, 0, 0, &iosb, pkn, cb + sizeof(SC_Cntr), 0, 0))
									{
										if (pkn->IsValid((ULONG)iosb.Information))
										{
											name[cchName] = 0;
											if (0 <= (i = ComboBox_AddString(hwndCB, name)))
											{
												if (0 <= ComboBox_SetItemData(hwndCB, i, pkn))
												{
													n++;
													pkn = 0;
												}
											}
										}
									}
									if (pkn) delete pkn;
								}

								NtClose(hFile);
							}
						}
					}

				} while (NextEntryOffset = DirInfo->NextEntryOffset);
			
				LocalFree(buf);
			}
		}

		NtClose(oa.RootDirectory);

		return n ? ComboBox_SetCurSel(hwndCB, 0), S_OK : HRESULT_FROM_NT(status); 
	}

	HRESULT OnInitDialog(HWND hwndDlg, PCWSTR* ppcsz)
	{
		log.Set(GetDlgItem(hwndDlg, IDC_EDIT1));

		HRESULT hr = OpenBKey(&_hKey, L"DFA1ECDB242447beBCA9FE60E043A304");

		if (0 > hr)
		{
			*ppcsz = L"Can not open private key!";
			return hr;
		}

		if (0 > (hr = EnumContainers(GetDlgItem(hwndDlg, IDC_COMBO1))))
		{
			*ppcsz = L"Not found any certificates!";
			return hr;
		}

		*ppcsz = L"internal error";
		return BthDlg::OnInitDialog(hwndDlg);
	}

	void OnDestroy(HWND hwndDlg)
	{
		if (ScSocket* pSocket = _pSocket)
		{
			_pSocket = 0;
			pSocket->Close();
			pSocket->Release();
		}

		if (PBTH_DEVICE_INFO_LIST pList = _pList)
		{
			LocalFree(pList);
		}

		HWND hwndCB = GetDlgItem(hwndDlg, IDC_COMBO1);

		if (ULONG n = ComboBox_GetCount(hwndCB))
		{
			do 
			{
				if (SC_Cntr* pkn = (SC_Cntr*)ComboBox_GetItemData(hwndCB, --n))
				{
					delete pkn;
				}
			} while (n);
		}

		if (BCRYPT_KEY_HANDLE hKey = _hKey)
		{
			BCryptDestroyKey(hKey);
		}

		BthDlg::OnDestroy();
	}

	void Insert(HWND hwndDlg)
	{
		BTH_RADIO* port = GetSelectedPort();
		
		if (!port)
		{
			return;
		}

		HWND hwndCB = GetDlgItem(hwndDlg, IDC_COMBO2);
		
		int i;

		if (0 > (i = ComboBox_GetCurSel(hwndCB)))
		{
			return ;
		}

		PBTH_DEVICE_INFO deviceList = (PBTH_DEVICE_INFO)ComboBox_GetItemData(hwndCB, i);

		if (!deviceList)
		{
			return ;
		}

		hwndCB = GetDlgItem(hwndDlg, IDC_COMBO1);

		if (0 > (i = ComboBox_GetCurSel(hwndCB)))
		{
			return ;
		}

		SC_Cntr* pkn = (SC_Cntr*)ComboBox_GetItemData(hwndCB, i);

		if (!pkn)
		{
			return ;
		}

		HRESULT hr = E_OUTOFMEMORY;

		if (ScSocket* p = new ScSocket(_hKey, pkn, hwndDlg, port->_id, GetDlgItem(hwndDlg, IDC_EDIT1)))
		{
			ComboBox_SetItemData(hwndCB, i, 0), _hKey = 0;

			if (0 <= (hr = p->Create()))
			{
				if (0 <= (hr = p->Connect(deviceList->address, (USHORT)deviceList->classOfDevice)))
				{
					_pSocket = p;
					EnableWindow(GetDlgItem(hwndDlg, IDOK), FALSE);
					EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON2), TRUE);
					return ;
				}
				hr |= FACILITY_NT_BIT;
			}

			p->Release();
		}

		log << hr;
	}

	void RemoveSocket(HWND hwndDlg, bool bClose)
	{
		if (ScSocket* pSocket = _pSocket)
		{
			if (bClose) pSocket->Close();
			pSocket->Release();
			_pSocket = 0;
			EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON2), FALSE);
		}
	}

	virtual INT_PTR DialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
	{
		switch (uMsg)
		{
		case WM_DESTROY:
			OnDestroy(hwndDlg);
			break;

		case WM_INITDIALOG:
			if (0 > (lParam = OnInitDialog(hwndDlg, (PCWSTR*)&wParam)))
			{
				ShowErrorBox(hwndDlg, (HRESULT)lParam, (PCWSTR)wParam, MB_ICONHAND);
				EndDialog(hwndDlg, lParam);
			}
			return FALSE;

		case WM_DEVICECHANGE:
			OnInterfaceChange(hwndDlg, wParam, (PDEV_BROADCAST_DEVICEINTERFACE)lParam);
			return 0;

		case WM_BTH:
			switch (wParam)
			{
			case BTH_REQUEST::c_inq:
				OnInquireEnd(hwndDlg, (BTH_REQUEST*)lParam);
				break;
			case BTH_REQUEST::c_cnt:
				OnSdpConnect(hwndDlg, (BTH_REQUEST*)lParam);
				break;
			case BTH_REQUEST::c_srh:
				OnSdpSearch(hwndDlg, (BTH_REQUEST*)lParam);
				break;
			default:
				__debugbreak();
			}

			delete (BTH_REQUEST*)lParam;
			UnlockInquire(hwndDlg);
			return 0;

		case WM_COMMAND:
			switch (wParam)
			{
			case IDCANCEL:
				if (_dwInquireCount) { log << L"can not close while inquire active\r\n"; } else EndDialog(hwndDlg, STATUS_CANCELLED);
				break;

			case MAKEWPARAM(IDC_COMBO3, CBN_SELCHANGE):
				SetSelected(ComboBox_GetCurSel((HWND)lParam));
				break;

			case IDOK:
				Insert(hwndDlg);
				break;
			
			case IDC_BUTTON2:
				RemoveSocket(hwndDlg, true);
				break;

			case IDC_BUTTON1:
				StartInquire(hwndDlg);
				break;
			}
			return 0;

		case WM_RES:
			_hKey = (BCRYPT_KEY_HANDLE)wParam;
			if (HWND hwndCB = GetDlgItem(hwndDlg, IDC_COMBO1))
			{
				ComboBox_SetItemData(hwndCB, _dwIndex, lParam);
				if (GetBthCount() && ComboBox_GetCount(GetDlgItem(hwndDlg, IDC_COMBO2)))
				{
					EnableWindow(GetDlgItem(hwndDlg, IDOK), TRUE);
				}
			}
			else
			{
				delete (SC_Cntr*)lParam;
				EndDialog(hwndDlg, -1);
			}
			SetWindowLongPtrW(hwndDlg, DWLP_MSGRESULT, ScSocket::res_resp);
			return TRUE;

		case WM_CONNECT:
			//log(L"connect(%p)=%x\r\n", wParam, lParam);
			if (0 > lParam || !wParam)
			{
				RemoveSocket(hwndDlg, false);
			}
			return 0;

		case WM_DISCONNECT:
			//log(L"disconnect(%x)\r\n", lParam);
			RemoveSocket(hwndDlg, false);
			return 0;

		case WM_RECV:
			log(L"PERFORM_SECURITY_OPERATION: %s = %x\r\n", wParam, lParam);
			return 0;
		}

		return ZDlg::DialogProc(hwndDlg, uMsg, wParam, lParam);
	}
};

void InsertSC(HWND hwndDlg)
{
	InsertScDlg dlg;

	dlg.DoModal((HINSTANCE)&__ImageBase, MAKEINTRESOURCEW(IDD_DIALOG4), hwndDlg, 0);
}

_NT_END
