#include "StdAfx.h"

_NT_BEGIN
//#include "util.h"
#include "../asio/packet.h"

#define SHA1_HASH_LENGTH 20

enum RequestClientInfoClientId
{
	ClientIdNone	= 0,
	ClientIdXEnroll2003	= 1,
	ClientIdAutoEnroll2003	= 2,
	ClientIdWizard2003	= 3,
	ClientIdCertReq2003	= 4,
	ClientIdDefaultRequest	= 5,
	ClientIdAutoEnroll	= 6,
	ClientIdRequestWizard	= 7,
	ClientIdEOBO	= 8,
	ClientIdCertReq	= 9,
	ClientIdTest	= 10,
	ClientIdWinRT	= 11,
	ClientIdUserStart	= 1000
};

#define wszCERTTYPE_USER_SMARTCARD_LOGON L"SmartcardLogon"

HRESULT EncodeObject(_In_ PCSTR lpszStructType, _In_ const void *pvStructInfo, _Out_ BYTE** ppbEncoded, _Inout_ ULONG *pcbEncoded)
{
	return GetLastHr(CryptEncodeObjectEx(X509_ASN_ENCODING, lpszStructType, 
		pvStructInfo, CRYPT_ENCODE_ALLOC_FLAG|CRYPT_ENCODE_NO_SIGNATURE_BYTE_REVERSAL_FLAG, 0, ppbEncoded, pcbEncoded));
}

inline HRESULT EncodeObject(_In_ PCSTR lpszStructType, _In_ const void *pvStructInfo, _Out_ PDATA_BLOB blob)
{
	return EncodeObject(lpszStructType, pvStructInfo, &blob->pbData, &blob->cbData);
}

inline HRESULT EncodeExtension(_Out_ PCERT_EXTENSION Extension,
							   _In_ PCSTR pszObjId,
							   _In_ const void *pvStructInfo,
							   _In_ BOOL fCritical = FALSE,
							   _In_ PCSTR lpszStructType = 0 
							   )
{
	Extension->fCritical = fCritical;
	Extension->pszObjId = const_cast<PSTR>(pszObjId);
	return EncodeObject(lpszStructType ? lpszStructType : pszObjId, 
		pvStructInfo, &Extension->Value.pbData, &Extension->Value.cbData);
}

HRESULT EncodeCommonName(PCWSTR szName, PCERT_NAME_BLOB Subject)
{
	CERT_RDN_ATTR RDNAttr = { 
		const_cast<PSTR>(szOID_COMMON_NAME), CERT_RDN_UNICODE_STRING, { (ULONG)wcslen(szName) * sizeof(WCHAR), (PBYTE)szName }
	};

	CERT_RDN RDN = { 1, &RDNAttr };
	CERT_NAME_INFO cni = { 1, &RDN };

	return EncodeObject(X509_NAME, &cni, Subject);
}

NTSTATUS DoHash(PUCHAR pbData, ULONG cbData, PUCHAR pbOutput, ULONG cbOutput, PCWSTR pszAlgId);

//HRESULT DoHash(PUCHAR pbData, ULONG cbData, PUCHAR pbOutput, ULONG cbOutput, PCWSTR pszAlgId)
//{
//	return GetLastHr(CryptHashCertificate2(pszAlgId, 0, 0, pbData, cbData, pbOutput, &cbOutput));
//}

HRESULT SetRsaPublicKey(_Inout_ PCERT_PUBLIC_KEY_INFO SubjectPublicKeyInfo, 
						_In_ PBYTE pbKey, _Out_ PBYTE pbKeyId, _In_ ULONG cbKeyId)
{
	SubjectPublicKeyInfo->Algorithm.pszObjId = const_cast<PSTR>(szOID_RSA_RSA);

	HRESULT hr = EncodeObject(CNG_RSA_PUBLIC_KEY_BLOB, pbKey, 
		&SubjectPublicKeyInfo->PublicKey.pbData, &SubjectPublicKeyInfo->PublicKey.cbData);

	if (0 <= hr)
	{
		hr = GetLastHr(CryptHashCertificate2(BCRYPT_SHA1_ALGORITHM, 0, 0, 
			SubjectPublicKeyInfo->PublicKey.pbData, SubjectPublicKeyInfo->PublicKey.cbData,
			pbKeyId, &cbKeyId));
	}

	return hr;
}

