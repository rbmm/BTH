#include "stdafx.h"

_NT_BEGIN
#include <initguid.h>
#include <bthguid.h >
#include "debug.h" 
#include "common.h"

NTSTATUS BrbQueryItf(PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp, PKEVENT Event)
{
	IrpSp->MajorFunction = IRP_MJ_PNP;
	IrpSp->MinorFunction = IRP_MN_QUERY_INTERFACE;

	IrpSp->Parameters.QueryInterface.InterfaceSpecificData = NULL;

	NTSTATUS status = CallSync(DeviceObject, Irp, IrpSp, Event);

	DbgPrint("BrbQueryItf=%x\n", status);
	return status;
}

NTSTATUS SendBrbSync(PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp, PKEVENT Event, PBRB Brb)
{
	IrpSp->MajorFunction = IRP_MJ_INTERNAL_DEVICE_CONTROL;
	IrpSp->MinorFunction = 0;
	IrpSp->Parameters.DeviceIoControl.IoControlCode = IOCTL_INTERNAL_BTH_SUBMIT_BRB;
	IrpSp->Parameters.Others.Argument1 = Brb;

	NTSTATUS status = CallSync(DeviceObject, Irp, IrpSp, Event);

	DbgPrint("SendBrbSync<%x>=%x\n", Brb->BrbHeader.Type, status);
	
	return status;
}

NTSTATUS FDO::InitBrb()
{
	PDEVICE_OBJECT DeviceObject = _PhysicalDeviceObject;

	if (PIRP Irp = IoAllocateIrp( DeviceObject->StackSize, FALSE ))
	{
		KEVENT Event;
		KeInitializeEvent(&Event, SynchronizationEvent, FALSE);

		PIO_STACK_LOCATION IrpSp = IoGetNextIrpStackLocation( Irp );

		auto QueryInterface = &IrpSp->Parameters.QueryInterface;

		QueryInterface->InterfaceType = const_cast<PGUID>(&GUID_BTHDDI_PROFILE_DRIVER_INTERFACE);
		QueryInterface->Size = sizeof(BTH_PROFILE_DRIVER_INTERFACE);
		QueryInterface->Interface = &Interface;
		QueryInterface->Version = BTHDDI_PROFILE_DRIVER_INTERFACE_VERSION_FOR_QI;

		NTSTATUS status = BrbQueryItf(DeviceObject, Irp, IrpSp, &Event);

		IoFreeIrp(Irp);

		return status;
	}

	return STATUS_INSUFFICIENT_RESOURCES;
}

NTSTATUS FDO::OnCompleteUserIrp([[maybe_unused]] PDEVICE_OBJECT DeviceObject, PIRP Irp, PVOID Brb)
{
	FDO* This = reinterpret_cast<FDO*>(reinterpret_cast<_BRB_HEADER*>(Brb)->ClientContext[0]);
	
	if (Irp->PendingReturned)
	{
		IoMarkIrpPending(Irp);
	}

	ConnectionContext* FsContext = reinterpret_cast<ConnectionContext*>(IoGetCurrentIrpStackLocation(Irp)->FileObject->FsContext);

	DbgPrint("OnCompleteUserIrp<%p:%p>(status{%x, %p} brb<%p>[%x]{s=%x bt=%s})\n", This, FsContext,
		Irp->IoStatus.Status, Irp->IoStatus.Information, 
		Brb, reinterpret_cast<_BRB_HEADER*>(Brb)->Type,
		reinterpret_cast<_BRB_HEADER*>(Brb)->Status, GetBthError(reinterpret_cast<_BRB_HEADER*>(Brb)->BtStatus));

	switch (reinterpret_cast<_BRB_HEADER*>(Brb)->Type)
	{
	case BRB_L2CA_OPEN_CHANNEL:
		FsContext->OnOpenChannelCompleted(reinterpret_cast<_BRB_L2CA_OPEN_CHANNEL*>(Brb));
		break;
	case BRB_L2CA_ACL_TRANSFER:
		if (PMDL Mdl = reinterpret_cast<_BRB_L2CA_ACL_TRANSFER*>(Brb)->BufferMDL)
		{
			DbgPrint("--Mdl<%p>(f=%x va=%p)\n", Mdl, Mdl->MdlFlags, Mdl->MappedSystemVa);
			MmUnlockPages(Mdl);
			IoFreeMdl(Mdl);
		}
		FsContext->ReleaseConnection();
		break;
	}

	ExReleaseRundownProtection(This);
	return ContinueCompletion;
}

