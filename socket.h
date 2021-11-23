#pragma once

enum { WM_CONNECT = WM_APP, WM_DISCONNECT, WM_RECV, WM_BTH };

class BthSocket : public CTcpEndpoint
{
	PCSTR name;
	HWND _hwnd;
public:

	enum Type { t_client, t_server} _t;

private:

	virtual BOOL OnConnect(ULONG dwError)
	{
		PostMessageW(_hwnd, WM_CONNECT, _t, dwError);

		return TRUE;
	}

	virtual void OnDisconnect()
	{
		PostMessageW(_hwnd, WM_DISCONNECT, _t, 0);
	}

	virtual BOOL OnRecv(PSTR Buffer, ULONG cbTransferred)
	{
		DbgPrint("%s<%p>(%x)\n%.*s\n", __FUNCTION__, name, cbTransferred, cbTransferred, Buffer);
		SendMessageW(_hwnd, WM_RECV, cbTransferred, (LPARAM)Buffer);
		return TRUE;
	}

	virtual ~BthSocket()
	{
		DbgPrint("%s<%s>\n", __FUNCTION__, name);
	}

public:

	PSOCKADDR_BTH GetClientAddr()
	{
		return m_RemoteAddrLen == sizeof(SOCKADDR_BTH) && m_RemoteAddr.sa_family == AF_BTH ? &m_bthAddr : 0;
	}

	BthSocket(Type t, HWND hwnd, PCSTR name, CSocketObject* pAddress = 0) : _t(t), CTcpEndpoint(pAddress), name(name), _hwnd(hwnd) 
	{
		DbgPrint("%s<%s>\n", __FUNCTION__, name);
	}
};
