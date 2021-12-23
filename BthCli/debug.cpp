#include "stdafx.h"

_NT_BEGIN

#include "debug.h"

void Dump(PDEVICE_RELATIONS p)
{
	if (p)
	{
		if (ULONG n = p->Count)
		{
			DbgPrint("!! PDEVICE_RELATIONS(%p, %u) !!\n", p, n);

			PDEVICE_OBJECT* Objects = p->Objects, DeviceObject;
			do 
			{
				DeviceObject = *Objects++;
				DbgPrint("  %p %wZ\n", DeviceObject, &DeviceObject->DriverObject->DriverName);
			} while (--n);
		}
	}
}

PCSTR MajorMinorFunctionString(UCHAR MajorFunction, UCHAR MinorFunction, PCSTR& mnstr);

PCSTR PnPMinorFunctionString(IN UCHAR MinorFunction) { 
	switch (MinorFunction) { 
	case IRP_MN_START_DEVICE:                 return "START_DEVICE"; 
	case IRP_MN_QUERY_REMOVE_DEVICE:          return "QUERY_REMOVE_DEVICE"; 
	case IRP_MN_REMOVE_DEVICE:                return "REMOVE_DEVICE"; 
	case IRP_MN_CANCEL_REMOVE_DEVICE:         return "CANCEL_REMOVE_DEVICE"; 
	case IRP_MN_STOP_DEVICE:                  return "STOP_DEVICE"; 
	case IRP_MN_QUERY_STOP_DEVICE:            return "QUERY_STOP_DEVICE"; 
	case IRP_MN_CANCEL_STOP_DEVICE:           return "CANCEL_STOP_DEVICE"; 
	case IRP_MN_QUERY_DEVICE_RELATIONS:       return "QUERY_DEVICE_RELATIONS"; 
	case IRP_MN_QUERY_INTERFACE:              return "QUERY_INTERFACE"; 
	case IRP_MN_QUERY_CAPABILITIES:           return "QUERY_CAPABILITIES"; 
	case IRP_MN_QUERY_RESOURCES:              return "QUERY_RESOURCES"; 
	case IRP_MN_QUERY_RESOURCE_REQUIREMENTS:  return "QUERY_RESOURCE_REQUIREMENTS"; 
	case IRP_MN_QUERY_DEVICE_TEXT:            return "QUERY_DEVICE_TEXT"; 
	case IRP_MN_FILTER_RESOURCE_REQUIREMENTS: return "FILTER_RESOURCE_REQUIREMENTS"; 
	case IRP_MN_READ_CONFIG:                  return "READ_CONFIG"; 
	case IRP_MN_WRITE_CONFIG:                 return "WRITE_CONFIG"; 
	case IRP_MN_EJECT:                        return "EJECT"; 
	case IRP_MN_SET_LOCK:                     return "SET_LOCK"; 
	case IRP_MN_QUERY_ID:                     return "QUERY_ID"; 
	case IRP_MN_QUERY_PNP_DEVICE_STATE:       return "QUERY_PNP_DEVICE_STATE"; 
	case IRP_MN_QUERY_BUS_INFORMATION:        return "QUERY_BUS_INFORMATION"; 
	case IRP_MN_DEVICE_USAGE_NOTIFICATION:    return "DEVICE_USAGE_NOTIFICATION"; 
	case IRP_MN_SURPRISE_REMOVAL:             return "SURPRISE_REMOVAL"; 
	case IRP_MN_QUERY_LEGACY_BUS_INFORMATION: return "QUERY_LEGACY_BUS_INFORMATION"; 
	default:                                  return "UNKNOWN"; 
	} 
} 


PCSTR QueryDeviceRelationsString(DEVICE_RELATION_TYPE Type) { 
	switch (Type) { 
	case BusRelations:         return "BusRelations"; 
	case EjectionRelations:    return "EjectionRelations"; 
	case PowerRelations:       return "PowerRelations"; 
	case RemovalRelations:     return "RemovalRelations"; 
	case TargetDeviceRelation: return "TargetDeviceRelation"; 
	default:                   return "UnknownRelation"; 
	} 
} 

