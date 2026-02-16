// RcNetBridge -- in-process C# bridge for loading .NET ReClass.NET plugins.
//
// Called from C++ via ICLRRuntimeHost::ExecuteInDefaultAppDomain().
// The single entry point is Bridge.Initialize(string arg) where arg is:
//   "<hex_address_of_RcNetFunctions>|<plugin_dll_path>"
//
// The bridge:
//   1. Registers an AssemblyResolve handler that provides THIS assembly
//      when a plugin asks for "ReClassNET", so the stub types below satisfy
//      the plugin's type references.
//   2. Loads the plugin assembly and finds an ICoreProcessFunctions
//      implementation.
//   3. Creates [UnmanagedFunctionPointer] delegates wrapping each method.
//   4. Writes the native-callable function pointers into the RcNetFunctions
//      struct at the address provided by C++.

using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Runtime.InteropServices;

// ===========================================================================
// ReClass.NET stub types
// These mirror the subset of types from the ReClass.NET assembly that
// memory-reading plugins reference.  When the CLR resolves "ReClassNET"
// via our AssemblyResolve handler, it gets THIS assembly, and these types
// satisfy the plugin's type references.
//
// Types are placed in the exact namespaces used by the real ReClass.NET
// assembly so that plugins compiled against it resolve correctly.
// ===========================================================================

// --------------------------------------------------------------------------
// ReClassNET.Memory -- section enums (referenced by EnumerateRemoteSectionData)
// --------------------------------------------------------------------------
namespace ReClassNET.Memory
{
    public enum SectionProtection
    {
        NoAccess = 0,
        Read     = 1,
        Write    = 2,
        Execute  = 4,
        Guard    = 8
    }

    public enum SectionType
    {
        Unknown = 0,
        Private = 1,
        Mapped  = 2,
        Image   = 3
    }

    public enum SectionCategory
    {
        Unknown = 0,
        CODE    = 1,
        DATA    = 2,
        HEAP    = 3
    }
}

// --------------------------------------------------------------------------
// ReClassNET.Debugger -- debugger types (used by ICoreProcessFunctions)
// --------------------------------------------------------------------------
namespace ReClassNET.Debugger
{
    public enum DebugContinueStatus
    {
        Handled    = 0,
        NotHandled = 1
    }

    public enum HardwareBreakpointRegister
    {
        InvalidRegister = 0,
        Dr0 = 1,
        Dr1 = 2,
        Dr2 = 3,
        Dr3 = 4
    }

    public enum HardwareBreakpointTrigger
    {
        Execute = 0,
        Access  = 1,
        Write   = 2
    }

    public enum HardwareBreakpointSize
    {
        Size1 = 1,
        Size2 = 2,
        Size4 = 4,
        Size8 = 8
    }

    public struct ExceptionDebugInfo
    {
        public IntPtr ExceptionCode;
        public IntPtr ExceptionFlags;
        public IntPtr ExceptionAddress;
        public HardwareBreakpointRegister CausedBy;
        public RegisterInfo Registers;

        public struct RegisterInfo
        {
            public IntPtr Rax, Rbx, Rcx, Rdx;
            public IntPtr Rdi, Rsi, Rsp, Rbp, Rip;
            public IntPtr R8, R9, R10, R11, R12, R13, R14, R15;
        }
    }

    public struct DebugEvent
    {
        public DebugContinueStatus ContinueStatus;
        public IntPtr ProcessId;
        public IntPtr ThreadId;
        public ExceptionDebugInfo ExceptionInfo;
    }
}

// --------------------------------------------------------------------------
// ReClassNET.Core -- interface, enums, delegates, and data structs
// --------------------------------------------------------------------------
namespace ReClassNET.Core
{
    public enum ProcessAccess
    {
        Read  = 0,
        Write = 1,
        Full  = 2
    }

    public enum ControlRemoteProcessAction
    {
        Suspend   = 0,
        Resume    = 1,
        Terminate = 2
    }

