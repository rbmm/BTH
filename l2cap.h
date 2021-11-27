#pragma once
enum { WM_CONNECT = WM_APP, WM_DISCONNECT, WM_RECV };

class L2capSocket : public IO_OBJECT
{
	PCSTR name;
	HWND _hwnd;
	BTH_ADDR _btAddr;

	enum { c_connect = 'tcnc', c_callback = 'kblc', c_recv = 'vcer', c_send = 'dnes' };
public:

private:
	virtual void IOCompletionRoutine(CDataPacket* packet, DWORD Code, NTSTATUS status, ULONG_PTR dwNumberOfBytesTransfered, PVOID Pointer);

	virtual BOOL OnConnect(NTSTATUS status, _BRB_L2CA_OPEN_CHANNEL* OpenChannel);

	virtual void OnDisconnect(NTSTATUS status);

	virtual void NeedRecv(ULONG PacketLength);

	virtual void OnRecv(NTSTATUS status, _BRB_L2CA_ACL_TRANSFER* acl);

	virtual void OnSend(NTSTATUS status, _BRB_L2CA_ACL_TRANSFER* acl);

	virtual ~L2capSocket()
	{
		DbgPrint("%s<%s>\n", __FUNCTION__, name);
	}

public:
	NTSTATUS Recv(CDataPacket* packet);
	NTSTATUS Send(CDataPacket* packet);
	NTSTATUS Send(const void* pv, ULONG cb);

	L2capSocket(HWND hwnd, PCSTR name) : name(name), _hwnd(hwnd) 
	{
		DbgPrint("%s<%s>\n", __FUNCTION__, name);
	}

	HRESULT Create();
	NTSTATUS Connect(BTH_ADDR btAddr, USHORT Psm);
};
