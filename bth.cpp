#include "stdafx.h"
#include "resource.h"

_NT_BEGIN

#include "../asio/socket.h"
#include "../winZ/window.h"
#include "elog.h"
#include "socket.h"
#include "sdp.h"
extern volatile const UCHAR guz = 0;

CONFIGRET GetFriendlyName(_Out_ PWSTR* ppszName, _In_ PCWSTR pszDeviceInterface);

HRESULT GetLastErrorEx(ULONG dwError = GetLastError())
{
	NTSTATUS status = RtlGetLastNtStatus();
	return dwError == RtlNtStatusToDosErrorNoTeb(status) ? HRESULT_FROM_NT(status) : HRESULT_FROM_WIN32(dwError);
}


#define IOCTL_BTH_HCI_INQUIRE					BTH_CTL(BTH_IOCTL_BASE+0x400)
#define IOCTL_BTH_ENABLE_DISCOVERY				BTH_CTL(BTH_IOCTL_BASE+0x408)
#define IOCTL_BTH_GET_LOCALSERVICES				BTH_CTL(BTH_IOCTL_BASE+0x411)
#define IOCTL_BTH_GET_LOCALSERVICEINFO			BTH_CTL(BTH_IOCTL_BASE+0x412)
#define IOCTL_BTH_SET_LOCALSERVICEINFO			BTH_CTL(BTH_IOCTL_BASE+0x413)

// BTH_CTL(BTH_IOCTL_BASE+0x411) >>> HCI_GetLocalServices
// BTH_CTL(BTH_IOCTL_BASE+0x412) >>> HCI_GetLocalServiceInfo
// BTH_CTL(BTH_IOCTL_BASE+0x413) >>> HCI_SetLocalServiceInfo

NTSTATUS SyncIoctl(_In_ HANDLE FileHandle, 
				   _In_ ULONG IoControlCode,
				   _In_reads_bytes_opt_(InputBufferLength) PVOID InputBuffer,
				   _In_ ULONG InputBufferLength,
				   _Out_writes_bytes_opt_(OutputBufferLength) PVOID OutputBuffer,
				   _In_ ULONG OutputBufferLength,
				   _Out_opt_ PULONG_PTR Information = 0)
{
	IO_STATUS_BLOCK iosb;

	NTSTATUS status = NtDeviceIoControlFile(FileHandle, 0, 0, 0, &iosb, IoControlCode, 
		InputBuffer, InputBufferLength, OutputBuffer, OutputBufferLength);

	if (status == STATUS_PENDING)
	{
		if (WaitForSingleObject(FileHandle, INFINITE) != WAIT_OBJECT_0) __debugbreak();
		status = iosb.Status;
	}

	if (Information)
	{
		*Information = iosb.Information;
	}

	return status;
}


struct __declspec(uuid("00112233-4455-6677-8899-aabbccddeeff")) MyServiceClass;

ULONG RegisterService(_In_ UCHAR Port, _In_ WSAESETSERVICEOP essoperation, _Inout_ HANDLE* pRecordHandle);

class BtDlg : public ZDlg, ELog, LIST_ENTRY
{
	BTH_ADDR _btAddr;
	HANDLE _recordHandle = 0;
	BthSocket* _ps = 0, *_pc = 0;
	HDEVNOTIFY _HandleV = 0;
	HWND _hwndServerList, _hwndAddress;
	ULONG _port;
	ULONG _n = 0;
	ULONG _DevLockCount = 0;

	enum {
		ID_S_STATUS = IDC_STATIC1,
		ID_FROM_ADDRESS = IDC_STATIC3,
		ID_S_DISCONNECT = IDC_BUTTON3,
		ID_S_SEND = IDC_BUTTON2,
		ID_S_MSG = IDC_EDIT2,

		ID_C_STATUS = IDC_STATIC2,
		ID_CONNECT = IDC_BUTTON1,
		ID_TO_ADDRESS = IDC_COMBO2,
		ID_C_DISCONNECT = IDC_BUTTON4,
		ID_C_SEND = IDC_BUTTON5,
		ID_C_MSG = IDC_EDIT4,
		ID_DISCOVERY = IDC_BUTTON8,

		ID_START = IDC_BUTTON6,
		ID_STOP = IDC_BUTTON7,
		ID_LIST = IDC_COMBO1,
		ID_LOG = IDC_EDIT3,
		ID_LIST2 = IDC_COMBO2,
		ID_SDB = IDC_BUTTON9
	};

	struct BTH_PORT : LIST_ENTRY
	{
		BTH_ADDR btAddr = 0;
		HANDLE _hDevice = 0;
		ULONG hash = 0;