    public struct EnumerateProcessData
    {
        public IntPtr Id;
        public string Name;
        public string Path;
    }

    public struct EnumerateRemoteSectionData
    {
        public IntPtr                          BaseAddress;
        public IntPtr                          Size;
        public ReClassNET.Memory.SectionType       Type;
        public ReClassNET.Memory.SectionCategory   Category;
        public ReClassNET.Memory.SectionProtection Protection;
        public string                          Name;
        public string                          ModulePath;
    }

    public struct EnumerateRemoteModuleData
    {
        public IntPtr BaseAddress;
        public IntPtr Size;
        public string Path;
    }

    public delegate void EnumerateProcessCallback(ref EnumerateProcessData data);
    public delegate void EnumerateRemoteSectionCallback(ref EnumerateRemoteSectionData data);
    public delegate void EnumerateRemoteModuleCallback(ref EnumerateRemoteModuleData data);

    public interface ICoreProcessFunctions
    {
        void   EnumerateProcesses(EnumerateProcessCallback callbackProcess);
        IntPtr OpenRemoteProcess(IntPtr pid, ProcessAccess desiredAccess);
        bool   IsProcessValid(IntPtr process);
        void   CloseRemoteProcess(IntPtr process);
        bool   ReadRemoteMemory(IntPtr process, IntPtr address, ref byte[] buffer, int offset, int size);
        bool   WriteRemoteMemory(IntPtr process, IntPtr address, ref byte[] buffer, int offset, int size);
        void   EnumerateRemoteSectionsAndModules(
                   IntPtr process,
                   EnumerateRemoteSectionCallback callbackSection,
                   EnumerateRemoteModuleCallback  callbackModule);
        void   ControlRemoteProcess(IntPtr process, ControlRemoteProcessAction action);

        // Debugger methods -- stubs required for interface compatibility
        bool AttachDebuggerToProcess(IntPtr id);
        void DetachDebuggerFromProcess(IntPtr id);
        bool AwaitDebugEvent(ref ReClassNET.Debugger.DebugEvent evt, int timeoutInMilliseconds);
        void HandleDebugEvent(ref ReClassNET.Debugger.DebugEvent evt);
        bool SetHardwareBreakpoint(IntPtr id, IntPtr address,
                 ReClassNET.Debugger.HardwareBreakpointRegister register,
                 ReClassNET.Debugger.HardwareBreakpointTrigger trigger,
                 ReClassNET.Debugger.HardwareBreakpointSize size,
                 bool set);
    }
}

// --------------------------------------------------------------------------
// ReClassNET.Memory -- RemoteProcess stub
// --------------------------------------------------------------------------
namespace ReClassNET.Memory
{
    public class RemoteProcess { }
}

// --------------------------------------------------------------------------
// ReClassNET.Logger -- ILogger stub
// --------------------------------------------------------------------------
namespace ReClassNET.Logger
{
    public interface ILogger { }
}

// --------------------------------------------------------------------------
// Stub types for IPluginHost properties
// --------------------------------------------------------------------------
namespace ReClassNET.Forms
{
    public class MainForm { }
}

namespace ReClassNET
{
    public class Settings { }
}

// --------------------------------------------------------------------------
// ReClassNET.Plugins
// --------------------------------------------------------------------------
namespace ReClassNET.Plugins
{
    public abstract class Plugin : IDisposable
    {
        public virtual bool Initialize(IPluginHost host) { return true; }
        public virtual void Terminate() { }
        public virtual void Dispose() { }
    }

    public interface IPluginHost
    {
        ReClassNET.Forms.MainForm MainWindow { get; }
        System.Resources.ResourceManager Resources { get; }
        ReClassNET.Memory.RemoteProcess Process { get; }
        ReClassNET.Logger.ILogger Logger { get; }
        ReClassNET.Settings Settings { get; }
    }
}

// ===========================================================================
// Bridge
// ===========================================================================

