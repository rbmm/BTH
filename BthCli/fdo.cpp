#include "stdafx.h"

_NT_BEGIN

#include "common.h"
#include "debug.h"

#define IOCTL_L2CA_OPEN_CHANNEL CTL_CODE(FILE_DEVICE_BLUETOOTH, BRB_L2CA_OPEN_CHANNEL, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_L2CA_CLOSE_CHANNEL CTL_CODE(FILE_DEVICE_BLUETOOTH, BRB_L2CA_CLOSE_CHANNEL, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_L2CA_ACL_TRANSFER CTL_CODE(FILE_DEVICE_BLUETOOTH, BRB_L2CA_ACL_TRANSFER, METHOD_BUFFERED, FILE_ANY_ACCESS)

NTSTATUS OnComplete([[maybe_unused]] PDEVICE_OBJECT DeviceObject, PIRP Irp, PVOID fdo)
{
	DbgPrint("OnComplete(%x {%x, %p})\n", Irp->PendingReturned, Irp->IoStatus.Status, Irp->IoStatus.Information);
	
	if (Irp->PendingReturned) IoMarkIrpPending(Irp);

	ExReleaseRundownProtection(reinterpret_cast<FDO*>(fdo));

	return ContinueCompletion;
}

NTSTATUS FDO::PassDown(PIRP Irp, PIO_STACK_LOCATION IrpSp)
{
	PIO_STACK_LOCATION nextIrpSp = IrpSp - 1;
	*nextIrpSp = *IrpSp;
	nextIrpSp->Control = SL_INVOKE_ON_SUCCESS|SL_INVOKE_ON_ERROR|SL_INVOKE_ON_CANCEL;
	nextIrpSp->Context = this;
	nextIrpSp->CompletionRoutine = OnComplete;
	return IofCallDriver(_AttachedToDeviceObject, Irp);
}

NTSTATUS FDO::PassDownSynchronous(PIRP Irp, PIO_STACK_LOCATION IrpSp)
{
	KEVENT Event;
	KeInitializeEvent(&Event, SynchronizationEvent, FALSE);

	PIO_STACK_LOCATION nextIrpSp = IrpSp - 1;
	*nextIrpSp = *IrpSp;

	NTSTATUS status = CallSync(_AttachedToDeviceObject, Irp, nextIrpSp, &Event);

	ExReleaseRundownProtection(this);

	return status;
}

struct __declspec(uuid("11112222-3333-4444-5555-666677778888")) TestItf;

NTSTATUS SyncIoctl(
				   _In_  ULONG IoControlCode,
				   _In_  PDEVICE_OBJECT DeviceObject,
				   _In_opt_  PVOID InputBuffer,
				   _In_  ULONG InputBufferLength,
				   _Out_opt_ PVOID OutputBuffer,
				   _In_ ULONG OutputBufferLength,
				   _In_ BOOLEAN InternalDeviceIoControl,
				   _Out_opt_ PULONG_PTR BytesReturned = 0
				   )
{
	IO_STATUS_BLOCK iosb;
	KEVENT Event;

	if (PIRP Irp = IoBuildDeviceIoControlRequest(IoControlCode, DeviceObject, InputBuffer, InputBufferLength, 
		OutputBuffer, OutputBufferLength, InternalDeviceIoControl, &Event, &iosb))
	{
		KeInitializeEvent(&Event, NotificationEvent, FALSE);

		NTSTATUS status = IofCallDriver(DeviceObject, Irp);

		DbgPrint("ioctl_%x(%p)=%x\n", IoControlCode, Irp, status);

		if (status == STATUS_PENDING)
		{
			status = KeWaitForSingleObject(&Event, Executive, KernelMode, FALSE, 0);

			if (status == STATUS_WAIT_0)
			{
				status = iosb.Status;
			}

			DbgPrint("final status=%x\n", status);
		}

		if (BytesReturned)
		{
			*BytesReturned = iosb.Information;
		}

		return status;
	}

	return STATUS_INSUFFICIENT_RESOURCES;
}

NTSTATUS FDO::PnP(PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp)
{
	DbgPrint("FDO::PnP- %s\n", PnPMinorFunctionString(IrpSp->MinorFunction));

	NTSTATUS status;

	switch(IrpSp->MinorFunction)
	{
	case IRP_MN_QUERY_DEVICE_RELATIONS:
		DbgPrint("\nFDO:QueryDeviceRelations(%s) %p %x\n", 
			QueryDeviceRelationsString(IrpSp->Parameters.QueryDeviceRelations.Type), 
			Irp->IoStatus.Information, 
			Irp->IoStatus.Status);
		Dump(reinterpret_cast<PDEVICE_RELATIONS>(Irp->IoStatus.Information));
		break;

	case IRP_MN_REMOVE_DEVICE:
		DbgPrint("IRP_MN_REMOVE_DEVICE(%p)\n", Ptr);
		IoSetDeviceInterfaceState(this, TRUE);
		RtlFreeUnicodeString(this);
		status = PassDown(Irp, IrpSp);
		ExWaitForRundownProtectionRelease(this);
		DbgPrint("-= delete FDO:%p =-\n", DeviceObject);
		IoDetachDevice(_AttachedToDeviceObject);
		delete this;
		IoDeleteDevice(DeviceObject);
		return status;

	case IRP_MN_START_DEVICE:
		if (0 <= (status = PassDownSynchronous(Irp, IrpSp)) && 0 <= (status = InitBrb()))
		{
			status = IoRegisterDeviceInterface(_PhysicalDeviceObject, &__uuidof(TestItf), 0, this);
			DbgPrint("START_DEVICE:%x %wZ\n", status, static_cast<PCUNICODE_STRING>(this));
			if (0 <= (status))
			{
				status = IoSetDeviceInterfaceState(this, TRUE);
			}
		}
		return CompleteIrp(Irp, status);
	} 

	return PassDown(Irp, IrpSp);
}