		~BTH_PORT()
		{
			RemoveEntryList(this);
			if (HANDLE hDevice = _hDevice)
			{
				NtClose(hDevice);
			}
		}

		BTH_PORT(PLIST_ENTRY head)
		{
			InsertTailList(head, this);
		}
	};

	struct HCI_REQUEST : IO_STATUS_BLOCK 
	{
		HWND hwnd;
		ULONG code;
		ULONG time;
		BTH_ADDR btAddr;
		ULONGLONG hConnection;
		ULONG Data[];

		enum { c_inq = 'GIAC', c_cnt = 'TCNC', c_srh = 'HCRS', c_dsc = 'TCSD', c_atr = 'RTTA'};

		HCI_REQUEST(HWND hwnd, ULONG code) : hwnd(hwnd), code(code), time(GetTickCount()) {}

		void OnIoEnd()
		{
			if (HWND hWnd = hwnd)
			{
				if (PostMessageW(hWnd, WM_BTH, code, (LPARAM)this))
				{
					return ;
				}
			}
			delete this;
		}

		void* operator new(size_t s, ULONG ex = 0)
		{
			return LocalAlloc(0, s + ex);
		}

		void operator delete(void* pv)
		{
			LocalFree(pv);
		}

		static VOID NTAPI OnBthComplete(
			_In_ NTSTATUS /*status*/,
			_In_ ULONG_PTR /*Information*/,
			_In_ PVOID Context
			)
		{
			reinterpret_cast<HCI_REQUEST*>(Context)->OnIoEnd();
		}

		void CheckStatus(NTSTATUS status)
		{
			if (NT_ERROR(status))
			{
				Status = status;
				Information = 0;
				OnBthComplete(status, 0, this);
			}
		}

		ULONG GetIoTime()
		{
			return GetTickCount() - time;
		}
	};

	void LockDev()
	{
		if (!_DevLockCount++)
		{
			EnableWindow(_hwndServerList, FALSE);
		}
	}

	void UnlockDev()
	{
		if (!--_DevLockCount)
		{
			EnableWindow(_hwndServerList, TRUE);
		}
	}

	ULONG StartServer(HWND hwndDlg, CSocketObject* pAddress, _Out_ PCWSTR& fmt, _In_opt_ BTH_ADDR btAddr = 0)
	{
		SOCKADDR_BTH asi = { AF_BTH, btAddr, {}, BT_PORT_ANY };

		fmt = L"!! Create Address:\r\n";

		ULONG dwError = pAddress->CreateAddress((PSOCKADDR)&asi, sizeof(asi), BTHPROTO_RFCOMM);

		if (NOERROR == dwError)
		{			
			fmt = L"!! GetLocalAddr:\r\n";

			SOCKET_ADDRESS LocalAddr = { (PSOCKADDR)&asi, sizeof(asi) };

			dwError = pAddress->GetLocalAddr(&LocalAddr);

			if (dwError == NOERROR)
			{
				if (BthSocket* p = new BthSocket(BthSocket::t_server, hwndDlg, "server", pAddress))
				{
					fmt = L"!! Create Socket:\r\n";

					dwError = p->Create(0x2000, AF_BTH, BTHPROTO_RFCOMM);

					if (NOERROR == dwError)
					{
						fmt = L"!! Listen:\r\n";

						dwError = p->Listen();

						if (NOERROR == dwError)
						{
							_ps = p, _btAddr = asi.btAddr, _port = asi.port;

							WCHAR sz[64];
							swprintf_s(sz, _countof(sz), L"Server: My ID = %I64X:%x", asi.btAddr, asi.port);
							SetDlgItemTextW(hwndDlg, IDC_STATIC1, sz);

							int ids[] = { -ID_START, ID_STOP };
							EnableControls(hwndDlg, ids, _countof(ids));

							RegisterService((UCHAR)asi.port, RNRSERVICE_REGISTER, &_recordHandle);

							*this << L"Listening ...\r\n";

							return NOERROR;
						}
					}

					p->Release();
				}
			}
		}

		return dwError;
	}

	void StartServer(HWND hwndDlg, _In_ HANDLE hDevice, _In_opt_ BTH_ADDR btAddr = 0)
	{
		if (CSocketObject* pAddress = new CSocketObject)
		{
			PCWSTR fmt;

			if (ULONG dwError = StartServer(hwndDlg, pAddress, fmt, btAddr))
			{			
				*this << fmt << dwError;
			}
			else
			{
				//BthServEnableDiscovery(hDevice, TRUE);
				if (HCI_REQUEST* p = new HCI_REQUEST(0, 0))
				{
					ULONG u = 0x00010103;
					p->CheckStatus(NtDeviceIoControlFile(hDevice, 0, 0, p, p, IOCTL_BTH_ENABLE_DISCOVERY, &u, sizeof(u), 0, 0));
				}
			}

			pAddress->Release();
		}
	}

