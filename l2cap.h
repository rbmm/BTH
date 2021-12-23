#pragma once

#include "../asio/io.h"

class __declspec(novtable) L2capSocket : public IO_OBJECT
{
	BTH_ADDR _btAddr = 0;

	enum { c_connect = 'tcnc', c_callback = 'kblc', c_recv = 'vcer', c_send = 'dnes', c_disc = 'tcsd' };

protected:
	virtual ~L2capSocket()
	{
	}

	virtual void NeedRecv(ULONG PacketLength);

private:
	virtual void IOCompletionRoutine(CDataPacket* packet, DWORD Code, NTSTATUS status, ULONG_PTR dwNumberOfBytesTransfered, PVOID Pointer);

	virtual BOOL OnConnect(NTSTATUS status, _BRB_L2CA_OPEN_CHANNEL* OpenChannel) = 0;
	virtual void OnRecv(NTSTATUS status, _BRB_L2CA_ACL_TRANSFER* acl) = 0;
	virtual void OnSend(NTSTATUS /*status*/, _BRB_L2CA_ACL_TRANSFER* /*acl*/)
	{

	}
	virtual void OnDisconnect(NTSTATUS status) = 0;

public:

	NTSTATUS Recv(CDataPacket* packet);
	NTSTATUS Send(CDataPacket* packet);
	NTSTATUS Send(const void* pv, ULONG cb);

	HRESULT Create();
	NTSTATUS Disconnect();
	NTSTATUS Connect(BTH_ADDR btAddr, USHORT Psm);
};