NTSTATUS FDO::CommonDispatch(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	NTSTATUS status = STATUS_DELETE_PENDING;

	PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation(Irp);

	PFILE_OBJECT FileObject = IrpSp->FileObject;

	switch(IrpSp->MajorFunction)
	{
	case IRP_MJ_PNP:
		if (ExAcquireRundownProtection(this))
		{
			return PnP(DeviceObject, Irp, IrpSp);
		}
		break;

	case IRP_MJ_POWER:
	case IRP_MJ_SYSTEM_CONTROL:
		if (ExAcquireRundownProtection(this))
		{
			return PassDown(Irp, IrpSp);
		}
		break;

	case IRP_MJ_CLEANUP:
		status = OnCleanup(DeviceObject, FileObject);
		break;

	case IRP_MJ_CLOSE:
		status = OnClose(DeviceObject, FileObject);
		break;

	case IRP_MJ_CREATE:
		status = OnCreate(DeviceObject, FileObject, IrpSp);
		break;

	case IRP_MJ_DEVICE_CONTROL:
		if (FileObject && FileObject->FsContext2 == DeviceObject)
		{
			DbgPrint("DEVICE_CONTROL: file=%p irp=%p\n", FileObject, Irp);

			if (!IsListEmpty(&FileObject->IrpList))
			{
				if (PIO_COMPLETION_CONTEXT CompletionContext = FileObject->CompletionContext)
				{
					ULONG InputBufferLength = IrpSp->Parameters.DeviceIoControl.InputBufferLength;
					ULONG OutputBufferLength = IrpSp->Parameters.DeviceIoControl.OutputBufferLength;

					if (InputBufferLength != OutputBufferLength)
					{
						status = STATUS_INVALID_PARAMETER_2;
						break;
					}

					switch (IrpSp->Parameters.DeviceIoControl.IoControlCode)
					{
					case IOCTL_L2CA_OPEN_CHANNEL:
						if (InputBufferLength != sizeof(_BRB_L2CA_OPEN_CHANNEL))
						{
							status = STATUS_INVALID_PARAMETER_2;
							goto __0;
						}
						return OpenChannel(Irp, IrpSp - 1, FileObject, CompletionContext);

					case IOCTL_L2CA_ACL_TRANSFER:
						if (InputBufferLength != sizeof(_BRB_L2CA_ACL_TRANSFER))
						{
							status = STATUS_INVALID_PARAMETER_2;
							goto __0;
						}
						return AclTransfer(reinterpret_cast<ConnectionContext*>(FileObject->FsContext), Irp, IrpSp - 1);

					case IOCTL_L2CA_CLOSE_CHANNEL:
						if (InputBufferLength)
						{
							status = STATUS_INVALID_PARAMETER_2;
							goto __0;
						}
						reinterpret_cast<ConnectionContext*>(FileObject->FsContext)->Disconnect();
						status = STATUS_SUCCESS;
						goto __0;

					default: status = STATUS_INVALID_PARAMETER_1;
					}

					break;
				}
			}
			status = STATUS_PORT_NOT_SET;
			break;
		}
		status = STATUS_INVALID_PARAMETER;
		break;

	default:
		status = STATUS_NOT_IMPLEMENTED;
	}
__0:
	return CompleteIrp(Irp, status);
}

NTSTATUS FDO::OnCreate(PDEVICE_OBJECT DeviceObject, PFILE_OBJECT FileObject, PIO_STACK_LOCATION IrpSp)
{
	DbgPrint("%s<%p>\n", __FUNCTION__, this);

	if ((IrpSp->Parameters.Create.Options & (FILE_SYNCHRONOUS_IO_ALERT|FILE_SYNCHRONOUS_IO_NONALERT)) || 
		FileObject->FileName.Length || FileObject->RelatedFileObject)
	{
		return STATUS_INVALID_PARAMETER;
	}

	if (ConnectionContext* FsContext = new ConnectionContext(this))
	{
		FileObject->FsContext = FsContext;
		FileObject->FsContext2 = DeviceObject;
		return STATUS_SUCCESS;
	}

	return STATUS_INSUFFICIENT_RESOURCES;
}

NTSTATUS FDO::OnCleanup(PDEVICE_OBJECT DeviceObject, PFILE_OBJECT FileObject)
{
	DbgPrint("%s<%p>\n", __FUNCTION__, this);

	if (!FileObject || DeviceObject != FileObject->FsContext2)
	{
		return STATUS_INVALID_PARAMETER;
	}

	reinterpret_cast<ConnectionContext*>(FileObject->FsContext)->Rundown();

	return STATUS_SUCCESS;
}

NTSTATUS FDO::OnClose(PDEVICE_OBJECT DeviceObject, PFILE_OBJECT FileObject)
{
	DbgPrint("%s<%p>\n", __FUNCTION__, this);

	if (!FileObject || DeviceObject != FileObject->FsContext2)
	{
		return STATUS_INVALID_PARAMETER;
	}

	delete reinterpret_cast<ConnectionContext*>(FileObject->FsContext);
	
	FileObject->FsContext = 0;
	FileObject->FsContext2 = 0;

	return STATUS_SUCCESS;
}

_NT_END