	BTH_PORT* GetSelectedPort()
	{
		ULONG i = ComboBox_GetCurSel(_hwndServerList);
		if (i < _n)
		{
			PLIST_ENTRY head = this, entry = head;
			do 
			{
				entry = entry->Flink;
			} while (i--);

			return static_cast<BTH_PORT*>(entry);
		}

		return 0;
	}

	BTH_ADDR GetSelectedAddr()
	{
		if (BTH_PORT* port = GetSelectedPort())
		{
			return port->btAddr;
		}

		return 0;
	}

	HANDLE GetSelectedDevice()
	{
		if (BTH_PORT* port = GetSelectedPort())
		{
			return port->_hDevice;
		}

		return 0;
	}

	void EnumBTH(HWND hwndDlg)
	{
		CONFIGRET cr;
		ULONG cb = 0, rcb;
		union {
			PVOID buf;
			PZZWSTR Buffer;
		};

		PVOID stack = alloca(guz);
		do 
		{
			cr = CM_Get_Device_Interface_List_SizeW(&rcb, const_cast<GUID*>(&GUID_BTHPORT_DEVICE_INTERFACE), 0, CM_GET_DEVICE_INTERFACE_LIST_PRESENT);

			if (cr != CR_SUCCESS)
			{
				break;
			}

			if (cb < (rcb *= sizeof(WCHAR)))
			{
				cb = RtlPointerToOffset(buf = alloca(rcb - cb), stack);
			}

			cr = CM_Get_Device_Interface_ListW(const_cast<GUID*>(&GUID_BTHPORT_DEVICE_INTERFACE), 
				0, Buffer, cb, CM_GET_DEVICE_INTERFACE_LIST_PRESENT);

		} while (cr == CR_BUFFER_SMALL);

		if (cr == CR_SUCCESS)
		{
			while (*Buffer)
			{
				Add(hwndDlg, Buffer);
				Buffer += wcslen(Buffer) + 1;
			}
		}
	}

	void OnInitDialog(HWND hwndDlg)
	{
		SendMessage(hwndDlg, WM_SETICON, ICON_SMALL, (LPARAM)LoadIconW((HINSTANCE)&__ImageBase, MAKEINTRESOURCEW(IDI_BTH)));

		ELog::Set(GetDlgItem(hwndDlg, ID_LOG));

		_hwndServerList = GetDlgItem(hwndDlg, ID_LIST);
		_hwndAddress = GetDlgItem(hwndDlg, ID_TO_ADDRESS);

		DEV_BROADCAST_DEVICEINTERFACE NotificationFilter = { 
			sizeof(DEV_BROADCAST_DEVICEINTERFACE), DBT_DEVTYP_DEVICEINTERFACE, 0, GUID_BTHPORT_DEVICE_INTERFACE
		};

		if (HDEVNOTIFY HandleV = RegisterDeviceNotification(hwndDlg, &NotificationFilter, DEVICE_NOTIFY_WINDOW_HANDLE))
		{
			_HandleV = HandleV;
		}
		else
		{
			ULONG dwError = GetLastError();
			*this << L"!! RegisterDeviceNotification\r\n" << dwError;
		}

		EnumBTH(hwndDlg);
	}

	void Connect(HWND hwndDlg)
	{
		SOCKADDR_BTH asi = { AF_BTH, 0, __uuidof(MyServiceClass), BT_PORT_ANY  };

		HRESULT dwError = WSAEADDRNOTAVAIL;

		WCHAR sz[32];
		if (GetWindowTextW(_hwndAddress, sz, _countof(sz)))
		{
			PWSTR pc;
			asi.btAddr = _wcstoui64(sz, &pc, 16);

			switch (*pc)
			{
			case 0:
				break;
			case ':':
				asi.port = wcstoul(pc + 1, &pc, 16);
				if (!*pc) 
				{
					break;
				}
				[[fallthrough]];
			default:
				*this << WSAEADDRNOTAVAIL;
				return;
			}

			operator()(L"connecting to %I64X:%X ...\r\n", asi.btAddr, asi.port);

			if (BthSocket* p = new BthSocket(BthSocket::t_client, hwndDlg, "client"))
			{
				dwError = p->Create(0x8000, AF_BTH, BTHPROTO_RFCOMM);

				if (dwError == NOERROR)
				{
					dwError = p->Connect((PSOCKADDR)&asi, sizeof(SOCKADDR_BTH));

					if (dwError == NOERROR)
					{
						_pc = p;
						int ids[] = { -ID_CONNECT, -ID_TO_ADDRESS };
						EnableControls(hwndDlg, ids, _countof(ids));
						return ;
					}
				}

				p->Release();
			}
		}

		*this << L"!! connect fail:\r\n" << dwError;
	}