HRESULT SetRsaPublicKey(_Inout_ PCERT_PUBLIC_KEY_INFO SubjectPublicKeyInfo, 
						_In_ BCRYPT_KEY_HANDLE hKey,
						_Out_ PBYTE pbKeyId, 
						_In_ ULONG cbKeyId)
{
	PBYTE pbKey = 0;
	ULONG cbKey = 0;
	HRESULT hr;

	while (0 <= (hr = BCryptExportKey(hKey, 0, BCRYPT_RSAPUBLIC_BLOB, pbKey, cbKey, &cbKey, 0)))
	{
		if (pbKey)
		{
			return SetRsaPublicKey(SubjectPublicKeyInfo, pbKey, pbKeyId, cbKeyId);
		}

		pbKey = (PBYTE)alloca(cbKey);
	}

	return hr;
}

static PVOID pfnAlloc(_In_ size_t cbSize)
{
	if (CDataPacket* packet = new((ULONG)cbSize) CDataPacket)
	{
		return packet->getData();
	}

	return 0;
};

static void WINAPI pfnFree(_In_ PVOID pv)
{
	((CDataPacket*)pv - 1)->Release();
};

HRESULT 
EncodeSignAndEncode(_Out_ CDataPacket** request,
					_In_ BCRYPT_KEY_HANDLE hKey,
					_In_ PCSTR lpszInStructType, 
					_In_ const void *pvInStructInfo,
					_Out_opt_ PCRYPT_ALGORITHM_IDENTIFIER InSignatureAlgorithm, // inside pvInStructInfo
					_In_ PCSTR lpszOutStructType, 
					_In_ const void *pvOutStructInfo, 
					_In_ PCRYPT_DER_BLOB ToBeSigned, // inside pvOutStructInfo
					_Out_ PCRYPT_BIT_BLOB Signature, // inside pvOutStructInfo
					_Out_ PCRYPT_ALGORITHM_IDENTIFIER SignatureAlgorithm // inside pvOutStructInfo
			  )
{
	NTSTATUS status;

	BCRYPT_PKCS1_PADDING_INFO pi = {BCRYPT_SHA256_ALGORITHM};

	PSTR pszObjId = const_cast<PSTR>(szOID_RSA_SHA256RSA);

	SignatureAlgorithm->pszObjId = pszObjId;

	if (InSignatureAlgorithm)
	{
		*InSignatureAlgorithm = *SignatureAlgorithm;
	}

	if (0 <= (status = EncodeObject(lpszInStructType, pvInStructInfo, ToBeSigned)))
	{
		UCHAR hash[32];

		if (0 <= (status = DoHash(ToBeSigned->pbData, ToBeSigned->cbData, hash, sizeof(hash), BCRYPT_SHA256_ALGORITHM)))
		{
			while (0 <= (status = BCryptSignHash(hKey, &pi, hash, sizeof(hash), 
				Signature->pbData, Signature->cbData, &Signature->cbData, BCRYPT_PAD_PKCS1)))
			{
				if (Signature->pbData)
				{
					CRYPT_ENCODE_PARA EncodePara = { sizeof(EncodePara), pfnAlloc, pfnFree };

					CDataPacket* packet;
					ULONG cb;
					if (0 <= (status = GetLastHr(CryptEncodeObjectEx(X509_ASN_ENCODING, lpszOutStructType, 
						pvOutStructInfo, CRYPT_ENCODE_ALLOC_FLAG|CRYPT_ENCODE_NO_SIGNATURE_BYTE_REVERSAL_FLAG, 
						&EncodePara, &packet, &cb))))
					{
						*request = --packet;
						packet->setDataSize(cb);
					}

					break;
				}

				Signature->pbData = (PUCHAR)alloca(Signature->cbData);
			}
		}

		LocalFree(ToBeSigned->pbData);
	}

	return status;
}

HRESULT 
EncodeSignAndEncode(_Out_ CDataPacket** request,
					_In_ BCRYPT_KEY_HANDLE hKey,
					_In_ PCSTR lpszStructType, 
					_In_ const void *pvStructInfo,
					_Out_opt_ PCRYPT_ALGORITHM_IDENTIFIER SignatureAlgorithm // inside pvStructInfo
					)
{
	CERT_SIGNED_CONTENT_INFO csci { };

	return EncodeSignAndEncode(request, hKey, 
		lpszStructType, pvStructInfo, SignatureAlgorithm,
		X509_CERT, &csci, &csci.ToBeSigned, &csci.Signature, &csci.SignatureAlgorithm);
}