namespace RcNetBridge
{
    internal class StubPluginHost : ReClassNET.Plugins.IPluginHost
    {
        public ReClassNET.Forms.MainForm MainWindow => null;
        public System.Resources.ResourceManager Resources => null;
        public ReClassNET.Memory.RemoteProcess Process => null;
        public ReClassNET.Logger.ILogger Logger => null;
        public ReClassNET.Settings Settings => null;
    }

    public class Bridge
    {
        // -- Persistent state (static so it survives after Initialize returns) --

        private static ReClassNET.Core.ICoreProcessFunctions s_functions;
        private static readonly List<Delegate> s_pinned = new List<Delegate>();

        // -- Entry point called from C++ --------------------------------------

        /// <summary>
        /// Called by ICLRRuntimeHost::ExecuteInDefaultAppDomain.
        /// arg = "&lt;hex_address_of_RcNetFunctions&gt;|&lt;plugin_dll_path&gt;"
        /// Returns 0 on success, non-zero error code on failure.
        /// </summary>
        public static int Initialize(string arg)
        {
            try
            {
                int sep = arg.IndexOf('|');
                if (sep < 0) return 1; // bad arg

                long ptrValue = long.Parse(arg.Substring(0, sep), NumberStyles.HexNumber);
                IntPtr funcTablePtr = new IntPtr(ptrValue);
                string pluginPath = arg.Substring(sep + 1);

                // Set up assembly resolution
                string pluginDir = Path.GetDirectoryName(pluginPath) ?? ".";
                string parentDir = Path.GetDirectoryName(pluginDir);

                AppDomain.CurrentDomain.AssemblyResolve += (sender, resolveArgs) =>
                {
                    string asmName = new AssemblyName(resolveArgs.Name).Name;

                    // Provide our own assembly as the "ReClass.NET" stub
                    if (string.Equals(asmName, "ReClass.NET", StringComparison.OrdinalIgnoreCase))
                        return typeof(Bridge).Assembly;

                    // Search plugin directory and parent for other dependencies
                    string dllName = asmName + ".dll";
                    foreach (string dir in new[] { pluginDir, parentDir })
                    {
                        if (dir == null) continue;
                        string path = Path.Combine(dir, dllName);
                        if (File.Exists(path))
                            return Assembly.LoadFrom(path);
                    }
                    return null;
                };

                // Load plugin and find ICoreProcessFunctions
                if (!LoadPlugin(pluginPath))
                    return 2; // no implementation found

                // Write function pointers
                WriteFunctionPointers(funcTablePtr);
                return 0;
            }
            catch (Exception ex) when (ex is ReflectionTypeLoadException || ex is FileNotFoundException)
            {
                return 3;
            }
            catch
            {
                return 4;
            }
        }

        // -- Plugin loading ---------------------------------------------------

        private static bool LoadPlugin(string pluginPath)
        {
            Assembly asm = Assembly.LoadFrom(pluginPath);

            // Find a concrete type that implements ICoreProcessFunctions.
            // ReClass.NET plugins typically extend Plugin and directly
            // implement ICoreProcessFunctions on the same class.
            foreach (Type type in asm.GetExportedTypes())
            {
                if (type.IsAbstract || type.IsInterface) continue;

                Type iface = type.GetInterfaces().FirstOrDefault(i =>
                    i.FullName == "ReClassNET.Core.ICoreProcessFunctions");
                if (iface == null) continue;

                object instance = Activator.CreateInstance(type);

                // Try calling Initialize() but don't fail if it throws --
                // plugins use it for UI integration with the host app,
                // which we can't fully provide. The process functions
                // (ReadRemoteMemory, etc.) work without it.
                try
                {
                    MethodInfo init = type.GetMethod("Initialize",
                        BindingFlags.Public | BindingFlags.Instance,
                        null, new[] { typeof(ReClassNET.Plugins.IPluginHost) }, null);
                    if (init != null)
                        init.Invoke(instance, new object[] { new StubPluginHost() });
                }
                catch { }

                s_functions = (ReClassNET.Core.ICoreProcessFunctions)instance;
                return true;
            }

            return false;
        }

