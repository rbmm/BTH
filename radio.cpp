#include "stdafx.h"

_NT_BEGIN

#include "msg.h"
#include "util.h"
#include "radio.h"
#include "BthRequest.h"

BTH_RADIO::~BTH_RADIO()
{
	if (HANDLE hDevice = _hDevice)
	{
		NtClose(hDevice), _hDevice = 0;
	}

	DbgPrint("%s[%x]\n", __FUNCTION__, _id);
}

HRESULT BTH_RADIO::EnableScan()
{
	if (BTH_REQUEST* irp = new BTH_REQUEST(0, 0, 0))
	{
		// [0] 0-disabled,1-inquire=1,page=0,2-inquire=0,page=1,3-enabled
		// [1]
		// [2] 0,1
		// [3]
		static const ULONG u = 0x00010103;

		irp->CheckStatus(NtDeviceIoControlFile(_hDevice, 0, 0, irp, irp, 
			IOCTL_BTH_SET_SCAN_ENABLE, (void*)&u, sizeof(u), 0, 0));

		return S_OK;
	}

	return HRESULT_FROM_NT(STATUS_NO_MEMORY);
}

NTSTATUS BTH_RADIO::Init(_In_ PCWSTR pszDeviceInterface)
{
	HANDLE hDevice = CreateFileW(pszDeviceInterface, 0, 0, 0, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, 0);

	if (hDevice != INVALID_HANDLE_VALUE)
	{
		BTH_LOCAL_RADIO_INFO bli;

		NTSTATUS status = SyncIoctl(hDevice, IOCTL_BTH_GET_LOCAL_INFO, 0, 0, &bli, sizeof(bli));

		if (0 <= status)
		{
			if (0 <= (status = BTH_REQUEST::Bind(hDevice)))
			{
				_btAddr = bli.localInfo.address, _hDevice = hDevice;
				return S_OK;
			}
		}

		NtClose(hDevice);

		return HRESULT_FROM_NT(status);
	}

	return GetLastErrorEx();
}

_NT_END