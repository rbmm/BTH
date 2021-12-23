#include "stdafx.h"

_NT_BEGIN

#include "common.h"

NTSTATUS CompleteIrp(PIRP Irp, NTSTATUS status)
{
	Irp->IoStatus.Status = status;
	IofCompleteRequest(Irp, IO_NO_INCREMENT);
	return status;
}

NTSTATUS CALLBACK OnCompleteSync([[maybe_unused]] PDEVICE_OBJECT DeviceObject, PIRP Irp, PVOID Event)
{
	DbgPrint("OnCompleteSync(%x {%x, %p})\n", Irp->PendingReturned, Irp->IoStatus.Status, Irp->IoStatus.Information);

	if (Irp->PendingReturned) KeSetEvent((PKEVENT)Event, IO_NO_INCREMENT, FALSE);

	return StopCompletion;
}

NTSTATUS CallSync(PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp, PKEVENT Event)
{
	IrpSp->CompletionRoutine = OnCompleteSync;
	IrpSp->Context = Event;
	IrpSp->Control = SL_INVOKE_ON_SUCCESS|SL_INVOKE_ON_ERROR|SL_INVOKE_ON_CANCEL;

	Irp->IoStatus.Status = STATUS_NOT_SUPPORTED;

	NTSTATUS status = IoCallDriver( DeviceObject, Irp );

	if (STATUS_PENDING == status)
	{
		DbgPrint("Synchronous:Wait\n");

		if (KeWaitForSingleObject(&Event, Executive, KernelMode, FALSE, 0) != STATUS_WAIT_0)
		{
			__debugbreak();
		}

		status = Irp->IoStatus.Status;
	}

	if (IrpSp != IoGetNextIrpStackLocation( Irp )) __debugbreak();

	return status;
}

NTSTATUS NTAPI DiscComplete (
							 _In_ PDEVICE_OBJECT DeviceObject,
							 _In_ PIRP Irp,
							 _In_ PVOID /*Context*/
					   )
{
	DbgPrint("DISCOVERY end(%p %p %x %x)\n", Irp, DeviceObject, Irp->PendingReturned, Irp->IoStatus.Status);
	return ContinueCompletion;
}

#define IOCTL_BTH_ENABLE_DISCOVERY BTH_CTL(BTH_IOCTL_BASE+0x408)

NTSTATUS EnabdleDiscovery(PDEVICE_OBJECT DeviceObject)
{
	ULONG u = 0x00010103;

	if (PIRP Irp = IoBuildDeviceIoControlRequest(IOCTL_BTH_ENABLE_DISCOVERY, DeviceObject, &u, sizeof(u), 0, 0, FALSE, 0, 0))
	{
		Irp->UserIosb = &Irp->IoStatus;

		PIO_STACK_LOCATION IrpSp = IoGetNextIrpStackLocation(Irp);
		IrpSp->Control = SL_INVOKE_ON_CANCEL|SL_INVOKE_ON_ERROR|SL_INVOKE_ON_SUCCESS;
		IrpSp->CompletionRoutine = DiscComplete;

		NTSTATUS status = IofCallDriver(DeviceObject, Irp);

		DbgPrint("ENABLE_DISCOVERY(%p)=%x\n", Irp, status);

		return status;
	}

	return STATUS_INSUFFICIENT_RESOURCES;
}

_NT_END