void FDO::InitRequest(PIO_STACK_LOCATION IrpSp, PBRB Brb, bool bUserIrp)
{
	Brb->BrbHeader.ClientContext[0] = this;

	IrpSp->MajorFunction = IRP_MJ_INTERNAL_DEVICE_CONTROL;
	IrpSp->MinorFunction = 0;

	IrpSp->Parameters.DeviceIoControl.IoControlCode = IOCTL_INTERNAL_BTH_SUBMIT_BRB;
	IrpSp->Parameters.Others.Argument1 = Brb;

	IrpSp->CompletionRoutine = bUserIrp ? OnCompleteUserIrp : OnCompleteBrb;
	IrpSp->Context = Brb;
	IrpSp->Control = SL_INVOKE_ON_SUCCESS|SL_INVOKE_ON_ERROR|SL_INVOKE_ON_CANCEL;
}

NTSTATUS FDO::OpenChannel(
	PIRP Irp, 
	PIO_STACK_LOCATION IrpSp, 
	PFILE_OBJECT FileObject, 
	PIO_COMPLETION_CONTEXT CompletionContext
	)
{
	ConnectionContext* FsContext = reinterpret_cast<ConnectionContext*>(FileObject->FsContext);

	union {
		PVOID Buffer;
		PBRB Brb;
		_BRB_L2CA_OPEN_CHANNEL* Open;
	};

	Buffer = Irp->AssociatedIrp.SystemBuffer;

	NTSTATUS status;

	if (FsContext->SetPort(CompletionContext->Port, CompletionContext->Key, Open->CallbackContext))
	{
		if (FsContext->SetActive())
		{
			BTH_ADDR BtAddress = Open->BtAddress;
			USHORT Psm = Open->Psm;

			DbgPrint("OpenChannel[%I64X:%X]\n", BtAddress, Psm);

			BthInitializeBrb(Brb, BRB_L2CA_OPEN_CHANNEL);

			InitRequest(IrpSp, Brb, true);

			Open->BtAddress = BtAddress;
			Open->Psm = Psm;

			Open->ChannelFlags = CF_ROLE_EITHER;

			Open->ConfigOut.Flags = CFG_MTU;
			Open->ConfigOut.Mtu.Max = L2CAP_MAX_MTU;
			Open->ConfigOut.Mtu.Min = L2CAP_MIN_MTU;
			Open->ConfigOut.Mtu.Preferred = L2CAP_DEFAULT_MTU;

			Open->ConfigIn.Flags = CFG_MTU;
			Open->ConfigIn.Mtu.Max = L2CAP_MAX_MTU;
			Open->ConfigIn.Mtu.Min = L2CAP_MIN_MTU;
			Open->ConfigIn.Mtu.Preferred = L2CAP_DEFAULT_MTU;

			//
			// Get notificaiton about remote disconnect 
			//
			Open->CallbackFlags = CALLBACK_DISCONNECT|CALLBACK_RECV_PACKET;                                                   

			Open->Callback = ConnectionContext::BthIndicationCallback;
			Open->CallbackContext = FsContext;
			Open->ReferenceObject = FileObject;
			Open->IncomingQueueDepth = 8;

			if (ExAcquireRundownProtection(this))
			{
				return IofCallDriver(_PhysicalDeviceObject, Irp);
			}

			FsContext->SetNotActive();

			status = STATUS_DELETE_PENDING;
		}
		else
		{
			status = STATUS_SHARING_VIOLATION;
		}
	}
	else
	{
		status = STATUS_PORT_ALREADY_SET;
	}

	return CompleteIrp(Irp, status);
}

