#pragma once

#ifdef _WIN64
#define __movsp(x, y, n) __movsq((PULONG64)(x), (PULONG64)(y), n)
#else
#define __movsp(x, y, n) __movsd((PULONG)(x), (PULONG)(y), n)
#endif

#ifdef _WIN64
#define __stosp(x, y, n) __stosq((PULONG64)(x), (ULONG64)(y), n)
#else
#define __stosp(x, y, n) __stosd((PULONG)(x), (ULONG)(ULONG_PTR)(y), n)
#endif

#include "MiniPacket.h"
#include "../inc/rundown.h"

NTSTATUS CompleteIrp(PIRP Irp, NTSTATUS status);
NTSTATUS CALLBACK OnCompleteSync([[maybe_unused]] PDEVICE_OBJECT DeviceObject, PIRP Irp, PVOID Event);
NTSTATUS CallSync(PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp, PKEVENT Event);

struct FDO;

struct ConnectionContext
{
	BTH_ADDR _BtAddress;
	PVOID _Port;
	PVOID _Key;
	PVOID _CallbackContext;
	L2CAP_CHANNEL_HANDLE _ConnectionHandle;
	FDO* _Fdo;
	RundownProtection _ConnectionProtect;
	LONG _nRefCount;
	LONG _bActive;
	L2CAP_DISCONNECT_REASON _reason;

	enum { tag  = 'tcnc' };

	ConnectionContext(FDO* Fdo) : _Fdo(Fdo)
	{
		DbgPrint("%s<%p>\n", __FUNCTION__, this);
	}

	~ConnectionContext();

	void* operator new(size_t s)
	{
		if (PVOID pv = ExAllocatePoolWithTag(NonPagedPoolNx, s, tag))
		{
			RtlZeroMemory(pv, s);
			return pv;
		}
		return 0;
	}

	void operator delete(PVOID pv)
	{
		ExFreePoolWithTag(pv, tag);
	}

	BOOL SetActive()
	{
		return InterlockedExchangeNoFence(&_bActive, TRUE) == FALSE;
	}

	void SetNotActive()
	{
		_bActive = FALSE;
	}

	_NODISCARD BOOL AcquireConnection()
	{
		return _ConnectionProtect.Acquire();
	}

	void ReleaseConnection();

	void Disconnect()
	{
		_reason = HciDisconnect;
		Rundown();
	}

	void Rundown();
	void CloseChannel();

	void OnOpenChannelCompleted(_BRB_L2CA_OPEN_CHANNEL* Brb);

	static void NTAPI BthIndicationCallback(
		_In_ PVOID Context,
		_In_ INDICATION_CODE Indication,
		_In_ PINDICATION_PARAMETERS Parameters
		);

	void IndicationCallback(
		_In_ INDICATION_CODE Indication,
		_In_ PINDICATION_PARAMETERS Parameters
		);

	bool SetPort(PVOID Port, PVOID Key, PVOID CallbackContext);

	void NotifyUserMode(_In_ NTSTATUS IoStatus, _In_ ULONG_PTR IoStatusInformation);
};

struct FDO : LIST_ENTRY, EX_RUNDOWN_REF, UNICODE_STRING, BTH_PROFILE_DRIVER_INTERFACE
{
	PDEVICE_OBJECT _AttachedToDeviceObject;
	PDEVICE_OBJECT _PhysicalDeviceObject;

	NTSTATUS PassDown(PIRP Irp, PIO_STACK_LOCATION IrpSp);
	NTSTATUS PassDownSynchronous(PIRP Irp, PIO_STACK_LOCATION IrpSp);
	NTSTATUS PnP(PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);

	NTSTATUS InitBrb();

	NTSTATUS OpenChannel(PIRP Irp, PIO_STACK_LOCATION IrpSp, PFILE_OBJECT FileObject, PIO_COMPLETION_CONTEXT CompletionContext);
	NTSTATUS AclTransfer(ConnectionContext* FsContext, PIRP Irp, PIO_STACK_LOCATION IrpSp);
	
	NTSTATUS OnCleanup(PDEVICE_OBJECT DeviceObject, PFILE_OBJECT FileObject);
	NTSTATUS OnClose(PDEVICE_OBJECT DeviceObject, PFILE_OBJECT FileObject);
	NTSTATUS OnCreate(PDEVICE_OBJECT DeviceObject, PFILE_OBJECT FileObject, PIO_STACK_LOCATION IrpSp);

	NTSTATUS Start(PDEVICE_OBJECT DeviceObject);
	NTSTATUS StartServer(PDEVICE_OBJECT DeviceObject, BTH_ADDR btAddr);
	void SendBrbAsync(PBRB Brb);

	void InitRequest(PIO_STACK_LOCATION IrpSp, PBRB Brb, bool bUserIrp);
	void OnConnect(_In_ PINDICATION_PARAMETERS Parameters);

	static NTSTATUS NTAPI OnCompleteBrb([[maybe_unused]] PDEVICE_OBJECT DeviceObject, PIRP Irp, PVOID Brb);
	static NTSTATUS NTAPI OnCompleteUserIrp([[maybe_unused]] PDEVICE_OBJECT DeviceObject, PIRP Irp, PVOID Brb);

	void CloseChannel(BTH_ADDR BtAddress, L2CAP_CHANNEL_HANDLE ChannelHandle);

	NTSTATUS CommonDispatch(PDEVICE_OBJECT DeviceObject, PIRP Irp);

	FDO(PDEVICE_OBJECT PhysicalDeviceObject) : _PhysicalDeviceObject(PhysicalDeviceObject)
	{
		DbgPrint("%s<%p>\n", __FUNCTION__, this);
		ExInitializeRundownProtection(this);
		InitializeListHead(this);
	}

	~FDO()
	{
		DbgPrint("%s<%p>\n", __FUNCTION__, this);

		PINTERFACE_DEREFERENCE InterfaceDereference;

		if (InterfaceDereference = Interface.InterfaceDereference)
		{
			InterfaceDereference(Interface.Context);
		}
	}

	void* operator new(size_t , PDEVICE_OBJECT DeviceObject)
	{
		RtlZeroMemory(DeviceObject->DeviceExtension, sizeof(FDO));
		return DeviceObject->DeviceExtension;
	}

	void operator delete(void*) {}
};