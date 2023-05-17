#include "stdafx.h"
#include <compressapi.h>
#include "zip.h"

HRESULT Unzip(_In_reads_bytes_opt_(CompressedDataSize) LPCVOID CompressedData,
			  _In_ SIZE_T CompressedDataSize,
			  _Out_writes_bytes_(*UncompressedSize) PVOID* UncompressedBuffer,
			  _Out_ PULONG UncompressedSize)
{
	ULONG dwError;
	COMPRESSOR_HANDLE DecompressorHandle;

	if (CreateDecompressor(COMPRESS_ALGORITHM_MSZIP, 0, &DecompressorHandle))
	{
		SIZE_T UncompressedDataSize;

		if (Decompress(DecompressorHandle, CompressedData, CompressedDataSize, 0, 0, &UncompressedDataSize))
		{
			dwError = ERROR_INTERNAL_ERROR;
		}
		else if (ERROR_INSUFFICIENT_BUFFER == (dwError = GetLastError()))
		{
			dwError = ERROR_OUTOFMEMORY;

			if (PBYTE pb = new BYTE[UncompressedDataSize])
			{
				if (Decompress(DecompressorHandle, CompressedData, CompressedDataSize, pb, UncompressedDataSize, &UncompressedDataSize))
				{
					dwError = NOERROR;
					*UncompressedSize = (ULONG)UncompressedDataSize;
					*UncompressedBuffer = pb;
				}
				else
				{
					dwError = GetLastError();
					delete [] pb;
				}
			}
		}

		CloseDecompressor(DecompressorHandle);
	}
	else
	{
		dwError = GetLastError();
	}

	return HRESULT_FROM_WIN32(dwError);
}
