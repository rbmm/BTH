#include "stdafx.h"

_NT_BEGIN

#include "ScSocket.h"

ScSocket::~ScSocket()
{
	if ((ULONG)SendMessageW(_hwnd, WM_RES, (LPARAM)_hKey, (WPARAM)_pkn) != res_resp)
	{
		BCryptDestroyKey(_hKey);
		delete _pkn;
	}
	log(L"%s<%p>\r\n", __FUNCTIONW__, this);
}

void ScSocket::OnDisconnect(NTSTATUS status)
{
	PostMessageW(_hwnd, WM_DISCONNECT, 0, status);
	log(L"%s<%p>(%x)\r\n", __FUNCTIONW__, this, status);
}

BOOL ScSocket::OnConnect(NTSTATUS status, _BRB_L2CA_OPEN_CHANNEL* OpenChannel)
{
	log(L"%s<%p>%p(%x %x %x)=%x\r\n", __FUNCTIONW__, this, OpenChannel->ChannelHandle,
		OpenChannel->Hdr.Status, OpenChannel->Hdr.BtStatus, OpenChannel->Response, status);

	PostMessageW(_hwnd, WM_CONNECT, (WPARAM)OpenChannel->ChannelHandle, status);

	if (0 <= status && OpenChannel->ChannelHandle)
	{
		ULONG s = _pkn->GetSize();

		if (CDataPacket* packet = new(s) CDataPacket)
		{
			memcpy(packet->getData(), _pkn, s);
			packet->setDataSize(s);
			Send(packet);
		}
	}

	return TRUE;
}

void DumpBytes(const UCHAR* pb, ULONG cb);

NTSTATUS B_Sign(_In_                                        BCRYPT_KEY_HANDLE hKey,
				_In_opt_                                    VOID    *pPaddingInfo,
				_In_reads_bytes_(cbInput)                   PUCHAR   pbInput,
				_In_                                        ULONG   cbInput,
				_Out_writes_bytes_to_opt_(cbOutput, *pcbResult) PUCHAR   pbOutput,
				_In_                                        ULONG   cbOutput,
				_Out_                                       ULONG   *pcbResult)
{
	return BCryptSignHash(hKey, pPaddingInfo, pbInput, cbInput, pbOutput, cbOutput, pcbResult, BCRYPT_PAD_PKCS1);
}

NTSTATUS B_Decrypt(_In_                                        BCRYPT_KEY_HANDLE hKey,
				   _In_opt_                                    VOID    *pPaddingInfo,
				   _In_reads_bytes_(cbInput)                   PUCHAR   pbInput,
				   _In_                                        ULONG   cbInput,
				   _Out_writes_bytes_to_opt_(cbOutput, *pcbResult) PUCHAR   pbOutput,
				   _In_                                        ULONG   cbOutput,
				   _Out_                                       ULONG   *pcbResult)
{
	return BCryptDecrypt(hKey, pbInput, cbInput, pPaddingInfo, 0, 0, pbOutput, cbOutput, pcbResult, BCRYPT_PAD_PKCS1);
}

NTSTATUS B_Encrypt(_In_                                        BCRYPT_KEY_HANDLE hKey,
				   _In_opt_                                    VOID    *pPaddingInfo,
				   _In_reads_bytes_(cbInput)                   PUCHAR   pbInput,
				   _In_                                        ULONG   cbInput,
				   _Out_writes_bytes_to_opt_(cbOutput, *pcbResult) PUCHAR   pbOutput,
				   _In_                                        ULONG   cbOutput,
				   _Out_                                       ULONG   *pcbResult)
{
	return BCryptEncrypt(hKey, pbInput, cbInput, pPaddingInfo, 0, 0, pbOutput, cbOutput, pcbResult, BCRYPT_PAD_PKCS1);
}