        // -- Native-callable delegate types -----------------------------------
        // These match the C++ RcNetFunctions struct field order exactly.
        // On x64 Windows all calling conventions collapse to the Microsoft
        // x64 ABI, so StdCall is used for documentation / x86 correctness.

        [UnmanagedFunctionPointer(CallingConvention.StdCall)]
        delegate void DelEnumProcesses(IntPtr callback);

        [UnmanagedFunctionPointer(CallingConvention.StdCall)]
        delegate IntPtr DelOpenRemoteProcess(ulong id, int access);

        [UnmanagedFunctionPointer(CallingConvention.StdCall)]
        [return: MarshalAs(UnmanagedType.I1)]
        delegate bool DelIsProcessValid(IntPtr handle);

        [UnmanagedFunctionPointer(CallingConvention.StdCall)]
        delegate void DelCloseRemoteProcess(IntPtr handle);

        [UnmanagedFunctionPointer(CallingConvention.StdCall)]
        [return: MarshalAs(UnmanagedType.I1)]
        delegate bool DelReadRemoteMemory(IntPtr handle, IntPtr address,
            IntPtr buffer, int offset, int size);

        [UnmanagedFunctionPointer(CallingConvention.StdCall)]
        [return: MarshalAs(UnmanagedType.I1)]
        delegate bool DelWriteRemoteMemory(IntPtr handle, IntPtr address,
            IntPtr buffer, int offset, int size);

        [UnmanagedFunctionPointer(CallingConvention.StdCall)]
        delegate void DelEnumSectionsAndModules(IntPtr handle,
            IntPtr sectionCallback, IntPtr moduleCallback);

        [UnmanagedFunctionPointer(CallingConvention.StdCall)]
        delegate void DelControlRemoteProcess(IntPtr handle, int action);

        // Callback delegate types -- these point into C++ and are called by us.
        [UnmanagedFunctionPointer(CallingConvention.StdCall)]
        delegate void NativeProcessCallback(IntPtr data);

        [UnmanagedFunctionPointer(CallingConvention.StdCall)]
        delegate void NativeSectionCallback(IntPtr data);

        [UnmanagedFunctionPointer(CallingConvention.StdCall)]
        delegate void NativeModuleCallback(IntPtr data);

        // -- Write function pointers to the C++ struct ------------------------

        private static void WriteFunctionPointers(IntPtr funcTable)
        {
            // RcNetFunctions layout: 8 consecutive function pointers.
            int i = 0;
            WriteSlot(funcTable, i++, Pin<DelEnumProcesses>(EnumProcessesImpl));
            WriteSlot(funcTable, i++, Pin<DelOpenRemoteProcess>(OpenProcessImpl));
            WriteSlot(funcTable, i++, Pin<DelIsProcessValid>(IsProcessValidImpl));
            WriteSlot(funcTable, i++, Pin<DelCloseRemoteProcess>(CloseProcessImpl));
            WriteSlot(funcTable, i++, Pin<DelReadRemoteMemory>(ReadMemoryImpl));
            WriteSlot(funcTable, i++, Pin<DelWriteRemoteMemory>(WriteMemoryImpl));
            WriteSlot(funcTable, i++, Pin<DelEnumSectionsAndModules>(EnumSectionsModulesImpl));
            WriteSlot(funcTable, i++, Pin<DelControlRemoteProcess>(ControlProcessImpl));
        }

        private static IntPtr Pin<T>(T del) where T : class
        {
            Delegate d = del as Delegate;
            s_pinned.Add(d);  // prevent GC
            return Marshal.GetFunctionPointerForDelegate(d);
        }

        private static void WriteSlot(IntPtr table, int index, IntPtr value)
        {
            Marshal.WriteIntPtr(table, index * IntPtr.Size, value);
        }

        // -- Implementation methods -------------------------------------------

        // -- EnumerateProcesses --
        // C++ passes a native callback; we call the plugin, convert each
        // managed EnumerateProcessData to the packed native layout, and
        // forward to the native callback.

