#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <algorithm>
#include <iostream>
#include <string>
#include <unordered_map>
#include <map>

uintptr_t BaseAddress = reinterpret_cast<uintptr_t>(GetModuleHandleA(0));
#define UFG_RVA(x) (BaseAddress + x)
namespace UFG
{
    uint32_t Hash32(const char* str, uint32_t prevHash = 0xFFFFFFFF)
    {
        return reinterpret_cast<unsigned int(__fastcall*)(const char*, unsigned int)>(UFG_RVA(0x18B680))(str, prevHash);
    }

    uint32_t HashUpper32(const char* str, uint32_t prevHash = 0xFFFFFFFF)
    {
        return reinterpret_cast<unsigned int(__fastcall*)(const char*, unsigned int)>(UFG_RVA(0x18B720))(str, prevHash);
    }
}

#include "3rdParty/MinHook.h"

#define REDIRECTOR_NAME     "Redirector"
#define REDIRECTOR_DIR      REDIRECTOR_NAME"Data"

struct Redirector_t
{
    std::unordered_map<uint32_t, std::string> Map;

    void Add(const char* m_OriginalFileName, std::string m_NewFileName)
    {
        Map[UFG::HashUpper32(m_OriginalFileName)] = m_NewFileName;
    }

    void Add(std::string m_FileName)
    {
        std::string m_OriginalFileName = m_FileName;
        m_OriginalFileName.erase(0, sizeof(REDIRECTOR_NAME) - 1);

        Add(&m_OriginalFileName[0], m_FileName);
    }

    void Itter(std::string m_Path)
    {
        WIN32_FIND_DATAA m_FindData = { 0 };
        HANDLE m_Find = FindFirstFileA(&(m_Path + "\\*")[0], &m_FindData);
        if (m_Find == INVALID_HANDLE_VALUE)
            return;

        while (FindNextFileA(m_Find, &m_FindData) != 0)
        {
            if (m_FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            {
                if (m_FindData.cFileName[0] != '.')
                    Itter(m_Path + "\\" + m_FindData.cFileName);
                continue;
            }

            Add(m_Path + "\\" + m_FindData.cFileName);
        }

        FindClose(m_Find);
    }

    const char* Get(const char* m_FileName)
    {
        uint32_t m_Hash = UFG::HashUpper32(m_FileName);
        auto m_It = Map.find(m_Hash);
        if (m_It == Map.end())
            return nullptr;

        return &m_It->second[0];
    }
};
static Redirector_t Redirector;

struct Logger_t
{
    const char* Path = nullptr;

    Logger_t(const char* m_Path) 
    { 
        Path = m_Path;
        remove(m_Path);
    }

    void AppendString(std::string m_String)
    {
        FILE* m_File = fopen(Path, "a");
        if (m_File)
        {
            fwrite(&m_String[0], sizeof(char), m_String.size(), m_File);
            fclose(m_File);
        }
    }

    void Append(const char* m_Format, ...) 
    {
        char m_Buffer[2048];

        va_list m_Args;
        va_start(m_Args, m_Format);
        vsnprintf_s(m_Buffer, sizeof(m_Buffer), m_Format, m_Args);
        va_end(m_Args);

        AppendString(m_Buffer);
    }

#ifdef _DEBUG
    template<typename... Args>
    void AppendDebug(const char* m_Format, Args... m_Args) { Append(m_Format, m_Args...); }
#else
    template<typename... Args>
    void AppendDebug(const char* m_Format, Args... m_Args) { }
#endif

};
static Logger_t Logger((REDIRECTOR_DIR"\\Log.txt"));

namespace Hooks
{
    namespace BigFileSystem
    {
        typedef bool(__fastcall* m_tGetFileInfoFromBigFile)(const char*, void*, void*);
        m_tGetFileInfoFromBigFile m_oGetFileInfoFromBigFile;

        bool __fastcall GetFileInfoFromBigFile(const char* filename, void* _bigFile, void* _entry)
        {
            if (memcmp(filename, REDIRECTOR_NAME, sizeof(REDIRECTOR_NAME) - 1) == 0)
                return false; // If it's redirector folder it will instantly return false so it doesn't read from big file...

            return m_oGetFileInfoFromBigFile(filename, _bigFile, _entry);
        }
    }

    namespace DataStreamer
    {
        typedef bool(__fastcall* m_tQueueStream)(void*, const char*, uint32_t, uint32_t, uint32_t, uint32_t, void*, void*);
        m_tQueueStream m_oQueueStream;