	void EnableControls(HWND hwndDlg, PINT ids, ULONG n)
	{
		do 
		{
			BOOL bEnable = TRUE;
			int id = *ids++;
			if (0 > id)
			{
				id = -id, bEnable = FALSE;
			}
			if (id == ID_CONNECT && !ComboBox_GetCount(_hwndAddress))
			{
				bEnable = FALSE;
			}
			
			EnableWindow(GetDlgItem(hwndDlg, id), bEnable);
		} while (--n);
	}

	void DestroyClient()
	{
		if (BthSocket* pc = _pc)
		{
			pc->Close();
			pc->Release();
			_pc = 0;
		}
	}

	void OnConnectFail(HWND hwndDlg, WPARAM wParam, LPARAM lParam)
	{
		PCWSTR fmt;
		switch (wParam)
		{
		case BthSocket::t_client:
			DestroyClient();
			fmt = L"!! [client]: connect fail !\r\n";
			{
				int ids[] = { ID_CONNECT, ID_TO_ADDRESS };
				EnableControls(hwndDlg, ids, _countof(ids));
			}

			break;
		case BthSocket::t_server:
			fmt = L"!! [server]: listen fail !\r\n";
			SetDlgItemTextW(hwndDlg, IDC_STATIC1, L"Server:");
			EnableWindow(GetDlgItem(hwndDlg, ID_STOP), FALSE);
			if (_n)
			{
				EnableWindow(GetDlgItem(hwndDlg, ID_START), TRUE);
			}
			break;
		default:
			__debugbreak();
			return;
		}
		*this << fmt << (HRESULT)lParam;
	}

	void OnConnectOK(HWND hwndDlg, WPARAM wParam)
	{
		int ids[5], n;
		switch (wParam)
		{
		case BthSocket::t_client:
			ids[0] = ID_C_DISCONNECT, ids[1] = ID_C_SEND, ids[2] = ID_C_MSG, ids[3] = -ID_CONNECT, ids[4] = -ID_TO_ADDRESS, n = 5;
			*this << L"<<<< connected to server !\r\n";
			break;
		case BthSocket::t_server:
			ids[0] = ID_S_DISCONNECT, ids[1] = ID_S_SEND, ids[2] = ID_S_MSG, n = 3;

			if (PSOCKADDR_BTH psa = _ps->GetClientAddr())
			{
				WCHAR sz[64];
				swprintf_s(sz, _countof(sz), L"Client ID = %I64X:%X", psa->btAddr, psa->port);
				SetDlgItemTextW(hwndDlg, ID_FROM_ADDRESS, sz);
				operator()(L">>>> connect from %I64X:%X\r\n", psa->btAddr, psa->port);
			}
			break;
		default:
			__debugbreak();
			return;
		}

		EnableControls(hwndDlg, ids, n);
	}

	void Send(BthSocket* p, CDataPacket* packet)
	{
		if (ULONG dwError = p->Send(packet))
		{
			*this << L"send fail:\r\n" << dwError;
		}
		else
		{
			operator()(L"send %x bytes ...\r\n", packet->getDataSize());
		}

		packet->Release();

	}

	void Send(BthSocket* p, HWND hwndDlg, ULONG id)
	{
		HWND hwnd = GetDlgItem(hwndDlg, id);
		if (ULONG len = GetWindowTextLengthW(hwnd))
		{
			if (len < 8)
			{
				WCHAR sz[8], *c;
				GetWindowTextW(hwnd, sz, _countof(sz));
				if (*sz == '[' && (len = wcstoul(sz+1, &c, 16)) && *c++ == ']' && !*c)
				{
					if (CDataPacket* packet = new(len) CDataPacket)
					{
						memset(packet->getData(), '*', len);
						packet->setDataSize(len);

						Send(p, packet);
					}

					return;
				}
			}
			if (CDataPacket* packet = new(++len * sizeof(WCHAR)) CDataPacket)
			{
				PWSTR msg = (PWSTR)packet->getData();
				len = GetWindowTextW(hwnd, msg, len);
				msg[len++] = 0;
				packet->setDataSize(len * sizeof(WCHAR));

				Send(p, packet);
			}
		}
	}

