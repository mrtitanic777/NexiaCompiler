param (
    [string]$InputFile
)

$bytes = [System.IO.File]::ReadAllBytes($InputFile)

# The COFF Header starts at the beginning of an .obj file
# Machine Type is at offset 0 (2 bytes). 0x01F2 is PowerPC (Big Endian)
# We want to tell Windows it's 0x01F0 (PowerPC Little Endian) or 0x014C (x86) to trick it into opening.

function Swap32($pos) {
    $val = [BitConverter]::ToUInt32($bytes, $pos)
    $swapped = [System.Net.IPAddress]::HostToNetworkOrder([int]$val)
    $newBytes = [BitConverter]::GetBytes([uint32]$swapped)
    for($i=0; $i -lt 4; $i++) { $script:bytes[$pos + $i] = $newBytes[$i] }
}

# 1. Swap the Number of Symbols (Offset 12)
Swap32(12)

# 2. Swap the Pointer to Symbol Table (Offset 8)
Swap32(8)

# 3. Swap the Number of Sections (Offset 2) - 16-bit
$numSections = [BitConverter]::ToUInt16($bytes, 2)
# (Simplified for now, focusing on the main 32-bit offsets that crash dumpbin)

[System.IO.File]::WriteAllBytes($InputFile + ".le.obj", $bytes)
Write-Host "[*] Converted $InputFile to Little-Endian: $($InputFile).le.obj" -ForegroundColor Green