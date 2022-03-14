#include "stdafx.h"

_NT_BEGIN

#include "CM.h"
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

	ULONG cb = 0x80;

	CONFIGRET cr;

	do 
	{
		cr = CR_OUT_OF_MEMORY;

		if (buf = new UCHAR[cb])
		{
			if (CR_SUCCESS == (cr = CM_Get_Device_Interface_PropertyW(pszDeviceInterface, &DEVPKEY_NAME, &PropertyType, PropertyBuffer, &cb, 0)))
			{
				if (PropertyType == DEVPROP_TYPE_STRING)
				{
					*ppszName = pszName;
					return CR_SUCCESS;
				}

				cr = CR_WRONG_TYPE;
			}
			delete [] buf;
		}

	} while (cr == CR_BUFFER_SMALL);

	return cr;
}

ULONG OpenDevice(_Out_ PHANDLE phFile, _In_ const GUID* InterfaceClassGuid)
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
		cr = CM_Get_Device_Interface_List_SizeW(&rcb, const_cast<GUID*>(InterfaceClassGuid), 0, CM_GET_DEVICE_INTERFACE_LIST_PRESENT);

		if (cr != CR_SUCCESS)
		{
			break;
		}

		if (cb < (rcb *= sizeof(WCHAR)))
		{
			cb = RtlPointerToOffset(buf = alloca(rcb - cb), stack);
		}

		cr = CM_Get_Device_Interface_ListW(const_cast<GUID*>(InterfaceClassGuid), 
			0, Buffer, cb, CM_GET_DEVICE_INTERFACE_LIST_PRESENT);

	} while (cr == CR_BUFFER_SMALL);

	if (cr == CR_SUCCESS)
	{
		while (*Buffer)
		{
			HANDLE hFile = CreateFileW(Buffer, 0, 0, 0, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, 0);
			if (hFile != INVALID_HANDLE_VALUE)
			{
				*phFile = hFile;
				return NOERROR;
			}
			Buffer += wcslen(Buffer) + 1;
		}

		return ERROR_GEN_FAILURE;
	}

	return CM_MapCrToWin32Err(cr, ERROR_GEN_FAILURE);
}

//////////////////////////////////////////////////////////////////////////

CONFIGRET DevNodeString(_Out_ PWSTR* ppszName, _In_ DEVNODE dnDevInst, _In_ CONST DEVPROPKEY *PropertyKey)
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
			if (CR_SUCCESS == (cr = CM_Get_DevNode_PropertyW(dnDevInst, PropertyKey, &PropertyType, pb, &cb, 0)))
			{
				if (PropertyType == DEVPROP_TYPE_STRING)
				{
					*ppszName = sz;
					return CR_SUCCESS;
				}

				cr = CR_FAILURE;
			}
			LocalFree(pv);
		}

	} while (cr == CR_BUFFER_SMALL);

	return cr;
}

CONFIGRET GetDeviceId(_Out_ DEVINSTID_W* ppDeviceID, _In_ PCWSTR pszDeviceInterface)
{
	DEVPROPTYPE PropertyType;

	union {
		PBYTE PropertyBuffer;
		PVOID buf;
		DEVINSTID_W pDeviceID;
	};

	ULONG cb = 0x80;
	CONFIGRET cr;

	do 
	{
		cr = CR_OUT_OF_MEMORY;

		if (buf = LocalAlloc(0, cb))
		{
			if (CR_SUCCESS == (cr = CM_Get_Device_Interface_PropertyW(pszDeviceInterface, 
				&DEVPKEY_Device_InstanceId, &PropertyType, PropertyBuffer, &cb, 0)))
			{
				if (PropertyType == DEVPROP_TYPE_STRING)
				{
					*ppDeviceID = pDeviceID;
					return CR_SUCCESS;
				}

				cr = CR_FAILURE;
			}

			LocalFree(buf);
		}

	} while (cr == CR_BUFFER_SMALL);

	return cr;
}

_NT_END