	void OnDestroy()
	{
		StopServer();

		if (BthSocket* pc = _pc)
		{
			pc->Close();
			pc->Release();
			_pc = 0;
		}

		if (HDEVNOTIFY HandleV = _HandleV)
		{
			UnregisterDeviceNotification(HandleV);
		}
	}

	void Add(_In_ HWND hwndDlg, _In_ PCWSTR pszDeviceInterface)
	{
		operator()(L"++ %s\r\n", pszDeviceInterface);

		if (BTH_PORT* port = new BTH_PORT(this))
		{
			UNICODE_STRING us;
			RtlInitUnicodeString(&us, pszDeviceInterface);
			RtlHashUnicodeString(&us, FALSE, HASH_STRING_ALGORITHM_DEFAULT, &port->hash);

			NTSTATUS status;
			HANDLE hDevice = CreateFileW(pszDeviceInterface, 0, 0, 0, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, 0);

			if (hDevice != INVALID_HANDLE_VALUE)
			{
				RtlSetIoCompletionCallback(hDevice, HCI_REQUEST::OnBthComplete, 0);
				port->_hDevice = hDevice;

				BTH_LOCAL_RADIO_INFO bei;

				status = SyncIoctl(hDevice, IOCTL_BTH_GET_LOCAL_INFO, 0, 0, &bei, sizeof(bei));
				
				if (0 > status)
				{
					*this << L"!! IOCTL_BTH_GET_LOCAL_INFO\r\n" << HRESULT_FROM_NT(status);
				}
				else
				{
					operator()(L"[%I64X] \"%S\"\r\n", bei.localInfo.address, bei.localInfo.name);
					port->btAddr = bei.localInfo.address;
				}
			}
			else
			{
				status = GetLastErrorEx();
				*this << L"!! open file\r\n" << status;
			}

			PWSTR psz;
			if (CR_SUCCESS == GetFriendlyName(&psz, pszDeviceInterface))
			{
				operator()(L"%s\r\n", psz);
				int i;
				HWND hwndCB = _hwndServerList;

				if (0 > (i = ComboBox_AddString(hwndCB, psz)))
				{
					delete port;
				}
				else
				{
					ComboBox_SetCurSel(hwndCB, i);

					if (!_n++)
					{
						int ids[] = { ID_START, ID_CONNECT, ID_TO_ADDRESS, ID_LIST, ID_DISCOVERY };
						EnableControls(hwndDlg, ids, _countof(ids));
					}
				}

				LocalFree(psz);
			}

		}
	}

	void StopServer()
	{
		if (BthSocket* ps = _ps)
		{
			RegisterService((UCHAR)_port, RNRSERVICE_DELETE, &_recordHandle);

			ps->Close();
			ps->Release();
			_ps = 0;
			_btAddr = 0;
			_port = 0;
		}
	}

	void Remove(_In_ HWND hwndDlg, _In_ PCWSTR pszDeviceInterface)
	{
		operator()(L"-- %s\r\n", pszDeviceInterface);
		if (_n)
		{
			ULONG hash;
			UNICODE_STRING us;
			RtlInitUnicodeString(&us, pszDeviceInterface);
			RtlHashUnicodeString(&us, FALSE, HASH_STRING_ALGORITHM_DEFAULT, &hash);

			int i = 0;
			PLIST_ENTRY head = this, entry = head;
			do 
			{
				entry = entry->Flink;

				if (static_cast<BTH_PORT*>(entry)->hash == hash)
				{
					HWND hwndCB = _hwndServerList;
					ComboBox_DeleteString(hwndCB, i);
					ComboBox_SetCurSel(hwndCB, 0);

					BOOL b = FALSE;

					if (_btAddr == static_cast<BTH_PORT*>(entry)->btAddr)
					{
						StopServer();
						b = TRUE;
					}

					delete static_cast<BTH_PORT*>(entry);

					if (--_n)
					{
						if (b)
						{
							EnableWindow(GetDlgItem(hwndDlg, ID_START), TRUE);
						}
					}
					else
					{
						int ids[] = { -ID_START, -ID_CONNECT, -ID_TO_ADDRESS, -ID_LIST, -ID_DISCOVERY };
						EnableControls(hwndDlg, ids, _countof(ids));
					}
					return;
				}

				i++;

			} while (entry != head);
		}
	}

	void OnInterfaceChange(_In_ HWND hwndDlg, _In_ WPARAM wParam, _In_ PDEV_BROADCAST_DEVICEINTERFACE p)
	{
		switch (wParam)
		{
		case DBT_DEVICEREMOVECOMPLETE:
		case DBT_DEVICEARRIVAL:
			break;
		default: return;
		}

		if (p->dbcc_devicetype == DBT_DEVTYP_DEVICEINTERFACE && p->dbcc_classguid == GUID_BTHPORT_DEVICE_INTERFACE)
		{
			switch (wParam)
			{
			case DBT_DEVICEREMOVECOMPLETE:
				Remove(hwndDlg, p->dbcc_name);
				break;
			case DBT_DEVICEARRIVAL:
				Add(hwndDlg, p->dbcc_name);
				break;
			}
		}
	}

