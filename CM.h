#pragma once

CONFIGRET GetDeviceId(_Out_ DEVINSTID_W* ppDeviceID, _In_ PCWSTR pszDeviceInterface);

CONFIGRET DevNodeString(_Out_ PWSTR* ppszName, _In_ DEVNODE dnDevInst, _In_ CONST DEVPROPKEY *PropertyKey);

inline CONFIGRET Locate_DevNode(_Out_ PDEVNODE pdnDevInst,_In_ DEVINSTID_W pDeviceID)
{
	return CM_Locate_DevNodeW(pdnDevInst, pDeviceID, CM_LOCATE_DEVNODE_NORMAL);
}

CONFIGRET GetFriendlyName(_Out_ PWSTR* ppszName, _In_ PCWSTR pszDeviceInterface);