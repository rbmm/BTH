#pragma once

enum SCAlgId : UCHAR { 
	GIDS_RSA_1024_IDENTIFIER = 0x06,
	GIDS_RSA_2048_IDENTIFIER,
	// Unsupported key algorithm ids:
	//GIDS_RSA_3072_IDENTIFIER,
	//GIDS_RSA_4096_IDENTIFIER,
	//GIDS_ECC_192_IDENTIFIER,
	//GIDS_ECC_224_IDENTIFIER,
	//GIDS_ECC_256_IDENTIFIER,
	//GIDS_ECC_384_IDENTIFIER,
	//GIDS_ECC_521_IDENTIFIER,
};

struct SC_Cntr
{
	enum : USHORT { scTag = 'CS' } Tag;
	enum : UCHAR { bTag = 'b' } subTag;
	SCAlgId algid;
	USHORT PubKeyLength, CertLength;
	UCHAR MD5[16], cardid[16];
	UCHAR buf[/* PubKeyLength + CertLength */];

	void* operator new(size_t s, ULONG cb)
	{
		return LocalAlloc(0, s + cb);
	}

	void operator delete(void* pv)
	{
		LocalFree(pv);
	}

	PUCHAR GetPubKey()
	{
		return buf;
	}

	PUCHAR GetCert()
	{
		return buf + PubKeyLength;
	}

	ULONG GetSize()
	{
		return FIELD_OFFSET(SC_Cntr, buf) + PubKeyLength + CertLength;
	}

	BOOL IsValid(ULONG cb);
};
