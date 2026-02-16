#pragma once
// Subset of ReClass.NET native plugin types needed for the compatibility layer.
// Based on the ReClass.NET NativeCore plugin interface.
// Only types required by the 8 supported exports are included (no debug types).

#include <cstdint>

#ifdef _WIN32
#define RC_CALLCONV __stdcall
#else
#define RC_CALLCONV
#endif

// -- Basic types ----------------------------------------------------------

using RC_Pointer = void*;
using RC_Size    = uint64_t;
using RC_UnicodeChar = char16_t;

// -- Enums ----------------------------------------------------------------

enum class ProcessAccess
{
    Read    = 0,
    Write   = 1,
    Full    = 2
};

enum class SectionProtection
{
    NoAccess  = 0,
    Read      = 1,
    Write     = 2,
    Execute   = 4,
    Guard     = 8
};

enum class SectionType
{
    Unknown = 0,
    Private = 1,
    Mapped  = 2,
    Image   = 3
};

enum class SectionCategory
{
    Unknown = 0,
    CODE    = 1,
    DATA    = 2,
    HEAP    = 3
};

enum class ControlRemoteProcessAction
{
    Suspend = 0,
    Resume  = 1,
    Terminate = 2
};

// -- Callback data structures ---------------------------------------------

#pragma pack(push, 1)

struct EnumerateProcessData
{
    RC_Size     Id;
    RC_UnicodeChar Name[260];
    RC_UnicodeChar Path[260];
};

struct EnumerateRemoteSectionData
{
    RC_Pointer        BaseAddress;
    RC_Size           Size;
    SectionType       Type;
    SectionCategory   Category;
    SectionProtection Protection;
    RC_UnicodeChar    Name[16];
    RC_UnicodeChar    ModulePath[260];
};

struct EnumerateRemoteModuleData
{
    RC_Pointer     BaseAddress;
    RC_Size        Size;
    RC_UnicodeChar Path[260];
};

#pragma pack(pop)

// -- Callback typedefs ----------------------------------------------------

using EnumerateProcessCallback        = void(RC_CALLCONV*)(EnumerateProcessData* data);
using EnumerateRemoteSectionsCallback  = void(RC_CALLCONV*)(EnumerateRemoteSectionData* data);
using EnumerateRemoteModulesCallback   = void(RC_CALLCONV*)(EnumerateRemoteModuleData* data);

// -- Function pointer typedefs for resolved exports -----------------------

using FnEnumerateProcesses = void(RC_CALLCONV*)(EnumerateProcessCallback callback);

using FnOpenRemoteProcess  = RC_Pointer(RC_CALLCONV*)(RC_Size id, ProcessAccess desiredAccess);

using FnIsProcessValid     = bool(RC_CALLCONV*)(RC_Pointer handle);

using FnCloseRemoteProcess = void(RC_CALLCONV*)(RC_Pointer handle);

using FnReadRemoteMemory   = bool(RC_CALLCONV*)(RC_Pointer handle,
                                                 RC_Pointer address,
                                                 RC_Pointer buffer,
                                                 int offset,
                                                 int size);

using FnWriteRemoteMemory  = bool(RC_CALLCONV*)(RC_Pointer handle,
                                                  RC_Pointer address,
                                                  RC_Pointer buffer,
                                                  int offset,
                                                  int size);

using FnEnumerateRemoteSectionsAndModules =
    void(RC_CALLCONV*)(RC_Pointer handle,
                       EnumerateRemoteSectionsCallback sectionCallback,
                       EnumerateRemoteModulesCallback  moduleCallback);

using FnControlRemoteProcess = void(RC_CALLCONV*)(RC_Pointer handle,
                                                    ControlRemoteProcessAction action);

// -- Resolved function table ----------------------------------------------

struct RcNetFunctions
{
    FnEnumerateProcesses                EnumerateProcesses               = nullptr;
    FnOpenRemoteProcess                 OpenRemoteProcess                = nullptr;
    FnIsProcessValid                    IsProcessValid                   = nullptr;
    FnCloseRemoteProcess                CloseRemoteProcess               = nullptr;
    FnReadRemoteMemory                  ReadRemoteMemory                 = nullptr;
    FnWriteRemoteMemory                 WriteRemoteMemory                = nullptr;
    FnEnumerateRemoteSectionsAndModules EnumerateRemoteSectionsAndModules = nullptr;
    FnControlRemoteProcess              ControlRemoteProcess             = nullptr;
};
