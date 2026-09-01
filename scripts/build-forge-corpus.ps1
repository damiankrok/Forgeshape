<#
.SYNOPSIS
    Builds and verifies the permanent `.forge` v1 golden corpus.

.DESCRIPTION
    Writes testdata/forge/v1/ from the format specification in
    DATA_PACKAGE_SPEC.md and reports the SHA-256 of every fixture.

    This is a SECOND, INDEPENDENT implementation of the `.forge` v1 encoder.
    That is the whole point of it. The C++ codec's own self-tests
    (FSR1A-01..15) can prove that it decodes what it encodes, but only a
    separate implementation written from the specification can show that the
    specification is what the encoder actually implements — which is the claim
    "a `.forge` file is portable to another compatible ForgeShape installation"
    rests on. The two agree when the digests this script prints match the ones
    forgeshape_project_selftest.cpp asserts and DATA_PACKAGE_SPEC.md records.

    Nothing here touches a device, an emulator or adb. It needs no Android
    tooling and reads no repository source: it works from the specification and
    the two canonical documents alone.

    The two canonical documents are the same ones canonicalConstructionDocument()
    and canonicalSculptDocument() build in the C++ self-test. Every number in
    them is an exact binary fraction, so neither implementation has a rounding
    argument to make.

.PARAMETER OutputDirectory
    Where the corpus is written. Defaults to testdata/forge/v1 under the repo.

.PARAMETER VerifyOnly
    Reports the digests of the fixtures already on disk without rewriting them.
