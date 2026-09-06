# Profiling the client

Tools for working out where the game has gone when it stops responding. Both
drive a running `Game.exe` from outside it and change nothing on disk.

Both must be run from 32-bit PowerShell, so that the Windows calls they make
return the game's own 32-bit registers and module list rather than the WOW64
ones:

```
C:\Windows\SysWOW64\WindowsPowerShell\v1.0\powershell.exe -NoProfile -ExecutionPolicy Bypass -File <script>
```

## Stack-D2Exit.ps1

Samples every thread, recording its call stack alongside the CPU time it has
accumulated, and reports the stacks from whichever half-second buckets the whole
process spent near zero CPU. A stall that consumes no CPU is one spent waiting,
and this names what it waited on.

```
... -File tools\profiling\Stack-D2Exit.ps1 -Seconds 60
```

Reproduce the stall while it runs. The report ranks threads by CPU burnt over
the whole run, so the game's main loop comes first.

Stacks are recovered by scanning the thread's stack memory for values that
decode as return addresses, because 32-bit syscalls all funnel through a single
WOW64 stub and the instruction pointer alone cannot tell one wait from another.
The scan is a heuristic: the frames it reports are real code addresses, but
stale ones left on the stack can appear between live frames, so read the set of
frames rather than trusting the order absolutely.

Addresses come out as `Module+0xRVA`. Resolve them against the matching DLL,
noting that a return address points *after* the call that is executing:

```
objdump -d -M intel --start-address=0x... --stop-address=0x... "Game\D2Client.dll"
```

## Set-StormAsyncRate.ps1

Reads, and optionally sets, the byte budget Storm paces its asynchronous archive
reads against. Reports the budget, the quantum and the resulting interval:

```
... -File tools\profiling\Set-StormAsyncRate.ps1            # report only
... -File tools\profiling\Set-StormAsyncRate.ps1 -Value 0   # disable pacing
```

The game sets this budget to 256KB/s at startup, which is what made Save and
Exit stall while it waited for outstanding reads to drain. BH now lifts it on
entering a game, so against a current build this reports a budget of zero; the
tool remains useful for confirming that, and for measuring what a given budget
costs.
