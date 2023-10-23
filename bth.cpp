#include "stdafx.h"
#include "resource.h"

_NT_BEGIN

#include "../asio/socket.h"
#include "../winZ/window.h"
#include "elog.h"
#include "socket.h"
#include "sdp.h"
#include "BthDlg.h"
#include "BthRequest.h"
#include "util.h"
#include "cm.h"

extern volatile const UCHAR guz = 0;

class BtDlg : public BthDlg
{
	ELog log;
	BTH_ADDR _btAddr;
	HANDLE_SDP _recordHandle = 0;
	BthSocket* _ps = 0, *_pc = 0;
	HWND _hwndAddress;
	PBTH_DEVICE_INFO_LIST _pList = 0;
	ULONG _port;
	LONG _dwInquireCount = 0;

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

	void OnPortRemoved(BTH_RADIO* port, BthSocket** ppSocket)
	{
		if (BthSocket* pSocket = *ppSocket)
		{
			if (pSocket->get_id() == port->_id)
			{
				if (HANDLE_SDP hRecord = _recordHandle)
				{
					UnregisterService(port->_hDevice, hRecord);
					_recordHandle = 0;
				}

				pSocket->Close();
				pSocket->Release();
				*ppSocket = 0;
			}
		}
	}

	virtual void OnPortRemoved(BTH_RADIO* port)
	{
		log(L"--[%I64X]\r\n", port->_btAddr);

		OnPortRemoved(port, &_ps);
		OnPortRemoved(port, &_pc);
	}

	virtual void OnAnyBthExist(HWND hwndDlg, BOOL b)
	{
		const int ids_1[] = {  ID_START,  ID_CONNECT,  ID_TO_ADDRESS,  ID_LIST,  ID_DISCOVERY };
		const int ids_0[] = { -ID_START, -ID_CONNECT, -ID_TO_ADDRESS, -ID_LIST, -ID_DISCOVERY };
		EnableControls(hwndDlg, b ? ids_1 : ids_0, _countof(ids_0));
	}

