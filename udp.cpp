#include "stdafx.h"
#include "resource.h"
#include <netfw.h>
_NT_BEGIN
#include "../asio/socket.h"
#include "../winZ/window.h"

//////////////////////////////////////////////////////////////////////////

BOOL OpenPort(INetFwOpenPorts* fwOpenPorts, LONG portNumber, NET_FW_IP_PROTOCOL ipProtocol)
{
	BOOL fOk = FALSE;

	INetFwOpenPort* fwOpenPort;

	if (!CoCreateInstance(__uuidof(NetFwOpenPort), 0, CLSCTX_INPROC_SERVER, IID_PPV(fwOpenPort)))
	{
		if (BSTR fwBstrName = SysAllocString(L"{9F82CD7E-BA9F-48b7-BC82-02CCD524775A}"))
		{
			fOk = !fwOpenPort->put_Port(portNumber) &&
				!fwOpenPort->put_Name(fwBstrName) &&
				!fwOpenPort->put_Protocol(ipProtocol) &&
				!fwOpenPorts->Add(fwOpenPort);

			SysFreeString(fwBstrName);
		}

		fwOpenPort->Release();
	}

	return fOk;
}

BOOL OpenPort(WORD portNumber)
{
	BOOL fOk = FALSE;

	INetFwMgr* fwMgr;
	INetFwPolicy* fwPolicy;
	INetFwProfile* fwProfile;
	INetFwOpenPorts* fwOpenPorts;

	if (!CoCreateInstance(__uuidof(NetFwMgr), 0, CLSCTX_INPROC_SERVER, IID_PPV(fwMgr)))
	{
		if (!fwMgr->get_LocalPolicy(&fwPolicy))
		{
			if (!fwPolicy->get_CurrentProfile(&fwProfile))
			{
				if (!fwProfile->get_GloballyOpenPorts(&fwOpenPorts))
				{
					fOk = OpenPort(fwOpenPorts, portNumber, NET_FW_IP_PROTOCOL_UDP);

					fwOpenPorts->Release();
				}

				fwProfile->Release();
			}
			fwPolicy->Release();
		}
		fwMgr->Release();
	}

	return fOk;
}

//////////////////////////////////////////////////////////////////////////

void SendTestPacket(CUdpEndpoint* p, ULONG cb, PSOCKADDR to, ULONG len)
{
	if (CDataPacket* packet = new(cb = (cb + 1) & ~1) CDataPacket)
	{
		packet->setDataSize(cb);
		cb >>= 1;
		PUSHORT pu = (PUSHORT)packet->getData();
		do 
		{
			*pu++ = (USHORT)cb;
		} while (--cb);
		p->SendTo(to, len, packet);
		packet->Release();
	}
}

class YUdp : public CUdpEndpoint
{
public:
protected:
private:
	virtual void OnRecv(PSTR buf, ULONG cb, CDataPacket* packet, SOCKADDR_IN_EX* from)
	{
		DbgPrint("OnRecv(%p, %x)\n", buf, cb);
		if (!buf)
		{
			return;
		}

		{
			WCHAR sz[64];
			ULONG cch = _countof(sz);
			WSAAddressToStringW(&from->saAddress, from->dwAddressLength, 0, sz, &cch);
			DbgPrint("request from %S\r\n", sz);
		}

		SendTestPacket(this, cb, &from->saAddress, from->dwAddressLength);

		RecvFrom(packet);
	}
};

void UdpSrv()
{
	OpenPort(0x3333);
	if (YUdp* p = new YUdp)
	{
		SOCKADDR_IN6 Ipv6 {AF_INET6, 0x3333};
		if (!p->Create((PSOCKADDR)&Ipv6, sizeof(Ipv6)))
		{
			if (CDataPacket* packet = new(0x10000) CDataPacket)
			{
				p->RecvFrom(packet);
				packet->Release();

				MessageBoxW(0,0,L"udp test",MB_ICONINFORMATION);
			}
			p->Close();
		}
		p->Release();
	}
}

class XUdp : public CUdpEndpoint
{
	HANDLE _hThread = OpenThread(THREAD_ALERT, FALSE, GetCurrentThreadId());
public:
protected:
private:
	~XUdp()
	{
		NtClose(_hThread);
	}
	virtual void OnRecv(PSTR buf, ULONG cb, CDataPacket* /*packet*/, SOCKADDR_IN_EX* from)
	{
		DbgPrint("%x>OnRecv(%p, %x) ", GetCurrentThreadId(), buf, cb);
		if (buf)
		{
			CHAR sz[64];
			ULONG cch = _countof(sz);
			NTSTATUS status = -1;
			switch (from->saAddress.sa_family)
			{
			case AF_INET:
				status = RtlIpv4AddressToStringExA(&from->addr.Ipv4.sin_addr, from->addr.Ipv4.sin_port, sz, &cch);
				break;
			case AF_INET6:
				status = RtlIpv6AddressToStringExA(&from->addr.Ipv6.sin6_addr, 
					from->addr.Ipv6.sin6_scope_id ,from->addr.Ipv6.sin6_port, sz, &cch);
				break;
			}
			if (0 <= status)
			{
				DbgPrint("request from %s\r\n", sz);
			}
		}
		ZwAlertThread(_hThread);
	}
};

void ReadEnum();

void UdpCli()
{
	if (XUdp* p = new XUdp)
	{
		SOCKADDR_IN Ipv6 {AF_INET};
		if (!p->Create((PSOCKADDR)&Ipv6, sizeof(Ipv6)))
		{
			Ipv6.sin_port = 0x3333;
			;
			PCSTR pc;
			RtlIpv4StringToAddressA("78.46.177.20", TRUE, &pc, &Ipv6.sin_addr);
			//RtlIpv6StringToAddressA("fe80::e447:b17:8ac2:5b8", &pc, &Ipv6.sin6_addr);
			//RtlIpv6StringToAddressA("fe80::ccd3:17e7:746f:c0e", &pc, &Ipv6.sin6_addr);
			//Ipv6.sin_addr.S_un.S_addr = IP(192,168,1,160);

			//for (ULONG cb = 0x100;cb < 0x10000;cb <<= 1)
			ULONG cb = 0x100;
			ULONG i = 100, a = 0, b = 0;
			do
			{
				if (CDataPacket* packet = new(0x10000) CDataPacket)
				{
					p->RecvFrom(packet);
					packet->Release();
					SendTestPacket(p, cb, (PSOCKADDR)&Ipv6, sizeof(Ipv6));
				}
				LARGE_INTEGER ti = {-10000000,-1};

				switch (ZwDelayExecution(TRUE, &ti))
				{
				case STATUS_TIMEOUT:
					a++;
					break;
				case STATUS_ALERTED:
				case STATUS_SUCCESS:
					b++;
					break;
				default:
					__debugbreak();
				}

			}
			while(--i);
			MessageBoxW(0,0,0,0);
			p->Close();
		}
		p->Release();
	}
}


_NT_END