	void OnDisconnect(HWND hwndDlg, WPARAM wParam)
	{
		int ids[5], n;
		BthSocket* ps;
		switch (wParam)
		{
		case BthSocket::t_client:
			ids[0] = -ID_C_DISCONNECT, ids[1] = -ID_C_SEND, ids[2] = -ID_C_MSG, ids[3] = ID_CONNECT, ids[4] = ID_TO_ADDRESS, n = 5;
			*this << L"client disconnected !\r\n";
			if (ps = _pc)
			{
				ps->Close();
				ps->Release();
				_pc = 0;
			}
			break;
		case BthSocket::t_server:
			ids[0] = -ID_S_DISCONNECT, ids[1] = -ID_S_SEND, ids[2] = -ID_S_MSG, n = 3;

			*this <<  L"server disconnected !\r\n";
			SetDlgItemTextW(hwndDlg, ID_FROM_ADDRESS, L"Client ID =");
			if (ps = _ps)
			{
				ULONG dwError = ps->Listen();

				if (NOERROR == dwError)
				{
					int ids2[] = { -ID_START, ID_STOP };
					EnableControls(hwndDlg, ids2, _countof(ids2));
					*this << L"Listening ...\r\n";
				}
				else
				{
					*this << L"!! Listen:\r\n" << dwError;
				}
			}
			break;
		default:
			__debugbreak();
			return;
		}

		EnableControls(hwndDlg, ids, n);
	}

	void Discovery(HWND hwndDlg, HANDLE hDevice)
	{
		if (!hDevice)
		{
			return;
		}

#pragma pack(push, 1)
		struct HCI_INQUIRE 
		{
			ULONG ul;
			UCHAR u1; 
			UCHAR u2; 
		} hci_inc = { LAP_GIAC_VALUE, 5 };
#pragma pack(pop)

		ComboBox_ResetContent(_hwndAddress);
		EnableWindow(GetDlgItem(hwndDlg, ID_CONNECT), FALSE);

		if (HCI_REQUEST* p = new HCI_REQUEST(hwndDlg, HCI_REQUEST::c_inq))
		{
			LockDev();
			NTSTATUS status = NtDeviceIoControlFile(hDevice, 0, 0, p, p, IOCTL_BTH_HCI_INQUIRE, &hci_inc, sizeof(hci_inc), 0, 0);
			operator()(L"General Inquiry ... [%X]\r\n", status);
			p->CheckStatus(status);
		}
	}

	void DoSDP(HWND hwndDlg, HANDLE hDevice, BTH_ADDR address)
	{
		if (HCI_REQUEST* p = new(sizeof(BTH_SDP_CONNECT)) HCI_REQUEST(hwndDlg, HCI_REQUEST::c_cnt))
		{
			LockDev();
			PBTH_SDP_CONNECT psc = (PBTH_SDP_CONNECT)p->Data;
			psc->bthAddress = address;
			psc->fSdpConnect = 0;
			psc->hConnection = 0;
			psc->requestTimeout = SDP_REQUEST_TO_DEFAULT;

			p->btAddr = address;

			NTSTATUS status = NtDeviceIoControlFile(hDevice, 0, 0, p, p, IOCTL_BTH_SDP_CONNECT, 
				psc, sizeof(BTH_SDP_CONNECT), psc, sizeof(BTH_SDP_CONNECT));

			operator()(L"SDP_CONNECT to %I64x ... [%X]\r\n", address, status);
			p->CheckStatus(status);
		}
	}

	static void Disconnect(HANDLE hDevice, ULONGLONG hConnection)
	{
		if (HCI_REQUEST* p = new HCI_REQUEST(0, HCI_REQUEST::c_dsc))
		{
			p->CheckStatus(NtDeviceIoControlFile(hDevice, 0, 0, p, p, IOCTL_BTH_SDP_DISCONNECT, 
				&hConnection, sizeof(hConnection), 0, 0));
		}
	}

