#pragma once

PCSTR PnPMinorFunctionString(IN UCHAR MinorFunction);
PCSTR MajorMinorFunctionString(UCHAR MajorFunction, UCHAR MinorFunction, PCSTR& mnstr);
PCSTR QueryDeviceRelationsString(DEVICE_RELATION_TYPE Type);
PCSTR QueryIdString(int IdType);
PCSTR GetName(INDICATION_CODE Indication);
PCSTR GetBthError(BTHSTATUS BtStatus);
PCSTR GetName(L2CAP_DISCONNECT_REASON Reason);
void Dump(PDEVICE_RELATIONS p);