#pragma once

HRESULT Unzip(_In_reads_bytes_opt_(CompressedDataSize) LPCVOID CompressedData,
			  _In_ SIZE_T CompressedDataSize,
			  _Out_writes_bytes_(*UncompressedSize) PVOID* UncompressedBuffer,
			  _Out_ PULONG UncompressedSize);