	void OnSdpSearch(HWND hwndDlg, HANDLE hDevice, HCI_REQUEST* q)
	{
		NTSTATUS status = q->Status;
		BTH_ADDR btAddr = q->btAddr;
		operator()(L"SERVICE_SEARCH in [%I64X] = %X [%u ms]\r\n", btAddr, status, q->GetIoTime());
		
		Disconnect(hDevice, q->hConnection);

		if (0 > status)
		{
			*this << HRESULT_FROM_NT(status);
		}
		else
		{
			PBTH_SDP_STREAM_RESPONSE pr = (PBTH_SDP_STREAM_RESPONSE)q->Data;
			PUCHAR resp = pr->response;
			
			PWSTR psz = 0;
			ULONG cch = 0;
			ULONG responseSize = pr->responseSize;
			while (CryptBinaryToStringW(resp, responseSize, CRYPT_STRING_HEX, psz, &cch))
			{
				if (psz)
				{
					*this << psz;
					break;
				}

				psz = (PWSTR)alloca(cch * sizeof(WCHAR));
			}

			USHORT Port;
			if (GetProtocolValue(*this, resp, responseSize, RFCOMM_PROTOCOL_UUID16, sizeof(UINT8), &Port))
			{
				operator()(L"!! Found Server [%I64X:%X] !!\r\n", btAddr, Port);
				WCHAR sz[32];
				swprintf_s(sz, _countof(sz), L"%I64X:%X", btAddr, Port);
				int i = ComboBox_AddString(_hwndAddress, sz);
				if (0 <= i)
				{
					ComboBox_SetCurSel(_hwndAddress, i);
					EnableWindow(GetDlgItem(hwndDlg, ID_CONNECT), TRUE);
				}
			}
		}
	}

	void OnSdpConnect(HWND hwndDlg, HANDLE hDevice, HCI_REQUEST* q)
	{
		PBTH_SDP_CONNECT psc = (PBTH_SDP_CONNECT)q->Data;
		NTSTATUS status = q->Status;
		BTH_ADDR btAddr = q->btAddr;
		operator()(L"SDP_CONNECT to[%I64X] = %X [%u ms]\r\n", btAddr, status, q->GetIoTime());

		if (0 > status)
		{
__0:
			*this << HRESULT_FROM_NT(status);
		}
		else
		{
			if (HCI_REQUEST* p = new(FIELD_OFFSET(BTH_SDP_STREAM_RESPONSE, response[0x200])) HCI_REQUEST(hwndDlg, HCI_REQUEST::c_srh))
			{
				LockDev();
				p->hConnection = psc->hConnection;
				p->btAddr = btAddr;

				BTH_SDP_SERVICE_ATTRIBUTE_SEARCH_REQUEST sss = { 
					psc->hConnection, 0, { 
						{ {__uuidof(MyServiceClass)}, SDP_ST_UUID128}
					},
					{ 
						{SDP_ATTRIB_PROTOCOL_DESCRIPTOR_LIST, SDP_ATTRIB_PROTOCOL_DESCRIPTOR_LIST} 
					}
				};

				status = NtDeviceIoControlFile(hDevice, 0, 0, p, p, IOCTL_BTH_SDP_SERVICE_ATTRIBUTE_SEARCH, 
					&sss, sizeof(sss), p->Data, FIELD_OFFSET(BTH_SDP_STREAM_RESPONSE, response[0x200]));

				operator()(L"SERVICE_SEARCH in %I64x ... [%X]\r\n", btAddr, status);
				p->CheckStatus(status);
				if (0 > status)
				{
					goto __0;
				}
			}
		}
	}

	void OnDiscoveryEnd(HWND hwndDlg, HCI_REQUEST* ph)
	{
		NTSTATUS status = ph->Status;
		operator()(L"General Inquiry = %X [%u ms]\r\n", status, ph->GetIoTime());
		if (0 > ph->Status)
		{
__0:
			*this << HRESULT_FROM_NT(status);
			return ;
		}

		HANDLE hDevice = GetSelectedDevice();

		if (!hDevice)
		{
			return ;
		}

		ULONG numOfDevices;
		status = SyncIoctl(hDevice, IOCTL_BTH_GET_DEVICE_INFO, 0, 0, &numOfDevices, sizeof(numOfDevices));

		operator()(L"BTH_GET_DEVICE_INFO = %X [numOfDevices = %x]\r\n", status, numOfDevices);
		if (0 > status)
		{
			goto __0;
		}
		if (numOfDevices)
		{
			ULONG size = FIELD_OFFSET(BTH_DEVICE_INFO_LIST, deviceList[numOfDevices]);

			if (PBTH_DEVICE_INFO_LIST p = (PBTH_DEVICE_INFO_LIST)LocalAlloc(0, size))
			{
				if (0 <= (status = SyncIoctl(hDevice, IOCTL_BTH_GET_DEVICE_INFO, 0, 0, p, size)))
				{
					if (numOfDevices = p->numOfDevices)
					{
						PBTH_DEVICE_INFO deviceList = p->deviceList;
						do 
						{
							ULONG flags = deviceList->flags;
							operator()(L"found: [%I64X] \"%S\" (%x %x)\r\n", 
								BDIF_ADDRESS & flags ? deviceList->address : 0, 
								BDIF_NAME & flags ? deviceList->name : "",
								BDIF_COD  & flags ? deviceList->classOfDevice : 0, 
								flags);

							if (BDIF_ADDRESS & flags)
							{
								DoSDP(hwndDlg, hDevice, deviceList->address);
							}

						} while (deviceList++, --numOfDevices);
					}
				}

				LocalFree(p);
			}
		}
	}

