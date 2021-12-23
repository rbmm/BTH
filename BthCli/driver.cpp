#include "stdafx.h"

_NT_BEGIN

#include "common.h"

NTSTATUS CALLBACK CommonDispatch(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	return reinterpret_cast<FDO*>(DeviceObject->DeviceExtension)->CommonDispatch(DeviceObject, Irp);
}

NTSTATUS CALLBACK AddDevice(PDRIVER_OBJECT DriverObject, PDEVICE_OBJECT PhysicalDeviceObject)
{
	DbgPrint("AddDevice(%p, %p)\n", DriverObject, PhysicalDeviceObject);

	PDEVICE_OBJECT DeviceObject;

	NTSTATUS status = IoCreateDevice(DriverObject, sizeof(FDO), 0, FILE_DEVICE_BLUETOOTH, 
		PhysicalDeviceObject->Characteristics & FILE_DEVICE_SECURE_OPEN, TRUE, &DeviceObject);

	if (0 <= status)
	{
		DeviceObject->Flags |= PhysicalDeviceObject->Flags & 
			(DO_BUFFERED_IO|DO_DIRECT_IO|DO_SUPPORTS_TRANSACTIONS|DO_POWER_PAGABLE|DO_POWER_INRUSH);

		FDO* fdo = new(DeviceObject) FDO(PhysicalDeviceObject);

		if (0 > (status = IoAttachDeviceToDeviceStackSafe(DeviceObject, PhysicalDeviceObject, &fdo->_AttachedToDeviceObject)))
		{
			delete fdo;
			IoDeleteDevice(DeviceObject);
		}
		else
		{
			DeviceObject->Flags &= ~DO_DEVICE_INITIALIZING;

			DbgPrint("AttachDevice(%p << %p)\n", fdo->_AttachedToDeviceObject, DeviceObject);
		}
	}

	return status;
}

VOID NTAPI DriverUnload(PDRIVER_OBJECT DriverObject)
{
	DbgPrint("DriverUnload(%p)", DriverObject);
}

NTSTATUS NTAPI DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath)
{
	DbgPrint(__TIMESTAMP__": %wZ %p %x\n", RegistryPath, DriverObject, DriverObject->Flags);

	__stosp(DriverObject->MajorFunction, CommonDispatch, _countof(DriverObject->MajorFunction));

	DriverObject->DriverExtension->AddDevice = AddDevice;
	DriverObject->DriverUnload = DriverUnload;

	return  TestIoMiniPacket();
}

_NT_END