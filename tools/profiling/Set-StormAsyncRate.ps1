<#
  Reads, and optionally changes, Storm's asynchronous read rate limiter in a
  running Diablo II client.

  Storm paces async MPQ reads to a fixed bytes-per-second budget, servicing one
  quantum of bytes per (quantum * 1000 / rate) milliseconds. Fog initialises it
  to 256 KB/s, which on 1.13c makes the client's exit path wait out the limiter
  while it drains outstanding graphics loads.

  Setting the rate to 0 takes Storm's "no pacing configured" branch and disables
  the limiter outright. Nothing else in Storm reads this variable.

    C:\Windows\SysWOW64\WindowsPowerShell\v1.0\powershell.exe -NoProfile `
      -ExecutionPolicy Bypass -File tools\profiling\Set-StormAsyncRate.ps1            # read
    ... -File tools\profiling\Set-StormAsyncRate.ps1 -Value 0                         # disable
#>
param(
  [string]$ProcessName = 'Game',
  # Bytes per second. 0 disables pacing. Omit to only report the current value.
  [string]$Value
)

if ([IntPtr]::Size -ne 4) {
  Write-Error "Run from 32-bit PowerShell: C:\Windows\SysWOW64\WindowsPowerShell\v1.0\powershell.exe"
  exit 1
}

# Storm.dll RVAs, stable across the Storm shipped with 1.13c/1.13d.
$RVA_RATE    = 0x53124   # bytes per second budget, written only by Storm ord 284
$RVA_QUANTUM = 0x51a00   # bytes serviced per interval, written only by Storm ord 285

Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class PM {
  [DllImport("kernel32.dll", SetLastError=true)] public static extern IntPtr OpenProcess(uint a, bool i, int p);
  [DllImport("kernel32.dll", SetLastError=true)] public static extern bool ReadProcessMemory(IntPtr h, IntPtr a, byte[] b, int s, out int r);
  [DllImport("kernel32.dll", SetLastError=true)] public static extern bool WriteProcessMemory(IntPtr h, IntPtr a, byte[] b, int s, out int w);
  [DllImport("kernel32.dll")] public static extern bool CloseHandle(IntPtr h);
  public const uint ACCESS = 0x0008 | 0x0010 | 0x0020 | 0x0400; // VM_OPERATION | VM_READ | VM_WRITE | QUERY_INFORMATION
}
'@

$proc = Get-Process -Name $ProcessName -ErrorAction SilentlyContinue | Select-Object -First 1
if (-not $proc) { Write-Error "Process '$ProcessName' not found."; exit 1 }

$storm = $proc.Modules | Where-Object { $_.ModuleName -ieq 'Storm.dll' } | Select-Object -First 1
if (-not $storm) { Write-Error "Storm.dll not loaded in PID $($proc.Id)."; exit 1 }
$base = [uint32]$storm.BaseAddress.ToInt32()

$h = [PM]::OpenProcess([PM]::ACCESS, $false, $proc.Id)
if ($h -eq [IntPtr]::Zero) { Write-Error "OpenProcess failed ($([ComponentModel.Win32Exception]::new([Runtime.InteropServices.Marshal]::GetLastWin32Error()).Message))"; exit 1 }

function Read-U32([uint32]$rva) {
  $b = New-Object byte[] 4; $r = 0
  if (-not [PM]::ReadProcessMemory($h, [IntPtr]($base + $rva), $b, 4, [ref]$r)) { throw "read +0x$('{0:x}' -f $rva) failed" }
  return [BitConverter]::ToUInt32($b, 0)
}
function Write-U32([uint32]$rva, [uint32]$v) {
  $b = [BitConverter]::GetBytes($v); $w = 0
  if (-not [PM]::WriteProcessMemory($h, [IntPtr]($base + $rva), $b, 4, [ref]$w)) { throw "write +0x$('{0:x}' -f $rva) failed" }
}
function Show([string]$label) {
  $rate = Read-U32 $RVA_RATE
  $quantum = Read-U32 $RVA_QUANTUM
  if ($rate -eq 0) {
    "{0,-8} rate=0 (pacing disabled)   quantum={1} bytes" -f $label, $quantum
  } else {
    $interval = [Math]::Floor(($quantum * 1000) / $rate)
    "{0,-8} rate={1} B/s ({2} KB/s)   quantum={3} bytes   -> {4} ms per quantum" -f `
      $label, $rate, [Math]::Round($rate / 1KB), $quantum, $interval
  }
}

Write-Host ("Storm.dll base = 0x{0:x} in PID {1}" -f $base, $proc.Id)
Show 'before:'

if ($PSBoundParameters.ContainsKey('Value')) {
  $v = if ($Value -match '^0[xX]') { [Convert]::ToUInt32($Value.Substring(2), 16) } else { [uint32]$Value }
  Write-U32 $RVA_RATE $v
  Show 'after: '
  Write-Host "`nNow do a Save and Exit from a level that normally hangs, and compare."
  Write-Host "This change is in memory only - restarting the game restores the default."
}

[void][PM]::CloseHandle($h)
