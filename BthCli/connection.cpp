#include "stdafx.h"

_NT_BEGIN

#include "common.h"
#include "debug.h"
#include "../kpdb/module.h"

ConnectionContext::~ConnectionContext()
{
	if (PVOID Port = InterlockedExchangePointerNoFence(&_Port, 0))
	{
		DbgPrint("--Port(%p)\n", Port);
		ObfDereferenceObject(Port);
	}
	DbgPrint("%s<%p>\n", __FUNCTION__, this);
}

void ConnectionContext::CloseChannel()
{
	if (L2CAP_CHANNEL_HANDLE ChannelHandle = InterlockedExchangePointer(&_ConnectionHandle, 0))
	{
		if (BTH_ADDR BtAddress = InterlockedExchange64((LONG64*)&_BtAddress, 0))
		{
			DbgPrint("%s<%p>(%p [%I64X])\n", __FUNCTION__, this, ChannelHandle, BtAddress);
			_Fdo->CloseChannel(BtAddress, ChannelHandle);
		}
	}
	SetNotActive();
}

void ConnectionContext::ReleaseConnection()
{
	if (_ConnectionProtect.Release())
	{
		DbgPrint("%s<%p>\n", __FUNCTION__, this);
		CloseChannel();
		NotifyUserMode(_reason, IndicationRemoteDisconnect);
	}
}

void ConnectionContext::Rundown()
{
	DbgPrint("%s<%p>(%p)\n", __FUNCTION__, this, _ConnectionHandle);

	if (AcquireConnection())
	{
		_ConnectionProtect.Rundown_l();
		ReleaseConnection();
	}
}

_IRQL_requires_max_(DISPATCH_LEVEL)
void ConnectionContext::NotifyUserMode(_In_ NTSTATUS IoStatus, _In_ ULONG_PTR IoStatusInformation)
{
	if (PVOID Port = _Port)
	{
		if (IO_MINI_COMPLETION_PACKET_USER *packet = AllocateMiniCompletionPacket())
		{
			NTSTATUS status = IoSetIoCompletionEx (Port, _Key, _CallbackContext, IoStatus, IoStatusInformation, FALSE, packet);

			DbgPrint("SetIoCompletion(%p)=%X\n", packet, status);

			if (0 > status)
			{
				IoFreeMiniCompletionPacket(packet);
			}
		}
	}
}

_IRQL_requires_max_(DISPATCH_LEVEL)
void NTAPI ConnectionContext::BthIndicationCallback(
	_In_ PVOID Context,
	_In_ INDICATION_CODE Indication,
	_In_ PINDICATION_PARAMETERS Parameters
	)
{
	reinterpret_cast<ConnectionContext*>(Context)->IndicationCallback(Indication, Parameters);
}

void ConnectionContext::IndicationCallback(
	_In_ INDICATION_CODE Indication,
	_In_ PINDICATION_PARAMETERS Parameters
	)
{
#ifdef _KPDB_
	DumpStack(__FUNCTION__);
#endif

	DbgPrint("%s<%p>[%x](%s) [%I64X]\n", __FUNCTION__, this, _nRefCount, GetName(Indication), Parameters->BtAddress);

	switch (Indication)
	{
	case IndicationRemoteConnect:
	case IndicationRemoteConnectLE:
		DbgPrint("Connect: PSM = %X\n", Parameters->Parameters.Connect.Request.PSM);
		break;

	case IndicationAddReference:
		InterlockedIncrementNoFence(&_nRefCount);
		break;

	case IndicationReleaseReference:
		if (!InterlockedDecrement(&_nRefCount))
		{
			NotifyUserMode(0, IndicationReleaseReference);
		}
		break;

	case IndicationRemoteDisconnect:
		DbgPrint("Disconnect(%s, CloseNow=%X)\n", 
			GetName(Parameters->Parameters.Disconnect.Reason), Parameters->Parameters.Disconnect.CloseNow);

		_reason = Parameters->Parameters.Disconnect.Reason;
		Rundown();
		break;

	case IndicationRecvPacket:
		DbgPrint("RecvPacket: PacketLength=%X, TotalQueueLength=%X\n", 
			Parameters->Parameters.RecvPacket.PacketLength, 
			Parameters->Parameters.RecvPacket.TotalQueueLength);

		NotifyUserMode(Parameters->Parameters.RecvPacket.PacketLength, IndicationRecvPacket);
		break;
	}
}

bool ConnectionContext::SetPort(PVOID Port, PVOID Key, PVOID CallbackContext)
{
	if (PVOID CurrentPort = InterlockedCompareExchangePointerNoFence(&_Port, Port, 0))
	{
		if (CurrentPort != Port)
		{
			return false;
		}
	}
	else
	{
		ObfReferenceObject(Port);
		DbgPrint("++Port(%p)\n", Port);
	}

	_Key = Key, _CallbackContext = CallbackContext;

	return true;
}

void ConnectionContext::OnOpenChannelCompleted(_BRB_L2CA_OPEN_CHANNEL* Brb)
{
	L2CAP_CHANNEL_HANDLE ChannelHandle = Brb->ChannelHandle;

	DbgPrint("%s<%p>(Channel=%p %x %x %x)\n", __FUNCTION__, this, ChannelHandle, Brb->Response, Brb->Hdr.Status, GetBthError(Brb->Hdr.BtStatus));

	if (ChannelHandle)
	{
		_ConnectionHandle = ChannelHandle, _BtAddress = Brb->BtAddress;

		if (!_ConnectionProtect.Init())
		{
			__debugbreak();
		}
	}
}

_NT_END