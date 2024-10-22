/* SDK Includes */

#include "SDK/sdhd_sdk.hh"
#include "SDK/contrib/MinHook.h"
#pragma comment(lib, "SDK/contrib/MinHook.lib")

/* Includes */

#include <string>

/* Defines */

#define REDIRECTOR_NAME     "Redirector"
#define REDIRECTOR_DIR      REDIRECTOR_NAME "Data"  /* Directory must end with "Data" since resources use it for resource uid! */

//--------------------------------------------------------------------------
// Core
//--------------------------------------------------------------------------

class FileRedirect : public UFG::qNodeRB<FileRedirect>
{
public:
    char* mPath;

    FileRedirect(const char* original_path, const char* path)
    {
        mNode.mUID = UFG::qStringHashUpper32(original_path);

        size_t len = strlen(path) + 1;
        mPath = new char[len];
        memcpy(mPath, path, len);
    }
};

namespace core
{
    UFG::qTreeRB<FileRedirect> mFiles;

    void AddFileRedirect(const std::string& file_path)
    {
        const char* path = file_path.c_str();

        /* temp.bin files are automatically queued for load while loading perm.bin so we don't need to redirect them. */
        if (UFG::qStringFindInsensitive(path, ".temp.bin")) {
            return;
        }

        const char* original_path = &path[(sizeof(REDIRECTOR_NAME) - 1)];

        mFiles.Add(new FileRedirect(original_path, path));
    }

    const char* GetFileRedirect(const char* filename)
    {
        auto fileRedirect = mFiles.Get(UFG::qStringHashUpper32(filename));
        if (!fileRedirect) {
            return filename;
        }

        return fileRedirect->mPath;
    }

    void RecursiveSearch(const std::string& path)
    {
        WIN32_FIND_DATAA wFindData = { 0 };
        HANDLE hFind;
        {
            std::string find_path = path + "\\*";
            hFind = FindFirstFileA(find_path.c_str(), &wFindData);
        }

        if (hFind == INVALID_HANDLE_VALUE) {
            return;
        }

        while (FindNextFileA(hFind, &wFindData))
        {
            std::string file_path = (path + "\\" + wFindData.cFileName);

            if (wFindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            {
                if (wFindData.cFileName[0] == '.') {
                    continue;
                }

                RecursiveSearch(file_path);
                continue;
            }

            AddFileRedirect(file_path);
        }

        FindClose(hFind);
    }

    void Initialize()
    {
        std::string game_dir(MAX_PATH, 0);
        {
            GetModuleFileNameA(GetModuleHandleA(0), &game_dir[0], MAX_PATH);
            game_dir = game_dir.substr(0, game_dir.find_last_of("/\\"));
        }

        SetCurrentDirectoryA(game_dir.c_str());

        /* Initialize redirector files */
        RecursiveSearch(REDIRECTOR_DIR);
    }
}

//--------------------------------------------------------------------------
// Hooks
//--------------------------------------------------------------------------

namespace Hooks
{
    namespace BigFileSystem
    {
        typedef bool(__fastcall* Fn_GetFileInfoFromBigFile)(const char*, void*, void*);
        Fn_GetFileInfoFromBigFile gGetFileInfoFromBigFile;

        bool __fastcall GetFileInfoFromBigFile(char* filename, void** _bigFile, void** _entry)
        {
            /* Call original if filename doesn't begin with Redirector prefix. */
            if (memcmp(filename, REDIRECTOR_NAME, (sizeof(REDIRECTOR_NAME) - 1))) {
                return gGetFileInfoFromBigFile(filename, _bigFile, _entry);
            }

            return false;
        }
    }

    namespace DataStreamer
    {
        typedef bool(__fastcall* Fn_QueueStream)(void*, const char*, u32, u32, u32, u32, void*, void*);
        Fn_QueueStream gQueueStream;

        bool __fastcall QueueStream(void* handle, const char* filename, u32 mem_tag_qsymbol_uid, u32 data_type, u32 priority, u32 flags, void* callback, void* callbackParam)
        {
            return gQueueStream(handle, core::GetFileRedirect(filename), mem_tag_qsymbol_uid, data_type, priority, flags, callback, callbackParam);
        }
    }

    namespace StreamFileWrapper
    {
        typedef void*(__fastcall* Fn_ReadEntireFile)(const char*, u64*, void*, uint32_t, const char*);
        Fn_ReadEntireFile gReadEntireFile;

        void* ReadEntireFile(const char* filename, u64* loaded_size, void* memory_pool, u32 alloc_flags, const char* dbg_tag)
        {
            return gReadEntireFile(core::GetFileRedirect(filename), loaded_size, memory_pool, alloc_flags, dbg_tag);
        }

        typedef void*(__fastcall* Fn_ReadEntireFileAsync)(const char*, u32, void*, void*, void*, u32, const char*);
        Fn_ReadEntireFileAsync gReadEntireFileAsync;

        bool ReadEntireFileAsync(const char* filename, u32 queueClass, void* callback, void* callback_param, void* memory_pool, u32 alloc_flags, const char* dbg_tag)
        {
            return gReadEntireFileAsync(core::GetFileRedirect(filename), queueClass, callback, callback_param, memory_pool, alloc_flags, dbg_tag);
        }
    }

    void Initialize()
    {
        MH_Initialize();

        MH_CreateHook(SDK_RVA_PTR(0x229890), BigFileSystem::GetFileInfoFromBigFile, (void**)&BigFileSystem::gGetFileInfoFromBigFile);
        MH_CreateHook(SDK_RVA_PTR(0x22C7F0), StreamFileWrapper::ReadEntireFile, (void**)&StreamFileWrapper::gReadEntireFile);
        MH_CreateHook(SDK_RVA_PTR(0x22C9C0), StreamFileWrapper::ReadEntireFileAsync, (void**)&StreamFileWrapper::gReadEntireFileAsync);
        MH_CreateHook(SDK_RVA_PTR(0x22C360), DataStreamer::QueueStream, (void**)&DataStreamer::gQueueStream);
    }
}

//--------------------------------------------------------------------------

BOOL APIENTRY DllMain(HMODULE hModule, DWORD dwReason, LPVOID pReserved)
{
    if (dwReason == DLL_PROCESS_ATTACH)
    {
        if (!SDK::IsValidExecutable()) {
            return FALSE;
        }

        DisableThreadLibraryCalls(hModule);

        core::Initialize();
        Hooks::Initialize();
    }

    return TRUE;
}

