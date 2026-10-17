// Post-build manifest injector for MinGW builds.
// MinGW's CRT embeds a default manifest that conflicts with windres RT_MANIFEST.
// This tool uses UpdateResource to replace it after linking.
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

int wmain(int argc, wchar_t** argv) {
    if (argc != 3) {
        fwprintf(stderr, L"Usage: inject_manifest.exe <exe> <manifest>\n");
        return 1;
    }

    HANDLE hUpdate = BeginUpdateResourceW(argv[1], FALSE);
    if (!hUpdate) {
        fwprintf(stderr, L"BeginUpdateResource failed: %lu\n", GetLastError());
        return 1;
    }

    FILE* f = _wfopen(argv[2], L"rb");
    if (!f) {
        fwprintf(stderr, L"Cannot open manifest\n");
        return 1;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    BYTE* data = (BYTE*)malloc(size);
    fread(data, 1, size, f);
    fclose(f);

    // Resource ID 1 = CREATEPROCESS_MANIFEST_RESOURCE_ID
    // RT_MANIFEST and MAKEINTRESOURCEW return LPSTR in this MinGW
    LPCWSTR manifestType = (LPCWSTR)(uintptr_t)RT_MANIFEST;
    LPCWSTR manifestId = (LPCWSTR)(uintptr_t)1;
    BOOL ok = UpdateResourceW(hUpdate, manifestType, manifestId,
        MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US), data, size);

    if (!ok) {
        fwprintf(stderr, L"UpdateResource failed: %lu\n", GetLastError());
        free(data);
        EndUpdateResourceW(hUpdate, TRUE);
        return 1;
    }
    free(data);

    if (!EndUpdateResourceW(hUpdate, FALSE)) {
        fwprintf(stderr, L"EndUpdateResource failed: %lu\n", GetLastError());
        return 1;
    }

    wprintf(L"Manifest injected into %s\n", argv[1]);
    return 0;
}
