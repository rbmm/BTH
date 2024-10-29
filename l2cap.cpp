#include "stdafx.h"

_NT_BEGIN
#include "l2cap.h"
#include "msg.h"
void DumpBytes(const UCHAR* pb, ULONG cb);

#define IOCTL_L2CA_OPEN_CHANNEL CTL_CODE(FILE_DEVICE_BLUETOOTH, BRB_L2CA_OPEN_CHANNEL, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_L2CA_CLOSE_CHANNEL CTL_CODE(FILE_DEVICE_BLUETOOTH, BRB_L2CA_CLOSE_CHANNEL, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_L2CA_ACL_TRANSFER CTL_CODE(FILE_DEVICE_BLUETOOTH, BRB_L2CA_ACL_TRANSFER, METHOD_BUFFERED, FILE_ANY_ACCESS)

ULONG OpenDevice(_Out_ PHANDLE phFile, _In_ const GUID* InterfaceClassGuid);

struct __declspec(uuid("ADF8EB1B-0718-4366-A418-BB88F175D361")) BTH_CLI_GUID;

struct _BRB_L2CA_OPEN_CHANNEL_CONTEXT : _BRB_L2CA_OPEN_CHANNEL 
{
	NT_IRP* CbIrp;
};

void ReleaseCbIrp(NT_IRP* CbIrp, PTP_IO io, IO_OBJECT* pObj)
{
	LONG ref_count = InterlockedDecrement((PLONG)CbIrp->GetBuf());

	DbgPrint("ReleaseReference(%p)[%x]\n", CbIrp, ref_count);

	if (!ref_count)
	{
		DbgPrint("CbIrp: --%p\n", CbIrp);

		int n = 9;
		do 
		{
			TpCancelAsyncIoOperation(io);
			pObj->Release();

		} while (--n);

		CbIrp->Delete();
	}
}

void L2capSocket::IOCompletionRoutine(CDataPacket* packet, DWORD Code, NTSTATUS status, ULONG_PTR Information, PVOID Pointer)
{
	DbgPrint("%x: %s<%p>[%.4s](%x %x, %p %p)\n", GetCurrentThreadId(), __FUNCTION__, this, &Code, status, Information, Pointer, packet);
	
	switch (Code)
	{
	case c_connect:
		OnConnect(status, (_BRB_L2CA_OPEN_CHANNEL*)Pointer);
		if (0 > status)
		{
			ReleaseCbIrp(reinterpret_cast<_BRB_L2CA_OPEN_CHANNEL_CONTEXT*>(Pointer)->CbIrp, GetAsyncIo(), this);
		}
		break;
	case c_recv:
		reinterpret_cast<_BRB_L2CA_ACL_TRANSFER*>(Pointer)->Buffer = packet->getData();
		OnRecv(status, (_BRB_L2CA_ACL_TRANSFER*)Pointer);
		break;
	case c_send:
		reinterpret_cast<_BRB_L2CA_ACL_TRANSFER*>(Pointer)->Buffer = packet->getData();
		OnSend(status, (_BRB_L2CA_ACL_TRANSFER*)Pointer);
		break;
	case c_disc:
		DbgPrint("Disconnect=%x\n", status);
		break;
	case c_callback:
		AddRef();
		TpStartAsyncIoOperation(GetAsyncIo()); 
		switch (Information)
		{
		case IndicationRemoteDisconnect:
			OnDisconnect(status);
			ReleaseCbIrp(reinterpret_cast<NT_IRP*>(Pointer), GetAsyncIo(), this);
			break;
		case IndicationRecvPacket:
			NeedRecv(status);
			break;
		case IndicationReleaseReference:
			ReleaseCbIrp(reinterpret_cast<NT_IRP*>(Pointer), GetAsyncIo(), this);
			break;
		case IndicationAddReference:
			status = InterlockedIncrement((PLONG)reinterpret_cast<NT_IRP*>(Pointer)->GetBuf());
			DbgPrint("AddReference(%p)[%x]\n", Pointer, status);
			break;
		}
		break;
	default: __debugbreak();
	}
}

void L2capSocket::NeedRecv(ULONG PacketLength)
{
	DbgPrint("%s<%p>(%x)\n", __FUNCTION__, this, PacketLength);
	if (PacketLength <= MAXUSHORT)
	{
		if (CDataPacket* packet = new(PacketLength) CDataPacket)
		{
			Recv(packet);
			packet->Release();
		}
	}
	else
	{
		Disconnect();
	}
}

NTSTATUS L2capSocket::Disconnect()
{
	DbgPrint("%s<%p>(%x)\n", __FUNCTION__, this);
	NTSTATUS status = STATUS_INVALID_HANDLE;
	HANDLE hFile;
	if (LockHandle(hFile))
	{
		status = STATUS_INSUFFICIENT_RESOURCES;

		if (NT_IRP* Irp = new(0) NT_IRP(this, c_disc, 0))
		{
			Irp->CheckNtStatus(this, NtDeviceIoControlFile(hFile, 0, 0, Irp, Irp, IOCTL_L2CA_CLOSE_CHANNEL, 0, 0, 0, 0));
			status = 0;
		}
		UnlockHandle();
	}

	return status;
}

