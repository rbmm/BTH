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

_NT_END