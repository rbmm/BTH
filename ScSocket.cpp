#include "stdafx.h"

_NT_BEGIN

#include "ScSocket.h"

//C_ASSERT(FIELD_OFFSET(ScSocket, m_HandleLock)==0x10);

ScSocket::~ScSocket()
{
	if (_hKey)
	{
		BCryptDestroyKey(_hKey);
	}
	if (_hExchKey)
	{
		BCryptDestroyKey(_hExchKey);
	}
	if ((ULONG)SendMessageW(_hwnd, WM_RES, 0, (WPARAM)_pkn) != res_resp)
	{
		delete _pkn;
	}
	log(L"%s<%p>\r\n", __FUNCTIONW__, this);
	DbgPrint("%s<%p>\r\n", __FUNCTION__, this);
}

ScSocket::ScSocket(SC_Cntr* pkn, HWND hwnd, ULONG id, HWND hwndLog) : _pkn(pkn), _hwnd(hwnd), _id(id)
{
	DbgPrint("%s<%p>\r\n", __FUNCTION__, this);
	log.Set(hwndLog);
	log(L"%s<%p>\r\n", __FUNCTIONW__, this);
}

HRESULT ScSocket::Create()
{
	NTSTATUS hr;

	BCRYPT_ALG_HANDLE hAlgorithm;
	if (0 <= (hr = BCryptOpenAlgorithmProvider(&hAlgorithm, BCRYPT_RSA_ALGORITHM, MS_PRIMITIVE_PROVIDER, 0)))
	{
		BCRYPT_KEY_HANDLE hKey;
		hr = BCryptGenerateKeyPair(hAlgorithm, &hKey, 1024, 0);
		BCryptCloseAlgorithmProvider(hAlgorithm, 0);

		if (0 <= hr)
		{
			if (0 > (hr = BCryptFinalizeKeyPair(hKey, 0)))
			{
				BCryptDestroyKey(hKey);
			}
			else
			{
				_hExchKey = hKey;
				hr = __super::Create();
			}
		}
	}

	return hr;
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

	BOOL fOk = FALSE;

	if (0 <= status && OpenChannel->ChannelHandle)
	{
		ULONG s = _pkn->GetSize();
		ULONG s_a = (s + __alignof(BCRYPT_RSAKEY_BLOB) - 1) & ~(__alignof(BCRYPT_RSAKEY_BLOB) - 1);
		CDataPacket* packet = 0;
		PBYTE pb = 0;
		ULONG cb = 0;

		while (0 <= (status = BCryptExportKey(_hExchKey, 0, BCRYPT_RSAPUBLIC_BLOB, pb, cb, &cb, 0)))
		{
			if (pb)
			{
				SC_Cntr* pcc = reinterpret_cast<SC_Cntr*>(packet->getData());
				memcpy(pcc, _pkn, s);
				pcc->Tag = SC_Cntr::scTag;
				packet->setDataSize(s_a + cb);
				fOk = 0 <= Send(packet);
				break;
			}

			if (packet = new(s_a + cb) CDataPacket)
			{
				pb = (PBYTE)packet->getData() + s_a;
			}
			else
			{
				break;
			}
		}

		if (packet)
		{
			packet->Release();
		}
	}

	return fOk;
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
			q->opType = p->opType;
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
				q->opType = p->opType;
				q->Length = 0;
				packet->setDataSize(sizeof(REMOTE_APDU));
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

NTSTATUS ScSocket::ProcessPIN(PUCHAR pb, ULONG cb)
{
	log(L"ProcessPIN(%x)\r\n", cb);

	NTSTATUS status = BCryptDecrypt(_hExchKey, pb, cb, 0, 0, 0, pb, cb, &cb, BCRYPT_PAD_PKCS1);
	if (0 <= status)
	{
		if (cb != 0x20)
		{
			status = STATUS_INFO_LENGTH_MISMATCH;
		}
		else
		{
			BCRYPT_ALG_HANDLE hAlgorithm;
			if (0 <= (status = BCryptOpenAlgorithmProvider(&hAlgorithm, BCRYPT_AES_ALGORITHM, MS_PRIMITIVE_PROVIDER, 0)))
			{
				BCRYPT_KEY_HANDLE hKey;
				status = BCryptGenerateSymmetricKey(hAlgorithm, &hKey, 0, 0, pb, cb, 0);
				BCryptCloseAlgorithmProvider(hAlgorithm, 0);

				if (0 <= status)
				{
					SC_Cntr* pkn = _pkn;
					pb = pkn->GetPrivKey(&cb);
					PBYTE pb2 = (PBYTE)alloca(cb);
					status = BCryptDecrypt(hKey, pb, cb, 0, 0, 0, pb2, cb, &cb, BCRYPT_BLOCK_PADDING);
					BCryptDestroyKey(hKey);

					if (0 <= status)
					{
						if (0 <= (status = BCryptOpenAlgorithmProvider(&hAlgorithm, BCRYPT_RSA_ALGORITHM, MS_PRIMITIVE_PROVIDER, 0)))
						{
							status = BCryptImportKeyPair(hAlgorithm, 0, BCRYPT_RSAPRIVATE_BLOB, &_hKey, pb2, cb, 0);
							BCryptCloseAlgorithmProvider(hAlgorithm, 0);
						}
					}
				}
			}
		}
	}

	log(L"ProcessPIN = %x\r\n", status);
	return status;
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
	//DumpBytes((PUCHAR)p, acl->BufferSize);

	if (BufferSize < sizeof(REMOTE_APDU))
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

	case REMOTE_APDU::opReset:
		if (_hKey)
		{
			BCryptDestroyKey(_hKey);
			_hKey = 0;
		}
		log(L"InUse[%x]\r\n", p->Hint);
		return;

	case REMOTE_APDU::opPin:
		if (_hKey)
		{
			BCryptDestroyKey(_hKey);
			_hKey = 0;
		}
		if (CDataPacket* packet = new(sizeof(REMOTE_APDU)) CDataPacket)
		{
			REMOTE_APDU* q = (REMOTE_APDU*)packet->getData();
			q->Hint = p->Hint;
			q->Length = 0; 
			q->algId = 0 > ProcessPIN(p->buf, len) ? REMOTE_APDU::a_md2 : REMOTE_APDU::a_sha256;
			q->opType = p->opType;
			packet->setDataSize(sizeof(REMOTE_APDU));
			Send(packet);
			packet->Release();
			return ;
		}
		break;

	case REMOTE_APDU::opEncrypt:
		log(L"Ecrypt %x bytes [%p]\r\n", len, _hKey);
		if (_hKey && PostResponce(B_Encrypt, p, len)) return;
		break;

	case REMOTE_APDU::opDecrypt:
		log(L"Decrypt %x bytes [%p]\r\n", len, _hKey);
		if (_hKey && PostResponce(B_Decrypt, p, len)) return;
		break;

	case REMOTE_APDU::opSign:

		if (!_hKey)
		{
			goto __err;
		}

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
			log(L"sign(%s) %x bytes\r\n", pi.pszAlgId, len);
			if (PostResponce(B_Sign, p, len, &pi)) return;
		}
		break;
	}

	goto __err;
}

_NT_END