HRESULT myEncodeOsVersion(PCRYPT_ATTRIBUTE Attribute, PDATA_BLOB rgValue)
{
	ULONG M, m, b;
	RtlGetNtVersionNumbers(&M, &m, &b);
	char buf[32];
	CERT_NAME_VALUE cnvOSVer = { 
		CERT_RDN_IA5_STRING, { 
			(ULONG)sprintf_s(buf, _countof(buf), "%u.%u.%u." _CRT_STRINGIZE(VER_PLATFORM_WIN32_NT), M, m, b & 0x0FFFFFFF), (PBYTE)buf
		} 
	};

	Attribute->pszObjId = const_cast<PSTR>(szOID_OS_VERSION);
	Attribute->cValue = 1;
	Attribute->rgValue = rgValue;
	return EncodeObject(X509_NAME_VALUE, &cnvOSVer, rgValue);
}

HRESULT myEncodeCspInfo(PCRYPT_ATTRIBUTE Attribute, PDATA_BLOB rgValue)
{
	CRYPT_CSP_PROVIDER CSPProvider { 0, const_cast<PWSTR>(MS_KEY_STORAGE_PROVIDER), {} };

	CERT_NAME_VALUE cnvCSP = { CERT_RDN_ENCODED_BLOB };

	if (HRESULT hr = EncodeObject(szOID_ENROLLMENT_CSP_PROVIDER, &CSPProvider, &cnvCSP.Value))
	{
		return hr;
	}

	Attribute->pszObjId = const_cast<PSTR>(szOID_ENROLLMENT_CSP_PROVIDER);
	Attribute->cValue = 1;
	Attribute->rgValue = rgValue;

	return EncodeObject(X509_NAME_VALUE, &cnvCSP, rgValue);
}

void FreeBlocks(ULONG n, CRYPT_DER_BLOB rgValues[4])
{
	do 
	{
		if (PBYTE pb = rgValues[--n].pbData)
		{
			LocalFree(pb);
		}
	} while (n);
}

HRESULT EncodeUtf8String(PCWSTR pcsz, PDATA_BLOB rgValue)
{
	CERT_NAME_VALUE cnv = {
		CERT_RDN_UTF8_STRING, {
			(ULONG)wcslen(pcsz)*sizeof(WCHAR), (PBYTE)pcsz
		}
	};

	return EncodeObject(X509_UNICODE_NAME_VALUE, &cnv, rgValue);
}

HRESULT myEncodeClientInfo(PCRYPT_ATTRIBUTE Attribute, PDATA_BLOB rgValue, PCWSTR MachineName)
{
	CRYPT_DER_BLOB rgValues[4] = {};
	CRYPT_SEQUENCE_OF_ANY soa = { _countof(rgValues), rgValues };

	Attribute->pszObjId = const_cast<PSTR>(szOID_REQUEST_CLIENT_INFO);
	Attribute->cValue = 1;
	Attribute->rgValue = rgValue;

	static const RequestClientInfoClientId ClientId = ClientIdTest;

	HRESULT hr;

	0 <= (hr = EncodeObject(X509_INTEGER, &ClientId, &rgValues[0])) &&
		0 <= (hr = EncodeUtf8String(MachineName, &rgValues[1])) &&
		0 <= (hr = EncodeUtf8String(L"NT AUTHORITY\\SYSTEM", &rgValues[2])) &&
		0 <= (hr = EncodeUtf8String(L"rbmm", &rgValues[3])) &&
		0 <= (hr = EncodeObject(X509_SEQUENCE_OF_ANY, &soa, rgValue));

	FreeBlocks(_countof(rgValues), rgValues);

	return hr;
}

HRESULT SetKeyId(PCERT_EXTENSION rgExtension, _In_ PBYTE pbKeyId)
{
	CRYPT_HASH_BLOB KeyId = { SHA1_HASH_LENGTH, pbKeyId };

	return EncodeExtension(rgExtension, szOID_SUBJECT_KEY_IDENTIFIER, &KeyId);
}