	virtual INT_PTR DialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
	{
		switch (uMsg)
		{
		case WM_INITDIALOG:
			OnInitDialog(hwndDlg);
			return FALSE;

		case WM_KILLFOCUS:
			SendMessageW(GetDlgItem(hwndDlg, ID_LOG), EM_SETSEL, MAXULONG_PTR, 0);
			break;

		case WM_DEVICECHANGE:
			OnInterfaceChange(hwndDlg, wParam, (PDEV_BROADCAST_DEVICEINTERFACE)lParam);
			break;

		case WM_CONNECT:
			lParam ? OnConnectFail(hwndDlg, wParam, lParam) : OnConnectOK(hwndDlg, wParam);
			return 0;

		case WM_DISCONNECT:
			OnDisconnect(hwndDlg, wParam);
			return 0;

		case WM_RECV:
			operator()(L"recv %x bytes:\r\n", wParam);
			if (wParam < 0x100 && !(wParam & 1) &&
				!*(PWSTR)RtlOffsetToPointer(lParam, wParam - sizeof(WCHAR)))
			{
				*this << (PWSTR)lParam << L"\r\n";
			}
			return 0;

		case WM_DESTROY:
			OnDestroy();
			break;

		case WM_BTH:
			switch (wParam)
			{
			case HCI_REQUEST::c_inq:
				OnDiscoveryEnd(hwndDlg, (HCI_REQUEST*)lParam);
				break;
			case HCI_REQUEST::c_cnt:
				OnSdpConnect(hwndDlg, GetSelectedDevice(), (HCI_REQUEST*)lParam);
				break;
			case HCI_REQUEST::c_srh:
				OnSdpSearch(hwndDlg, GetSelectedDevice(), (HCI_REQUEST*)lParam);
				break;
			default:
				__debugbreak();
			}

			delete (HCI_REQUEST*)lParam;
			UnlockDev();
			break;

		case WM_COMMAND:
			switch (wParam)
			{
			case IDCANCEL:
				EndDialog(hwndDlg, 0);
				break;
			case ID_CONNECT:
				if (!_pc)
				{
					Connect(hwndDlg);
				}
				break;
			case ID_START:
				if (!_ps && _n)
				{
					if (BTH_PORT* port = GetSelectedPort())
					{
						StartServer(hwndDlg, port->_hDevice, port->btAddr);
					}
				}
				break;
			case ID_STOP:
				StopServer();
				break;
			case ID_S_DISCONNECT:
				if (_ps)
				{
					_ps->Disconnect();
				}
				break;
			case ID_C_DISCONNECT:
				if (_pc)
				{
					_pc->Disconnect();
				}
				break;
			case ID_C_SEND:
				if (_pc) Send(_pc, hwndDlg, ID_C_MSG);
				break;
			case ID_S_SEND:
				if (_ps) Send(_ps, hwndDlg, ID_S_MSG);
				break;
			case ID_DISCOVERY:
				Discovery(hwndDlg, GetSelectedDevice());
				break;
			}
			return 0;
		}
		return ZDlg::DialogProc(hwndDlg, uMsg, wParam, lParam);
	}
public:

	BtDlg()
	{
		InitializeListHead(this);
	}
};

void bthDlg()
{
	WSADATA wd;
	if (!WSAStartup(WINSOCK_VERSION, &wd))
	{
		BtDlg dlg;
		dlg.DoModal((HINSTANCE)&__ImageBase, MAKEINTRESOURCEW(IDD_DIALOG1), 0, 0);
		WSACleanup();
	}
}

#include "../inc/initterm.h"

void Install();
BOOL BuildSdp(USHORT Psm, UCHAR Cn);

void CALLBACK ep(void*)
{
	//BuildSdp(0x8877, 0x66);
	//if (IsDebuggerPresent())Install();

	initterm();
	bthDlg();
	IO_RUNDOWN::g_IoRundown.BeginRundown();
	destroyterm();
	ExitProcess(0);
}

_NT_END