#pragma once

#include "../asio/socket.h"
#include "pfx.h"
#include "SCC.h"

enum { WM_DONE = WM_APP, WM_SRV_CLOSED };

class PfxSocket : public CTcpEndpoint
{
	HWND _hwnd;
	PFX_CONTEXT* _ctx;
	ULONG _id;

	ULONG _cbReceived = 0, _cbNeed = 0;
	union {
		SC_Cntr _sc;
		UCHAR _buf[];
	};

	enum { max_sc_size = 0x4000 };

private:

	virtual BOOL OnRecv(PSTR /*Buffer*/, ULONG cbTransferred);

	virtual ULONG GetRecvBuffers(WSABUF lpBuffers[2], void** ppv);

	virtual BOOL OnConnect(ULONG dwError);

	virtual void OnDisconnect();

	virtual ~PfxSocket();

public:

	PfxSocket(HWND hwnd, ULONG id, CSocketObject* pAddress, PFX_CONTEXT* ctx);

	ULONG get_id()
	{
		return _id;
	}

	void* operator new(size_t s)
	{
		return CTcpEndpoint::operator new(s + max_sc_size);
	}
};
