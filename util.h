#pragma once

HRESULT GetLastErrorEx(ULONG dwError = GetLastError());

#pragma pack(push, 1)
struct BTH_DEVICE_INQUIRY 
{
	ULONG Lap;					// LAP_GIAC_VALUE or LAP_LIAC_VALUE
	UCHAR TimeoutMultiplier;	// requested length of inquiry (seconds) [ 1, SDP_MAX_INQUIRY_SECONDS)
	UCHAR TransmitPwrLvl;		// <= 0x14
};
#pragma pack(pop)

//
// Input:  BTH_DEVICE_INQUIRY
// Output:  ULONG numOfDevices
//

#define IOCTL_BTH_DEVICE_INQUIRY				BTH_CTL(BTH_IOCTL_BASE+0x400)

#define IOCTL_BTH_SET_SCAN_ENABLE				BTH_CTL(BTH_IOCTL_BASE+0x408)
#define IOCTL_BTH_GET_LOCALSERVICES				BTH_CTL(BTH_IOCTL_BASE+0x411)
#define IOCTL_BTH_GET_LOCALSERVICEINFO			BTH_CTL(BTH_IOCTL_BASE+0x412)
#define IOCTL_BTH_SET_LOCALSERVICEINFO			BTH_CTL(BTH_IOCTL_BASE+0x413)

// BTH_CTL(BTH_IOCTL_BASE+0x411) >>> HCI_GetLocalServices
// BTH_CTL(BTH_IOCTL_BASE+0x412) >>> HCI_GetLocalServiceInfo
// BTH_CTL(BTH_IOCTL_BASE+0x413) >>> HCI_SetLocalServiceInfo

NTSTATUS SyncIoctl(_In_ HANDLE FileHandle, 
				   _In_ ULONG IoControlCode,
				   _In_reads_bytes_opt_(InputBufferLength) PVOID InputBuffer,
				   _In_ ULONG InputBufferLength,
				   _Out_writes_bytes_opt_(OutputBufferLength) PVOID OutputBuffer,
				   _In_ ULONG OutputBufferLength,
				   _Out_opt_ PULONG_PTR Information = 0);

struct __declspec(uuid("ADF8EB1B-0718-4366-A418-BB88F175D360")) MyServiceClass;

inline NTSTATUS UnregisterService(_In_ HANDLE hDevice, _In_ HANDLE_SDP hRecord)
{
	return SyncIoctl(hDevice, IOCTL_BTH_SDP_REMOVE_RECORD, &hRecord, sizeof(hRecord), 0, 0);
}

NTSTATUS RegisterService(_In_ HANDLE hDevice, _In_ LPCGUID guid, _In_ UCHAR Port, _Out_ PHANDLE_SDP phRecord);

NTSTATUS DoHash(PUCHAR pbData, ULONG cbData, PUCHAR pbOutput, ULONG cbOutput, PCWSTR pszAlgId);

#define MD5_HASH_SIZE 16

inline NTSTATUS h_MD5(LPCVOID pbData, ULONG cbData, PUCHAR pbOutput)
{
	return DoHash((PUCHAR)pbData, cbData, pbOutput, MD5_HASH_SIZE, BCRYPT_MD5_ALGORITHM);
}

int ShowErrorBox(HWND hwnd, HRESULT dwError, PCWSTR pzCaption, UINT uType = MB_OK);

NTSTATUS OpenBKey(_Out_ BCRYPT_KEY_HANDLE *phKey, _In_ PCWSTR pszKeyName);

void FixBase64(PWSTR pszString, ULONG cch);
void UnFixBase64(PWSTR pszString, ULONG cch);