PCSTR QueryIdString(int IdType) { 
	switch (IdType) { 
	case BusQueryDeviceID:           return "BusQueryDeviceID"; 
	case BusQueryHardwareIDs:        return "BusQueryHardwareIDs"; 
	case BusQueryCompatibleIDs:      return "BusQueryCompatibleIDs"; 
	case BusQueryInstanceID:         return "BusQueryInstanceID"; 
	case BusQueryDeviceSerialNumber: return "BusQueryDeviceSerialNumber"; 
	case ~DeviceTextDescription:         return "DeviceTextDescription"; 
	case ~DeviceTextLocationInformation: return "DeviceTextLocationInformation"; 
	default:                         return "BusQueryUnknown"; 
	} 
}

PCSTR GetName(INDICATION_CODE Indication)
{
	switch (Indication)
	{
	case IndicationAddReference: return "AddReference";
	case IndicationReleaseReference: return "ReleaseReference";
	case IndicationRemoteConnect: return "RemoteConnect";
	case IndicationRemoteDisconnect: return "RemoteDisconnect";
	case IndicationRemoteConfigRequest: return "RemoteConfigRequest";
	case IndicationRemoteConfigResponse: return "RemoteConfigResponse";
	case IndicationFreeExtraOptions: return "FreeExtraOptions";
	case IndicationRecvPacket: return "RecvPacket";
	case IndicationPairDevice: return "PairDevice";
	case IndicationUnpairDevice: return "UnpairDevice";
	case IndicationUnpersonalizeDevice: return "UnpersonalizeDevice";
	case IndicationRemoteConnectLE: return "RemoteConnectLE";
	}
	return "?";
}

PCSTR GetName(L2CAP_DISCONNECT_REASON Reason)
{
	switch (Reason)
	{
	case HciDisconnect: return "HciDisconnect";
	case L2capDisconnectRequest: return "L2capDisconnectRequest";
	case RadioPoweredDown: return "RadioPoweredDown";
	case HardwareRemoval: return "HardwareRemoval";
	}
	return "?";
}