        private static void EnumProcessesImpl(IntPtr nativeCallbackPtr)
        {
            try
            {
                if (s_functions == null || nativeCallbackPtr == IntPtr.Zero) return;

                NativeProcessCallback nativeCb =
                    Marshal.GetDelegateForFunctionPointer<NativeProcessCallback>(nativeCallbackPtr);

                // Native layout (pack=1): uint64 Id + char16[260] Name + char16[260] Path
                const int kStructSize = 8 + 520 + 520; // 1048 bytes

                s_functions.EnumerateProcesses(
                    (ref ReClassNET.Core.EnumerateProcessData data) =>
                {
                    IntPtr mem = Marshal.AllocHGlobal(kStructSize);
                    try
                    {
                        // Zero-fill
                        byte[] zeros = new byte[kStructSize];
                        Marshal.Copy(zeros, 0, mem, kStructSize);

                        // Id (8 bytes at offset 0)
                        Marshal.WriteInt64(mem, 0, data.Id.ToInt64());

                        // Name (char16[260] at offset 8)
                        if (data.Name != null)
                        {
                            char[] chars = data.Name.ToCharArray();
                            int count = Math.Min(chars.Length, 259);
                            Marshal.Copy(chars, 0, new IntPtr(mem.ToInt64() + 8), count);
                        }

                        // Path (char16[260] at offset 528)
                        if (data.Path != null)
                        {
                            char[] chars = data.Path.ToCharArray();
                            int count = Math.Min(chars.Length, 259);
                            Marshal.Copy(chars, 0, new IntPtr(mem.ToInt64() + 528), count);
                        }

                        nativeCb(mem);
                    }
                    finally
                    {
                        Marshal.FreeHGlobal(mem);
                    }
                });
            }
            catch { /* swallow -- don't crash the host process */ }
        }

        // -- OpenRemoteProcess --
        private static IntPtr OpenProcessImpl(ulong id, int access)
        {
            try
            {
                if (s_functions == null) return IntPtr.Zero;
                return s_functions.OpenRemoteProcess(
                    new IntPtr((long)id),
                    (ReClassNET.Core.ProcessAccess)access);
            }
            catch { return IntPtr.Zero; }
        }

        // -- IsProcessValid --
        private static bool IsProcessValidImpl(IntPtr handle)
        {
            try
            {
                if (s_functions == null) return false;
                return s_functions.IsProcessValid(handle);
            }
            catch { return false; }
        }

        // -- CloseRemoteProcess --
        private static void CloseProcessImpl(IntPtr handle)
        {
            try { s_functions?.CloseRemoteProcess(handle); }
            catch { }
        }

        // -- ReadRemoteMemory --
        // C++ provides a native buffer pointer. We read into a managed array
        // via the plugin's interface, then copy to the native buffer.
        private static bool ReadMemoryImpl(IntPtr handle, IntPtr address,
            IntPtr buffer, int offset, int size)
        {
            try
            {
                if (s_functions == null || size <= 0) return false;

                byte[] managed = new byte[size];
                bool ok = s_functions.ReadRemoteMemory(
                    handle, address, ref managed, 0, size);

                if (ok)
                    Marshal.Copy(managed, 0, new IntPtr(buffer.ToInt64() + offset), size);

                return ok;
            }
            catch { return false; }
        }

        // -- WriteRemoteMemory --
        private static bool WriteMemoryImpl(IntPtr handle, IntPtr address,
            IntPtr buffer, int offset, int size)
        {
            try
            {
                if (s_functions == null || size <= 0) return false;

                byte[] managed = new byte[size];
                Marshal.Copy(new IntPtr(buffer.ToInt64() + offset), managed, 0, size);

                return s_functions.WriteRemoteMemory(
                    handle, address, ref managed, 0, size);
            }
            catch { return false; }
        }

