<#
  Stack-sampling profiler for the Diablo II client.

  Samples every thread's call stack by scanning its stack memory for values that
  look like return addresses, so a blocked thread can be attributed to whatever
  code blocked it. Needed because 32-bit syscalls all funnel through a single
  WOW64 stub, which leaves the instruction pointer alone unable to tell one wait
  from another.

    C:\Windows\SysWOW64\WindowsPowerShell\v1.0\powershell.exe -NoProfile `
      -ExecutionPolicy Bypass -File tools\profiling\Stack-D2Exit.ps1 -Seconds 60
#>
param(
  [string]$ProcessName = 'Game',
  [int]$Seconds = 60,
  [int]$Hz = 25,
  [double]$BucketSeconds = 0.5,
  # A bucket whose whole-process CPU is under this is treated as a freeze.
  [double]$IdleMsPerBucket = 50
)

if ([IntPtr]::Size -ne 4) {
  Write-Error "Run from 32-bit PowerShell: C:\Windows\SysWOW64\WindowsPowerShell\v1.0\powershell.exe"
  exit 1
}

$csharp = @'
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Runtime.InteropServices;

public class Mod {
  public string Name; public uint Base; public uint Size; public byte[] Image;
  public uint End { get { return Base + Size; } }
}

public class Sample {
  public double T; public uint Tid; public long Cpu; public uint[] Frames;
}

public static class StackSampler {
  [DllImport("kernel32.dll", SetLastError=true)] static extern IntPtr OpenProcess(uint a, bool inh, int pid);
  [DllImport("kernel32.dll")] static extern IntPtr OpenThread(uint a, bool inh, uint tid);
  [DllImport("kernel32.dll")] static extern uint SuspendThread(IntPtr h);
  [DllImport("kernel32.dll")] static extern int ResumeThread(IntPtr h);
  [DllImport("kernel32.dll")] static extern bool CloseHandle(IntPtr h);
  [DllImport("kernel32.dll")] static extern bool GetThreadTimes(IntPtr h, out long c, out long e, out long k, out long u);
  [DllImport("kernel32.dll")] static extern bool ReadProcessMemory(IntPtr h, IntPtr addr, byte[] buf, int size, out int read);

  [StructLayout(LayoutKind.Sequential)]
  public struct CONTEXT {
    public uint ContextFlags;
    public uint Dr0, Dr1, Dr2, Dr3, Dr6, Dr7;
    [MarshalAs(UnmanagedType.ByValArray, SizeConst=112)] public byte[] FloatSave;
    public uint SegGs, SegFs, SegEs, SegDs;
    public uint Edi, Esi, Ebx, Edx, Ecx, Eax;
    public uint Ebp, Eip, SegCs, EFlags, Esp, SegSs;
    [MarshalAs(UnmanagedType.ByValArray, SizeConst=512)] public byte[] ExtendedRegisters;
  }
  [DllImport("kernel32.dll")] static extern bool GetThreadContext(IntPtr h, ref CONTEXT c);

  const uint CONTEXT_CONTROL = 0x10001;
  const uint THREAD_ACCESS = 0x0002 | 0x0008 | 0x0040;
  const uint PROCESS_ACCESS = 0x0010 | 0x0400;
  const int STACK_BYTES = 32 * 1024;
  const int CHUNK = 4 * 1024;
  const int MAX_FRAMES = 20;

  static List<Mod> mods;
  static IntPtr hProc;

  public static void Init(int pid, string[] names, uint[] bases, uint[] sizes) {
    hProc = OpenProcess(PROCESS_ACCESS, false, pid);
    if (hProc == IntPtr.Zero) throw new Exception("OpenProcess failed: " + Marshal.GetLastWin32Error());
    mods = new List<Mod>();
    long budget = 192L * 1024 * 1024;
    for (int i = 0; i < names.Length; i++) {
      Mod m = new Mod();
      m.Name = names[i]; m.Base = bases[i]; m.Size = sizes[i];
      if (sizes[i] <= 32 * 1024 * 1024 && budget - sizes[i] > 0) {
        byte[] buf = new byte[sizes[i]];
        int got;
        // A partial read still yields usable code bytes for the low part of the image.
        ReadProcessMemory(hProc, (IntPtr)bases[i], buf, buf.Length, out got);
        if (got > 0) { m.Image = buf; budget -= sizes[i]; }
      }
      mods.Add(m);
    }
    mods.Sort(delegate(Mod a, Mod b) { return a.Base.CompareTo(b.Base); });
  }

  static Mod Find(uint addr) {
    int lo = 0, hi = mods.Count - 1;
    while (lo <= hi) {
      int mid = (lo + hi) / 2;
      if (addr < mods[mid].Base) hi = mid - 1;
      else if (addr >= mods[mid].End) lo = mid + 1;
      else return mods[mid];
    }
    return null;
  }

  // True when the bytes just before addr decode as a call, which is what makes
  // addr a plausible return address rather than incidental stack data.
  static bool IsReturnAddress(uint addr) {
    Mod m = Find(addr);
    if (m == null || m.Image == null) return false;
    uint off = addr - m.Base;
    if (off < 8 || off >= (uint)m.Image.Length) return false;
    byte[] im = m.Image;
    if (im[off - 5] == 0xE8) return true;
    for (int k = 2; k <= 7; k++) {
      if (im[off - k] == 0xFF && ((im[off - k + 1] >> 3) & 7) == 2) return true;
    }
    if (im[off - 7] == 0x9A) return true;
    return false;
  }

  public static List<Sample> Run(int pid, double seconds, int hz) {
    List<Sample> results = new List<Sample>();
    Dictionary<uint, long> prevCpu = new Dictionary<uint, long>();
    Process proc = Process.GetProcessById(pid);
    CONTEXT ctx = new CONTEXT();
    ctx.FloatSave = new byte[112];
    ctx.ExtendedRegisters = new byte[512];
    byte[] stack = new byte[STACK_BYTES];
    byte[] chunk = new byte[CHUNK];
    Stopwatch sw = Stopwatch.StartNew();
    int periodMs = Math.Max(1, 1000 / hz);

    while (sw.Elapsed.TotalSeconds < seconds) {
      proc.Refresh();
      foreach (ProcessThread pt in proc.Threads) {
        uint tid = (uint)pt.Id;
        IntPtr h = OpenThread(THREAD_ACCESS, false, tid);
        if (h == IntPtr.Zero) continue;
        try {
          long c, e, k, u;
          if (!GetThreadTimes(h, out c, out e, out k, out u)) continue;
          long cpu = k + u;
          long delta = prevCpu.ContainsKey(tid) ? cpu - prevCpu[tid] : 0;
          prevCpu[tid] = cpu;

          if (SuspendThread(h) == uint.MaxValue) continue;
          uint[] frames;
          try {
            ctx.ContextFlags = CONTEXT_CONTROL;
            if (!GetThreadContext(h, ref ctx)) continue;
            // ReadProcessMemory fails for the whole range if any part of it is
            // unmapped, and a shallow stack has less than STACK_BYTES left above
            // ESP. Read a chunk at a time and stop at the stack's end.
            int read = 0;
            while (read < STACK_BYTES) {
              int got;
              if (!ReadProcessMemory(hProc, (IntPtr)(ctx.Esp + (uint)read), chunk, CHUNK, out got) || got <= 0) break;
              Buffer.BlockCopy(chunk, 0, stack, read, got);
              read += got;
              if (got < CHUNK) break;
            }
            if (read < 4) {
              frames = new uint[] { ctx.Eip };
            } else {
              List<uint> f = new List<uint>(MAX_FRAMES);
              f.Add(ctx.Eip);
              uint last = 0;
              for (int off = 0; off + 4 <= read && f.Count < MAX_FRAMES; off += 4) {
                uint v = BitConverter.ToUInt32(stack, off);
                if (v == last) continue;
                if (IsReturnAddress(v)) { f.Add(v); last = v; }
              }
              frames = f.ToArray();
            }
          } finally { ResumeThread(h); }
          Sample s = new Sample();
          s.T = sw.Elapsed.TotalSeconds; s.Tid = tid; s.Cpu = delta; s.Frames = frames;
          results.Add(s);
        } finally { CloseHandle(h); }
      }
      System.Threading.Thread.Sleep(periodMs);
    }
    return results;
  }

  public static string Resolve(uint addr) {
    Mod m = Find(addr);
    if (m == null) return String.Format("?+0x{0:x}", addr);
    return String.Format("{0}+0x{1:x}", m.Name, addr - m.Base);
  }
}
'@
Add-Type -TypeDefinition $csharp

$proc = Get-Process -Name $ProcessName -ErrorAction SilentlyContinue | Select-Object -First 1
if (-not $proc) { Write-Error "Process '$ProcessName' not found."; exit 1 }

$names = @(); $bases = @(); $sizes = @()
foreach ($m in $proc.Modules) {
  $names += [IO.Path]::GetFileNameWithoutExtension($m.ModuleName)
  $bases += [uint32]$m.BaseAddress.ToInt32()
  $sizes += [uint32]$m.ModuleMemorySize
}
[StackSampler]::Init($proc.Id, $names, $bases, $sizes)
Write-Host "Sampling PID $($proc.Id), $($names.Count) modules, $Seconds s at $Hz Hz. Reproduce the slow exit now."

$samples = [StackSampler]::Run($proc.Id, [double]$Seconds, $Hz)
Write-Host "`n=== $($samples.Count) stack samples ===`n"

function Ms([double]$t) { [Math]::Round($t / 10000.0, 1) }
function Sig($frames, [int]$depth) {
  $r = @()
  foreach ($f in $frames) { $r += [StackSampler]::Resolve($f); if ($r.Count -ge $depth) { break } }
  return ($r -join ' < ')
}

$rows = $samples | ForEach-Object {
  [pscustomobject]@{
    B = [Math]::Floor($_.T / $BucketSeconds) * $BucketSeconds
    Tid = $_.Tid; Cpu = $_.Cpu; Frames = $_.Frames
  }
}

$bucketCpu = @{}
$rows | Group-Object B | ForEach-Object { $bucketCpu[[double]$_.Name] = Ms (($_.Group | Measure-Object Cpu -Sum).Sum) }

Write-Host "--- Timeline (whole-process CPU per bucket) ---"
$bucketCpu.Keys | Sort-Object | ForEach-Object {
  $flag = if ($bucketCpu[$_] -lt $IdleMsPerBucket) { '   <-- FROZEN' } else { '' }
  "{0,6}s  cpu={1,7} ms{2}" -f $_, $bucketCpu[$_], $flag
}

$hangBuckets = @($bucketCpu.Keys | Where-Object { $bucketCpu[$_] -lt $IdleMsPerBucket } | Sort-Object)
if (-not $hangBuckets) {
  Write-Host "`nNo frozen buckets seen - the hang was not captured in this window."
  exit 0
}
Write-Host "`nFrozen window: $($hangBuckets[0])s .. $($hangBuckets[-1])s ($($hangBuckets.Count) buckets)"

