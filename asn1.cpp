#include "stdafx.h"

_NT_BEGIN
#include "asn1.h"

BOOL TLV_IsValid(_In_ const BYTE *pb, _In_ ULONG cb)
{
	if (cb)
	{
		union {
			ULONG Len;
			struct {
				SCHAR l_0;
				SCHAR l_1;
				SCHAR l_2;
				SCHAR l_3;
			};
		};

		do 
		{
			union {
				ULONG uTag;
				struct {
					SCHAR t_0;
					SCHAR t_1;
					SCHAR t_2;
					SCHAR t_3;
				};
				struct {
					UCHAR tag : 5;
					UCHAR composite : 1;
					UCHAR cls : 2;
				};
			};

			uTag = *pb++, cb--;

			if (tag == 0x1F)
			{
				if (!cb--)
				{
					return FALSE;
				}

				if (0 > (t_1 = *pb++))
				{
					if (!cb--)
					{
						return FALSE;
					}

					if (0 > (t_2 = *pb++))
					{
						if (!cb--)
						{
							return FALSE;
						}

						t_3 = *pb++;
					}
				}
			}

			if (!uTag)
			{
				Len = 0;
				continue;
			}

			if (!cb--)
			{
				return FALSE;
			}

			Len = *pb++;

			if (0 > l_0)
			{
				if ((Len &= ~0x80) > cb)
				{
					return FALSE;
				}

				cb -= Len;

				switch (Len)
				{
				case 4:
					l_3 = *pb++;
					l_2 = *pb++;
				case 2:
					l_1 = *pb++;
				case 1:
					l_0 = *pb++;
				case 0:
					break;
				default: return 0;
				}
			}

			if (Len > cb || (composite && (Len && !TLV_IsValid(pb, Len))))
			{
				return FALSE;
			}

		} while (pb += Len, cb -= Len);

		return TRUE;
	}

	return FALSE;
}

ULONG TLV_TagLen(_Inout_ const BYTE **ppbTag, _Inout_ ULONG *pcbTag, _Out_ const BYTE **ppbValue, _Out_ ULONG *pLen)
{
	union {
		ULONG uTag;
		struct {
			SCHAR t_0;
			SCHAR t_1;
			SCHAR t_2;
			SCHAR t_3;
		};
		struct {
			UCHAR tag : 5;
			UCHAR type : 1;
			UCHAR cls : 2;
		};
	};

	union {
		ULONG Len;
		struct {
			SCHAR l_0;
			SCHAR l_1;
			SCHAR l_2;
			SCHAR l_3;
		};
	};

	ULONG cb = *pcbTag;
	const BYTE *pb = *ppbTag;

	uTag = *pb++, --cb;

	if (tag == 0x1F)
	{
		if (0 > (--cb, t_1 = *pb++) && 0 > (--cb, t_2 = *pb++))
		{
			t_3 = *pb++, --cb;
		}
	}

	if (uTag)
	{
		Len = *pb++, --cb;

		if (0 > l_0)
		{
			cb -= (Len &= ~0x80);

			switch (Len)
			{
			case 4:
				l_3 = *pb++;
				l_2 = *pb++;
			case 2:
				l_1 = *pb++;
			case 1:
				l_0 = *pb++;
			case 0:
				break;
			default: __debugbreak();
			}
		}
	}
	else
	{
		Len = 0;
	}

	*pcbTag = cb - Len, *ppbTag = pb + Len, *pLen = Len, *ppbValue = pb;

	return uTag;
}

_NT_END