NTSTATUS FDO::AclTransfer(ConnectionContext* FsContext, PIRP Irp, PIO_STACK_LOCATION IrpSp)
{
	NTSTATUS status = STATUS_PORT_DISCONNECTED;

	if (FsContext->AcquireConnection())
	{
		union {
			PVOID SystemBuffer;
			PBRB Brb;
			_BRB_L2CA_ACL_TRANSFER* Acl;
		};

		SystemBuffer = Irp->AssociatedIrp.SystemBuffer;

		BTH_ADDR BtAddress = Acl->BtAddress;
		ULONG TransferFlags = Acl->TransferFlags;
		ULONG BufferSize = Acl->BufferSize;
		PVOID Buf = Acl->Buffer;
		LONGLONG Timeout = Acl->Timeout;

		DbgPrint("AclTransfer(%I64X, %x, %x)\n", BtAddress, BufferSize, TransferFlags);

		BthInitializeBrb(Brb, BRB_L2CA_ACL_TRANSFER);

		InitRequest(IrpSp, Brb, true);

		if (PMDL Mdl = IoAllocateMdl(Buf, BufferSize, FALSE, FALSE, 0))
		{
			BOOL bPagesLocked = FALSE;

			__try
			{
				MmProbeAndLockPages(Mdl, KernelMode, 
					TransferFlags & ACL_TRANSFER_DIRECTION_IN ? IoWriteAccess : IoReadAccess);

				bPagesLocked = TRUE;
			}
			__except(EXCEPTION_EXECUTE_HANDLER)
			{
				if (!NT_ERROR(status = GetExceptionCode()))
				{
					status = STATUS_ACCESS_VIOLATION;
				}

				DbgPrint("MmProbeAndLockPages(%p)=%x\n", Buf, status);
			}

			if (bPagesLocked)
			{
				Acl->BtAddress = BtAddress;
				Acl->ChannelHandle = FsContext->_ConnectionHandle;
				Acl->TransferFlags = TransferFlags;
				Acl->BufferSize = BufferSize;
				Acl->Buffer = 0;
				Acl->BufferMDL = Mdl;
				Acl->Timeout = Timeout;

				if (ExAcquireRundownProtection(this))
				{
					DbgPrint("++Mdl=%p\n", Mdl);
					return IofCallDriver(_PhysicalDeviceObject, Irp);
				}

				MmUnlockPages(Mdl);
			}

			IoFreeMdl(Mdl);
		}
		else
		{
			status = STATUS_INSUFFICIENT_RESOURCES;
		}

		FsContext->ReleaseConnection();
	}

	return CompleteIrp(Irp, status);
}

NTSTATUS FDO::OnCompleteBrb([[maybe_unused]] PDEVICE_OBJECT DeviceObject, PIRP Irp, PVOID Brb)
{
	FDO* This = reinterpret_cast<FDO*>(reinterpret_cast<_BRB_HEADER*>(Brb)->ClientContext[0]);

	DbgPrint("OnCompleteBrb<%p>(status{%x, %p} brb<%p>[%x]{s=%x bt=%s})\n", This,
		Irp->IoStatus.Status, Irp->IoStatus.Information, 
		Brb, reinterpret_cast<_BRB_HEADER*>(Brb)->Type,
		reinterpret_cast<_BRB_HEADER*>(Brb)->Status, GetBthError(reinterpret_cast<_BRB_HEADER*>(Brb)->BtStatus));

	This->BthFreeBrb(reinterpret_cast<PBRB>(Brb));

	IoFreeIrp(Irp);

	ExReleaseRundownProtection(This);

	return StopCompletion;
}

void FDO::SendBrbAsync(PBRB Brb)
{
	if (ExAcquireRundownProtection(this))
	{
		PDEVICE_OBJECT DeviceObject = _PhysicalDeviceObject;

		if (PIRP Irp = IoAllocateIrp( DeviceObject->StackSize, FALSE )) 
		{
			PIO_STACK_LOCATION IrpSp = IoGetNextIrpStackLocation( Irp );

			InitRequest(IrpSp, Brb, false);

			DbgPrint("SendBrbAsync<%p>(%x)\n", Brb, Brb->BrbHeader.Type);

			IofCallDriver(DeviceObject, Irp);

			return ;
		}

		ExReleaseRundownProtection(this);
	}

	BthFreeBrb(Brb);
}

void FDO::CloseChannel(BTH_ADDR BtAddress, L2CAP_CHANNEL_HANDLE ChannelHandle)
{
	if (PBRB Brb = BthAllocateBrb(BRB_L2CA_CLOSE_CHANNEL, '*brB'))
	{
		Brb->BrbL2caCloseChannel.BtAddress = BtAddress;
		Brb->BrbL2caCloseChannel.ChannelHandle = ChannelHandle;

		DbgPrint("CloseChannel(%p [%I64x])\n", ChannelHandle, BtAddress);

		SendBrbAsync(Brb);
	}
}

_NT_END