HRESULT SetTemplateName(PCERT_EXTENSION rgExtension, _In_ PCWSTR pcszTemplateName)
{
	CERT_NAME_VALUE cnv = {
		CERT_RDN_UNICODE_STRING, {
			(ULONG)wcslen(pcszTemplateName)*sizeof(WCHAR), (PBYTE)pcszTemplateName
		}
	};

	return EncodeExtension(rgExtension, szOID_ENROLL_CERTTYPE_EXTENSION, &cnv, FALSE, X509_UNICODE_NAME_VALUE);
}

HRESULT myEncodeExtensions(_Out_ PCRYPT_ATTRIBUTE Attribute, 
						   _Out_ PDATA_BLOB rgValue, 
						   _In_ PBYTE pbKeyId,
						   _In_ PCWSTR pcszTemplateName
						   )
{
	CERT_EXTENSION rgExtension[2] = {};
	CERT_EXTENSIONS ext = { 0, rgExtension };

	Attribute->pszObjId = const_cast<PSTR>(szOID_RSA_certExtensions);
	Attribute->cValue = 1;
	Attribute->rgValue = rgValue;

	HRESULT hr;
	0 <= (hr = SetKeyId(&rgExtension[ext.cExtension++], pbKeyId)) &&
		0 <= (hr = SetTemplateName(&rgExtension[ext.cExtension++], pcszTemplateName)) &&
		0 <= (hr = EncodeObject(X509_EXTENSIONS, &ext, rgValue));

	ULONG n = _countof(rgExtension);
	do 
	{
		if (PBYTE pb = rgExtension[--n].Value.pbData)
		{
			LocalFree(pb);
		}
	} while (n);

	return hr;
}

HRESULT CreateBKey(_Out_ BCRYPT_KEY_HANDLE *phKey)
{
	BCRYPT_ALG_HANDLE hAlgorithm;
	
	NTSTATUS status = BCryptOpenAlgorithmProvider(&hAlgorithm, BCRYPT_RSA_ALGORITHM, MS_PRIMITIVE_PROVIDER, 0);
	
	if (0 <= status)
	{
		BCRYPT_KEY_HANDLE hKey;
		
		status = BCryptGenerateKeyPair(hAlgorithm, &hKey, 2048, 0);
		
		BCryptCloseAlgorithmProvider(hAlgorithm, 0);

		if (0 <= status)
		{
			if (0 <= (status = BCryptFinalizeKeyPair(hKey, 0)))
			{
				*phKey = hKey;
				return S_OK;
			}

			BCryptDestroyKey(hKey);
		}
	}

	return HRESULT_FROM_NT(status);
}

HRESULT BuildPkcs10(_In_ PCWSTR pwszName, _Out_ CDataPacket** request, _Out_ BCRYPT_KEY_HANDLE* phKey)
{
	HRESULT hr;

	BCRYPT_KEY_HANDLE hKey;

	if (S_OK == (hr = CreateBKey(&hKey)))
	{
		DATA_BLOB rgValues[4] = {};
		CRYPT_ATTRIBUTE rgAttribute[4];
		CERT_REQUEST_INFO cri = { CERT_REQUEST_V1, {}, {}, _countof(rgAttribute), rgAttribute };

		if (0 <= (hr = EncodeCommonName(pwszName, &cri.Subject)))
		{
			UCHAR KeyId[SHA1_HASH_LENGTH];

			if (0 <= (hr = SetRsaPublicKey(&cri.SubjectPublicKeyInfo, hKey, KeyId, sizeof(KeyId))))
			{
				0 <= (hr = myEncodeOsVersion(&rgAttribute[0], &rgValues[0])) &&
					0 <= (hr = myEncodeCspInfo(&rgAttribute[1], &rgValues[1])) &&
					0 <= (hr = myEncodeClientInfo(&rgAttribute[2], &rgValues[2], L"Smart")) &&
					0 <= (hr = myEncodeExtensions(&rgAttribute[3], &rgValues[3], KeyId, wszCERTTYPE_USER_SMARTCARD_LOGON)) &&
					0 <= (hr = EncodeSignAndEncode(request, hKey, X509_CERT_REQUEST_TO_BE_SIGNED, &cri, 0));

				LocalFree(cri.SubjectPublicKeyInfo.PublicKey.pbData);
			}

			LocalFree(cri.Subject.pbData);
		}

		FreeBlocks(_countof(rgValues), rgValues);

		if (0 <= hr)
		{
			*phKey = hKey;
			return S_OK;
		}

		BCryptDestroyKey(hKey);
	}

	return HRESULT_FROM_WIN32(hr);
}

_NT_END
