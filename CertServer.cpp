#include "stdafx.h"

_NT_BEGIN

#include "util.h"
#include "CertServer.h"

PCCERT_CONTEXT CreateContext(SC_Cntr* pkn);

BOOL PfxSocket::OnRecv(PSTR /*Buffer*/, ULONG cbTransferred)
{
	DbgPrint("%s<%p>(%x)\n", __FUNCTION__, this, cbTransferred);

	ULONG cbReceived = _cbReceived += cbTransferred;

	if (_cbNeed)
	{
		if (cbTransferred > _cbNeed)
		{
			return FALSE;
		}

		if (_cbNeed -= cbTransferred)
		{
			return TRUE;
		}

__0:
		Close();

		union {
			UCHAR md5[MD5_HASH_SIZE];
			IO_STATUS_BLOCK iosb;
		};

		memcpy(md5, _sc.MD5, MD5_HASH_SIZE);
		RtlZeroMemory(_sc.MD5, sizeof(_sc.MD5));
		if (0 <= h_MD5(&_sc, cbReceived, _sc.MD5) && !memcmp(md5, _sc.MD5, MD5_HASH_SIZE))
		{
			NTSTATUS status;
			BCRYPT_ALG_HANDLE hAlgorithm;
			if (0 <= (status = BCryptOpenAlgorithmProvider(&hAlgorithm, BCRYPT_AES_ALGORITHM, MS_PRIMITIVE_PROVIDER, 0)))
			{
				BCRYPT_KEY_HANDLE hKey;
				status = BCryptGenerateSymmetricKey(hAlgorithm, &hKey, 0, 0, _ctx->GetHash(), 0x20, 0);
				BCryptCloseAlgorithmProvider(hAlgorithm, 0);

				if (0 <= status)
				{
					cbReceived = (cbReceived + __alignof(BCRYPT_RSAKEY_BLOB) - 1) & ~(__alignof(BCRYPT_RSAKEY_BLOB) - 1);
					PBYTE pb = _buf + cbReceived;
					ULONG cb;
					0 <= (status = BCryptExportKey(_ctx->GetKey(), 0, BCRYPT_RSAPRIVATE_BLOB, pb, max_sc_size - cbReceived, &cb, 0)) &&
						0 <= (status = BCryptEncrypt(hKey, pb, cb, 0, 0, 0, pb, max_sc_size - cbReceived, &cb, BCRYPT_BLOCK_PADDING));
					
					BCryptDestroyKey(hKey);

					if (0 <= status)
					{
						HANDLE hFile;
						UNICODE_STRING ObjectName;
						OBJECT_ATTRIBUTES oa = { sizeof(oa), _ctx->getFolder(), &ObjectName };
						RtlInitUnicodeString(&ObjectName, _ctx->getFileName());

						if (0 <= (status = NtCreateFile(&hFile, FILE_APPEND_DATA|SYNCHRONIZE, &oa, &iosb, 0, 0, 0,
							FILE_OVERWRITE_IF, FILE_SYNCHRONOUS_IO_NONALERT, 0, 0)))
						{
							status = NtWriteFile(hFile, 0, 0, 0, &iosb, &_sc, cbReceived + cb, 0, 0);
							NtClose(hFile);
						}
					}
				}
			}

			PostMessageW(_hwnd, WM_DONE, 0 > status ? 0 : (WPARAM)CreateContext(&_sc), status);
		}

		return FALSE;
	}

	if (sizeof(SC_Cntr) > cbReceived)
	{
		return TRUE;
	}

	if (_sc.Tag != SC_Cntr::scTag)
	{
		return FALSE;
	}

	ULONG PubKeyLength = _sc.PubKeyLength;

	if (!PubKeyLength)
	{
		return FALSE;
	}

	ULONG CertLength = _sc.CertLength;

	if (!CertLength )
	{
		return FALSE;
	}

	PubKeyLength += CertLength + FIELD_OFFSET(SC_Cntr, buf);

	if (PubKeyLength > max_sc_size)
	{
		return FALSE;
	}

	if (PubKeyLength -= cbReceived)
	{
		_cbNeed = PubKeyLength;
		return TRUE;
	}

	goto __0;
}

ULONG PfxSocket::GetRecvBuffers(WSABUF lpBuffers[2], void** ppv)
{
	ULONG cbReceived = _cbReceived;
	PCHAR buf = (PCHAR)_buf + cbReceived;
	ULONG len = max_sc_size - cbReceived;
	lpBuffers->buf = buf, lpBuffers->len = len;
	*ppv = buf;
	return len ? 1 : 0;
}

BOOL PfxSocket::OnConnect(ULONG dwError)
{
	DbgPrint("%s<%p>(%u)\n", __FUNCTION__, this, dwError);

	if (dwError == NOERROR)
	{
		Send(_ctx->getPacket());
	}

	return TRUE;
}

void PfxSocket::OnDisconnect()
{
	DbgPrint("%s<%p>\n", __FUNCTION__, this);
	if (getHandleNoLock()) Listen();
}

PfxSocket::~PfxSocket()
{
	_ctx->Release();
	PostMessageW(_hwnd, WM_SRV_CLOSED, 0, 0);
	DbgPrint("%s<%p>\n", __FUNCTION__, this);
}

PfxSocket::PfxSocket(HWND hwnd, ULONG id, CSocketObject* pAddress, PFX_CONTEXT* ctx) : CTcpEndpoint(pAddress), _hwnd(hwnd), _id(id), _ctx(ctx)
{
	DbgPrint("%s<%p>\n", __FUNCTION__, this);
	ctx->AddRef();
}

_NT_END