	virtual ULONG GetComboId()
	{
		return ID_LIST;
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

	void LockInquire(HWND hwndDlg)
	{
		if (!_dwInquireCount++)
		{
			EnableWindow(GetDlgItem(hwndDlg, ID_DISCOVERY), FALSE);
			EnableWindow(GetDlgItem(hwndDlg, ID_CONNECT), TRUE);

			ComboBox_ResetContent(_hwndAddress);
			if (PBTH_DEVICE_INFO_LIST pList = _pList)
			{
				LocalFree(pList);
				_pList = 0;
			}
		}
	}

	void UnlockInquire(HWND hwndDlg)
	{
		if (!--_dwInquireCount)
		{
			EnableWindow(GetDlgItem(hwndDlg, ID_DISCOVERY), TRUE);

			log(L"... Inquire\r\n");
		}
	}

	HRESULT StartServer(HWND hwndDlg, CSocketObject* pAddress, _Out_ PCWSTR& fmt, BTH_RADIO* port)
	{
		SOCKADDR_BTH asi = { AF_BTH, port->_btAddr, {}, BT_PORT_ANY };

		fmt = L"!! Create Address:\r\n";

		ULONG dwError = pAddress->CreateAddress((PSOCKADDR)&asi, sizeof(asi), BTHPROTO_RFCOMM);

		if (NOERROR == dwError)
		{			
			fmt = L"!! GetLocalAddr:\r\n";

			SOCKET_ADDRESS LocalAddr = { (PSOCKADDR)&asi, sizeof(asi) };

			dwError = pAddress->GetLocalAddr(&LocalAddr);

			if (dwError == NOERROR)
			{
				if (BthSocket* p = new BthSocket(BthSocket::t_server, hwndDlg, port->_id, "server", pAddress))
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

							NTSTATUS status = RegisterService(port->_hDevice, &__uuidof(MyServiceClass),(UCHAR)asi.port, &_recordHandle);

							log(L"RegisterService()=%I64X, %x\r\n", _recordHandle, status);
							
							if (0 <= status)
							{
								port->EnableScan();
							}

							return HRESULT_FROM_NT(status);
						}
					}

					p->Release();
				}
			}
		}

		return HRESULT_FROM_WIN32(dwError);
	}

	void StartServer(HWND hwndDlg, BTH_RADIO* port)
	{
		if (CSocketObject* pAddress = new CSocketObject)
		{
			PCWSTR fmt;

			HRESULT hr = StartServer(hwndDlg, pAddress, fmt, port);
			if (0 > hr)
			{			
				log << fmt << hr;
			}
			pAddress->Release();
		}
	}

	BTH_ADDR GetSelectedAddr()
	{
		if (BTH_RADIO* port = GetSelectedPort())
		{
			return port->_btAddr;
		}

		return 0;
	}

	HANDLE GetSelectedDevice()
	{
		if (BTH_RADIO* port = GetSelectedPort())
		{
			return port->_hDevice;
		}

		return 0;
	}

	void OnInitDialog(HWND hwndDlg)
	{
		log.Set(GetDlgItem(hwndDlg, ID_LOG));

		_hwndAddress = GetDlgItem(hwndDlg, ID_TO_ADDRESS);

		BthDlg::OnInitDialog(hwndDlg);
	}

	void Connect(HWND hwndDlg)
	{
		BTH_RADIO* port = GetSelectedPort();
		if (!port)
		{
			return ;
		}

		HRESULT dwError = WSAEADDRNOTAVAIL;

		int i = ComboBox_GetCurSel(_hwndAddress);
		if (0 > i)
		{
			return ;
		}

		PBTH_DEVICE_INFO deviceList = (PBTH_DEVICE_INFO)ComboBox_GetItemData(_hwndAddress, i);

		if (!deviceList)
		{
			return ;
		}

		//__uuidof(MyServiceClass)
		SOCKADDR_BTH asi = { AF_BTH, deviceList->address, {}, deviceList->classOfDevice };
		
		log(L"connecting to %I64X:%X ...\r\n", asi.btAddr, asi.port);

		if (BthSocket* p = new BthSocket(BthSocket::t_client, hwndDlg, port->_id, "client"))
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

		log << L"!! connect fail:\r\n" << dwError;
	}

	void EnableControls(HWND hwndDlg, const INT* ids, ULONG n)
	{
		do 
		{
			BOOL bEnable = TRUE;
			int id = *ids++;
			if (0 > id)
			{
				id = -id, bEnable = FALSE;
			}

			switch (id)
			{
			case ID_CONNECT:
				if (_pc || !ComboBox_GetCount(_hwndAddress))
				{
					bEnable = FALSE;
				}
				break;
			case ID_START:
				if (_ps)
				{
					bEnable = FALSE;
				}
				break;
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
			if (GetBthCount())
			{
				EnableWindow(GetDlgItem(hwndDlg, ID_START), TRUE);
			}
			break;
		default:
			__debugbreak();
			return;
		}
		log << fmt << (HRESULT)lParam;
	}

	void OnConnectOK(HWND hwndDlg, WPARAM wParam)
	{
		int ids[5], n;
		switch (wParam)
		{
		case BthSocket::t_client:
			ids[0] = ID_C_DISCONNECT, ids[1] = ID_C_SEND, ids[2] = ID_C_MSG, ids[3] = -ID_CONNECT, ids[4] = -ID_TO_ADDRESS, n = 5;
			log << L"<<<< connected to server !\r\n";
			break;
		case BthSocket::t_server:
			ids[0] = ID_S_DISCONNECT, ids[1] = ID_S_SEND, ids[2] = ID_S_MSG, n = 3;

			if (PSOCKADDR_BTH psa = _ps->GetClientAddr())
			{
				WCHAR sz[64];
				swprintf_s(sz, _countof(sz), L"Client ID = %I64X:%X", psa->btAddr, psa->port);
				SetDlgItemTextW(hwndDlg, ID_FROM_ADDRESS, sz);
				log(L">>>> connect from %I64X:%X\r\n", psa->btAddr, psa->port);
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
			log << L"send fail:\r\n" << dwError;
		}
		else
		{
			log(L"send %x bytes ...\r\n", packet->getDataSize());
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

	void StopServer()
	{
		if (BthSocket* ps = _ps)
		{
			if (HANDLE_SDP hRecord = _recordHandle)
			{
				if (BTH_RADIO* port = getByRadioId(ps->get_id()))
				{
					UnregisterService(port->_hDevice, hRecord);
					_recordHandle = 0;
				}
			}

			ps->Close();
			ps->Release();
			_ps = 0;
			_btAddr = 0;
			_port = 0;
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
			log << L"client disconnected !\r\n";
			if (ps = _pc)
			{
				ps->Close();
				ps->Release();
				_pc = 0;
			}
			break;
		case BthSocket::t_server:
			ids[0] = -ID_S_DISCONNECT, ids[1] = -ID_S_SEND, ids[2] = -ID_S_MSG, n = 3;

			log <<  L"server disconnected !\r\n";
			SetDlgItemTextW(hwndDlg, ID_FROM_ADDRESS, L"Client ID =");
			if (ps = _ps)
			{
				ULONG dwError = ps->Listen();

				if (NOERROR == dwError)
				{
					int ids2[] = { -ID_START, ID_STOP };
					EnableControls(hwndDlg, ids2, _countof(ids2));
					log << L"Listening ...\r\n";
				}
				else
				{
					log << L"!! Listen:\r\n" << dwError;
				}
			}
			break;
		default:
			__debugbreak();
			return;
		}

		EnableControls(hwndDlg, ids, n);
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
				BTH_DEVICE_INQUIRY hci_inc = { LAP_GIAC_VALUE, SDP_DEFAULT_INQUIRY_SECONDS };

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
								ConnectToSDP(hwndDlg, hDevice, bthId, deviceList);
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

	void ConnectToSDP(HWND hwndDlg, HANDLE hDevice, ULONG bthId, PBTH_DEVICE_INFO deviceInfo)
	{
		if (BTH_REQUEST* irp = AllocateRequest(hwndDlg, bthId, BTH_REQUEST::c_cnt, sizeof(BTH_SDP_CONNECT)))
		{
			BTH_ADDR address = deviceInfo->address;
			PBTH_SDP_CONNECT psc = (PBTH_SDP_CONNECT)irp->Data;
			psc->bthAddress = address;
			psc->fSdpConnect = 0;
			psc->hConnection = 0;
			psc->requestTimeout = SDP_REQUEST_TO_DEFAULT;

			irp->deviceInfo = deviceInfo;

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
		PBTH_DEVICE_INFO deviceInfo = irp->deviceInfo;
		BTH_ADDR btAddr = deviceInfo->address;
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
			irp->deviceInfo = deviceInfo;

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
		PBTH_DEVICE_INFO deviceInfo = irp->deviceInfo;
		BTH_ADDR btAddr = deviceInfo->address;
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

		if (GetProtocolValue(log, resp, responseSize, RFCOMM_PROTOCOL_UUID16, sizeof(UINT8), &Port))
		{
			log(L"!! Found Device [%I64X:%X] !!\r\n", btAddr, Port);

			deviceInfo->classOfDevice = Port;
			int i;
			WCHAR name[BTH_MAX_NAME_SIZE];
			if (deviceInfo->flags & BDIF_NAME)
			{
				i = MultiByteToWideChar(CP_UTF8, 0, deviceInfo->name, MAXULONG, name, _countof(name));
			}
			else
			{
				i = swprintf_s(name, _countof(name), L"[%I64X]", btAddr);
			}

			if (0 < i && 0 <= (i = ComboBox_AddString(_hwndAddress, name)))
			{
				ComboBox_SetItemData(_hwndAddress, i, (LPARAM)deviceInfo);
				ComboBox_SetCurSel(_hwndAddress, i);

				if (i == 0)
				{
					EnableWindow(_hwndAddress, TRUE);
					if (!_pc) 
					{
						EnableWindow(GetDlgItem(hwndDlg, ID_CONNECT), TRUE);
					}
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

	void OnDestroy()
	{
		if (PBTH_DEVICE_INFO_LIST pList = _pList)
		{
			LocalFree(pList);
		}
		BthDlg::OnDestroy();
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
			log(L"recv %x bytes:\r\n", wParam);
			if (wParam < 0x100 && !(wParam & 1) &&
				!*(PWSTR)RtlOffsetToPointer(lParam, wParam - sizeof(WCHAR)))
			{
				log << (PWSTR)lParam << L"\r\n";
			}
			return 0;

		case WM_DESTROY:
			OnDestroy();
			break;

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
				if (_dwInquireCount) { log << L"can not close while inquire active\r\n"; } else EndDialog(hwndDlg, 0);
				break;
			case ID_CONNECT:
				if (!_pc)
				{
					Connect(hwndDlg);
				}
				break;
			case ID_START:
				if (!_ps)
				{
					if (BTH_RADIO* port = GetSelectedPort())
					{
						StartServer(hwndDlg, port);
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
				StartInquire(hwndDlg);
				break;
			}
			return 0;
		}
		return ZDlg::DialogProc(hwndDlg, uMsg, wParam, lParam);
	}
};

void bthDlg(HWND hwndDlg)
{
	BtDlg dlg;
	dlg.DoModal((HINSTANCE)&__ImageBase, MAKEINTRESOURCEW(IDD_DIALOG1), hwndDlg, 0);
}

#include "../inc/initterm.h"
#include "CertServer.h"
void InsertSC(HWND hwndDlg);

HRESULT Install();
BOOL BuildSdp(USHORT Psm, UCHAR Cn);
void L2Test();
void ImportCert(HWND hwndDlg);
HRESULT Uninstall();
NTSTATUS IsInstalled();

DWORD HashString(PCSTR lpsz, DWORD hash = 0)
{
	while (char c = *lpsz++) hash = hash * 33 + (c | 0x20);
	return hash;
}

void PrintHash(PCSTR lpsz)
{
	DbgPrint("0x%08X, // \"%s\"\n", HashString(lpsz), lpsz);
}

INT_PTR CALLBACK StartDialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_COMMAND:
		void (*fn)(HWND) = 0;
		switch (wParam)
		{
		case IDCANCEL:
			EndDialog(hwndDlg, lParam);
			break;
		case IDC_BUTTON1:
			fn = ImportCert;
			break;
		case IDC_BUTTON2:
			fn = InsertSC;
			break;
		case IDC_BUTTON3:
			fn = bthDlg;
			break;
		case IDC_BUTTON4:
			if (0 > IsInstalled())
			{
				ShowErrorBox(hwndDlg,  Install(), L"Install", MB_OK);
			}
			else
			{
				ShowErrorBox(hwndDlg, STATUS_ALREADY_COMPLETE|FACILITY_NT_BIT, L"already installed", MB_ICONINFORMATION);
			}
			break;
		case IDC_BUTTON5:
			lParam = Uninstall();
			break;
		}
		if (fn)
		{
			ShowWindow(hwndDlg, SW_HIDE);
			fn(HWND_DESKTOP);
			SetWindowPos(hwndDlg, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE|SWP_NOSIZE|SWP_SHOWWINDOW);
		}
		break;
	}
	return 0;
}

//#pragma warning(disable: 4483) // Allow use of __identifier
//__identifier("*");
void UdpSrv();
void UdpCli();


void CALLBACK ep(void*)
{
	initterm();

	if (0 <= CoInitializeEx(0, COINIT_DISABLE_OLE1DDE|COINIT_APARTMENTTHREADED))
	{
		WSADATA wd;
		if (!WSAStartup(WINSOCK_VERSION, &wd))
		{
			//UdpCli();
			DialogBoxParamW((HINSTANCE)&__ImageBase, MAKEINTRESOURCEW(IDD_DIALOG5), 0, StartDialogProc, 0);
			//if (GetTickCount())
			//{
			//	UdpCli();
			//	UdpSrv();
			//}
			//else
			//{
			//}
			WSACleanup();
		}
		CoUninitialize();
	}

	IO_RUNDOWN::g_IoRundown.BeginRundown();
	destroyterm();
	ExitProcess(0);
}

_NT_END