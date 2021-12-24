#pragma once

#include "SCC.h"
#include "l2cap.h"
#include "msg.h"
#include "elog.h"

class ScSocket : public L2capSocket
{
	BCRYPT_KEY_HANDLE _hKey;
	SC_Cntr* _pkn;
	HWND _hwnd;
	ELog log;
	ULONG _id;

	virtual ~ScSocket();

	virtual BOOL OnConnect(NTSTATUS status, _BRB_L2CA_OPEN_CHANNEL* OpenChannel);
	virtual void OnDisconnect(NTSTATUS status);
	virtual void OnRecv(NTSTATUS status, _BRB_L2CA_ACL_TRANSFER* acl);

	typedef NTSTATUS (*BCryptXYZ)(
		_In_                                        BCRYPT_KEY_HANDLE hKey,
		_In_opt_                                    VOID    *pPaddingInfo,
		_In_reads_bytes_(cbInput)                   PUCHAR   pbInput,
		_In_                                        ULONG   cbInput,
		_Out_writes_bytes_to_opt_(cbOutput, *pcbResult) PUCHAR   pbOutput,
		_In_                                        ULONG   cbOutput,
		_Out_                                       ULONG   *pcbResult);

	struct REMOTE_APDU 
	{
		USHORT Length;
		enum OP_TYPE : UCHAR { opSign, opDecrypt, opEncrypt } opType;
		enum ALG_ID : UCHAR { a_md2, a_md4, a_md5, a_sha1, a_sha256, a_sha384, a_sha512 } algId; // in case opType == opSign
		ULONG Hint;
		UCHAR buf[/*Length*/];
	};

	BOOL PostResponce(BCryptXYZ fn, REMOTE_APDU* p, ULONG len, BCRYPT_PKCS1_PADDING_INFO *pPaddingInfo = 0);

public:

	enum { res_resp = '~~~~' };

	ScSocket(BCRYPT_KEY_HANDLE hKey, SC_Cntr* pkn, HWND hwnd, ULONG id, HWND hwndLog);

	ULONG get_id()
	{
		return _id;
	}
};