PCSTR GetBthError(BTHSTATUS BtStatus)
{
	switch (BtStatus)
	{
	case BTH_ERROR_SUCCESS                           : return "SUCCESS"; // (0x00)
	case BTH_ERROR_UNKNOWN_HCI_COMMAND               : return "UNKNOWN_HCI_COMMAND"; // (0x01)
	case BTH_ERROR_NO_CONNECTION                     : return "NO_CONNECTION"; // (0x02)
	case BTH_ERROR_HARDWARE_FAILURE                  : return "HARDWARE_FAILURE"; // (0x03)
	case BTH_ERROR_PAGE_TIMEOUT                      : return "PAGE_TIMEOUT"; // (0x04)
	case BTH_ERROR_AUTHENTICATION_FAILURE            : return "AUTHENTICATION_FAILURE"; // (0x05)
	case BTH_ERROR_KEY_MISSING                       : return "KEY_MISSING"; // (0x06)
	case BTH_ERROR_MEMORY_FULL                       : return "MEMORY_FULL"; // (0x07)
	case BTH_ERROR_CONNECTION_TIMEOUT                : return "CONNECTION_TIMEOUT"; // (0x08)
	case BTH_ERROR_MAX_NUMBER_OF_CONNECTIONS         : return "MAX_NUMBER_OF_CONNECTIONS"; // (0x09)
	case BTH_ERROR_MAX_NUMBER_OF_SCO_CONNECTIONS     : return "MAX_NUMBER_OF_SCO_CONNECTIONS"; // (0x0a)
	case BTH_ERROR_ACL_CONNECTION_ALREADY_EXISTS     : return "ACL_CONNECTION_ALREADY_EXISTS"; // (0x0b)
	case BTH_ERROR_COMMAND_DISALLOWED                : return "COMMAND_DISALLOWED"; // (0x0c)
	case BTH_ERROR_HOST_REJECTED_LIMITED_RESOURCES   : return "HOST_REJECTED_LIMITED_RESOURCES"; // (0x0d)
	case BTH_ERROR_HOST_REJECTED_SECURITY_REASONS    : return "HOST_REJECTED_SECURITY_REASONS"; // (0x0e)
	case BTH_ERROR_HOST_REJECTED_PERSONAL_DEVICE     : return "HOST_REJECTED_PERSONAL_DEVICE"; // (0x0f)
	case BTH_ERROR_HOST_TIMEOUT                      : return "HOST_TIMEOUT"; // (0x10)
	case BTH_ERROR_UNSUPPORTED_FEATURE_OR_PARAMETER  : return "UNSUPPORTED_FEATURE_OR_PARAMETER"; // (0x11)
	case BTH_ERROR_INVALID_HCI_PARAMETER             : return "INVALID_HCI_PARAMETER"; // (0x12)
	case BTH_ERROR_REMOTE_USER_ENDED_CONNECTION      : return "REMOTE_USER_ENDED_CONNECTION"; // (0x13)
	case BTH_ERROR_REMOTE_LOW_RESOURCES              : return "REMOTE_LOW_RESOURCES"; // (0x14)
	case BTH_ERROR_REMOTE_POWERING_OFF               : return "REMOTE_POWERING_OFF"; // (0x15)
	case BTH_ERROR_LOCAL_HOST_TERMINATED_CONNECTION  : return "LOCAL_HOST_TERMINATED_CONNECTION"; // (0x16)
	case BTH_ERROR_REPEATED_ATTEMPTS                 : return "REPEATED_ATTEMPTS"; // (0x17)
	case BTH_ERROR_PAIRING_NOT_ALLOWED               : return "PAIRING_NOT_ALLOWED"; // (0x18)
	case BTH_ERROR_UKNOWN_LMP_PDU                    : return "UKNOWN_LMP_PDU"; // (0x19)
	case BTH_ERROR_UNSUPPORTED_REMOTE_FEATURE        : return "UNSUPPORTED_REMOTE_FEATURE"; // (0x1a)
	case BTH_ERROR_SCO_OFFSET_REJECTED               : return "SCO_OFFSET_REJECTED"; // (0x1b)
	case BTH_ERROR_SCO_INTERVAL_REJECTED             : return "SCO_INTERVAL_REJECTED"; // (0x1c)
	case BTH_ERROR_SCO_AIRMODE_REJECTED              : return "SCO_AIRMODE_REJECTED"; // (0x1d)
	case BTH_ERROR_INVALID_LMP_PARAMETERS            : return "INVALID_LMP_PARAMETERS"; // (0x1e)
	case BTH_ERROR_UNSPECIFIED_ERROR                 : return "UNSPECIFIED_ERROR"; // (0x1f)
	case BTH_ERROR_UNSUPPORTED_LMP_PARM_VALUE        : return "UNSUPPORTED_LMP_PARM_VALUE"; // (0x20)
	case BTH_ERROR_ROLE_CHANGE_NOT_ALLOWED           : return "ROLE_CHANGE_NOT_ALLOWED"; // (0x21)
	case BTH_ERROR_LMP_RESPONSE_TIMEOUT              : return "LMP_RESPONSE_TIMEOUT"; // (0x22)
	case BTH_ERROR_LMP_TRANSACTION_COLLISION         : return "LMP_TRANSACTION_COLLISION"; // (0x23)
	case BTH_ERROR_LMP_PDU_NOT_ALLOWED               : return "LMP_PDU_NOT_ALLOWED"; // (0x24)
	case BTH_ERROR_ENCRYPTION_MODE_NOT_ACCEPTABLE    : return "ENCRYPTION_MODE_NOT_ACCEPTABLE"; // (0x25)
	case BTH_ERROR_UNIT_KEY_NOT_USED                 : return "UNIT_KEY_NOT_USED"; // (0x26)
	case BTH_ERROR_QOS_IS_NOT_SUPPORTED              : return "QOS_IS_NOT_SUPPORTED"; // (0x27)
	case BTH_ERROR_INSTANT_PASSED                    : return "INSTANT_PASSED"; // (0x28)
	case BTH_ERROR_PAIRING_WITH_UNIT_KEY_NOT_SUPPORTED : return "PAIRING_WITH_UNIT_KEY_NOT_SUPPORTED"; // (0x29)
	case BTH_ERROR_DIFFERENT_TRANSACTION_COLLISION   : return "DIFFERENT_TRANSACTION_COLLISION"; // (0x2a)
	case BTH_ERROR_QOS_UNACCEPTABLE_PARAMETER        : return "QOS_UNACCEPTABLE_PARAMETER"; // (0x2c)
	case BTH_ERROR_QOS_REJECTED                      : return "QOS_REJECTED"; // (0x2d)
	case BTH_ERROR_CHANNEL_CLASSIFICATION_NOT_SUPPORTED : return "CHANNEL_CLASSIFICATION_NOT_SUPPORTED"; // (0x2e)
	case BTH_ERROR_INSUFFICIENT_SECURITY             : return "INSUFFICIENT_SECURITY"; // (0x2f)
	case BTH_ERROR_PARAMETER_OUT_OF_MANDATORY_RANGE  : return "PARAMETER_OUT_OF_MANDATORY_RANGE"; // (0x30)
	case BTH_ERROR_ROLE_SWITCH_PENDING               : return "ROLE_SWITCH_PENDING"; // (0x32)
	case BTH_ERROR_RESERVED_SLOT_VIOLATION           : return "RESERVED_SLOT_VIOLATION"; // (0x34)
	case BTH_ERROR_ROLE_SWITCH_FAILED                : return "ROLE_SWITCH_FAILED"; // (0x35)
	case BTH_ERROR_EXTENDED_INQUIRY_RESPONSE_TOO_LARGE : return "EXTENDED_INQUIRY_RESPONSE_TOO_LARGE"; // (0x36)
	case BTH_ERROR_SECURE_SIMPLE_PAIRING_NOT_SUPPORTED_BY_HOST : return "SECURE_SIMPLE_PAIRING_NOT_SUPPORTED_BY_HOST"; // (0x37)
	case BTH_ERROR_HOST_BUSY_PAIRING                 : return "HOST_BUSY_PAIRING"; // (0x38)
	case BTH_ERROR_CONNECTION_REJECTED_DUE_TO_NO_SUITABLE_CHANNEL_FOUND : return "CONNECTION_REJECTED_DUE_TO_NO_SUITABLE_CHANNEL_FOUND"; // (0x39)
	case BTH_ERROR_CONTROLLER_BUSY                   : return "CONTROLLER_BUSY"; // (0x3a)
	case BTH_ERROR_UNACCEPTABLE_CONNECTION_INTERVAL  : return "UNACCEPTABLE_CONNECTION_INTERVAL"; // (0x3b)
	case BTH_ERROR_DIRECTED_ADVERTISING_TIMEOUT      : return "DIRECTED_ADVERTISING_TIMEOUT"; // (0x3c)
	case BTH_ERROR_CONNECTION_TERMINATED_DUE_TO_MIC_FAILURE : return "CONNECTION_TERMINATED_DUE_TO_MIC_FAILURE"; // (0x3d)
	case BTH_ERROR_CONNECTION_FAILED_TO_BE_ESTABLISHED : return "CONNECTION_FAILED_TO_BE_ESTABLISHED"; // (0x3e)
	case BTH_ERROR_MAC_CONNECTION_FAILED             : return "MAC_CONNECTION_FAILED"; // (0x3f)
	case BTH_ERROR_COARSE_CLOCK_ADJUSTMENT_REJECTED  : return "COARSE_CLOCK_ADJUSTMENT_REJECTED"; // (0x40)
	case BTH_ERROR_TYPE_0_SUBMAP_NOT_DEFINED         : return "TYPE_0_SUBMAP_NOT_DEFINED"; // (0x41)
	case BTH_ERROR_UNKNOWN_ADVERTISING_IDENTIFIER    : return "UNKNOWN_ADVERTISING_IDENTIFIER"; // (0x42)
	case BTH_ERROR_LIMIT_REACHED                     : return "LIMIT_REACHED"; // (0x43)
	case BTH_ERROR_OPERATION_CANCELLED_BY_HOST       : return "OPERATION_CANCELLED_BY_HOST"; // (0X44)
	case BTH_ERROR_PACKET_TOO_LONG                   : return "PACKET_TOO_LONG"; // (0x45)
	case BTH_ERROR_UNSPECIFIED                       : return "UNSPECIFIED"; // (0xFF)
	}

	return "?";
}

void DumpBytes(const UCHAR* pb, ULONG cb)
{
	if (cb)
	{
		do 
		{
			ULONG m = min(16, cb);
			cb -= m;
			char buf[128], *sz = buf;
			do 
			{
				sz += sprintf(sz, "%02x ", *pb++);
			} while (--m);
			*sz++ = '\n', *sz++ = 0;
			DbgPrint(buf);
		} while (cb);
	}
}

_NT_END