BOOL ScSocket::PostResponce(BCryptXYZ fn, REMOTE_APDU* p, ULONG len, BCRYPT_PKCS1_PADDING_INFO *pPaddingInfo/* = 0*/)
{
	NTSTATUS status;
	PUCHAR pb = 0;
	ULONG cb = 0;
	REMOTE_APDU* q = 0;
	CDataPacket* packet = 0;
	BCRYPT_KEY_HANDLE hKey = _hKey;

	while (0 <= (status = fn(hKey, pPaddingInfo, p->buf, len, pb, cb, &cb)))
	{
		if (pb)
		{
			q->Hint = p->Hint;
			q->Length = (USHORT)cb;
			packet->setDataSize(sizeof(REMOTE_APDU) + cb);
			break;
		}

		if (packet = new(sizeof(REMOTE_APDU) + cb) CDataPacket)
		{
			q = (REMOTE_APDU*)packet->getData();
			pb = q->buf;
		}
		else
		{
			return FALSE;
		}
	}

	log(L"PostResponce(%p, %x, %x)\r\n", fn, status, cb);
	PostMessageW(_hwnd, WM_RECV, (WPARAM)(pPaddingInfo ? pPaddingInfo->pszAlgId : L""), status);

	if (0 > status)
	{
		if (!packet)
		{
			if (packet = new(sizeof(REMOTE_APDU)) CDataPacket)
			{
				q = (REMOTE_APDU*)packet->getData();
				q->Hint = p->Hint;
				q->Length = 0;
			}
			else
			{
				return FALSE;
			}
		}
	}

	Send(packet);
	packet->Release();

	return TRUE;
}

void ScSocket::OnRecv(NTSTATUS status, _BRB_L2CA_ACL_TRANSFER* acl)
{
	if (0 > status)
	{
__err:
		Disconnect();
		return ;
	}

	ULONG BufferSize = acl->BufferSize;
	REMOTE_APDU* p = (REMOTE_APDU*)acl->Buffer;

	log(L"%s<%p>(s=%x %x/%x)\r\n", __FUNCTIONW__, this, status, acl->RemainingBufferSize, BufferSize);
	DumpBytes((PUCHAR)p, acl->BufferSize);

	if (BufferSize <= sizeof(REMOTE_APDU))
	{
		goto __err;
	}

	ULONG len = p->Length;

	if (len + sizeof(REMOTE_APDU) != BufferSize)
	{
		goto __err;
	}

	switch (p->opType)
	{
	default: goto __err;

	case REMOTE_APDU::opEncrypt:
		if (PostResponce(B_Encrypt, p, len)) return;
		break;

	case REMOTE_APDU::opDecrypt:
		if (PostResponce(B_Decrypt, p, len)) return;
		break;

	case REMOTE_APDU::opSign:

		BCRYPT_PKCS1_PADDING_INFO pi = { };

		switch (p->algId)
		{
		case REMOTE_APDU::a_md2:
			if (len == 16) pi.pszAlgId = BCRYPT_MD2_ALGORITHM;
			break;
		case REMOTE_APDU::a_md4:
			if (len == 16) pi.pszAlgId = BCRYPT_MD4_ALGORITHM;
			break;
		case REMOTE_APDU::a_md5:
			if (len == 16) pi.pszAlgId = BCRYPT_MD5_ALGORITHM;
			break;
		case REMOTE_APDU::a_sha1:
			if (len == 20) pi.pszAlgId = BCRYPT_SHA1_ALGORITHM;
			break;
		case REMOTE_APDU::a_sha256:
			if (len == 32) pi.pszAlgId = BCRYPT_SHA256_ALGORITHM;
			break;
		case REMOTE_APDU::a_sha384:
			if (len == 48) pi.pszAlgId = BCRYPT_SHA384_ALGORITHM;
			break;
		case REMOTE_APDU::a_sha512:
			if (len == 64) pi.pszAlgId = BCRYPT_SHA512_ALGORITHM;
			break;
		}

		if (pi.pszAlgId)
		{
			log(L"sign(%s)\r\n", pi.pszAlgId);
			if (PostResponce(B_Sign, p, len, &pi)) return;
		}
		break;
	}

	goto __err;
}

_NT_END