HRESULT L2capSocket::Create()
{
	HANDLE hFile;
	if (ULONG err = OpenDevice(&hFile, &__uuidof(BTH_CLI_GUID)))
	{
		return HRESULT_FROM_WIN32(err);
	}

	NTSTATUS status = NT_IRP::BindIoCompletion(this, hFile);

	if (0 > status)
	{
		NtClose(hFile);
		return HRESULT_FROM_NT(status);
	}

	Assign(hFile);

	return STATUS_SUCCESS;
}

NTSTATUS L2capSocket::Connect(BTH_ADDR BtAddress, USHORT Psm)
{
	NTSTATUS status = STATUS_INVALID_HANDLE;
	HANDLE hFile;
	if (LockHandle(hFile))
	{
		status = STATUS_INSUFFICIENT_RESOURCES;

		if (NT_IRP* CbIrp = new(sizeof(LONG)) NT_IRP(this, c_callback, 0))
		{
			DbgPrint("CbIrp: ++%p\n", CbIrp);

			if (NT_IRP* Irp = new(sizeof(_BRB_L2CA_OPEN_CHANNEL_CONTEXT)) NT_IRP(this, c_connect, 0))
			{
				PTP_IO io = GetAsyncIo();
				int n = 8;
				do 
				{
					AddRef();
					TpStartAsyncIoOperation(io);
				} while (--n);

				PLONG pRef = (PLONG)CbIrp->NotDelete();
				*pRef = 1;

				DbgPrint("Irp=%p\n", Irp);
				_btAddr = BtAddress;

				_BRB_L2CA_OPEN_CHANNEL_CONTEXT* OpenChannel = (_BRB_L2CA_OPEN_CHANNEL_CONTEXT*)Irp->SetPointer();
				RtlZeroMemory(OpenChannel, sizeof(_BRB_L2CA_OPEN_CHANNEL));

				OpenChannel->BtAddress = BtAddress;
				OpenChannel->Psm = Psm;
				OpenChannel->CallbackContext = CbIrp;
				OpenChannel->CbIrp = CbIrp;

				Irp->CheckNtStatus(this, NtDeviceIoControlFile(hFile, 0, 0, Irp, Irp, IOCTL_L2CA_OPEN_CHANNEL, 
					OpenChannel, sizeof(_BRB_L2CA_OPEN_CHANNEL), OpenChannel, sizeof(_BRB_L2CA_OPEN_CHANNEL)));

				status = 0;
			}
			else
			{
				CbIrp->Delete();
			}
		}
		UnlockHandle();
	}

	return status;
}

NTSTATUS L2capSocket::Send(CDataPacket* packet)
{
	NTSTATUS status = STATUS_INVALID_HANDLE;
	HANDLE hFile;
	if (LockHandle(hFile))
	{
		status = STATUS_INSUFFICIENT_RESOURCES;

		if (NT_IRP* Irp = new(sizeof(_BRB_L2CA_ACL_TRANSFER)) NT_IRP(this, c_send, packet))
		{
			_BRB_L2CA_ACL_TRANSFER* acl = (_BRB_L2CA_ACL_TRANSFER*)Irp->SetPointer();
			RtlZeroMemory(acl, sizeof(_BRB_L2CA_ACL_TRANSFER));

			acl->BtAddress = _btAddr;
			acl->TransferFlags = ACL_TRANSFER_DIRECTION_OUT;
			acl->BufferSize = packet->getDataSize();
			acl->Buffer = packet->getData();

			Irp->CheckNtStatus(this, NtDeviceIoControlFile(hFile, 0, 0, Irp, Irp, IOCTL_L2CA_ACL_TRANSFER, 
				acl, sizeof(_BRB_L2CA_ACL_TRANSFER), 
				acl, sizeof(_BRB_L2CA_ACL_TRANSFER)));

			status = 0;
		}
		UnlockHandle();
	}

	return status;
}

NTSTATUS L2capSocket::Send(const void* pv, ULONG cb)
{
	if (CDataPacket* packet = new(cb) CDataPacket)
	{
		memcpy(packet->getData(), pv, cb);
		packet->setDataSize(cb);
		NTSTATUS status = Send(packet);
		packet->Release();
		return status;
	}

	return STATUS_INSUFFICIENT_RESOURCES;
}

NTSTATUS L2capSocket::Recv(CDataPacket* packet)
{
	NTSTATUS status = STATUS_INVALID_HANDLE;
	HANDLE hFile;
	if (LockHandle(hFile))
	{
		status = STATUS_INSUFFICIENT_RESOURCES;

		if (NT_IRP* Irp = new(sizeof(_BRB_L2CA_ACL_TRANSFER)) NT_IRP(this, c_recv, packet))
		{
			_BRB_L2CA_ACL_TRANSFER* acl = (_BRB_L2CA_ACL_TRANSFER*)Irp->SetPointer();
			RtlZeroMemory(acl, sizeof(_BRB_L2CA_ACL_TRANSFER));

			acl->BtAddress = _btAddr;
			acl->TransferFlags = ACL_TRANSFER_DIRECTION_IN|ACL_SHORT_TRANSFER_OK;
			acl->BufferSize = packet->getFreeSize();
			acl->Buffer = packet->getFreeBuffer();

			Irp->CheckNtStatus(this, NtDeviceIoControlFile(hFile, 0, 0, Irp, Irp, IOCTL_L2CA_ACL_TRANSFER, 
				acl, sizeof(_BRB_L2CA_ACL_TRANSFER), acl, sizeof(_BRB_L2CA_ACL_TRANSFER)));

			status = 0;
		}

		UnlockHandle();
	}

	return status;
}

_NT_END