$hang = $rows | Where-Object { $hangBuckets -contains $_.B }

# Rank threads by CPU burnt over the whole run, so the game's main loop is
# obvious and its stack gets read first.
$cpuByTid = @{}
$rows | Group-Object Tid | ForEach-Object { $cpuByTid[$_.Name] = Ms (($_.Group | Measure-Object Cpu -Sum).Sum) }

Write-Host "`n--- Stacks during the freeze, by thread (busiest first, top 2 stacks each) ---"
$hang | Group-Object Tid | Sort-Object { $cpuByTid[$_.Name] } -Descending | ForEach-Object {
  Write-Host "`n  Thread $($_.Name)  ($($_.Count) samples, $($cpuByTid[$_.Name]) ms CPU over the whole run)"
  $_.Group | Group-Object { Sig $_.Frames 12 } | Sort-Object Count -Descending | Select-Object -First 2 | ForEach-Object {
    Write-Host "    x$($_.Count)"
    ($_.Name -split ' < ') | ForEach-Object { Write-Host "      $_" }
  }
}

Write-Host "`n--- Modules present on any stack during the freeze ---"
$hang | ForEach-Object { $_.Frames } | ForEach-Object { ([StackSampler]::Resolve($_)) -replace '\+.*','' } |
  Group-Object | Sort-Object Count -Descending | Select-Object -First 20 Count, Name | Format-Table -AutoSize
