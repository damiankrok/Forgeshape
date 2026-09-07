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
#
# SCNE v2 (Stage 018A) appends, per body, a FLAGS byte (bit0 hidden, bit1
# locked; every other bit reserved and zero) and a length-prefixed UTF-8 NAME.
# It is written ONLY when `-V2` is passed, which mirrors the C++ encoder's rule:
# a project of visible, unlocked, unnamed bodies stays at v1 and byte-identical,
# so every fixture written before Stage 018A is unchanged. An imported body's
# name stays IMPT's and is written EMPTY here -- one owner per representation.
function New-ScenePayload {
    param($Bodies, [uint64] $NextObjectId, [uint64] $ActiveObjectId, [switch] $V2)
    $p = New-ByteBuffer
    Add-U32 $p ([uint32] $Bodies.Count)
    Add-U64 $p $NextObjectId
    Add-U64 $p $ActiveObjectId
    foreach ($body in $Bodies) {
        Add-U64 $p ([uint64] $body.ObjectId)
        foreach ($value in $body.Transform) { Add-F64 $p $value }
        if ($V2) {
            $flags = 0
            if ($body.PSObject.Properties['Hidden'] -and $body.Hidden) { $flags = $flags -bor 0x01 }
            if ($body.PSObject.Properties['Locked'] -and $body.Locked) { $flags = $flags -bor 0x02 }
            Add-U8 $p ([byte] $flags)
            $name = ''
            if ($body.PSObject.Properties['Name'] -and $body.Name) { $name = [string] $body.Name }
            $nameBytes = [System.Text.Encoding]::UTF8.GetBytes($name)
            Add-U16 $p ([uint16] $nameBytes.Length)
            if ($nameBytes.Length -gt 0) { Add-Bytes $p $nameBytes }
        }
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

# CADB v1: bodyCount, then per body an ObjectId, the workplane FILE code, the
# sketch's next entity id, the extruded profile's anchor entity id, the
# direction FILE code, the binary64 depth, and the entity list -- each entity
# an id, a kind FILE code and that kind's own values.
#
# The AUTHORED truth and nothing derived: no polygon, no triangle, no vertex.
# The mesh is regenerated from exactly this on load.
#
#   plane      1 XY, 2 XZ, 3 YZ
#   direction  1 along the normal, 2 against it
#   kind       1 Line (start u,v, end u,v)
#              2 Polyline (flags bit0 closed, vertexCount, then u,v pairs)
#              3 Rectangle (centre u,v, width, height)
#              4 Circle (centre u,v, radius)
function New-CadPayload {
    param($Bodies)
    $p = New-ByteBuffer
    Add-U32 $p ([uint32] $Bodies.Count)
    foreach ($body in $Bodies) {
        Add-U64 $p ([uint64] $body.ObjectId)
        Add-U8  $p $body.PlaneCode
        Add-U32 $p ([uint32] $body.NextEntityId)
        Add-U32 $p ([uint32] $body.ProfileEntityId)
        Add-U8  $p $body.DirectionCode
        Add-F64 $p $body.Depth
        $entities = @($body.Entities)
        Add-U32 $p ([uint32] $entities.Count)
        foreach ($entity in $entities) {
            Add-U32 $p ([uint32] $entity.Id)
            Add-U8  $p $entity.KindCode
            switch ($entity.KindCode) {
                1 { foreach ($value in $entity.Values) { Add-F64 $p $value } }
                2 {
                    Add-U8  $p $(if ($entity.Closed) { 1 } else { 0 })
                    Add-U32 $p ([uint32] ($entity.Values.Count / 2))
                    foreach ($value in $entity.Values) { Add-F64 $p $value }
                }
                3 { foreach ($value in $entity.Values) { Add-F64 $p $value } }
                4 { foreach ($value in $entity.Values) { Add-F64 $p $value } }
            }
        }
    }
    return $p.ToArray()
}

function New-PrimitiveSourceFeature {
    return @([pscustomobject]@{ LocalFeatureId = 1; KindCode = 1 })
}

# ---------------------------------------------------------------------------
# CADB v2 (CAD-A3): a sketch supported by another body's planar face
# ---------------------------------------------------------------------------
#
# The v2 record is the v1 record with a support block after the workplane
# code: a supportKind byte (0 world plane, 1 face) and, for a face, the
# producer's ObjectId, its feature id, the face token (kind 1 CapPlane, 2
# CapFar, 3 Side; the profile-edge entity id and local index, both 0 for a
# cap) and the u64 lineage token. Written ONLY when at least one body is
# face-supported; a world-only project stays v1, byte for byte.
function New-CadPayloadV2 {
    param($Bodies)
    $p = New-ByteBuffer
    Add-U32 $p ([uint32] $Bodies.Count)
    foreach ($body in $Bodies) {
        Add-U64 $p ([uint64] $body.ObjectId)
        Add-U8  $p $body.PlaneCode
        if ($null -ne $body.Support) {
            Add-U8  $p 1
            Add-U64 $p ([uint64] $body.Support.ProducerObjectId)
            Add-U32 $p ([uint32] $body.Support.FeatureId)
            Add-U8  $p $body.Support.FaceKindCode
            Add-U32 $p ([uint32] $body.Support.EdgeEntityId)
            Add-U32 $p ([uint32] $body.Support.EdgeLocalIndex)
            Add-U64 $p ([uint64] $body.Support.LineageToken)
        } else {
            Add-U8  $p 0
        }
        Add-U32 $p ([uint32] $body.NextEntityId)
        Add-U32 $p ([uint32] $body.ProfileEntityId)
        Add-U8  $p $body.DirectionCode
        Add-F64 $p $body.Depth
        $entities = @($body.Entities)
        Add-U32 $p ([uint32] $entities.Count)
        foreach ($entity in $entities) {
            Add-U32 $p ([uint32] $entity.Id)
            Add-U8  $p $entity.KindCode
            switch ($entity.KindCode) {
                1 { foreach ($value in $entity.Values) { Add-F64 $p $value } }
                2 {
                    Add-U8  $p $(if ($entity.Closed) { 1 } else { 0 })
                    Add-U32 $p ([uint32] ($entity.Values.Count / 2))
                    foreach ($value in $entity.Values) { Add-F64 $p $value }
                }
                3 { foreach ($value in $entity.Values) { Add-F64 $p $value } }
                4 { foreach ($value in $entity.Values) { Add-F64 $p $value } }
            }
        }
    }
    return $p.ToArray()
}

# ---------------------------------------------------------------------------
# CADB v3 (SKETCH-UX-R1): the curve entities
# ---------------------------------------------------------------------------
#
# The v3 record is the v2 record with two entity kinds added. v3 ALWAYS writes
# the v2 support block, present or not, because a version is a superset of the
# one below it -- see DATA_PACKAGE_SPEC.md 7d.
#
#   kind 5  Arc     start u,v, mid u,v, end u,v   (three points ON the curve)
#   kind 6  Spline  pointCount (2..32), then u,v pairs the curve interpolates
#
# Both store AUTHORED points only. No centre, no radius, no sweep, no control
# handle and no tessellated point: every one of those is derived on load.
function New-CadPayloadV3 {
    param($Bodies)
    $p = New-ByteBuffer
    Add-U32 $p ([uint32] $Bodies.Count)
    foreach ($body in $Bodies) {
        Add-U64 $p ([uint64] $body.ObjectId)
        Add-U8  $p $body.PlaneCode
        if ($null -ne $body.Support) {
            Add-U8  $p 1
            Add-U64 $p ([uint64] $body.Support.ProducerObjectId)
            Add-U32 $p ([uint32] $body.Support.FeatureId)
            Add-U8  $p $body.Support.FaceKindCode
            Add-U32 $p ([uint32] $body.Support.EdgeEntityId)
            Add-U32 $p ([uint32] $body.Support.EdgeLocalIndex)
            Add-U64 $p ([uint64] $body.Support.LineageToken)
        } else {
            Add-U8  $p 0
        }
        Add-U32 $p ([uint32] $body.NextEntityId)
        Add-U32 $p ([uint32] $body.ProfileEntityId)
        Add-U8  $p $body.DirectionCode
        Add-F64 $p $body.Depth
        $entities = @($body.Entities)
        Add-U32 $p ([uint32] $entities.Count)
        foreach ($entity in $entities) {
            Add-U32 $p ([uint32] $entity.Id)
            Add-U8  $p $entity.KindCode
            switch ($entity.KindCode) {
                1 { foreach ($value in $entity.Values) { Add-F64 $p $value } }
                2 {
                    Add-U8  $p $(if ($entity.Closed) { 1 } else { 0 })
                    Add-U32 $p ([uint32] ($entity.Values.Count / 2))
                    foreach ($value in $entity.Values) { Add-F64 $p $value }
                }
                3 { foreach ($value in $entity.Values) { Add-F64 $p $value } }
                4 { foreach ($value in $entity.Values) { Add-F64 $p $value } }
                5 { foreach ($value in $entity.Values) { Add-F64 $p $value } }
                6 {
                    Add-U32 $p ([uint32] ($entity.Values.Count / 2))
                    foreach ($value in $entity.Values) { Add-F64 $p $value }
                }
            }
        }
    }
    return $p.ToArray()
}

# The lineage token: FNV-1a over 64 bits, as DATA_PACKAGE_SPEC.md 7c states it.
#
# Every mixed value is a u64 fed least-significant byte first (eight steps of
# XOR the byte, multiply by the prime). The values, in order: the extruded
# profile's anchor entity id; the face count; then for every face in
# enumeration order its token code (kind << 56 | edgeEntityId << 16 |
# edgeLocalIndex & 0xFFFF, with kind 0 CapPlane, 1 CapFar, 2 Side) and its
# eligibility (1, or 0 for a circle's cylindrical side). Enumeration order is
# CapPlane, CapFar, then one Side per profile edge in profile order. A zero
# result is reported as 1.
#
# Arithmetic is done in BigInteger, through its explicit operator methods, and
# masked to 64 bits at every step: Windows PowerShell has no unsigned 64-bit
# multiply, its infix bitwise operators are not defined for BigInteger, and a
# silent conversion here would still produce a plausible-looking token.
$script:Fnv64Mask = [System.Numerics.BigInteger]::Pow(2, 64) - 1
function Add-Fnv64 {
    param([System.Numerics.BigInteger] $Hash, [System.Numerics.BigInteger] $Value)
    $prime = [System.Numerics.BigInteger]::Parse('1099511628211')
    for ($i = 0; $i -lt 8; $i++) {
        $byte = [System.Numerics.BigInteger]::op_BitwiseAnd(
            [System.Numerics.BigInteger]::op_RightShift($Value, 8 * $i),
            [System.Numerics.BigInteger] 255)
        $Hash = [System.Numerics.BigInteger]::op_ExclusiveOr($Hash, $byte)
        $Hash = [System.Numerics.BigInteger]::op_BitwiseAnd(
            [System.Numerics.BigInteger]::Multiply($Hash, $prime), $script:Fnv64Mask)
    }
    return $Hash
}

function Get-CadFaceTokenCode {
    param([int] $KindCode, [uint32] $EdgeEntityId, [uint32] $EdgeLocalIndex)
    # KindCode here is the DOMAIN enumeration (0 CapPlane, 1 CapFar, 2 Side),
    # not the file code (1, 2, 3): the token code is what the domain hashes.
    $code = [System.Numerics.BigInteger]::op_LeftShift([System.Numerics.BigInteger] $KindCode, 56)
    $code = [System.Numerics.BigInteger]::op_BitwiseOr($code,
        [System.Numerics.BigInteger]::op_LeftShift([System.Numerics.BigInteger] $EdgeEntityId, 16))
    $code = [System.Numerics.BigInteger]::op_BitwiseOr($code,
        [System.Numerics.BigInteger] ($EdgeLocalIndex -band 0xFFFF))
    return $code
}

# The topology signature of a body whose extruded profile is ONE entity with
# EdgeCount planar edges (4 for a rectangle, 32 for a circle) -- the only
# shapes the v2 corpus uses as producers. $Curved is true for a circle, whose
# 32 sides are reported and never eligible.
function Get-CadTopologySignature {
    param([uint32] $ProfileEntityId, [int] $EdgeCount, [bool] $Curved)
    $hash = [System.Numerics.BigInteger]::Parse('14695981039346656037')
    $hash = Add-Fnv64 $hash ([System.Numerics.BigInteger] $ProfileEntityId)
    $hash = Add-Fnv64 $hash ([System.Numerics.BigInteger] (2 + $EdgeCount))
    $hash = Add-Fnv64 $hash (Get-CadFaceTokenCode 0 0 0)
    $hash = Add-Fnv64 $hash ([System.Numerics.BigInteger] 1)
    $hash = Add-Fnv64 $hash (Get-CadFaceTokenCode 1 0 0)
    $hash = Add-Fnv64 $hash ([System.Numerics.BigInteger] 1)
    for ($k = 0; $k -lt $EdgeCount; $k++) {
        $hash = Add-Fnv64 $hash (Get-CadFaceTokenCode 2 $ProfileEntityId ([uint32] $k))
        $hash = Add-Fnv64 $hash ([System.Numerics.BigInteger] $(if ($Curved) { 0 } else { 1 }))
    }
    if ($hash.IsZero) { $hash = [System.Numerics.BigInteger]::One }
    return [uint64]::Parse($hash.ToString())
}

function New-RectangleBody {
    param([int] $ObjectId, [int] $PlaneCode, [double] $CentreU, [double] $CentreV,
          [double] $Width, [double] $Height, [double] $Depth, [int] $DirectionCode)
    return [pscustomobject]@{
        ObjectId = $ObjectId; PlaneCode = $PlaneCode; Support = $null
        NextEntityId = 2; ProfileEntityId = 1; DirectionCode = $DirectionCode; Depth = $Depth
        Entities = @([pscustomobject]@{ Id = 1; KindCode = 3
                                        Values = @($CentreU, $CentreV, $Width, $Height) })
    }
}

# A support on a rectangle producer's face. The producer's lineage is the
# signature of a one-rectangle profile: two caps and four eligible sides.
function New-RectangleFaceSupport {
    param([int] $ProducerObjectId, [int] $FaceKindCode, [uint32] $EdgeEntityId,
          [uint32] $EdgeLocalIndex)
    return [pscustomobject]@{
        ProducerObjectId = $ProducerObjectId; FeatureId = 1; FaceKindCode = $FaceKindCode
        EdgeEntityId = $EdgeEntityId; EdgeLocalIndex = $EdgeLocalIndex
        LineageToken = (Get-CadTopologySignature 1 4 $false)
    }
}

$script:IdentityPlacement = @(0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0)

# CAD FACE SKETCH CAP: a producer rectangle on XY, translated, and a dependent
# rectangle supported by its far cap. A face-supported body's own SCNE
# placement is the unused identity.
function New-CadFaceSketchCapFile {
    $sceneBodies = @(
        [pscustomobject]@{ ObjectId = 1; Transform = @(1.5, 0.5, -2.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0) },
        [pscustomobject]@{ ObjectId = 2; Transform = $script:IdentityPlacement }
    )
    $producer = New-RectangleBody 1 1 0.0 0.0 2.0 2.0 2.0 1
    $dependent = New-RectangleBody 2 1 0.25 -0.25 1.0 1.0 0.5 1
    $dependent.Support = New-RectangleFaceSupport 1 2 0 0
    $scne = New-Section 'SCNE' 1 $true (New-ScenePayload $sceneBodies 3 2)
    $cadb = New-Section 'CADB' 2 $true (New-CadPayloadV2 @($producer, $dependent))
    return New-ForgeFile 1 @($scne, $cadb) 8
}

# CAD FACE SKETCH SIDE: the same producer at the origin, and a dependent on the
# producer's second profile edge (edge entity 1, local index 1), extruded
# AGAINST the face normal.
function New-CadFaceSketchSideFile {
    $sceneBodies = @(
        [pscustomobject]@{ ObjectId = 1; Transform = $script:IdentityPlacement },
        [pscustomobject]@{ ObjectId = 2; Transform = $script:IdentityPlacement }
    )
    $producer = New-RectangleBody 1 1 0.0 0.0 2.0 2.0 2.0 1
    $dependent = New-RectangleBody 2 1 0.0 0.0 0.5 0.5 0.25 2
    $dependent.Support = New-RectangleFaceSupport 1 3 1 1
    $scne = New-Section 'SCNE' 1 $true (New-ScenePayload $sceneBodies 3 2)
    $cadb = New-Section 'CADB' 2 $true (New-CadPayloadV2 @($producer, $dependent))
    return New-ForgeFile 1 @($scne, $cadb) 8
}

# CAD FACE CHAIN: A (world XZ, turned 45 degrees) <- B on A's far cap <- C, a
# circle on B's far cap. B's lineage is A's rectangle signature; C's is B's.
function New-CadFaceChainFile {
    $sceneBodies = @(
        [pscustomobject]@{ ObjectId = 1; Transform = @(0.0, 0.0, 0.0, 0.0, 45.0, 0.0, 1.0, 1.0, 1.0) },
        [pscustomobject]@{ ObjectId = 2; Transform = $script:IdentityPlacement },
        [pscustomobject]@{ ObjectId = 3; Transform = $script:IdentityPlacement }
    )
    $a = New-RectangleBody 1 2 0.0 0.0 3.0 3.0 1.0 1
    $b = New-RectangleBody 2 1 0.0 0.0 1.5 1.5 0.5 1
    $b.Support = New-RectangleFaceSupport 1 2 0 0
    $c = [pscustomobject]@{
        ObjectId = 3; PlaneCode = 1
        Support = New-RectangleFaceSupport 2 2 0 0
        NextEntityId = 2; ProfileEntityId = 1; DirectionCode = 1; Depth = 0.25
        Entities = @([pscustomobject]@{ Id = 1; KindCode = 4; Values = @(0.0, 0.0, 0.5) })
    }
    $scne = New-Section 'SCNE' 1 $true (New-ScenePayload $sceneBodies 4 3)
    $cadb = New-Section 'CADB' 2 $true (New-CadPayloadV2 @($a, $b, $c))
    return New-ForgeFile 1 @($scne, $cadb) 8
}

# CAD BAD FACE REF: the side fixture with the dependent naming side 7 of a
# rectangle that has sides 0..3. Built from scratch with the bad value: every
# length and checksum is right, the producer exists and the lineage matches,
# and only the face resolution can refuse it.
function New-CadBadFaceRefFile {
    $sceneBodies = @(
        [pscustomobject]@{ ObjectId = 1; Transform = $script:IdentityPlacement },
        [pscustomobject]@{ ObjectId = 2; Transform = $script:IdentityPlacement }
    )
    $producer = New-RectangleBody 1 1 0.0 0.0 2.0 2.0 2.0 1
    $dependent = New-RectangleBody 2 1 0.0 0.0 0.5 0.5 0.25 2
    $dependent.Support = New-RectangleFaceSupport 1 3 1 7
    $scne = New-Section 'SCNE' 1 $true (New-ScenePayload $sceneBodies 3 2)
    $cadb = New-Section 'CADB' 2 $true (New-CadPayloadV2 @($producer, $dependent))
    return New-ForgeFile 1 @($scne, $cadb) 8
}

# CAD DEPENDENCY CYCLE: the chain with B on C's far cap and C on B's, while A
# stays a world body. B's lineage is C's circle signature (two caps and 32
# ineligible sides), so ONLY the cycle can refuse it.
function New-CadDependencyCycleFile {
    $sceneBodies = @(
        [pscustomobject]@{ ObjectId = 1; Transform = @(0.0, 0.0, 0.0, 0.0, 45.0, 0.0, 1.0, 1.0, 1.0) },
        [pscustomobject]@{ ObjectId = 2; Transform = $script:IdentityPlacement },
        [pscustomobject]@{ ObjectId = 3; Transform = $script:IdentityPlacement }
    )
    $a = New-RectangleBody 1 2 0.0 0.0 3.0 3.0 1.0 1
    $b = New-RectangleBody 2 1 0.0 0.0 1.5 1.5 0.5 1
    $b.Support = [pscustomobject]@{
        ProducerObjectId = 3; FeatureId = 1; FaceKindCode = 2; EdgeEntityId = 0; EdgeLocalIndex = 0
        LineageToken = (Get-CadTopologySignature 1 32 $true)
    }
    $c = [pscustomobject]@{
        ObjectId = 3; PlaneCode = 1
        Support = New-RectangleFaceSupport 2 2 0 0
        NextEntityId = 2; ProfileEntityId = 1; DirectionCode = 1; Depth = 0.25
        Entities = @([pscustomobject]@{ Id = 1; KindCode = 4; Values = @(0.0, 0.0, 0.5) })
    }
    $scne = New-Section 'SCNE' 1 $true (New-ScenePayload $sceneBodies 4 3)
    $cadb = New-Section 'CADB' 2 $true (New-CadPayloadV2 @($a, $b, $c))
    return New-ForgeFile 1 @($scne, $cadb) 8
}

# MIXED CAD FACE: a Construction Box, an Imported Mesh, a CAD producer and a
# dependent on its far cap -- SCNE, CONS, IMPT and a v2 CADB side by side.
function New-MixedCadFaceFile {
    $sceneBodies = @(
        [pscustomobject]@{ ObjectId = 1; Transform = $script:IdentityPlacement },
        [pscustomobject]@{ ObjectId = 2; Transform = (New-CanonicalImportedPlacement) },
        [pscustomobject]@{ ObjectId = 3; Transform = @(-2.5, 1.25, 0.5, 0.0, 0.0, 90.0, 1.0, 1.0, 1.0) },
        [pscustomobject]@{ ObjectId = 4; Transform = $script:IdentityPlacement }
    )
    $sourceBodies = @(
        [pscustomobject]@{ ObjectId = 1; PrimitiveCode = 1
                           Parameters = $script:CanonicalSharedParameters
                           Features = New-PrimitiveSourceFeature }
    )
    $producer = New-RectangleBody 3 1 0.0 0.0 2.0 2.0 1.0 1
    $dependent = New-RectangleBody 4 1 0.0 0.0 1.0 0.5 0.5 1
    $dependent.Support = New-RectangleFaceSupport 3 2 0 0
    $scne = New-Section 'SCNE' 1 $true (New-ScenePayload $sceneBodies 5 4)
    $cons = New-Section 'CONS' 1 $true (New-ConstructionPayload $sourceBodies)
    $impt = New-Section 'IMPT' 1 $true (New-ImportedPayload @(New-CanonicalImportedEntry 2))
    $cadb = New-Section 'CADB' 2 $true (New-CadPayloadV2 @($producer, $dependent))
    return New-ForgeFile 1 @($scne, $cons, $impt, $cadb) 13
}

# ---------------------------------------------------------------------------
# The v3 curve fixtures (SKETCH-UX-R1)
# ---------------------------------------------------------------------------
#
# Every coordinate is an exact binary fraction, so neither implementation has a
# rounding argument to make about the bytes.

# The canonical semicircle: centre (0,0), radius 1, from (1,0) through (0,1) to
# (-1,0), and the straight chord that closes it -- a "D" profile that is one
# chain of two entities, one curved and one straight.
function New-ArcProfileBody {
    param([int] $ObjectId, [double] $Depth)
    return [pscustomobject]@{
        ObjectId = $ObjectId; PlaneCode = 1; Support = $null
        NextEntityId = 3; ProfileEntityId = 1; DirectionCode = 1; Depth = $Depth
        Entities = @(
            [pscustomobject]@{ Id = 1; KindCode = 5
                               Values = @(1.0, 0.0, 0.0, 1.0, -1.0, 0.0) },
            [pscustomobject]@{ Id = 2; KindCode = 1
                               Values = @(-1.0, 0.0, 1.0, 0.0) })
    }
}

# A four-point spline and the chord that closes it.
function New-SplineProfileBody {
    param([int] $ObjectId, [double] $Depth)
    return [pscustomobject]@{
        ObjectId = $ObjectId; PlaneCode = 1; Support = $null
        NextEntityId = 3; ProfileEntityId = 1; DirectionCode = 1; Depth = $Depth
        Entities = @(
            [pscustomobject]@{ Id = 1; KindCode = 6
                               Values = @(-1.0, 0.0, -0.5, 0.75, 0.5, 0.75, 1.0, 0.0) },
            [pscustomobject]@{ Id = 2; KindCode = 1
                               Values = @(1.0, 0.0, -1.0, 0.0) })
    }
}

# CAD ARC PROFILE: one body, one arc and one line closing it. The smallest v3
# file there is.
function New-CadArcProfileFile {
    $sceneBodies = @([pscustomobject]@{ ObjectId = 1
                                        Transform = @(0.5, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0) })
    $scne = New-Section 'SCNE' 1 $true (New-ScenePayload $sceneBodies 2 1)
    $cadb = New-Section 'CADB' 3 $true (New-CadPayloadV3 @(New-ArcProfileBody 1 1.5))
    return New-ForgeFile 1 @($scne, $cadb) 8
}

# CAD SPLINE PROFILE: one body, one spline and one line closing it.
function New-CadSplineProfileFile {
    $sceneBodies = @([pscustomobject]@{ ObjectId = 1; Transform = $script:IdentityPlacement })
    $scne = New-Section 'SCNE' 1 $true (New-ScenePayload $sceneBodies 2 1)
    $cadb = New-Section 'CADB' 3 $true (New-CadPayloadV3 @(New-SplineProfileBody 1 0.75))
    return New-ForgeFile 1 @($scne, $cadb) 8
}

# CAD MIXED CURVE PROFILE: an arc body, a spline body and a plain rectangle
# body side by side -- every entity kind the format has, in one v3 CADB, with a
# Construction Body beside them so CONS stays sparse.
function New-CadMixedCurveProfileFile {
    $sceneBodies = @(
        [pscustomobject]@{ ObjectId = 1; Transform = $script:IdentityPlacement },
        [pscustomobject]@{ ObjectId = 2; Transform = @(2.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0) },
        [pscustomobject]@{ ObjectId = 3; Transform = @(-2.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0) },
        [pscustomobject]@{ ObjectId = 4; Transform = @(0.0, 3.0, 0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0) }
    )
    $sourceBodies = @(
        [pscustomobject]@{ ObjectId = 1; PrimitiveCode = 1
                           Parameters = $script:CanonicalSharedParameters
                           Features = New-PrimitiveSourceFeature }
    )
    $scne = New-Section 'SCNE' 1 $true (New-ScenePayload $sceneBodies 5 2)
    $cons = New-Section 'CONS' 1 $true (New-ConstructionPayload $sourceBodies)
    $cadb = New-Section 'CADB' 3 $true (New-CadPayloadV3 @(
        (New-ArcProfileBody 2 1.0),
        (New-SplineProfileBody 3 0.5),
        (New-RectangleBody 4 1 0.0 0.0 1.5 1.5 1.0 1)))
    return New-ForgeFile 1 @($scne, $cons, $cadb) 9
}

# CAD FACE CURVE: a rectangle producer and a dependent whose sketch is a CURVE
# profile supported by the producer's far cap. v3 carrying a v2 support block,
# which is the whole point of v3 being a superset.
function New-CadFaceCurveFile {
    $sceneBodies = @(
        [pscustomobject]@{ ObjectId = 1; Transform = @(0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0) },
        [pscustomobject]@{ ObjectId = 2; Transform = $script:IdentityPlacement }
    )
    $producer = New-RectangleBody 1 1 0.0 0.0 4.0 4.0 1.0 1
    $dependent = New-ArcProfileBody 2 0.5
    $dependent.Support = New-RectangleFaceSupport 1 2 0 0
    $scne = New-Section 'SCNE' 1 $true (New-ScenePayload $sceneBodies 3 2)
    $cadb = New-Section 'CADB' 3 $true (New-CadPayloadV3 @($producer, $dependent))
    return New-ForgeFile 1 @($scne, $cadb) 8
}

# CAD BAD ARC: the arc fixture with its three points made COLLINEAR, CONSTRUCTED
# that way rather than generated and then mutated. No circle passes through
# three collinear points, so only the semantic check can refuse it -- every
# length, count and CRC is correct.
function New-CadBadArcFile {
    $sceneBodies = @([pscustomobject]@{ ObjectId = 1; Transform = $script:IdentityPlacement })
    $body = New-ArcProfileBody 1 1.0
    $body.Entities[0].Values = @(-1.0, 0.0, 0.0, 0.0, 1.0, 0.0)
    $scne = New-Section 'SCNE' 1 $true (New-ScenePayload $sceneBodies 2 1)
    $cadb = New-Section 'CADB' 3 $true (New-CadPayloadV3 @($body))
    return New-ForgeFile 1 @($scne, $cadb) 8
}

# CAD BAD SPLINE: the spline fixture whose two ENDS coincide. A chainable
# entity that closes on itself is a loop the one chain walker cannot read, and
# the domain refuses it. Constructed with the bad value in place.
function New-CadBadSplineFile {
    $sceneBodies = @([pscustomobject]@{ ObjectId = 1; Transform = $script:IdentityPlacement })
    $body = New-SplineProfileBody 1 1.0
    $body.Entities[0].Values = @(-1.0, 0.0, -0.5, 0.75, 0.5, 0.75, -1.0, 0.0)
    $scne = New-Section 'SCNE' 1 $true (New-ScenePayload $sceneBodies 2 1)
    $cadb = New-Section 'CADB' 3 $true (New-CadPayloadV3 @($body))
    return New-ForgeFile 1 @($scne, $cadb) 8
}


# ---------------------------------------------------------------------------
# CADB v4 (CAD-EXT-R1): the extrusion's extent
# ---------------------------------------------------------------------------
#
# The v4 record is the v3 record with two fields: an extentCode BEFORE the
# direction it qualifies, and the -N distance AFTER the depth. v4 ALWAYS writes
# the v2 support block and understands the v3 entity kinds, because a version is
# a superset of the one below it -- see DATA_PACKAGE_SPEC.md 7e.
#
#   extentCode  1 One Side, 2 Symmetric, 3 Two Sides
#   Depth       the PRIMARY distance: One Side's length on the direction's side,
#               Symmetric's length on EACH side, Two Sides' +N distance (A)
#   Second      the -N distance (B), Two Sides only; exactly 0.0 otherwise
#
# One Side is exactly the direction + depth pair every version since v1 has
# carried, which is what lets a One Side project keep the bytes it always had.
function New-CadPayloadV4 {
    param($Bodies)
    $p = New-ByteBuffer
    Add-U32 $p ([uint32] $Bodies.Count)
    foreach ($body in $Bodies) {
        Add-U64 $p ([uint64] $body.ObjectId)
        Add-U8  $p $body.PlaneCode
        if ($null -ne $body.Support) {
            Add-U8  $p 1
            Add-U64 $p ([uint64] $body.Support.ProducerObjectId)
            Add-U32 $p ([uint32] $body.Support.FeatureId)
            Add-U8  $p $body.Support.FaceKindCode
            Add-U32 $p ([uint32] $body.Support.EdgeEntityId)
            Add-U32 $p ([uint32] $body.Support.EdgeLocalIndex)
            Add-U64 $p ([uint64] $body.Support.LineageToken)
        } else {
            Add-U8  $p 0
        }
        Add-U32 $p ([uint32] $body.NextEntityId)
        Add-U32 $p ([uint32] $body.ProfileEntityId)
        Add-U8  $p $(if ($null -ne $body.ExtentCode) { $body.ExtentCode } else { 1 })
        Add-U8  $p $body.DirectionCode
        Add-F64 $p $body.Depth
        Add-F64 $p $(if ($null -ne $body.Second) { $body.Second } else { 0.0 })
        $entities = @($body.Entities)
        Add-U32 $p ([uint32] $entities.Count)
        foreach ($entity in $entities) {
            Add-U32 $p ([uint32] $entity.Id)
            Add-U8  $p $entity.KindCode
            switch ($entity.KindCode) {
                1 { foreach ($value in $entity.Values) { Add-F64 $p $value } }
                2 {
                    Add-U8  $p $(if ($entity.Closed) { 1 } else { 0 })
                    Add-U32 $p ([uint32] ($entity.Values.Count / 2))
                    foreach ($value in $entity.Values) { Add-F64 $p $value }
                }
                3 { foreach ($value in $entity.Values) { Add-F64 $p $value } }
                4 { foreach ($value in $entity.Values) { Add-F64 $p $value } }
                5 { foreach ($value in $entity.Values) { Add-F64 $p $value } }
                6 {
                    Add-U32 $p ([uint32] ($entity.Values.Count / 2))
                    foreach ($value in $entity.Values) { Add-F64 $p $value }
                }
            }
        }
    }
    return $p.ToArray()
}

# A circle body, centred at the origin. The v4 corpus uses one so the new
# extent is pinned over a curved profile as well as a straight one.
function New-CircleBody {
    param([int] $ObjectId, [int] $PlaneCode, [double] $Radius, [double] $Depth,
          [int] $DirectionCode)
    return [pscustomobject]@{
        ObjectId = $ObjectId; PlaneCode = $PlaneCode; Support = $null
        NextEntityId = 2; ProfileEntityId = 1; DirectionCode = $DirectionCode; Depth = $Depth
        ExtentCode = 1; Second = 0.0
        Entities = @([pscustomobject]@{ Id = 1; KindCode = 4; Values = @(0.0, 0.0, $Radius) })
    }
}

# The two extent shapes, applied to a body built by any of the helpers above.
# Symmetric and Two Sides both canonicalise the direction to 1, because a mode
# that names both sides has no side left for a direction to choose.
function Set-SymmetricExtent {
    param($Body, [double] $PerSide)
    $Body.ExtentCode = 2
    $Body.DirectionCode = 1
    $Body.Depth = $PerSide
    $Body.Second = 0.0
    return $Body
}

function Set-TwoSidesExtent {
    param($Body, [double] $A, [double] $B)
    $Body.ExtentCode = 3
    $Body.DirectionCode = 1
    $Body.Depth = $A
    $Body.Second = $B
    return $Body
}

# A rectangle body carrying the v4 extent fields, so the extent helpers above
# have somewhere to write. Identical to New-RectangleBody otherwise.
function New-ExtentRectangleBody {
    param([int] $ObjectId, [int] $PlaneCode, [double] $Width, [double] $Height,
          [double] $Depth, [int] $DirectionCode)
    $body = New-RectangleBody $ObjectId $PlaneCode 0.0 0.0 $Width $Height $Depth $DirectionCode
    return ($body | Add-Member -NotePropertyName ExtentCode -NotePropertyValue 1 -PassThru |
        Add-Member -NotePropertyName Second -NotePropertyValue 0.0 -PassThru)
}

# CAD SYMMETRIC: one rectangle body reaching 0.75 m each side of its XY sketch
# plane. The smallest v4 file there is.
function New-CadSymmetricFile {
    $sceneBodies = @([pscustomobject]@{ ObjectId = 1; Transform = $script:IdentityPlacement })
    $body = Set-SymmetricExtent (New-ExtentRectangleBody 1 1 2.0 1.0 1.0 1) 0.75
    $scne = New-Section 'SCNE' 1 $true (New-ScenePayload $sceneBodies 2 1)
    $cadb = New-Section 'CADB' 4 $true (New-CadPayloadV4 @($body))
    return New-ForgeFile 1 @($scne, $cadb) 8
}

# CAD TWO SIDES: one circle body on XZ with two UNEQUAL distances -- 1.25 m
# along the normal and 0.5 m against it.
function New-CadTwoSidesFile {
    $sceneBodies = @([pscustomobject]@{ ObjectId = 1; Transform = $script:IdentityPlacement })
    $body = Set-TwoSidesExtent (New-CircleBody 1 2 0.5 1.0 1) 1.25 0.5
    $scne = New-Section 'SCNE' 1 $true (New-ScenePayload $sceneBodies 2 1)
    $cadb = New-Section 'CADB' 4 $true (New-CadPayloadV4 @($body))
    return New-ForgeFile 1 @($scne, $cadb) 8
}

# CAD FACE EXTENT: a One Side producer with a SYMMETRIC dependent on its far cap
# and a TWO SIDES dependent on one of its sides -- v4 carrying a v2 support
# block, which is the whole point of a version being a superset.
function New-CadFaceExtentFile {
    $sceneBodies = @(
        [pscustomobject]@{ ObjectId = 1; Transform = $script:IdentityPlacement },
        [pscustomobject]@{ ObjectId = 2; Transform = $script:IdentityPlacement },
        [pscustomobject]@{ ObjectId = 3; Transform = $script:IdentityPlacement }
    )
    $producer = New-ExtentRectangleBody 1 1 4.0 4.0 1.0 1
    $onCap = Set-SymmetricExtent (New-ExtentRectangleBody 2 1 1.0 1.0 1.0 1) 0.25
    $onCap.Support = New-RectangleFaceSupport 1 2 0 0
    $onSide = Set-TwoSidesExtent (New-ExtentRectangleBody 3 1 0.5 0.5 1.0 1) 0.375 0.125
    $onSide.Support = New-RectangleFaceSupport 1 3 1 1
    $scne = New-Section 'SCNE' 1 $true (New-ScenePayload $sceneBodies 4 1)
    $cadb = New-Section 'CADB' 4 $true (New-CadPayloadV4 @($producer, $onCap, $onSide))
    return New-ForgeFile 1 @($scne, $cadb) 8
}

# MIXED CAD EXTENT: a legacy ONE SIDE body beside a Symmetric and a Two Sides
# one, on the three world planes, with a sparse CONS next to them. The fixture
# that proves a v4 section still carries the One Side pair unchanged.
function New-MixedCadExtentFile {
    $sceneBodies = @(
        [pscustomobject]@{ ObjectId = 1; Transform = $script:IdentityPlacement },
        [pscustomobject]@{ ObjectId = 2; Transform = @(2.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0) },
        [pscustomobject]@{ ObjectId = 3; Transform = @(-2.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0) },
        [pscustomobject]@{ ObjectId = 4; Transform = @(0.0, 3.0, 0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0) }
    )
    $sourceBodies = @(
        [pscustomobject]@{ ObjectId = 1; PrimitiveCode = 1
                           Parameters = $script:CanonicalSharedParameters
                           Features = New-PrimitiveSourceFeature }
    )
    $oneSide = New-ExtentRectangleBody 2 1 1.5 1.5 1.0 2
    $symmetric = Set-SymmetricExtent (New-ExtentRectangleBody 3 2 1.0 2.0 1.0 1) 0.625
    $twoSides = Set-TwoSidesExtent (New-CircleBody 4 3 0.75 1.0 1) 1.0 0.25
    $scne = New-Section 'SCNE' 1 $true (New-ScenePayload $sceneBodies 5 2)
    $cons = New-Section 'CONS' 1 $true (New-ConstructionPayload $sourceBodies)
    $cadb = New-Section 'CADB' 4 $true (New-CadPayloadV4 @($oneSide, $symmetric, $twoSides))
    return New-ForgeFile 1 @($scne, $cons, $cadb) 9
}

# CAD BAD EXTENT: the symmetric fixture with an extentCode of 9, CONSTRUCTED
# that way rather than generated and then mutated. Every length, count and CRC
# is correct, so only the semantic check can refuse it.
function New-CadBadExtentFile {
    $sceneBodies = @([pscustomobject]@{ ObjectId = 1; Transform = $script:IdentityPlacement })
    $body = Set-SymmetricExtent (New-ExtentRectangleBody 1 1 2.0 1.0 1.0 1) 0.75
    $body.ExtentCode = 9
    $scne = New-Section 'SCNE' 1 $true (New-ScenePayload $sceneBodies 2 1)
    $cadb = New-Section 'CADB' 4 $true (New-CadPayloadV4 @($body))
    return New-ForgeFile 1 @($scne, $cadb) 8
}

# CAD BAD TWO SIDES: a Two Sides body whose BOTH distances are zero -- an
# extrusion with no extent at all, which the domain refuses rather than clamps.
function New-CadBadTwoSidesFile {
    $sceneBodies = @([pscustomobject]@{ ObjectId = 1; Transform = $script:IdentityPlacement })
    $body = Set-TwoSidesExtent (New-CircleBody 1 2 0.5 1.0 1) 0.0 0.0
    $scne = New-Section 'SCNE' 1 $true (New-ScenePayload $sceneBodies 2 1)
    $cadb = New-Section 'CADB' 4 $true (New-CadPayloadV4 @($body))
    return New-ForgeFile 1 @($scne, $cadb) 8
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
# The IMPORT-01B fixtures
# ---------------------------------------------------------------------------

# The Frozen Sculpt Mesh both IMPORT-01B fixtures put on an IMPORTED body.
#
# Deliberately NOT the imported geometry it was frozen from: a sculpt mesh is
# its own positions and its own topology, and a fixture where the two matched
# could not tell a decoder that confused them apart. Every number is an exact
# binary fraction, so neither implementation has a rounding argument to make.
#
# SourceStale is false because an Imported Mesh is immutable for the life of its
# body: nothing can make one stale, so nothing can write that bit for one.
function New-CanonicalImportedSculptEntry {
    param([int] $ObjectId)
    return [pscustomobject]@{
        ObjectId        = $ObjectId
        RenderBothSides = $true
        SourceStale     = $false
        HasEdits        = $true
        Positions       = @(0.25, 0.0,  0.0,
                            1.75, 0.0,  0.0,
                            0.0,  2.5,  0.0,
                            0.5,  0.75, 3.25)
        Indices         = @(0, 1, 2, 0, 1, 3, 0, 2, 3, 1, 2, 3)
    }
}

# IMPORTED + SCULPT: one body, geometry from a file, sculpted, reopening in
# Sculpt.
#
# The fixture that pins the generalized SCUL rule -- a sculpt entry's body may
# have CONS or IMPT as its source. There is no CONS section here at all, so a
# reader that still required one refuses the file rather than opening half of a
# body; every build before IMPORT-01B did exactly that.
function New-ImportedSculptFile {
    $sceneBodies = @(
        [pscustomobject]@{ ObjectId = 1; Transform = (New-CanonicalImportedPlacement) }
    )
    $scne = New-Section 'SCNE' 1 $true (New-ScenePayload $sceneBodies 2 1)
    # SCUL is the required branch of a Sculpt project; IMPT is ALWAYS required.
    $scul = New-Section 'SCUL' 1 $true (New-SculptPayload @((New-CanonicalImportedSculptEntry 1)))
    $impt = New-Section 'IMPT' 1 $true (New-ImportedPayload @((New-CanonicalImportedEntry 1)))
    return New-ForgeFile 2 @($scne, $scul, $impt) 6
}

# ALL FOUR COMBINATIONS at once: a plain Construction Body, a Construction Body
# with a sculpt mesh, a plain Imported Mesh, and an Imported Mesh with a sculpt
# mesh.
#
# The representations interleave and the SCUL entries sit over bodies of BOTH
# source kinds, so a reader that assumed a contiguous block of either, or that
# keyed a sculpt entry to a Construction body, fails here.
function New-MixedImportedSculptFile {
    $sceneBodies = @(
        [pscustomobject]@{ ObjectId = 1; Transform = @(0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0) },
        [pscustomobject]@{ ObjectId = 2; Transform = @(1.5, 0.5, -2.0, 370.0, 0.0, 90.0, 1.0, 1.0, 1.0) },
        [pscustomobject]@{ ObjectId = 3; Transform = (New-CanonicalImportedPlacement) },
        [pscustomobject]@{ ObjectId = 4; Transform = @(-2.5, 1.25, 0.5, 0.0, 45.0, 0.0, 1.0, 2.0, 1.0) }
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
            Positions       = @(0.0,  0.0,  0.0,
                                1.5,  0.0,  0.0,
                                0.0,  1.25, 0.0,
                                0.25, 0.5,  1.75)
            Indices         = @(0, 1, 2, 0, 1, 3, 0, 2, 3, 1, 2, 3)
        },
        (New-CanonicalImportedSculptEntry 4)
    )
    $importedEntries = @((New-CanonicalImportedEntry 3), (New-CanonicalImportedEntry 4))
    $scne = New-Section 'SCNE' 1 $true (New-ScenePayload $sceneBodies 5 4)
    $cons = New-Section 'CONS' 1 $false (New-ConstructionPayload $sourceBodies)
    $scul = New-Section 'SCUL' 1 $true (New-SculptPayload $sculptEntries)
    $impt = New-Section 'IMPT' 1 $true (New-ImportedPayload $importedEntries)
    return New-ForgeFile 2 @($scne, $cons, $scul, $impt) 7
}

# ---------------------------------------------------------------------------
# The CAD-R0-A1A2 fixtures
# ---------------------------------------------------------------------------
#
# Every coordinate, size and depth is an exact binary fraction.

# CAD RECTANGLE: one body, a rectangle on the XZ plane extruded along +Y, at a
# placement with 370 degrees and a non-uniform scale. No CONS at all: a CAD
# Body needs no Construction Source standing in for it. CADB is ALWAYS
# required, on IMPT's terms, and the header announces it with bit3.
function New-CadRectangleFile {
    $sceneBodies = @(
        [pscustomobject]@{ ObjectId = 1; Transform = @(0.5, -0.25, 1.25, 370.0, -45.5, 12.25, 1.25, 2.0, 0.5) }
    )
    $cadBodies = @(
        [pscustomobject]@{
            ObjectId = 1; PlaneCode = 2; NextEntityId = 2; ProfileEntityId = 1
            DirectionCode = 1; Depth = 1.5
            Entities = @([pscustomobject]@{ Id = 1; KindCode = 3; Values = @(0.5, 0.25, 2.0, 1.0) })
        }
    )
    $scne = New-Section 'SCNE' 1 $true (New-ScenePayload $sceneBodies 2 1)
    $cadb = New-Section 'CADB' 1 $true (New-CadPayload $cadBodies)
    return New-ForgeFile 1 @($scne, $cadb) 8
}

# CAD CIRCLE: one body, a circle on the YZ plane extruded AGAINST its normal.
function New-CadCircleFile {
    $sceneBodies = @(
        [pscustomobject]@{ ObjectId = 1; Transform = @(0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0) }
    )
    $cadBodies = @(
        [pscustomobject]@{
            ObjectId = 1; PlaneCode = 3; NextEntityId = 2; ProfileEntityId = 1
            DirectionCode = 2; Depth = 0.5
            Entities = @([pscustomobject]@{ Id = 1; KindCode = 4; Values = @(-0.5, 0.5, 0.75) })
        }
    )
    $scne = New-Section 'SCNE' 1 $true (New-ScenePayload $sceneBodies 2 1)
    $cadb = New-Section 'CADB' 1 $true (New-CadPayload $cadBodies)
    return New-ForgeFile 1 @($scne, $cadb) 8
}

# MIXED CAD: a Construction Body beside two CAD Bodies -- a closed polyline
# profile with an unrelated open line in the same sketch, and a loop of three
# lines -- so a SPARSE CONS sits next to a CADB and the CADB carries every
# entity kind and both profile-closing rules.
function New-MixedCadFile {
    $sceneBodies = @(
        [pscustomobject]@{ ObjectId = 1; Transform = @(0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0) },
        [pscustomobject]@{ ObjectId = 2; Transform = @(1.5, 0.5, -2.0, 370.0, 0.0, 90.0, 1.0, 1.0, 1.0) },
        [pscustomobject]@{ ObjectId = 3; Transform = @(-2.5, 1.25, 0.5, 0.0, 45.0, 0.0, 1.0, 2.0, 1.0) }
    )
    $sourceBodies = @(
        [pscustomobject]@{ ObjectId = 1; PrimitiveCode = 1
                           Parameters = $script:CanonicalSharedParameters
                           Features = New-PrimitiveSourceFeature }
    )
    $cadBodies = @(
        [pscustomobject]@{
            ObjectId = 2; PlaneCode = 1; NextEntityId = 3; ProfileEntityId = 1
            DirectionCode = 1; Depth = 2.0
            Entities = @(
                [pscustomobject]@{ Id = 1; KindCode = 2; Closed = $true
                                   Values = @(0.0, 0.0, 2.0, 0.0, 2.0, 1.0, 1.0, 2.0, 0.0, 1.0) },
                [pscustomobject]@{ Id = 2; KindCode = 1; Values = @(3.0, 3.0, 4.0, 4.5) })
        },
        [pscustomobject]@{
            ObjectId = 3; PlaneCode = 2; NextEntityId = 4; ProfileEntityId = 1
            DirectionCode = 1; Depth = 0.25
            Entities = @(
                [pscustomobject]@{ Id = 1; KindCode = 1; Values = @(0.0, 0.0, 2.0, 0.0) },
                [pscustomobject]@{ Id = 2; KindCode = 1; Values = @(2.0, 0.0, 0.0, 2.0) },
                [pscustomobject]@{ Id = 3; KindCode = 1; Values = @(0.0, 2.0, 0.0, 0.0) })
        }
    )
    $scne = New-Section 'SCNE' 1 $true (New-ScenePayload $sceneBodies 4 2)
    $cons = New-Section 'CONS' 1 $true (New-ConstructionPayload $sourceBodies)
    $cadb = New-Section 'CADB' 1 $true (New-CadPayload $cadBodies)
    return New-ForgeFile 1 @($scne, $cons, $cadb) 9
}

# CAD BAD PLANE: the rectangle fixture with its workplane code set to 9 and
# the CADB payload's CRC RECOMPUTED, so every length and checksum is right and
# only the semantic check can refuse it.
function New-CadBadPlaneFixture {
    param([byte[]] $Rectangle)
    $bytes = $Rectangle.Clone()
    # Header 28, then SCNE (24 + 4 + 8 + 8 + 80) = 124 bytes, so the CADB
    # section header starts at 152 and its payload at 176. bodyCount(4) +
    # objectId(8) puts the plane code at payload + 12.
    $cadHeader = 28 + 24 + 100
    $payloadStart = $cadHeader + 24
    $payloadBytes = [System.BitConverter]::ToUInt64((ConvertTo-LittleEndian $bytes[($cadHeader + 8)..($cadHeader + 15)]), 0)
    $bytes[$payloadStart + 12] = 9
    $payload = $bytes[$payloadStart..($payloadStart + $payloadBytes - 1)]
    Set-U32At $bytes ($cadHeader + 16) (Get-Crc32 $payload)
    return $bytes
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
$cadRectangle = New-CadRectangleFile

# ---------------------------------------------------------------------------
# Stage 018A: the two SCNE v2 fixtures
# ---------------------------------------------------------------------------

# OBJECT STATE: three Construction Bodies carrying, between them, every piece of
# per-body state SCNE v2 adds -- a stored NAME, a HIDDEN body and a LOCKED one.
# The first body is named and locked, the second is hidden, the third is a plain
# default so the file also proves a v2 section still writes an unmarked body.
#
# This is the compatibility fixture for the version bump: a reader that
# understands v2 must come back with exactly these three states, and a reader
# that does not must refuse the file rather than open it with everything visible
# and unlocked.
function New-ObjectStateFile {
    $sceneBodies = @(
        [pscustomobject]@{ ObjectId = 1; Transform = $script:IdentityPlacement
                           Name = 'housing'; Locked = $true },
        [pscustomobject]@{ ObjectId = 2
                           Transform = @(2.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0)
                           Hidden = $true },
        [pscustomobject]@{ ObjectId = 3
                           Transform = @(-2.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0) }
    )
    $sourceBodies = @(
        [pscustomobject]@{ ObjectId = 1; PrimitiveCode = 1
                           Parameters = $script:CanonicalSharedParameters
                           Features = New-PrimitiveSourceFeature },
        [pscustomobject]@{ ObjectId = 2; PrimitiveCode = 1
                           Parameters = $script:CanonicalSharedParameters
                           Features = New-PrimitiveSourceFeature },
        [pscustomobject]@{ ObjectId = 3; PrimitiveCode = 1
                           Parameters = $script:CanonicalSharedParameters
                           Features = New-PrimitiveSourceFeature }
    )
    $scne = New-Section 'SCNE' 2 $true (New-ScenePayload $sceneBodies 4 1 -V2)
    $cons = New-Section 'CONS' 1 $true (New-ConstructionPayload $sourceBodies)
    return New-ForgeFile 1 @($scne, $cons) 1
}

# OBJECT STATE, BAD FLAGS: the same shape with a RESERVED flag bit set on the
# first body. CONSTRUCTED with the bad value in place rather than generated and
# then mutated, exactly as the four corrupt v2/v3 CAD fixtures are, so the file
# is a deliberate statement about the format and not a patched byte.
#
# The decoder must refuse it (`BadPayload`): a future flag this build cannot
# honour must never be silently masked off, because that would open a project
# with a state the writer meant and the reader dropped.
function New-ObjectStateBadFlagsFile {
    $sceneBodies = @(
        [pscustomobject]@{ ObjectId = 1; Transform = $script:IdentityPlacement },
        [pscustomobject]@{ ObjectId = 2
                           Transform = @(2.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0) }
    )
    $sourceBodies = @(
        [pscustomobject]@{ ObjectId = 1; PrimitiveCode = 1
                           Parameters = $script:CanonicalSharedParameters
                           Features = New-PrimitiveSourceFeature },
        [pscustomobject]@{ ObjectId = 2; PrimitiveCode = 1
                           Parameters = $script:CanonicalSharedParameters
                           Features = New-PrimitiveSourceFeature }
    )
    # The payload is written here rather than through New-ScenePayload, because
    # that function can only produce LEGAL flag bytes -- which is the right
    # shape for it and the reason this one is spelled out.
    $p = New-ByteBuffer
    Add-U32 $p ([uint32] $sceneBodies.Count)
    Add-U64 $p ([uint64] 3)
    Add-U64 $p ([uint64] 1)
    $first = $true
    foreach ($body in $sceneBodies) {
        Add-U64 $p ([uint64] $body.ObjectId)
        foreach ($value in $body.Transform) { Add-F64 $p $value }
        # 0x04 is reserved in v2. Set on the first body only.
        if ($first) { Add-U8 $p ([byte] 0x04) } else { Add-U8 $p ([byte] 0x00) }
        Add-U16 $p ([uint16] 0)
        $first = $false
    }
    $scne = New-Section 'SCNE' 2 $true $p.ToArray()
    $cons = New-Section 'CONS' 1 $true (New-ConstructionPayload $sourceBodies)
    return New-ForgeFile 1 @($scne, $cons) 1
}

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
    'imported_sculpt_v1.forge'        = (New-ImportedSculptFile)
    'mixed_imported_sculpt_v1.forge'  = (New-MixedImportedSculptFile)
    'cad_rectangle_v1.forge'          = $cadRectangle
    'cad_circle_v1.forge'             = (New-CadCircleFile)
    'mixed_cad_v1.forge'              = (New-MixedCadFile)
    'cad_bad_plane_v1.forge'          = (New-CadBadPlaneFixture $cadRectangle)
    'cad_face_sketch_cap_v2.forge'    = (New-CadFaceSketchCapFile)
    'cad_face_sketch_side_v2.forge'   = (New-CadFaceSketchSideFile)
    'cad_face_chain_v2.forge'         = (New-CadFaceChainFile)
    'mixed_cad_face_v2.forge'         = (New-MixedCadFaceFile)
    'cad_bad_face_ref_v2.forge'       = (New-CadBadFaceRefFile)
    'cad_dependency_cycle_v2.forge'   = (New-CadDependencyCycleFile)
    'cad_arc_profile_v3.forge'        = (New-CadArcProfileFile)
    'cad_spline_profile_v3.forge'     = (New-CadSplineProfileFile)
    'cad_mixed_curve_profile_v3.forge' = (New-CadMixedCurveProfileFile)
    'cad_face_curve_v3.forge'         = (New-CadFaceCurveFile)
    'cad_bad_arc_v3.forge'            = (New-CadBadArcFile)
    'cad_bad_spline_v3.forge'         = (New-CadBadSplineFile)
    'cad_symmetric_v4.forge'          = (New-CadSymmetricFile)
    'cad_two_sides_v4.forge'          = (New-CadTwoSidesFile)
    'cad_face_extent_v4.forge'        = (New-CadFaceExtentFile)
    'mixed_cad_extent_v4.forge'       = (New-MixedCadExtentFile)
    'cad_bad_extent_v4.forge'         = (New-CadBadExtentFile)
    'cad_bad_two_sides_v4.forge'      = (New-CadBadTwoSidesFile)
    'object_state_v2.forge'           = (New-ObjectStateFile)
    'object_state_bad_flags_v2.forge' = (New-ObjectStateBadFlagsFile)
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
Write-Host 'Digests the C++ self-test (FSR1A-12, IMP01A-19, IMP01B-11/12) must assert:'
Write-Host ("  construction:          {0}" -f ($rows | Where-Object Fixture -eq 'construction_multibody_v1.forge').Sha256)
Write-Host ("  sculpt:                {0}" -f ($rows | Where-Object Fixture -eq 'sculpt_mixed_v1.forge').Sha256)
Write-Host ("  imported_only:         {0}" -f ($rows | Where-Object Fixture -eq 'imported_only_v1.forge').Sha256)
Write-Host ("  construction_imported: {0}" -f ($rows | Where-Object Fixture -eq 'construction_imported_v1.forge').Sha256)
Write-Host ("  mixed_imported:        {0}" -f ($rows | Where-Object Fixture -eq 'mixed_imported_v1.forge').Sha256)
Write-Host ("  imported_sculpt:       {0}" -f ($rows | Where-Object Fixture -eq 'imported_sculpt_v1.forge').Sha256)
Write-Host ("  mixed_imported_sculpt: {0}" -f ($rows | Where-Object Fixture -eq 'mixed_imported_sculpt_v1.forge').Sha256)
Write-Host 'Digests the C++ self-test (CADR0-33/34/36) must assert:'
Write-Host ("  cad_rectangle:         {0}" -f ($rows | Where-Object Fixture -eq 'cad_rectangle_v1.forge').Sha256)
Write-Host ("  cad_circle:            {0}" -f ($rows | Where-Object Fixture -eq 'cad_circle_v1.forge').Sha256)
Write-Host ("  mixed_cad:             {0}" -f ($rows | Where-Object Fixture -eq 'mixed_cad_v1.forge').Sha256)
Write-Host ("  cad_bad_plane:         {0}" -f ($rows | Where-Object Fixture -eq 'cad_bad_plane_v1.forge').Sha256)
Write-Host 'Digests the C++ self-test (CADA3-46..51) must assert:'
Write-Host ("  cad_face_sketch_cap:   {0}" -f ($rows | Where-Object Fixture -eq 'cad_face_sketch_cap_v2.forge').Sha256)
Write-Host ("  cad_face_sketch_side:  {0}" -f ($rows | Where-Object Fixture -eq 'cad_face_sketch_side_v2.forge').Sha256)
Write-Host ("  cad_face_chain:        {0}" -f ($rows | Where-Object Fixture -eq 'cad_face_chain_v2.forge').Sha256)
Write-Host ("  mixed_cad_face:        {0}" -f ($rows | Where-Object Fixture -eq 'mixed_cad_face_v2.forge').Sha256)
Write-Host ("  cad_bad_face_ref:      {0}" -f ($rows | Where-Object Fixture -eq 'cad_bad_face_ref_v2.forge').Sha256)
Write-Host ("  cad_dependency_cycle:  {0}" -f ($rows | Where-Object Fixture -eq 'cad_dependency_cycle_v2.forge').Sha256)
Write-Host 'Digests the C++ self-test (CADUXR1-38) must assert:'
Write-Host ("  cad_arc_profile:       {0}" -f ($rows | Where-Object Fixture -eq 'cad_arc_profile_v3.forge').Sha256)
Write-Host ("  cad_spline_profile:    {0}" -f ($rows | Where-Object Fixture -eq 'cad_spline_profile_v3.forge').Sha256)
Write-Host ("  cad_mixed_curve:       {0}" -f ($rows | Where-Object Fixture -eq 'cad_mixed_curve_profile_v3.forge').Sha256)
Write-Host ("  cad_face_curve:        {0}" -f ($rows | Where-Object Fixture -eq 'cad_face_curve_v3.forge').Sha256)
Write-Host ("  cad_bad_arc:           {0}" -f ($rows | Where-Object Fixture -eq 'cad_bad_arc_v3.forge').Sha256)
Write-Host ("  cad_bad_spline:        {0}" -f ($rows | Where-Object Fixture -eq 'cad_bad_spline_v3.forge').Sha256)
Write-Host 'Digests the C++ self-test (CADEXT-10 c..h) must assert:'
Write-Host ("  cad_symmetric:         {0}" -f ($rows | Where-Object Fixture -eq 'cad_symmetric_v4.forge').Sha256)
Write-Host ("  cad_two_sides:         {0}" -f ($rows | Where-Object Fixture -eq 'cad_two_sides_v4.forge').Sha256)
Write-Host ("  cad_face_extent:       {0}" -f ($rows | Where-Object Fixture -eq 'cad_face_extent_v4.forge').Sha256)
Write-Host ("  mixed_cad_extent:      {0}" -f ($rows | Where-Object Fixture -eq 'mixed_cad_extent_v4.forge').Sha256)
Write-Host ("  cad_bad_extent:        {0}" -f ($rows | Where-Object Fixture -eq 'cad_bad_extent_v4.forge').Sha256)
Write-Host ("  cad_bad_two_sides:     {0}" -f ($rows | Where-Object Fixture -eq 'cad_bad_two_sides_v4.forge').Sha256)
Write-Host 'Digests the C++ self-test (OBJ018A-15/16) must assert:'
Write-Host ("  object_state:          {0}" -f ($rows | Where-Object Fixture -eq 'object_state_v2.forge').Sha256)
Write-Host ("  object_state_bad_flags:{0}" -f ($rows | Where-Object Fixture -eq 'object_state_bad_flags_v2.forge').Sha256)