#>
[CmdletBinding()]
param(
    [string] $OutputDirectory,
    [switch] $VerifyOnly
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
if (-not $OutputDirectory) {
    $OutputDirectory = Join-Path $root 'testdata\forge\v1'
}

# ---------------------------------------------------------------------------
# Explicit little-endian primitives
# ---------------------------------------------------------------------------
#
# Every field goes through BitConverter and is then normalised to
# least-significant-byte-first EXPLICITLY, so the encoding is a property of the
# format rather than of the host this script happens to run on. Reproducing
# binary64 bit patterns by hand would be a second bug surface for no gain, and
# doing the integers a different way from the scalars would be two conventions
# where the format has one.

function New-ByteBuffer { , (New-Object System.Collections.Generic.List[byte]) }

function Add-Bytes {
    param($Buffer, [byte[]] $Value)
    foreach ($b in $Value) { $Buffer.Add($b) }
}

function ConvertTo-LittleEndian {
    param([byte[]] $Bytes)
    if (-not [System.BitConverter]::IsLittleEndian) { [array]::Reverse($Bytes) }
    return $Bytes
}

function Add-U8 { param($Buffer, [int] $Value) $Buffer.Add([byte]($Value -band 0xFF)) }

function Add-U16 {
    param($Buffer, [int] $Value)
    Add-Bytes $Buffer (ConvertTo-LittleEndian ([System.BitConverter]::GetBytes([uint16] $Value)))
}

function Add-U32 {
    param($Buffer, [uint32] $Value)
    Add-Bytes $Buffer (ConvertTo-LittleEndian ([System.BitConverter]::GetBytes([uint32] $Value)))
}

function Add-U64 {
    param($Buffer, [uint64] $Value)
    Add-Bytes $Buffer (ConvertTo-LittleEndian ([System.BitConverter]::GetBytes([uint64] $Value)))
}

function Add-F32 {
    param($Buffer, [float] $Value)
    Add-Bytes $Buffer (ConvertTo-LittleEndian ([System.BitConverter]::GetBytes([single] $Value)))
}

function Add-F64 {
    param($Buffer, [double] $Value)
    Add-Bytes $Buffer (ConvertTo-LittleEndian ([System.BitConverter]::GetBytes([double] $Value)))
}

function Add-Ascii {
    param($Buffer, [string] $Text)
    Add-Bytes $Buffer ([System.Text.Encoding]::ASCII.GetBytes($Text))
}

# CRC-32/ISO-HDLC: poly 0x04C11DB7, reflected 0xEDB88320, init 0xFFFFFFFF,
# refin/refout true, xorout 0xFFFFFFFF. Bitwise, exactly as the C++ side does
# it, so the two implementations share the constants and nothing else.
#
# Carried in a signed 64-bit accumulator and masked back to 32 bits at every
# step. Windows PowerShell parses 0xFFFFFFFF as a signed Int32 (-1) and has no
# unsigned bitwise operators, so a literal 32-bit register here would be wrong
# in a way that still produced plausible-looking output.
function Get-Crc32 {
    param([byte[]] $Bytes)
    [long] $crc = 0xFFFFFFFFL
    foreach ($b in $Bytes) {
        $crc = $crc -bxor ([long] $b)
        for ($bit = 0; $bit -lt 8; $bit++) {
            $shifted = ($crc -shr 1) -band 0x7FFFFFFFL
            if (($crc -band 1L) -ne 0) {
                $crc = $shifted -bxor 0xEDB88320L
            } else {
                $crc = $shifted
            }
        }
    }
    return [uint32] (($crc -bxor 0xFFFFFFFFL) -band 0xFFFFFFFFL)
}

# ---------------------------------------------------------------------------
# The envelope
# ---------------------------------------------------------------------------

function New-Section {
    param([string] $Tag, [int] $SectionVersion, [bool] $Required, [byte[]] $Payload)
    $section = New-ByteBuffer
    Add-Ascii $section $Tag
    Add-U16 $section $SectionVersion
    Add-U16 $section $(if ($Required) { 1 } else { 0 })
    Add-U64 $section ([uint64] $Payload.Length)
    Add-U32 $section (Get-Crc32 $Payload)
    Add-U32 $section 0
    Add-Bytes $section $Payload
    return $section.ToArray()
}

function New-ForgeFile {
    param([int] $ProjectKind, [byte[][]] $Sections, [int] $HeaderFlags)
    $total = 28
    foreach ($s in $Sections) { $total += $s.Length }

    $file = New-ByteBuffer
    Add-Ascii $file 'FORGESH1'
    Add-U16 $file 1                       # major
    Add-U16 $file 0                       # minor
    Add-U16 $file 28                      # headerBytes
    Add-U8  $file $ProjectKind
    Add-U8  $file $HeaderFlags
    Add-U32 $file ([uint32] $Sections.Count)
    Add-U64 $file ([uint64] $total)
    foreach ($s in $Sections) { Add-Bytes $file $s }
    return $file.ToArray()
}

# ---------------------------------------------------------------------------
# The two canonical documents
# ---------------------------------------------------------------------------

# SCNE v1: bodyCount, nextObjectId, activeObjectId, then per body an ObjectId
# and nine binary64 placement values (position XYZ, rotation XYZ, scale XYZ).
function New-ScenePayload {
    param($Bodies, [uint64] $NextObjectId, [uint64] $ActiveObjectId)
    $p = New-ByteBuffer
    Add-U32 $p ([uint32] $Bodies.Count)
    Add-U64 $p $NextObjectId
    Add-U64 $p $ActiveObjectId
    foreach ($body in $Bodies) {
        Add-U64 $p ([uint64] $body.ObjectId)
        foreach ($value in $body.Transform) { Add-F64 $p $value }
    }
    return $p.ToArray()
}

# CONS v1: bodyCount, then per body an ObjectId, the active primitive's FILE
# code, all six remembered parameter sets in canonical order, and the feature
# list.
function New-ConstructionPayload {
    param($Bodies)
    $p = New-ByteBuffer
    Add-U32 $p ([uint32] $Bodies.Count)
    foreach ($body in $Bodies) {
        Add-U64 $p ([uint64] $body.ObjectId)
        Add-U8  $p $body.PrimitiveCode
        foreach ($value in $body.Parameters) { Add-F64 $p $value }
        # Re-wrapped with @() before anything asks for its Count. PowerShell
        # unwraps a one-element array on return, and a bare PSCustomObject has
        # no usable Count — which silently wrote a feature COUNT of zero beside
        # a feature record that was there. The file still had the right length,
        # so only a byte-for-byte comparison against the C++ encoder found it.
        $features = @($body.Features)
        Add-U32 $p ([uint32] $features.Count)
        foreach ($feature in $features) {
            Add-U32 $p ([uint32] $feature.LocalFeatureId)
            Add-U8  $p $feature.KindCode
        }
    }
    return $p.ToArray()
}

# SCUL v1: entryCount, then per entry an ObjectId, a flags byte, the two
# counts, the local float32 positions and the index buffer.
function New-SculptPayload {
    param($Entries)
    $p = New-ByteBuffer
    Add-U32 $p ([uint32] $Entries.Count)
    foreach ($entry in $Entries) {
        Add-U64 $p ([uint64] $entry.ObjectId)
        $flags = 0
        if ($entry.RenderBothSides) { $flags = $flags -bor 1 }
        if ($entry.SourceStale) { $flags = $flags -bor 2 }
        if ($entry.HasEdits) { $flags = $flags -bor 4 }
        Add-U8  $p $flags
        Add-U32 $p ([uint32] ($entry.Positions.Count / 3))
        Add-U32 $p ([uint32] $entry.Indices.Count)
        foreach ($value in $entry.Positions) { Add-F32 $p $value }
        foreach ($index in $entry.Indices) { Add-U32 $p ([uint32] $index) }
    }
    return $p.ToArray()
}

# IMPT v1: entryCount, then per entry an ObjectId, the name's length and bytes,
# the three counts, the local float32 positions, the float32 normals, the index
# buffer, and the submesh ranges with their flags byte.
#
# This is the first section that carries GEOMETRY as project truth. A
# Construction Body's mesh is regenerated from its parameters on load; an
# imported object has no parameters and no source file to go back to, so losing
# these arrays would lose the object.
function New-ImportedPayload {
    param($Entries)
    $p = New-ByteBuffer
    Add-U32 $p ([uint32] $Entries.Count)
    foreach ($entry in $Entries) {
        Add-U64 $p ([uint64] $entry.ObjectId)
        # UTF-8, no terminator and no fixed-width padding, so the same name is
        # always the same bytes. The corpus names are ASCII, which UTF-8 encodes
        # identically -- but the encoding is stated rather than assumed.
        $nameBytes = [System.Text.Encoding]::UTF8.GetBytes($entry.Name)
        Add-U16 $p $nameBytes.Length
        Add-Bytes $p $nameBytes
        Add-U32 $p ([uint32] ($entry.Positions.Count / 3))
        Add-U32 $p ([uint32] $entry.Indices.Count)
        Add-U32 $p ([uint32] $entry.Batches.Count)
        foreach ($value in $entry.Positions) { Add-F32 $p $value }
        foreach ($value in $entry.Normals) { Add-F32 $p $value }
        foreach ($index in $entry.Indices) { Add-U32 $p ([uint32] $index) }
        foreach ($batch in $entry.Batches) {
            Add-U32 $p ([uint32] $batch.FirstIndex)
            Add-U32 $p ([uint32] $batch.IndexCount)
            Add-U8  $p $(if ($batch.DoubleSided) { 1 } else { 0 })
        }
    }
    return $p.ToArray()
}

function New-PrimitiveSourceFeature {
    return @([pscustomobject]@{ LocalFeatureId = 1; KindCode = 1 })
}

# The one Imported Mesh every imported fixture carries.
#
# Four vertices, two submeshes with DIFFERENT doubleSided answers, and every
# number an exact binary fraction, so neither implementation has a rounding
# argument to make.
function New-CanonicalImportedEntry {
    param([int] $ObjectId)
    return [pscustomobject]@{
        ObjectId  = $ObjectId
        Name      = 'head_low'
        Positions = @(0.0, 0.0, 0.0,
                      1.5, 0.0, 0.0,
                      0.0, 2.25, 0.0,
                      0.0, 0.0, 3.5)
        Normals   = @(0.0, 0.0, 1.0,
                      0.0, 1.0, 0.0,
                      1.0, 0.0, 0.0,
                      0.0, 0.0, -1.0)
        Indices   = @(0, 1, 2, 0, 2, 3)
        Batches   = @(
            [pscustomobject]@{ FirstIndex = 0; IndexCount = 3; DoubleSided = $false },
            [pscustomobject]@{ FirstIndex = 3; IndexCount = 3; DoubleSided = $true })
    }
}

# Where an imported fixture's object sits: the translation its source node
# stated, and nothing else. Rotation and scale are the identity because an
# import BAKES the node's linear part into the geometry.
function New-CanonicalImportedPlacement {
    return @(1.5, -0.25, 4.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0)
}

# The six remembered parameter sets both mixed fixtures give every Construction
# Body, so the fixtures differ in which BRANCHES are present rather than in
# incidental numbers.
$script:CanonicalSharedParameters = @(2.0, 1.0, 0.5, 1.0, 2.0, 1.5, 1.0, 2.0, 1.0, 2.0, 2.0, 2.0)

# Six bodies, one per primitive kind, each with a non-default placement that
# includes an uncanonicalized 370 degrees and a non-uniform scale, and all six
# remembered parameter sets different from every other body's.
function New-CanonicalConstructionFile {
    $sceneBodies = @()
    $sourceBodies = @()
    for ($i = 1; $i -le 6; $i++) {
        $n = [double] $i
        # Every element is parenthesised on purpose: PowerShell binds the comma
        # TIGHTER than arithmetic, so `0.5 * $n, -0.25 * $n` would parse as
        # `0.5 * ($n, -0.25) * $n` and fail on an array multiply.
        $sceneBodies += [pscustomobject]@{
            ObjectId  = $i
            Transform = @(
                (0.5 * $n), (-0.25 * $n), (1.25 * $n),
                (370.0), (-45.5), (12.25 * $n),
                (1.0 + 0.25 * $n), (2.0), (0.5))
        }
        $sourceBodies += [pscustomobject]@{
            ObjectId      = $i
            PrimitiveCode = $i    # 1 Box .. 6 Plane, in the file's own numbering
            Parameters    = @(
                (2.0 + 0.5 * $n), (1.0 + 0.25 * $n), (0.5 + 0.125 * $n),  # box
                (1.5 + 0.25 * $n), (3.0 + 0.5 * $n),                      # cylinder
                (2.25 + 0.25 * $n),                                       # sphere
                (1.75 + 0.25 * $n), (2.5 + 0.5 * $n),                     # cone
                (1.0 + 0.125 * $n), (4.0 + 0.25 * $n),                    # capsule
                (3.0 + 0.5 * $n), (1.5 + 0.25 * $n))                      # plane
            Features      = New-PrimitiveSourceFeature
        }
    }
    $scne = New-Section 'SCNE' 1 $true (New-ScenePayload $sceneBodies 7 4)
    $cons = New-Section 'CONS' 1 $true (New-ConstructionPayload $sourceBodies)
    return New-ForgeFile 1 @($scne, $cons) 1
}

# The legacy mixed state: two bodies, both with a Construction Source, the
# second also carrying an edited Frozen Sculpt Mesh, reopening in Sculpt.
function New-CanonicalSculptFile {
    $shared = @(2.0, 1.0, 0.5, 1.0, 2.0, 1.5, 1.0, 2.0, 1.0, 2.0, 2.0, 2.0)
    $sceneBodies = @(
        [pscustomobject]@{ ObjectId = 1; Transform = @(0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0) },
        [pscustomobject]@{ ObjectId = 2; Transform = @(1.5, 0.5, -2.0, 370.0, 0.0, 90.0, 1.0, 1.0, 1.0) }
    )
    $sourceBodies = @(
        [pscustomobject]@{ ObjectId = 1; PrimitiveCode = 1; Parameters = $shared; Features = New-PrimitiveSourceFeature },
        [pscustomobject]@{ ObjectId = 2; PrimitiveCode = 3; Parameters = $shared; Features = New-PrimitiveSourceFeature }
    )
    $sculptEntries = @(
        [pscustomobject]@{
            ObjectId        = 2
            RenderBothSides = $false
            SourceStale     = $true
            HasEdits        = $true
            Positions       = @(0.0, 0.0, 0.0,
                                1.5, 0.0, 0.0,
                                0.0, 1.25, 0.0,
                                0.25, 0.5, 1.75)
            Indices         = @(0, 1, 2, 0, 1, 3, 0, 2, 3, 1, 2, 3)
        }
    )
    $scne = New-Section 'SCNE' 1 $true (New-ScenePayload $sceneBodies 3 2)
    # CONS is the OPTIONAL retained companion in a Sculpt project, and SCUL is
    # the required branch. The required bit follows the project kind, which is
    # the whole of how the two branches are told apart.
    $cons = New-Section 'CONS' 1 $false (New-ConstructionPayload $sourceBodies)
    $scul = New-Section 'SCUL' 1 $true (New-SculptPayload $sculptEntries)
    return New-ForgeFile 2 @($scne, $cons, $scul) 3
}

# ---------------------------------------------------------------------------
# The IMPORT-01A fixtures
# ---------------------------------------------------------------------------

# IMPORTED-ONLY: one body, no Construction branch at all.
#
# The fixture that proves an imported object needs no Construction Source
# standing in for it: the file carries SCNE and IMPT and nothing else, and its
# header flags say so.
function New-ImportedOnlyFile {
    $sceneBodies = @(
        [pscustomobject]@{ ObjectId = 1; Transform = (New-CanonicalImportedPlacement) }
    )
    $scne = New-Section 'SCNE' 1 $true (New-ScenePayload $sceneBodies 2 1)
    # ALWAYS required, in either project kind, unlike CONS and SCUL whose
    # required bit follows the header's ProjectKind. Those two are branches of
    # data a reader can skip because the other still describes the same bodies;
    # an Imported Mesh is the only copy of its own geometry.
    $impt = New-Section 'IMPT' 1 $true (New-ImportedPayload @((New-CanonicalImportedEntry 1)))
    return New-ForgeFile 1 @($scne, $impt) 4
}

# CONSTRUCTION + IMPORTED: a SPARSE CONS beside an IMPT.
#
# This is the fixture that pins the generalized CONS rule -- one entry per body
# that HAS a Construction Source, in scene order, rather than one per body.
function New-ConstructionImportedFile {
    $sceneBodies = @(
        [pscustomobject]@{ ObjectId = 1; Transform = @(0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0) },
        [pscustomobject]@{ ObjectId = 2; Transform = (New-CanonicalImportedPlacement) }
    )
    $sourceBodies = @(
        [pscustomobject]@{ ObjectId = 1; PrimitiveCode = 1
                           Parameters = $script:CanonicalSharedParameters
                           Features = New-PrimitiveSourceFeature }
    )
    $scne = New-Section 'SCNE' 1 $true (New-ScenePayload $sceneBodies 3 2)
    $cons = New-Section 'CONS' 1 $true (New-ConstructionPayload $sourceBodies)
    $impt = New-Section 'IMPT' 1 $true (New-ImportedPayload @((New-CanonicalImportedEntry 2)))
    return New-ForgeFile 1 @($scne, $cons, $impt) 5
}

# ALL THREE BRANCHES at once, reopening in Sculpt on the sculpted body.
#
# The representations are INTERLEAVED rather than grouped, so a reader that
# assumed a contiguous block of either would fail here.
function New-MixedImportedFile {
    $sceneBodies = @(
        [pscustomobject]@{ ObjectId = 1; Transform = @(0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0) },
        [pscustomobject]@{ ObjectId = 2; Transform = @(1.5, 0.5, -2.0, 370.0, 0.0, 90.0, 1.0, 1.0, 1.0) },
        [pscustomobject]@{ ObjectId = 3; Transform = (New-CanonicalImportedPlacement) }
    )
    $sourceBodies = @(
        [pscustomobject]@{ ObjectId = 1; PrimitiveCode = 1
                           Parameters = $script:CanonicalSharedParameters
                           Features = New-PrimitiveSourceFeature },
        [pscustomobject]@{ ObjectId = 2; PrimitiveCode = 3
                           Parameters = $script:CanonicalSharedParameters
                           Features = New-PrimitiveSourceFeature }
    )
    $sculptEntries = @(
        [pscustomobject]@{
            ObjectId        = 2
            RenderBothSides = $false
            SourceStale     = $true
            HasEdits        = $true
            Positions       = @(0.0, 0.0, 0.0,
                                1.5, 0.0, 0.0,
                                0.0, 1.25, 0.0,
                                0.25, 0.5, 1.75)
            Indices         = @(0, 1, 2, 0, 1, 3, 0, 2, 3, 1, 2, 3)
        }
    )
    $scne = New-Section 'SCNE' 1 $true (New-ScenePayload $sceneBodies 4 2)
    $cons = New-Section 'CONS' 1 $false (New-ConstructionPayload $sourceBodies)
    $scul = New-Section 'SCUL' 1 $true (New-SculptPayload $sculptEntries)
    $impt = New-Section 'IMPT' 1 $true (New-ImportedPayload @((New-CanonicalImportedEntry 3)))
    return New-ForgeFile 2 @($scne, $cons, $scul, $impt) 7
}

# ---------------------------------------------------------------------------
# The deliberately broken fixtures
# ---------------------------------------------------------------------------
#
# Each is derived from the canonical Construction file by changing exactly one
# thing, so the case a fixture exercises is unambiguous.

function Set-U32At {
    param([byte[]] $Bytes, [int] $Offset, [uint32] $Value)
    $encoded = ConvertTo-LittleEndian ([System.BitConverter]::GetBytes([uint32] $Value))
    [array]::Copy($encoded, 0, $Bytes, $Offset, 4)
}

function Set-U64At {
    param([byte[]] $Bytes, [int] $Offset, [uint64] $Value)
    $encoded = ConvertTo-LittleEndian ([System.BitConverter]::GetBytes([uint64] $Value))
    [array]::Copy($encoded, 0, $Bytes, $Offset, 8)
}

function New-CorruptCrcFixture {
    param([byte[]] $Canonical)
    $bytes = $Canonical.Clone()
    # One bit inside the SCNE payload. Every length, count and section header
    # stays exactly right, so only the checksum can catch it.
    $bytes[28 + 24 + 5] = [byte]($bytes[28 + 24 + 5] -bxor 0x01)
    return $bytes
}

function New-TruncatedFixture {
    param([byte[]] $Canonical)
    return $Canonical[0..($Canonical.Length - 41)]
}

function New-UnsupportedMajorFixture {
    param([byte[]] $Canonical)
    $bytes = $Canonical.Clone()
    $bytes[8] = 2
    return $bytes
}

function New-ExtraSectionFixture {
    param([byte[]] $Canonical, [bool] $Required)
    $payload = [byte[]] @(1, 2, 3, 4, 5, 6, 7, 8)
    $section = New-Section 'XTRA' 1 $Required $payload
    $bytes = New-Object byte[] ($Canonical.Length + $section.Length)
    [array]::Copy($Canonical, 0, $bytes, 0, $Canonical.Length)
    [array]::Copy($section, 0, $bytes, $Canonical.Length, $section.Length)
    Set-U32At $bytes 16 ([uint32] 3)
    Set-U64At $bytes 20 ([uint64] $bytes.Length)
    return $bytes
}

# ---------------------------------------------------------------------------
# Write and report
# ---------------------------------------------------------------------------

function Get-Sha256Hex {
    param([byte[]] $Bytes)
    $sha = [System.Security.Cryptography.SHA256]::Create()
    try {
        return (($sha.ComputeHash($Bytes) | ForEach-Object { $_.ToString('x2') }) -join '')
    } finally {
        $sha.Dispose()
    }
}

if (-not (Test-Path $OutputDirectory)) {
    New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
}

$construction = New-CanonicalConstructionFile
$sculpt = New-CanonicalSculptFile

$fixtures = [ordered]@{
    'construction_multibody_v1.forge' = $construction
    'sculpt_mixed_v1.forge'           = $sculpt
    'corrupt_crc_v1.forge'            = (New-CorruptCrcFixture $construction)
    'truncated_v1.forge'              = (New-TruncatedFixture $construction)
    'unsupported_major_v1.forge'      = (New-UnsupportedMajorFixture $construction)
    'unknown_optional_v1.forge'       = (New-ExtraSectionFixture $construction $false)
    'unknown_required_v1.forge'       = (New-ExtraSectionFixture $construction $true)
    'imported_only_v1.forge'          = (New-ImportedOnlyFile)
    'construction_imported_v1.forge'  = (New-ConstructionImportedFile)
    'mixed_imported_v1.forge'         = (New-MixedImportedFile)
}

$rows = New-Object System.Collections.Generic.List[object]
foreach ($name in $fixtures.Keys) {
    $bytes = [byte[]] $fixtures[$name]
    $path = Join-Path $OutputDirectory $name
    if (-not $VerifyOnly) {
        [System.IO.File]::WriteAllBytes($path, $bytes)
    }
    $onDisk = if (Test-Path $path) { [System.IO.File]::ReadAllBytes($path) } else { $null }
    $rows.Add([pscustomobject]@{
        Fixture   = $name
        Bytes     = $bytes.Length
        Sha256    = Get-Sha256Hex $bytes
        OnDisk    = if ($onDisk) { (Get-Sha256Hex $onDisk) -eq (Get-Sha256Hex $bytes) } else { $false }
    })
}

$rows | Format-Table -AutoSize
Write-Host ''
Write-Host 'Digests the C++ self-test (FSR1A-12, IMP01A-19) must assert:'
Write-Host ("  construction:          {0}" -f ($rows | Where-Object Fixture -eq 'construction_multibody_v1.forge').Sha256)
Write-Host ("  sculpt:                {0}" -f ($rows | Where-Object Fixture -eq 'sculpt_mixed_v1.forge').Sha256)
Write-Host ("  imported_only:         {0}" -f ($rows | Where-Object Fixture -eq 'imported_only_v1.forge').Sha256)
Write-Host ("  construction_imported: {0}" -f ($rows | Where-Object Fixture -eq 'construction_imported_v1.forge').Sha256)
Write-Host ("  mixed_imported:        {0}" -f ($rows | Where-Object Fixture -eq 'mixed_imported_v1.forge').Sha256)
