param(
  [Parameter(Mandatory=$true)][string]$Bin,     # Windows path to champsim exe
  [Parameter(Mandatory=$true)][string]$Trace,   # Windows path to ONE .xz trace (copied $Cores times)
  [Parameter(Mandatory=$true)][int]$Cores,
  [Parameter(Mandatory=$true)][string]$Log,      # Windows path for stdout redirect
  [Parameter(Mandatory=$true)][string]$Warmup,
  [Parameter(Mandatory=$true)][string]$Sim,
  [string]$Bypass   = "none",
  [string]$PfL1     = "no",
  [string]$PfL2     = "no",
  [string]$PfL3     = "no",
  [string]$Arch     = "glc",
  [string]$Affinity = "F0"   # hex, cores 4-7 = F0
)
$ErrorActionPreference = "Stop"
if (-not (Test-Path $Bin))   { Write-Output "BIN_NOT_FOUND: $Bin"; exit 3 }
if (-not (Test-Path $Trace)) { Write-Output "TRACE_NOT_FOUND: $Trace"; exit 3 }

$traces = @()
for ($i = 0; $i -lt $Cores; $i++) { $traces += $Trace }
$cmd = @('-warmup_instructions',$Warmup,'-simulation_instructions',$Sim,
         '--arch',$Arch,'--bypass',$Bypass,
         '--pf_l1',$PfL1,'--pf_l2',$PfL2,'--pf_l3',$PfL3,
         '-traces') + $traces

$sw = [System.Diagnostics.Stopwatch]::StartNew()
$p  = Start-Process -FilePath $Bin -ArgumentList $cmd -NoNewWindow -PassThru -RedirectStandardOutput $Log
try { $p.ProcessorAffinity = [IntPtr]([Convert]::ToInt64($Affinity,16)) } catch { Write-Output ("AFFINITY_FAIL: " + $_.Exception.Message) }
try { $p.PriorityClass = [System.Diagnostics.ProcessPriorityClass]::RealTime } catch { Write-Output ("PRIO_FAIL(fallback): " + $_.Exception.Message) }
$p.WaitForExit()
$sw.Stop()
Write-Output ("WALL_SECONDS=" + [math]::Round($sw.Elapsed.TotalSeconds,1))
Write-Output ("EXIT_CODE=" + $p.ExitCode)
