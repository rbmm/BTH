#pragma once

struct BTH_RADIO : LIST_ENTRY
{
	BTH_ADDR _btAddr = 0;
	HANDLE _hDevice = 0;
	ULONG _id;
	ULONG _hash;

	BTH_RADIO(ULONG id, ULONG hash) : _id(id), _hash(hash)
	{
		DbgPrint("%s[%x]\n", __FUNCTION__, id);
	}

	~BTH_RADIO();

	HRESULT EnableScan();

	NTSTATUS Init(_In_ PCWSTR pszDeviceInterface);
};
