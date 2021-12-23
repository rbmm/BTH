#include "stdafx.h"

_NT_BEGIN

#include "MiniPacket.h"

static ULONG PacketType;

_IRQL_requires_max_(DISPATCH_LEVEL)
IO_MINI_COMPLETION_PACKET_USER* AllocateMiniCompletionPacket()
{
	if (IO_MINI_COMPLETION_PACKET_USER* packet = (IO_MINI_COMPLETION_PACKET_USER*)
		ExAllocatePoolWithTag(NonPagedPoolNx, sizeof(IO_MINI_COMPLETION_PACKET_USER), ' pcI'))
	{
		packet->PacketType = PacketType;
		packet->MiniPacketCallback = 0;
		packet->Context = 0;
		packet->Allocated = TRUE;

		return packet;
	}

	return 0;
}

static void NTAPI MiniPacketCallback( IO_MINI_COMPLETION_PACKET_USER * , PVOID  )
{
}

NTSTATUS TestIoMiniPacket()
{
	NTSTATUS status = STATUS_NOT_SUPPORTED;
	if (IO_MINI_COMPLETION_PACKET_USER* packet = IoAllocateMiniCompletionPacket(MiniPacketCallback, TestIoMiniPacket))
	{
		if (packet->MiniPacketCallback == MiniPacketCallback &&
			packet->Context == TestIoMiniPacket &&
			packet->Allocated)
		{
			IoInitializeMiniCompletionPacket(packet, 0, 0);
			if (!packet->Allocated)
			{
				PacketType = packet->PacketType;
				status = STATUS_SUCCESS;
				DbgPrint("PacketType = %x\n", PacketType);
			}
		}
		IoFreeMiniCompletionPacket(packet);
	}
	return status;
}

_NT_END