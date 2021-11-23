#include "stdafx.h"

_NT_BEGIN

extern volatile const UCHAR guz;

CONFIGRET GetFriendlyName(_Out_ PWSTR* ppszName, _In_ DEVINST dnDevInst)
{
	DEVPROPTYPE PropertyType;

	union {
		PVOID pv;
		PWSTR sz;
		PBYTE pb;
	};

	ULONG cb = 0x80;
	CONFIGRET cr;

	do 
	{
		cr = CR_OUT_OF_MEMORY;

		if (pv = LocalAlloc(0, cb))
		{
			if (CR_SUCCESS == (cr = CM_Get_DevNode_PropertyW(dnDevInst, &DEVPKEY_NAME, &PropertyType, pb, &cb, 0)))
			{
				*ppszName = sz;
				return CR_SUCCESS;
			}
			LocalFree(pv);
		}

	} while (cr == CR_BUFFER_SMALL);

	return cr;
}

CONFIGRET GetFriendlyName(_Out_ PWSTR* ppszName, _In_ PCWSTR pszDeviceInterface)
{
	DEVPROPTYPE PropertyType;

	union {
		PBYTE PropertyBuffer;
		PVOID buf;
		PWSTR pszName;
		DEVINSTID_W pDeviceID;
	};

	ULONG cb = 0, rcb = 0x80;
	CONFIGRET cr;
	PVOID stack = alloca(guz);

	do 
	{
		if (cb < rcb)
		{
			rcb = cb = RtlPointerToOffset(buf = alloca(rcb - cb), stack);
		}

		cr = CM_Get_Device_Interface_PropertyW(pszDeviceInterface, 
			&DEVPKEY_Device_InstanceId, &PropertyType, PropertyBuffer, &rcb, 0);

	} while (cr == CR_BUFFER_SMALL);

	if (cr == CR_SUCCESS)
	{
		if (PropertyType != DEVPROP_TYPE_STRING)
		{
			return CR_FAILURE;
		}

		DEVINST dnDevInst;

		if (CR_SUCCESS == (cr = CM_Locate_DevNodeW(&dnDevInst, pDeviceID, CM_LOCATE_DEVNODE_NORMAL)))
		{
			return GetFriendlyName(ppszName, dnDevInst);
		}
	}

	return cr;
}

_NT_END