        bool __fastcall QueueStream(void* handle, const char* filename, uint32_t mem_tag_qsymbol_uid, uint32_t data_type, uint32_t priority, uint32_t flags, void* callback, void* callbackParam)
        {
            const char* m_RedirectFileName = Redirector.Get(filename);
            if (m_RedirectFileName)
            {
                Logger.AppendDebug("[ QueueStream->Redirect ] %s -> %s\n", filename, m_RedirectFileName);
                filename = m_RedirectFileName;
            }
            else
                Logger.AppendDebug("[ QueueStream ] %s\n", filename);


            return m_oQueueStream(handle, filename, mem_tag_qsymbol_uid, data_type, priority, flags, callback, callbackParam);
        }
    }

    namespace StreamFileWrapper
    {
        typedef void*(__fastcall* m_tReadEntireFile)(const char*, uint64_t*, void*, uint32_t, const char*);
        m_tReadEntireFile m_oReadEntireFile;

        void* ReadEntireFile(const char* filename, uint64_t* loaded_size, void* memory_pool, uint32_t alloc_flags, const char* dbg_tag)
        {
            const char* m_RedirectFileName = Redirector.Get(filename);
            if (m_RedirectFileName)
            {
                Logger.AppendDebug("[ ReadEntireFile->Redirect ] %s -> %s\n", filename, m_RedirectFileName);
                filename = m_RedirectFileName;
            }
            else
                Logger.AppendDebug("[ ReadEntireFile ] %s\n", filename);

            return m_oReadEntireFile(filename, loaded_size, memory_pool, alloc_flags, dbg_tag);
        }

        typedef void* (__fastcall* m_tReadEntireFileAsync)(const char*, uint32_t, void*, void*, void*, uint32_t, const char*);
        m_tReadEntireFileAsync m_oReadEntireFileAsync;

        bool ReadEntireFileAsync(const char* filename, uint32_t queueClass, void* callback, void* callback_param, void* memory_pool, uint32_t alloc_flags, const char* dbg_tag)
        {
            const char* m_RedirectFileName = Redirector.Get(filename);
            if (m_RedirectFileName)
            {
                Logger.AppendDebug("[ ReadEntireFileAsync->Redirect ] %s -> %s\n", filename, m_RedirectFileName);
                filename = m_RedirectFileName;
            }
            else
                Logger.AppendDebug("[ ReadEntireFileAsync ] %s\n", filename);

            return m_oReadEntireFileAsync(filename, queueClass, callback, callback_param, memory_pool, alloc_flags, dbg_tag);
        }
    }
}

DWORD __stdcall MainThread(void* m_Reserved)
{
    CreateDirectoryA(REDIRECTOR_DIR, 0);

    Redirector.Itter(REDIRECTOR_DIR);
    {
        Logger.Append("[ Redirector ] Found %u files...\n", Redirector.Map.size());
        for (auto& m_Pair : Redirector.Map)
            Logger.Append("[ Redirector ] %s\n", &m_Pair.second[0]);

        Logger.Append("\n");
    }

    MH_Initialize();
    auto MH_AddHook = [](uintptr_t m_Function, void* m_Hook, void** m_Original = nullptr)
    {
        MH_CreateHook(reinterpret_cast<void*>(m_Function), m_Hook, m_Original);
        MH_EnableHook(reinterpret_cast<void*>(m_Function));
    };

    MH_AddHook(UFG_RVA(0x229890), Hooks::BigFileSystem::GetFileInfoFromBigFile,     (void**)&Hooks::BigFileSystem::m_oGetFileInfoFromBigFile);
    MH_AddHook(UFG_RVA(0x22C7F0), Hooks::StreamFileWrapper::ReadEntireFile,         (void**)&Hooks::StreamFileWrapper::m_oReadEntireFile);
    MH_AddHook(UFG_RVA(0x22C9C0), Hooks::StreamFileWrapper::ReadEntireFileAsync,    (void**)&Hooks::StreamFileWrapper::m_oReadEntireFileAsync);
    MH_AddHook(UFG_RVA(0x22C360), Hooks::DataStreamer::QueueStream,                 (void**)&Hooks::DataStreamer::m_oQueueStream);
    
    return 0;
}

int __stdcall DllMain(HMODULE m_Module, DWORD m_Reason, void* m_Reserved)
{
    if (m_Reason == DLL_PROCESS_ATTACH)
    {
        HANDLE m_Thread = CreateThread(0, 0, MainThread, m_Module, 0, 0);
        if (m_Thread && m_Thread != INVALID_HANDLE_VALUE)
            CloseHandle(m_Thread);
    }

    return 1;
}
