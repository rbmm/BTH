#pragma once

PSDP_NODE InitNodeUuid(PSDP_NODE pNode, LPCGUID guid);
PSDP_NODE InitNodeUuid(PSDP_NODE pNode, ULONG size, ULONG value);
PSDP_NODE InitNodeUint(PSDP_NODE pNode, ULONG size, ULONG64 value);
PSDP_NODE InitNodeBool(PSDP_NODE pNode, BOOLEAN b);
PSDP_NODE InitNodeString(PSDP_NODE pNode, PCSTR string);
PSDP_NODE InitNodeSequence(PSDP_NODE pNode);
PSDP_NODE AppendNode(PSDP_NODE pParentNode, PSDP_NODE pNode);
PBYTE NodeToStream(PSDP_NODE pNode, PBYTE pb, ULONG cb);
ULONG GetSize(PSDP_NODE pNode);
