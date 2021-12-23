#define _NTDRIVER_
#define NOWINBASEINTERLOCK
#define _NTOS_
#define _KERNEL_MODE
#include "../inc/stdafx.h"

#include <intrin.h>
_NT_BEGIN
#include <ws2bth.h >
#include <bthddi.h>
#include <bthioctl.h>
#include <sdpnode.h>
#include <bthsdpddi.h>
_NT_END