        // -- EnumerateRemoteSectionsAndModules --
        private static void EnumSectionsModulesImpl(IntPtr handle,
            IntPtr sectionCallbackPtr, IntPtr moduleCallbackPtr)
        {
            try
            {
                if (s_functions == null) return;

                // Section callback -- forward to native
                // Native layout (pack=1): RC_Pointer Base(8) + RC_Size Size(8) +
                //   SectionType(4) + SectionCategory(4) + SectionProtection(4) +
                //   char16 Name[16](32) + char16 ModulePath[260](520) = 580 bytes
                NativeSectionCallback nativeSectionCb = (sectionCallbackPtr != IntPtr.Zero)
                    ? Marshal.GetDelegateForFunctionPointer<NativeSectionCallback>(sectionCallbackPtr)
                    : null;

                // Module callback -- forward to native
                // Native layout (pack=1): RC_Pointer Base(8) + RC_Size Size(8) +
                //   char16 Path[260](520) = 536 bytes
                NativeModuleCallback nativeModuleCb = (moduleCallbackPtr != IntPtr.Zero)
                    ? Marshal.GetDelegateForFunctionPointer<NativeModuleCallback>(moduleCallbackPtr)
                    : null;

                s_functions.EnumerateRemoteSectionsAndModules(handle,
                    // Section callback
                    (ref ReClassNET.Core.EnumerateRemoteSectionData sdata) =>
                    {
                        if (nativeSectionCb == null) return;

                        const int kSize = 8 + 8 + 4 + 4 + 4 + 32 + 520; // 580
                        IntPtr mem = Marshal.AllocHGlobal(kSize);
                        try
                        {
                            byte[] z = new byte[kSize];
                            Marshal.Copy(z, 0, mem, kSize);

                            Marshal.WriteInt64(mem, 0, sdata.BaseAddress.ToInt64());
                            Marshal.WriteInt64(mem, 8, sdata.Size.ToInt64());
                            Marshal.WriteInt32(mem, 16, (int)sdata.Type);
                            Marshal.WriteInt32(mem, 20, (int)sdata.Category);
                            Marshal.WriteInt32(mem, 24, (int)sdata.Protection);

                            if (sdata.Name != null)
                            {
                                char[] c = sdata.Name.ToCharArray();
                                Marshal.Copy(c, 0, new IntPtr(mem.ToInt64() + 28),
                                    Math.Min(c.Length, 15));
                            }
                            if (sdata.ModulePath != null)
                            {
                                char[] c = sdata.ModulePath.ToCharArray();
                                Marshal.Copy(c, 0, new IntPtr(mem.ToInt64() + 60),
                                    Math.Min(c.Length, 259));
                            }

                            nativeSectionCb(mem);
                        }
                        finally { Marshal.FreeHGlobal(mem); }
                    },
                    // Module callback
                    (ref ReClassNET.Core.EnumerateRemoteModuleData mdata) =>
                    {
                        if (nativeModuleCb == null) return;

                        const int kSize = 8 + 8 + 520; // 536
                        IntPtr mem = Marshal.AllocHGlobal(kSize);
                        try
                        {
                            byte[] z = new byte[kSize];
                            Marshal.Copy(z, 0, mem, kSize);

                            Marshal.WriteInt64(mem, 0, mdata.BaseAddress.ToInt64());
                            Marshal.WriteInt64(mem, 8, mdata.Size.ToInt64());

                            if (mdata.Path != null)
                            {
                                char[] c = mdata.Path.ToCharArray();
                                Marshal.Copy(c, 0, new IntPtr(mem.ToInt64() + 16),
                                    Math.Min(c.Length, 259));
                            }

                            nativeModuleCb(mem);
                        }
                        finally { Marshal.FreeHGlobal(mem); }
                    });
            }
            catch { }
        }

        // -- ControlRemoteProcess --
        private static void ControlProcessImpl(IntPtr handle, int action)
        {
            try
            {
                s_functions?.ControlRemoteProcess(handle,
                    (ReClassNET.Core.ControlRemoteProcessAction)action);
            }
            catch { }
        }
    }
}
