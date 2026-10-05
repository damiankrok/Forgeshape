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

# ---------------------------------------------------------------------------
# CADB v5 (CAD-VERTICAL-SLICE-R1): regions with holes and the retained feature
# chain
# ---------------------------------------------------------------------------
#
# The v5 record is the v4 record with a TAIL after the first feature's
# entities: the first feature's REGIONS block, then laterFeatureCount (0..15)
# and that many LATER features in chain (application) order -- see
# DATA_PACKAGE_SPEC.md 7f. v5 ALWAYS writes the v2 support block and the v4
# extent fields and understands the v3 entity kinds, because a version is a
# superset of the one below it.
#
#   REGIONS    holeCount and the FIRST chosen region's hole anchors, then
#              additionalRegionCount and, per further region, its outer-loop
#              anchor with its own hole count and hole anchors
#   operation  2 Add (union), 3 Cut (difference). 1 New Body is what the first
#              feature IS, and is never stored as a later one
#
# A later feature has NO workplane code and NO support block of its own: its
# sketch is authored on its own local XY and placed by a face of an EARLIER
# feature of the SAME body, named by feature id, face token and that feature's
# lineage token. Nothing derived is stored -- no loop, no region polygon, and
# nothing the boolean kernel produces.
#
# These writers encode EXACTLY what the objects handed to them say and validate
# nothing, so a corrupt v5 fixture is CONSTRUCTED by building its body with the
# bad value in place, never by patching bytes afterwards.

# entityCount, then every entity as 7b and 7d lay it out. Shared by the first
# feature and every later one, which carry the same entity list shape.
function Add-CadEntityList {
    param($Buffer, $Entities)
    $list = @(if ($null -ne $Entities) { $Entities })
    Add-U32 $Buffer ([uint32] $list.Count)
    foreach ($entity in $list) {
        Add-U32 $Buffer ([uint32] $entity.Id)
        Add-U8  $Buffer $entity.KindCode
        switch ($entity.KindCode) {
            1 { foreach ($value in $entity.Values) { Add-F64 $Buffer $value } }
            2 {
                Add-U8  $Buffer $(if ($entity.Closed) { 1 } else { 0 })
                Add-U32 $Buffer ([uint32] ($entity.Values.Count / 2))
                foreach ($value in $entity.Values) { Add-F64 $Buffer $value }
            }
            3 { foreach ($value in $entity.Values) { Add-F64 $Buffer $value } }
            4 { foreach ($value in $entity.Values) { Add-F64 $Buffer $value } }
            5 { foreach ($value in $entity.Values) { Add-F64 $Buffer $value } }
            6 {
                Add-U32 $Buffer ([uint32] ($entity.Values.Count / 2))
                foreach ($value in $entity.Values) { Add-F64 $Buffer $value }
            }
        }
    }
}

# The REGIONS block. A selection of one region without holes -- the R0 profile,
# which profileEntityId has named since v1 -- is two zero counts, 8 bytes.
#
# Every list is re-wrapped with @(if ...) rather than @(...): @($null) is an
# array holding ONE null, which would write a count of 1 beside no anchor.
function Add-CadRegions {
    param($Buffer, $Regions)
    $holes = @(if ($null -ne $Regions -and $null -ne $Regions.Holes) { $Regions.Holes })
    $additional = @(if ($null -ne $Regions -and $null -ne $Regions.Additional) { $Regions.Additional })
    Add-U32 $Buffer ([uint32] $holes.Count)
    foreach ($anchor in $holes) { Add-U32 $Buffer ([uint32] $anchor) }
    Add-U32 $Buffer ([uint32] $additional.Count)
    foreach ($region in $additional) {
        Add-U32 $Buffer ([uint32] $region.OuterAnchorId)
        $regionHoles = @(if ($null -ne $region.Holes) { $region.Holes })
        Add-U32 $Buffer ([uint32] $regionHoles.Count)
        foreach ($anchor in $regionHoles) { Add-U32 $Buffer ([uint32] $anchor) }
    }
}

# One later feature: the 56-byte fixed record, its entities and its REGIONS.
function Add-CadLaterFeature {
    param($Buffer, $Feature)
    Add-U32 $Buffer ([uint32] $Feature.FeatureId)
    Add-U8  $Buffer $Feature.OperationCode
    Add-U32 $Buffer ([uint32] $Feature.SupportFeatureId)
    Add-U8  $Buffer $Feature.FaceKindCode
    Add-U32 $Buffer ([uint32] $Feature.EdgeEntityId)
    Add-U32 $Buffer ([uint32] $Feature.EdgeLocalIndex)
    Add-U64 $Buffer ([uint64] $Feature.LineageToken)
    Add-U32 $Buffer ([uint32] $Feature.NextEntityId)
    Add-U32 $Buffer ([uint32] $Feature.ProfileEntityId)
    Add-U8  $Buffer $Feature.ExtentCode
    Add-U8  $Buffer $Feature.DirectionCode
    Add-F64 $Buffer $Feature.Depth
    Add-F64 $Buffer $Feature.Second
    Add-CadEntityList $Buffer $Feature.Entities
    Add-CadRegions $Buffer $Feature.Regions
}

function New-CadPayloadV5 {
    param($Bodies)
    $p = New-ByteBuffer
    $bodyList = @($Bodies)
    Add-U32 $p ([uint32] $bodyList.Count)
    foreach ($body in $bodyList) {
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
        Add-U8  $p $body.ExtentCode
        Add-U8  $p $body.DirectionCode
        Add-F64 $p $body.Depth
        Add-F64 $p $body.Second
        Add-CadEntityList $p $body.Entities
        # --- the v5 tail ---
        Add-CadRegions $p $body.Regions
        $later = @(if ($null -ne $body.LaterFeatures) { $body.LaterFeatures })
        Add-U32 $p ([uint32] $later.Count)
        foreach ($feature in $later) { Add-CadLaterFeature $p $feature }
    }
    return $p.ToArray()
}

# The side faces one sketch loop contributes to a signature, in the loop's
# counter-clockwise polygon order: one per polygon edge, numbered by the entity
# that edge came from and its 0-based local index there. For a single-entity
# loop that is a rectangle's four planar edges from its (-w/2, -h/2) corner, or
# a circle's 32 curved ones (kSketchCircleSegments) from +U. $Curved marks an
# edge that facets a curved surface, which is never eligible.
function New-CadLoopSides {
    param([uint32] $EntityId, [int] $EdgeCount, [bool] $Curved)
    $sides = New-Object System.Collections.Generic.List[object]
    for ($k = 0; $k -lt $EdgeCount; $k++) {
        $sides.Add([pscustomobject]@{
            EdgeEntityId = $EntityId; EdgeLocalIndex = [uint32] $k; Eligible = (-not $Curved) })
    }
    return , $sides.ToArray()
}

# One chosen region as the signature sees it: its outer loop's anchor and
# sides, and each hole's anchor and sides.
function New-CadRegionFaces {
    param([uint32] $OuterAnchorId, $OuterSides, $Holes)
    return [pscustomobject]@{
        OuterAnchorId = $OuterAnchorId
        OuterSides    = @(if ($null -ne $OuterSides) { $OuterSides })
        Holes         = @(if ($null -ne $Holes) { $Holes })
    }
}

function New-CadHoleFaces {
    param([uint32] $AnchorId, $Sides)
    return [pscustomobject]@{ AnchorId = $AnchorId; Sides = @(if ($null -ne $Sides) { $Sides }) }
}

# The lineage token GENERALIZED to regions, as DATA_PACKAGE_SPEC.md 7f states
# it: the 7c signature of a feature's own face topology, whose face list is
#
#   CapPlane, CapFar,
#   then for each chosen region in ascending outer-anchor order: one Side per
#   edge of its OUTER loop, then for each of its holes in ascending anchor
#   order, one Side per edge of that HOLE loop -- each in its loop's polygon
#   order.
#
# Code and mix are exactly 7c's (Get-CadFaceTokenCode, Add-Fnv64), and so is
# the zero guard. Eligibility is the side's own answer, and 0 for EVERY face --
# the caps included -- when the feature is a Cut, because a Cut leaves its faces
# behind as the inside of a pocket. For one region without holes this reduces to
# 7c exactly, which the fixture section below asserts against
# Get-CadTopologySignature before it writes a byte.
function Get-CadFeatureTopologySignature {
    param([uint32] $ProfileEntityId, $Regions, [switch] $Cut)
    $faces = New-Object System.Collections.Generic.List[object]
    $faces.Add([pscustomobject]@{ Kind = 0; EdgeEntityId = [uint32] 0; EdgeLocalIndex = [uint32] 0; Eligible = $true })
    $faces.Add([pscustomobject]@{ Kind = 1; EdgeEntityId = [uint32] 0; EdgeLocalIndex = [uint32] 0; Eligible = $true })
    $regionList = @(if ($null -ne $Regions) { $Regions })
    foreach ($region in @($regionList | Sort-Object { [uint32] $_.OuterAnchorId })) {
        foreach ($side in @($region.OuterSides)) {
            $faces.Add([pscustomobject]@{ Kind = 2; EdgeEntityId = [uint32] $side.EdgeEntityId
                                          EdgeLocalIndex = [uint32] $side.EdgeLocalIndex
                                          Eligible = [bool] $side.Eligible })
        }
        $holeList = @(if ($null -ne $region.Holes) { $region.Holes })
        foreach ($hole in @($holeList | Sort-Object { [uint32] $_.AnchorId })) {
            foreach ($side in @($hole.Sides)) {
                $faces.Add([pscustomobject]@{ Kind = 2; EdgeEntityId = [uint32] $side.EdgeEntityId
                                              EdgeLocalIndex = [uint32] $side.EdgeLocalIndex
                                              Eligible = [bool] $side.Eligible })
            }
        }
    }
    $hash = [System.Numerics.BigInteger]::Parse('14695981039346656037')
    $hash = Add-Fnv64 $hash ([System.Numerics.BigInteger] $ProfileEntityId)
    $hash = Add-Fnv64 $hash ([System.Numerics.BigInteger] $faces.Count)
    foreach ($face in $faces) {
        $hash = Add-Fnv64 $hash (Get-CadFaceTokenCode $face.Kind $face.EdgeEntityId $face.EdgeLocalIndex)
        $eligible = $face.Eligible -and (-not $Cut)
        $hash = Add-Fnv64 $hash ([System.Numerics.BigInteger] $(if ($eligible) { 1 } else { 0 }))
    }
    if ($hash.IsZero) { $hash = [System.Numerics.BigInteger]::One }
    return [uint64]::Parse($hash.ToString())
}

# The two producer signatures the v5 corpus stands on. The plain rectangle
# (entity 1, no hole) is the 7c six-face signature; the holed one adds the
# circle hole's 32 curved, ineligible sides (entity 2) after the rectangle's
# four -- 38 faces.
function Get-CadPlainRectangleLineage {
    return Get-CadFeatureTopologySignature 1 @(
        New-CadRegionFaces 1 (New-CadLoopSides 1 4 $false) @())
}

function Get-CadHoledRectangleLineage {
    return Get-CadFeatureTopologySignature 1 @(
        New-CadRegionFaces 1 (New-CadLoopSides 1 4 $false) @(
            New-CadHoleFaces 2 (New-CadLoopSides 2 32 $true)))
}

function New-CadRectangleEntity {
    param([uint32] $Id, [double] $CentreU, [double] $CentreV, [double] $Width, [double] $Height)
    return [pscustomobject]@{ Id = $Id; KindCode = 3; Values = @($CentreU, $CentreV, $Width, $Height) }
}

function New-CadCircleEntity {
    param([uint32] $Id, [double] $CentreU, [double] $CentreV, [double] $Radius)
    return [pscustomobject]@{ Id = $Id; KindCode = 4; Values = @($CentreU, $CentreV, $Radius) }
}

# A region selection: the FIRST chosen region's hole anchors, and any further
# chosen regions as { OuterAnchorId; Holes }.
function New-CadRegionSelection {
    param($Holes, $Additional)
    return [pscustomobject]@{
        Holes      = @(if ($null -ne $Holes) { $Holes })
        Additional = @(if ($null -ne $Additional) { $Additional })
    }
}

# A world-supported first feature carrying the v5 tail.
function New-CadV5Body {
    param([int] $ObjectId, [int] $PlaneCode, [uint32] $NextEntityId, [uint32] $ProfileEntityId,
          [int] $ExtentCode, [int] $DirectionCode, [double] $Depth, [double] $Second,
          $Entities, $Regions, $LaterFeatures)
    return [pscustomobject]@{
        ObjectId = $ObjectId; PlaneCode = $PlaneCode; Support = $null
        NextEntityId = $NextEntityId; ProfileEntityId = $ProfileEntityId
        ExtentCode = $ExtentCode; DirectionCode = $DirectionCode; Depth = $Depth; Second = $Second
        Entities = @(if ($null -ne $Entities) { $Entities })
        Regions = $Regions
        LaterFeatures = @(if ($null -ne $LaterFeatures) { $LaterFeatures })
    }
}

function New-CadLaterFeature {
    param([uint32] $FeatureId, [int] $OperationCode, [uint32] $SupportFeatureId,
          [int] $FaceKindCode, [uint32] $EdgeEntityId, [uint32] $EdgeLocalIndex,
          [uint64] $LineageToken, [uint32] $NextEntityId, [uint32] $ProfileEntityId,
          [int] $ExtentCode, [int] $DirectionCode, [double] $Depth, [double] $Second,
          $Entities, $Regions)
    return [pscustomobject]@{
        FeatureId = $FeatureId; OperationCode = $OperationCode; SupportFeatureId = $SupportFeatureId
        FaceKindCode = $FaceKindCode; EdgeEntityId = $EdgeEntityId; EdgeLocalIndex = $EdgeLocalIndex
        LineageToken = $LineageToken; NextEntityId = $NextEntityId; ProfileEntityId = $ProfileEntityId
        ExtentCode = $ExtentCode; DirectionCode = $DirectionCode; Depth = $Depth; Second = $Second
        Entities = @(if ($null -ne $Entities) { $Entities })
        Regions = $Regions
    }
}

# Every v5 fixture is one world-supported CAD body in a Construction project,
# with the SCNE record the single-body v4 fixtures write: ObjectId 1 at the
# identity placement, allocator high-water mark 2, SCNE v1.
function New-CadV5File {
    param($Body)
    $sceneBodies = @([pscustomobject]@{ ObjectId = 1; Transform = $script:IdentityPlacement })
    $scne = New-Section 'SCNE' 1 $true (New-ScenePayload $sceneBodies 2 1)
    $cadb = New-Section 'CADB' 5 $true (New-CadPayloadV5 @($Body))
    return New-ForgeFile 1 @($scne, $cadb) 8
}

# The holed base: a 4 x 3 m rectangle (entity 1) around a 0.8 m-radius circle
# (entity 2) on XY, extruded One Side 1 m along +Z as the region BETWEEN them.
# $HoleAnchors is the stored hole list: [2] is the one the sketch derives.
function New-CadHoledBaseBody {
    param($HoleAnchors, $LaterFeatures)
    return New-CadV5Body -ObjectId 1 -PlaneCode 1 -NextEntityId 3 -ProfileEntityId 1 `
        -ExtentCode 1 -DirectionCode 1 -Depth 1.0 -Second 0.0 `
        -Entities @((New-CadRectangleEntity 1 0.0 0.0 4.0 3.0), (New-CadCircleEntity 2 0.0 0.0 0.8)) `
        -Regions (New-CadRegionSelection -Holes $HoleAnchors) -LaterFeatures $LaterFeatures
}

# CAD REGION HOLE: the holed base and nothing else -- the selection is one
# region WITH a hole, which no earlier version can say.
function New-CadRegionHoleFile {
    param($HoleAnchors = @(2))
    return New-CadV5File (New-CadHoledBaseBody -HoleAnchors $HoleAnchors)
}

# CAD FEATURE ADD: a 2 x 2 m block extruded 1 m along +Z, and ONE later Add --
# a 0.8 m square on the block's far cap (feature 1, CapFar) extruded 0.5 m along
# the face normal. The three identity fields are parameters so each corrupt
# fixture below is CONSTRUCTED with its bad value in place.
function New-CadFeatureAddFile {
    param([uint32] $FeatureId = 2, [int] $OperationCode = 2, [uint32] $SupportFeatureId = 1)
    $add = New-CadLaterFeature -FeatureId $FeatureId -OperationCode $OperationCode `
        -SupportFeatureId $SupportFeatureId -FaceKindCode 2 -EdgeEntityId 0 -EdgeLocalIndex 0 `
        -LineageToken (Get-CadPlainRectangleLineage) -NextEntityId 2 -ProfileEntityId 1 `
        -ExtentCode 1 -DirectionCode 1 -Depth 0.5 -Second 0.0 `
        -Entities @(New-CadRectangleEntity 1 0.0 0.0 0.8 0.8) -Regions (New-CadRegionSelection)
    $body = New-CadV5Body -ObjectId 1 -PlaneCode 1 -NextEntityId 2 -ProfileEntityId 1 `
        -ExtentCode 1 -DirectionCode 1 -Depth 1.0 -Second 0.0 `
        -Entities @(New-CadRectangleEntity 1 0.0 0.0 2.0 2.0) `
        -Regions (New-CadRegionSelection) -LaterFeatures @($add)
    return New-CadV5File $body
}

# CAD FEATURE CUT: the same block and ONE later Cut -- a 0.3 m-radius circle on
# the far cap, extruded 0.5 m AGAINST the face normal, into the block.
function New-CadFeatureCutFile {
    $cut = New-CadLaterFeature -FeatureId 2 -OperationCode 3 -SupportFeatureId 1 `
        -FaceKindCode 2 -EdgeEntityId 0 -EdgeLocalIndex 0 `
        -LineageToken (Get-CadPlainRectangleLineage) -NextEntityId 2 -ProfileEntityId 1 `
        -ExtentCode 1 -DirectionCode 2 -Depth 0.5 -Second 0.0 `
        -Entities @(New-CadCircleEntity 1 0.0 0.0 0.3) -Regions (New-CadRegionSelection)
    $body = New-CadV5Body -ObjectId 1 -PlaneCode 1 -NextEntityId 2 -ProfileEntityId 1 `
        -ExtentCode 1 -DirectionCode 1 -Depth 1.0 -Second 0.0 `
        -Entities @(New-CadRectangleEntity 1 0.0 0.0 2.0 2.0) `
        -Regions (New-CadRegionSelection) -LaterFeatures @($cut)
    return New-CadV5File $body
}

# CAD FEATURE CHAIN: the holed base, then an Add and a Cut, both standing on
# feature 1's far cap at the 38-face holed signature -- a Symmetric 0.6 m square
# boss at (1.4, 0), 0.25 m each side of the cap, and then a 0.3 m-radius pocket
# at (-1.4, 0), 0.5 m against the face normal. Applied in chain order.
function New-CadFeatureChainFile {
    $lineage = Get-CadHoledRectangleLineage
    $add = New-CadLaterFeature -FeatureId 2 -OperationCode 2 -SupportFeatureId 1 `
        -FaceKindCode 2 -EdgeEntityId 0 -EdgeLocalIndex 0 `
        -LineageToken $lineage -NextEntityId 2 -ProfileEntityId 1 `
        -ExtentCode 2 -DirectionCode 1 -Depth 0.25 -Second 0.0 `
        -Entities @(New-CadRectangleEntity 1 1.4 0.0 0.6 0.6) -Regions (New-CadRegionSelection)
    $cut = New-CadLaterFeature -FeatureId 3 -OperationCode 3 -SupportFeatureId 1 `
        -FaceKindCode 2 -EdgeEntityId 0 -EdgeLocalIndex 0 `
        -LineageToken $lineage -NextEntityId 2 -ProfileEntityId 1 `
        -ExtentCode 1 -DirectionCode 2 -Depth 0.5 -Second 0.0 `
        -Entities @(New-CadCircleEntity 1 -1.4 0.0 0.3) -Regions (New-CadRegionSelection)
    return New-CadV5File (New-CadHoledBaseBody -HoleAnchors @(2) -LaterFeatures @($add, $cut))
}

# ---------------------------------------------------------------------------
# CADB v6 (CAD-V6-S1): the retained sketch table and the selection variant
# ---------------------------------------------------------------------------
#
# v6 is its own body layout, not a tail on v5 -- see DATA_PACKAGE_SPEC.md 7g.
# A body carries its two id high-water marks, then a TABLE of retained
# sketches, then its features in chain order, each naming a sketch BY ID
# rather than carrying one inline:
#
#   body       objectId u64 | nextSketchId u32 | nextFeatureId u32 |
#              sketchCount u32 | SKETCH x sketchCount |
#              featureCount u32 | FEATURE x featureCount
#   SKETCH     sketchId u32 | placement u8 | its payload | nextEntityId u32 |
#              entityCount u32 and the entities exactly as 7b/7d
#   placement  1 workplane      u8 workplaneCode
#              2 body face      the 7c TopoRef (29 bytes); sketch on local XY
#              3 feature face   the 7f support (21 bytes); sketch on local XY
#   FEATURE    featureId u32 | operation u8 | sketchId u32 | extent u8 |
#              direction u8 | depth f64 | secondDistance f64 |
#              selectionKind u8 (1 LoopRegions, 2 PlanarFaces) |
#              1: profileEntityId u32 and the 7f REGIONS block
#              2: FACES
#   FACES      faceCount u32 | per face: CYCLE outer, holeCount u32, CYCLE x holes
#   CYCLE      fragmentCount u32 | FRAGMENT x fragmentCount
#   FRAGMENT   sourceEntityId u32 | sourceEdgeLocalIndex u32 | CUT start |
#              CUT end | reversed u8 (0 or 1)
#   CUT        kind u8 (1 SourceStart, 2 Intersection, 3 SourceEnd); an
#              Intersection only is followed by partnerEntityId u32,
#              partnerEdgeLocalIndex u32 and ordinal u32
#
# A planar face is written EXACTLY as the fixture states it: this builder does
# not derive an arrangement. Each positive fixture below states the canonical
# ref its sketch's arrangement produces (DATA_PACKAGE_SPEC.md 7g lists them),
# and the C++ self-test proves the production encoder, deriving the same face
# from the same sketch, writes the same bytes. As for v5, these writers
# validate nothing, so a corrupt fixture is CONSTRUCTED with its bad value in
# place.

function Add-CadCut {
    param($Buffer, $Cut)
    Add-U8 $Buffer $Cut.Kind
    if ($Cut.Kind -eq 2) {
        Add-U32 $Buffer ([uint32] $Cut.PartnerEntityId)
        Add-U32 $Buffer ([uint32] $Cut.PartnerEdgeLocalIndex)
        Add-U32 $Buffer ([uint32] $Cut.Ordinal)
    }
}

function Add-CadFragmentCycle {
    param($Buffer, $Cycle)
    $fragments = @(if ($null -ne $Cycle) { $Cycle })
    Add-U32 $Buffer ([uint32] $fragments.Count)
    foreach ($fragment in $fragments) {
        Add-U32 $Buffer ([uint32] $fragment.SourceEntityId)
        Add-U32 $Buffer ([uint32] $fragment.SourceEdgeLocalIndex)
        Add-CadCut $Buffer $fragment.Start
        Add-CadCut $Buffer $fragment.End
        Add-U8  $Buffer $(if ($fragment.Reversed) { 1 } else { 0 })
    }
}

# A v6 FACE (7g): the kind code, the edge, and -- for code 4, a FRAGMENT side
# (CAD-V6-S2) -- the two cuts that bound the piece, in the source's order.
function Add-CadFaceV6 {
    param($Buffer, $Support)
    Add-U8  $Buffer $Support.FaceKindCode
    Add-U32 $Buffer ([uint32] $Support.EdgeEntityId)
    Add-U32 $Buffer ([uint32] $Support.EdgeLocalIndex)
    if ($Support.FaceKindCode -eq 4) {
        Add-CadCut $Buffer $Support.FragmentStart
        Add-CadCut $Buffer $Support.FragmentEnd
    }
}

function Add-CadSketchV6 {
    param($Buffer, $Sketch)
    Add-U32 $Buffer ([uint32] $Sketch.SketchId)
    Add-U8  $Buffer $Sketch.PlacementCode
    switch ($Sketch.PlacementCode) {
        1 { Add-U8 $Buffer $Sketch.PlaneCode }
        2 {
            Add-U64 $Buffer ([uint64] $Sketch.Support.ProducerObjectId)
            Add-U32 $Buffer ([uint32] $Sketch.Support.FeatureId)
            Add-CadFaceV6 $Buffer $Sketch.Support
            Add-U64 $Buffer ([uint64] $Sketch.Support.LineageToken)
        }
        3 {
            Add-U32 $Buffer ([uint32] $Sketch.Support.FeatureId)
            Add-CadFaceV6 $Buffer $Sketch.Support
            Add-U64 $Buffer ([uint64] $Sketch.Support.LineageToken)
        }
    }
    Add-U32 $Buffer ([uint32] $Sketch.NextEntityId)
    Add-CadEntityList $Buffer $Sketch.Entities
}

function Add-CadFeatureV6 {
    param($Buffer, $Feature)
    Add-U32 $Buffer ([uint32] $Feature.FeatureId)
    Add-U8  $Buffer $Feature.OperationCode
    Add-U32 $Buffer ([uint32] $Feature.SketchId)
    Add-U8  $Buffer $Feature.ExtentCode
    Add-U8  $Buffer $Feature.DirectionCode
    Add-F64 $Buffer $Feature.Depth
    Add-F64 $Buffer $Feature.Second
    Add-U8  $Buffer $Feature.SelectionKind
    if ($Feature.SelectionKind -eq 1) {
        Add-U32 $Buffer ([uint32] $Feature.ProfileEntityId)
        Add-CadRegions $Buffer $Feature.Regions
    } else {
        $faces = @(if ($null -ne $Feature.Faces) { $Feature.Faces })
        Add-U32 $Buffer ([uint32] $faces.Count)
        foreach ($face in $faces) {
            Add-CadFragmentCycle $Buffer $face.Outer
            $holes = @(if ($null -ne $face.Holes) { $face.Holes })
            Add-U32 $Buffer ([uint32] $holes.Count)
            foreach ($hole in $holes) { Add-CadFragmentCycle $Buffer $hole }
        }
    }
}

function New-CadPayloadV6 {
    param($Bodies)
    $p = New-ByteBuffer
    $bodyList = @($Bodies)
    Add-U32 $p ([uint32] $bodyList.Count)
    foreach ($body in $bodyList) {
        Add-U64 $p ([uint64] $body.ObjectId)
        Add-U32 $p ([uint32] $body.NextSketchId)
        Add-U32 $p ([uint32] $body.NextFeatureId)
        $sketches = @($body.Sketches)
        Add-U32 $p ([uint32] $sketches.Count)
        foreach ($sketch in $sketches) { Add-CadSketchV6 $p $sketch }
        $features = @($body.Features)
        Add-U32 $p ([uint32] $features.Count)
        foreach ($feature in $features) { Add-CadFeatureV6 $p $feature }
    }
    return $p.ToArray()
}

# Cut and fragment literals: the canonical tuple, nothing else.
function New-CadCut {
    param([int] $Kind, [uint32] $PartnerEntityId = 0, [uint32] $PartnerEdgeLocalIndex = 0,
          [uint32] $Ordinal = 0)
    return [pscustomobject]@{ Kind = $Kind; PartnerEntityId = $PartnerEntityId
                              PartnerEdgeLocalIndex = $PartnerEdgeLocalIndex; Ordinal = $Ordinal }
}
function New-CadSourceStart { return New-CadCut 1 }
function New-CadSourceEnd { return New-CadCut 3 }
function New-CadCrossing {
    param([uint32] $PartnerEntityId, [uint32] $PartnerEdgeLocalIndex, [uint32] $Ordinal)
    return New-CadCut 2 $PartnerEntityId $PartnerEdgeLocalIndex $Ordinal
}
function New-CadFragment {
    param([uint32] $SourceEntityId, [uint32] $SourceEdgeLocalIndex, $Start, $End,
          [switch] $Reversed)
    return [pscustomobject]@{ SourceEntityId = $SourceEntityId
                              SourceEdgeLocalIndex = $SourceEdgeLocalIndex
                              Start = $Start; End = $End; Reversed = [bool] $Reversed }
}
function New-CadPlanarFace {
    param($Outer, $Holes)
    return [pscustomobject]@{ Outer = @($Outer); Holes = @(if ($null -ne $Holes) { $Holes }) }
}

function New-CadSketchV6 {
    param([uint32] $SketchId, [int] $PlacementCode, [int] $PlaneCode, $Support,
          [uint32] $NextEntityId, $Entities)
    return [pscustomobject]@{
        SketchId = $SketchId; PlacementCode = $PlacementCode; PlaneCode = $PlaneCode
        Support = $Support; NextEntityId = $NextEntityId
        Entities = @(if ($null -ne $Entities) { $Entities })
    }
}

function New-CadFeatureV6 {
    param([uint32] $FeatureId, [int] $OperationCode, [uint32] $SketchId, [int] $ExtentCode,
          [int] $DirectionCode, [double] $Depth, [double] $Second, [int] $SelectionKind,
          [uint32] $ProfileEntityId, $Regions, $Faces)
    return [pscustomobject]@{
        FeatureId = $FeatureId; OperationCode = $OperationCode; SketchId = $SketchId
        ExtentCode = $ExtentCode; DirectionCode = $DirectionCode; Depth = $Depth; Second = $Second
        SelectionKind = $SelectionKind; ProfileEntityId = $ProfileEntityId; Regions = $Regions
        Faces = @(if ($null -ne $Faces) { $Faces })
    }
}

function New-CadV6Body {
    param([uint32] $NextSketchId, [uint32] $NextFeatureId, $Sketches, $Features)
    return [pscustomobject]@{ ObjectId = 1; NextSketchId = $NextSketchId
                              NextFeatureId = $NextFeatureId
                              Sketches = @($Sketches); Features = @($Features) }
}

# Every v6 fixture is one CAD body in a Construction project, with the SCNE
# record every single-body v4/v5 fixture writes.
function New-CadV6File {
    param($Body)
    $sceneBodies = @([pscustomobject]@{ ObjectId = 1; Transform = $script:IdentityPlacement })
    $scne = New-Section 'SCNE' 1 $true (New-ScenePayload $sceneBodies 2 1)
    $cadb = New-Section 'CADB' 6 $true (New-CadPayloadV6 @($Body))
    return New-ForgeFile 1 @($scne, $cadb) 8
}

function New-CadLineEntity {
    param([uint32] $Id, [double] $U0, [double] $V0, [double] $U1, [double] $V1)
    return [pscustomobject]@{ Id = $Id; KindCode = 1; Values = @($U0, $V0, $U1, $V1) }
}

# A One Side New Body base on a world-XY root sketch.
function New-CadV6Base {
    param([int] $SelectionKind, [uint32] $ProfileEntityId, $Regions, $Faces,
          [double] $Depth = 1.0, [uint32] $SketchId = 1)
    return New-CadFeatureV6 -FeatureId 1 -OperationCode 1 -SketchId $SketchId -ExtentCode 1 `
        -DirectionCode 1 -Depth $Depth -Second 0.0 -SelectionKind $SelectionKind `
        -ProfileEntityId $ProfileEntityId -Regions $Regions -Faces $Faces
}

# CAD SKETCH SHARED: ONE retained sketch -- a 4 x 3 m rectangle (entity 1)
# around a 1 x 1 m square (entity 2) on XY -- extruded by TWO features. The
# base selects the ring AND the square (their union, the whole 4 x 3 block),
# 1 m along +Z; feature 2 is an Add of the square alone, 0.5 m AGAINST the
# normal: a boss under the block, from the very sketch the block came from.
# `$SecondSketchId` is what feature 2 names, so the missing-sketch refusal is
# constructed from this one.
function New-CadSketchSharedFile {
    param([uint32] $SecondSketchId = 1)
    $sketch = New-CadSketchV6 -SketchId 1 -PlacementCode 1 -PlaneCode 1 -NextEntityId 3 `
        -Entities @((New-CadRectangleEntity 1 0.0 0.0 4.0 3.0), (New-CadRectangleEntity 2 0.0 0.0 1.0 1.0))
    $base = New-CadV6Base -SelectionKind 1 -ProfileEntityId 1 `
        -Regions (New-CadRegionSelection -Holes @(2) -Additional @(
            [pscustomobject]@{ OuterAnchorId = 2; Holes = @() }))
    $boss = New-CadFeatureV6 -FeatureId 2 -OperationCode 2 -SketchId $SecondSketchId -ExtentCode 1 `
        -DirectionCode 2 -Depth 0.5 -Second 0.0 -SelectionKind 1 -ProfileEntityId 2 `
        -Regions (New-CadRegionSelection)
    return New-CadV6File (New-CadV6Body -NextSketchId 2 -NextFeatureId 3 -Sketches @($sketch) `
        -Features @($base, $boss))
}

# The rectangle-crossing-circle sketch of PF-S1-01: a 4 x 3 m rectangle
# (entity 1) and a 0.5 m circle (entity 2) centred ON its right side (u = 2).
function New-CadLensSketch {
    param($ExtraEntities, [uint32] $NextEntityId = 3)
    $entities = @((New-CadRectangleEntity 1 0.0 0.0 4.0 3.0), (New-CadCircleEntity 2 2.0 0.0 0.5))
    $entities += @(if ($null -ne $ExtraEntities) { $ExtraEntities })
    return New-CadSketchV6 -SketchId 1 -PlacementCode 1 -PlaneCode 1 -NextEntityId $NextEntityId `
        -Entities $entities
}

# The lens INSIDE the rectangle, canonically: the rectangle's right side
# (1.1) between its two crossings with the circle, walked forward, then the
# circle's left arc (2.0) between its two crossings with that side.
#   1.1[X(2.0#0) > X(2.0#1)]  2.0[X(1.1#0) > X(1.1#1)]
# `$CircleEndOrdinal` and `$Rotate` construct the two refusals below.
function New-CadLensFace {
    param([uint32] $CircleEndOrdinal = 1, [switch] $Rotate)
    $side = New-CadFragment 1 1 (New-CadCrossing 2 0 0) (New-CadCrossing 2 0 1)
    $arc = New-CadFragment 2 0 (New-CadCrossing 1 1 0) (New-CadCrossing 1 1 $CircleEndOrdinal)
    $outer = if ($Rotate) { @($arc, $side) } else { @($side, $arc) }
    return New-CadPlanarFace -Outer $outer
}

# CAD FACE LENS: the lens face alone, extruded 1 m -- the cell a crossing makes,
# which no loop-region selection can name.
function New-CadFaceLensFile {
    param([int] $SelectionKind = 2, $Face, $ExtraEntities, [uint32] $NextEntityId = 3)
    if ($null -eq $Face) { $Face = New-CadLensFace }
    $base = New-CadV6Base -SelectionKind $SelectionKind -Faces @($Face)
    return New-CadV6File (New-CadV6Body -NextSketchId 2 -NextFeatureId 2 `
        -Sketches @(New-CadLensSketch -ExtraEntities $ExtraEntities -NextEntityId $NextEntityId) `
        -Features @($base))
}

# CAD FACE PROTRUSION: PF-S1-02 without its two dangling lines -- the 4 x 3 m
# rectangle and three lines (entities 2..4) closing a 1 x 1 m square against its
# right side through two T-junctions. The protrusion, canonically:
#   ~1.1[X(2.0#0) > X(4.0#0)]  2.0[S>E]  3.0[S>E]  4.0[S>E]
function New-CadFaceProtrusionFile {
    $sketch = New-CadSketchV6 -SketchId 1 -PlacementCode 1 -PlaneCode 1 -NextEntityId 5 `
        -Entities @((New-CadRectangleEntity 1 0.0 0.0 4.0 3.0),
                    (New-CadLineEntity 2 2.0 -0.5 3.0 -0.5),
                    (New-CadLineEntity 3 3.0 -0.5 3.0 0.5),
                    (New-CadLineEntity 4 3.0 0.5 2.0 0.5))
    $face = New-CadPlanarFace -Outer @(
        (New-CadFragment 1 1 (New-CadCrossing 2 0 0) (New-CadCrossing 4 0 0) -Reversed),
        (New-CadFragment 2 0 (New-CadSourceStart) (New-CadSourceEnd)),
        (New-CadFragment 3 0 (New-CadSourceStart) (New-CadSourceEnd)),
        (New-CadFragment 4 0 (New-CadSourceStart) (New-CadSourceEnd)))
    $base = New-CadV6Base -SelectionKind 2 -Faces @($face)
    return New-CadV6File (New-CadV6Body -NextSketchId 2 -NextFeatureId 2 -Sketches @($sketch) `
        -Features @($base))
}

# The lens two crossing circles make, canonically:
#   1.0[X(2.0#1) > X(2.0#0)]  2.0[X(1.0#0) > X(1.0#1)]
function New-CadTwoCircleLensFace {
    return New-CadPlanarFace -Outer @(
        (New-CadFragment 1 0 (New-CadCrossing 2 0 1) (New-CadCrossing 2 0 0)),
        (New-CadFragment 2 0 (New-CadCrossing 1 0 0) (New-CadCrossing 1 0 1)))
}

# CAD FACE TWO CIRCLES: PF-S1-03 -- two 1 m circles at u = -0.4 and +0.4 -- and
# the lens between them.
function New-CadFaceTwoCirclesFile {
    $sketch = New-CadSketchV6 -SketchId 1 -PlacementCode 1 -PlaneCode 1 -NextEntityId 3 `
        -Entities @((New-CadCircleEntity 1 -0.4 0.0 1.0), (New-CadCircleEntity 2 0.4 0.0 1.0))
    $base = New-CadV6Base -SelectionKind 2 -Faces @(New-CadTwoCircleLensFace)
    return New-CadV6File (New-CadV6Body -NextSketchId 2 -NextFeatureId 2 -Sketches @($sketch) `
        -Features @($base))
}

# CAD MIXED SELECTION: a LoopRegions base -- the 2 x 2 m block, 1 m -- and a
# PlanarFaces Add on its far cap: a second retained sketch (placement 3,
# feature 1 CapFar at the six-face rectangle signature) of two 0.5 m circles at
# u = -0.2 and +0.2, selecting their lens, 0.25 m along the face normal.
# `$SecondSketchId` constructs the duplicate-id refusal.
function New-CadMixedSelectionFile {
    param([uint32] $SecondSketchId = 2)
    $root = New-CadSketchV6 -SketchId 1 -PlacementCode 1 -PlaneCode 1 -NextEntityId 2 `
        -Entities @(New-CadRectangleEntity 1 0.0 0.0 2.0 2.0)
    $support = [pscustomobject]@{ FeatureId = 1; FaceKindCode = 2; EdgeEntityId = 0
                                  EdgeLocalIndex = 0; LineageToken = (Get-CadPlainRectangleLineage) }
    $onCap = New-CadSketchV6 -SketchId $SecondSketchId -PlacementCode 3 -Support $support `
        -NextEntityId 3 -Entities @((New-CadCircleEntity 1 -0.2 0.0 0.5), (New-CadCircleEntity 2 0.2 0.0 0.5))
    $base = New-CadV6Base -SelectionKind 1 -ProfileEntityId 1 -Regions (New-CadRegionSelection)
    $lens = New-CadFeatureV6 -FeatureId 2 -OperationCode 2 -SketchId $SecondSketchId -ExtentCode 1 `
        -DirectionCode 1 -Depth 0.25 -Second 0.0 -SelectionKind 2 -Faces @(New-CadTwoCircleLensFace)
    return New-CadV6File (New-CadV6Body -NextSketchId 3 -NextFeatureId 3 -Sketches @($root, $onCap) `
        -Features @($base, $lens))
}

# ---------------------------------------------------------------------------
# CAD-V6-S2: a FRAGMENT side face (7g FACE code 4) and the PlanarFaces lineage
# ---------------------------------------------------------------------------
#
# A PlanarFaces feature's face list is: CapPlane, CapFar, then ONE Side per
# fragment of its union's boundary -- component by component in canonical
# order, the outer cycle's fragments in its canonical walk, then each hole's.
# For a selection of ONE face that union is the face itself, so its side list
# is exactly the stored outer cycle, fragment for fragment. A fragment that is
# its whole source edge wears the 7c whole-edge code; a proper piece wears
#
#   0x03 << 56 | (FNV-1a 64 over its v6 FACE bytes after the kind: edge entity
#                 u32, edge local u32, CUT start, CUT end) & (2^56 - 1)
#
# and the feature's signature is 7c's rule over that list with the selection's
# anchor 0 (a face selection has none).

function Add-FnvBytes {
    param([System.Numerics.BigInteger] $Hash, [byte[]] $Bytes)
    $prime = [System.Numerics.BigInteger]::Parse('1099511628211')
    foreach ($b in $Bytes) {
        $Hash = [System.Numerics.BigInteger]::op_ExclusiveOr($Hash, [System.Numerics.BigInteger] $b)
        $Hash = [System.Numerics.BigInteger]::op_BitwiseAnd(
            [System.Numerics.BigInteger]::Multiply($Hash, $prime), $script:Fnv64Mask)
    }
    return $Hash
}

function Get-CadFragmentTokenCode {
    param([uint32] $EdgeEntityId, [uint32] $EdgeLocalIndex, $Start, $End)
    $buffer = New-ByteBuffer
    Add-U32 $buffer $EdgeEntityId
    Add-U32 $buffer $EdgeLocalIndex
    Add-CadCut $buffer $Start
    Add-CadCut $buffer $End
    $hash = Add-FnvBytes ([System.Numerics.BigInteger]::Parse('14695981039346656037')) $buffer.ToArray()
    $low = [System.Numerics.BigInteger]::op_BitwiseAnd($hash,
        [System.Numerics.BigInteger]::Pow(2, 56) - 1)
    return [System.Numerics.BigInteger]::op_BitwiseOr(
        [System.Numerics.BigInteger]::op_LeftShift([System.Numerics.BigInteger] 3, 56), $low)
}

# The lens base feature's signature: two caps, then its two sides -- the
# rectangle's right side between the crossings (straight, eligible) and the
# circle's arc between them (curved, never eligible) -- both proper pieces.
function Get-CadLensLineage {
    $hash = [System.Numerics.BigInteger]::Parse('14695981039346656037')
    $hash = Add-Fnv64 $hash ([System.Numerics.BigInteger] 0)
    $hash = Add-Fnv64 $hash ([System.Numerics.BigInteger] 4)
    $hash = Add-Fnv64 $hash (Get-CadFaceTokenCode 0 0 0)
    $hash = Add-Fnv64 $hash ([System.Numerics.BigInteger] 1)
    $hash = Add-Fnv64 $hash (Get-CadFaceTokenCode 1 0 0)
    $hash = Add-Fnv64 $hash ([System.Numerics.BigInteger] 1)
    $hash = Add-Fnv64 $hash (Get-CadFragmentTokenCode 1 1 (New-CadCrossing 2 0 0) (New-CadCrossing 2 0 1))
    $hash = Add-Fnv64 $hash ([System.Numerics.BigInteger] 1)
    $hash = Add-Fnv64 $hash (Get-CadFragmentTokenCode 2 0 (New-CadCrossing 1 1 0) (New-CadCrossing 1 1 1))
    $hash = Add-Fnv64 $hash ([System.Numerics.BigInteger] 0)
    if ($hash.IsZero) { $hash = [System.Numerics.BigInteger]::One }
    return [uint64]::Parse($hash.ToString())
}

# CAD FRAGMENT SUPPORT: the lens body, and a second retained sketch standing
# on the lens's STRAIGHT side -- a fragment of the rectangle's right side
# (placement 3, FACE code 4) -- with a 0.3 m square Added 0.2 m out of it.
function New-CadFragmentSupportFile {
    $support = [pscustomobject]@{ FeatureId = 1; FaceKindCode = 4; EdgeEntityId = 1
                                  EdgeLocalIndex = 1
                                  FragmentStart = (New-CadCrossing 2 0 0)
                                  FragmentEnd = (New-CadCrossing 2 0 1)
                                  LineageToken = (Get-CadLensLineage) }
    $onSide = New-CadSketchV6 -SketchId 2 -PlacementCode 3 -Support $support -NextEntityId 2 `
        -Entities @(New-CadRectangleEntity 1 0.0 0.0 0.3 0.3)
    $base = New-CadV6Base -SelectionKind 2 -Faces @(New-CadLensFace)
    $boss = New-CadFeatureV6 -FeatureId 2 -OperationCode 2 -SketchId 2 -ExtentCode 1 `
        -DirectionCode 1 -Depth 0.2 -Second 0.0 -SelectionKind 1 -ProfileEntityId 1 `
        -Regions (New-CadRegionSelection)
    return New-CadV6File (New-CadV6Body -NextSketchId 3 -NextFeatureId 3 `
        -Sketches @((New-CadLensSketch), $onSide) -Features @($base, $boss))
}

# CAD SPLINE FACE: the lens fixture with a Spline (entity 3) added to its
# sketch. Built while a spline disabled the arrangement, and refused then
# (UnsupportedCurve). Since CAD-V6-S2-CORRECTION-FILL-HUD-R1 a spline is
# intersected span by span; this one stands clear of both curves, bounds no
# face, and the lens resolves: the SAME bytes are now a valid file.
function New-CadSplineFaceFile {
    $spline = [pscustomobject]@{ Id = 3; KindCode = 6; Values = @(-1.0, -1.0, 0.0, -0.5, 1.0, -1.0) }
    return New-CadFaceLensFile -ExtraEntities @($spline) -NextEntityId 4
}

# CAD OVERLAP FACE: the 4 x 3 m rectangle and a line (entity 2) lying ALONG its
# bottom side from u = -1 to u = +1 -- a shared stretch with no finite set of
# nodes to split at: AmbiguousOverlap. The stored face is the rectangle's whole
# boundary, well formed; the sketch has no arrangement to resolve it in.
function New-CadOverlapFaceFile {
    $sketch = New-CadSketchV6 -SketchId 1 -PlacementCode 1 -PlaneCode 1 -NextEntityId 3 `
        -Entities @((New-CadRectangleEntity 1 0.0 0.0 4.0 3.0), (New-CadLineEntity 2 -1.0 -1.5 1.0 -1.5))
    $face = New-CadPlanarFace -Outer @(
        (New-CadFragment 1 0 (New-CadSourceStart) (New-CadSourceEnd)),
        (New-CadFragment 1 1 (New-CadSourceStart) (New-CadSourceEnd)),
        (New-CadFragment 1 2 (New-CadSourceStart) (New-CadSourceEnd)),
        (New-CadFragment 1 3 (New-CadSourceStart) (New-CadSourceEnd)))
    $base = New-CadV6Base -SelectionKind 2 -Faces @($face)
    return New-CadV6File (New-CadV6Body -NextSketchId 2 -NextFeatureId 2 -Sketches @($sketch) `
        -Features @($base))
}

# ---------------------------------------------------------------------------
# CADB v7 (CAD-V6-REVOLVE-NEWBODY-E2E-R1): an explicit feature KIND
# ---------------------------------------------------------------------------
#
# v7 is v6's layout (7g) with a KIND after every feature id and a payload of
# that kind's own -- DATA_PACKAGE_SPEC.md 7h:
#
#   FEATURE    featureId u32 | kind u8 (1 Extrude, 2 Revolve) | KIND PAYLOAD
#   EXTRUDE    exactly the v6 bytes after featureId (operation .. selection)
#   REVOLVE    operation u8 (1 New Body, the only one in R1) | sketchId u32 |
#              axisEntityId u32 | axisEdgeLocalIndex u32 | angleDegrees f64 |
#              direction u8 (1 Positive, 2 Negative) |
#              selectionKind u8 and its block, exactly as 7g
#
# A Revolve stores no extent, no depth and no second distance: nothing of it is
# an Extrude field. These writers validate nothing, so the two corrupt
# fixtures are CONSTRUCTED with their bad value in place.

function Add-CadSelectionV7 {
    param($Buffer, $Feature)
    Add-U8  $Buffer $Feature.SelectionKind
    if ($Feature.SelectionKind -eq 1) {
        Add-U32 $Buffer ([uint32] $Feature.ProfileEntityId)
        Add-CadRegions $Buffer $Feature.Regions
    } else {
        $faces = @(if ($null -ne $Feature.Faces) { $Feature.Faces })
        Add-U32 $Buffer ([uint32] $faces.Count)
        foreach ($face in $faces) {
            Add-CadFragmentCycle $Buffer $face.Outer
            $holes = @(if ($null -ne $face.Holes) { $face.Holes })
            Add-U32 $Buffer ([uint32] $holes.Count)
            foreach ($hole in $holes) { Add-CadFragmentCycle $Buffer $hole }
        }
    }
}

function Add-CadFeatureV7 {
    param($Buffer, $Feature)
    Add-U32 $Buffer ([uint32] $Feature.FeatureId)
    Add-U8  $Buffer $Feature.KindCode
    # The payload follows the SHAPE of the feature object, never its kind
    # byte: a feature built as a Revolve writes the Revolve payload whatever
    # kind it carries, so KindCode 9 constructs an unknown kind standing in
    # front of an otherwise well-formed payload.
    if ($null -ne $Feature.PSObject.Properties['AxisEntityId']) {
        Add-U8  $Buffer $Feature.OperationCode
        Add-U32 $Buffer ([uint32] $Feature.SketchId)
        Add-U32 $Buffer ([uint32] $Feature.AxisEntityId)
        Add-U32 $Buffer ([uint32] $Feature.AxisEdgeLocalIndex)
        Add-F64 $Buffer $Feature.AngleDegrees
        Add-U8  $Buffer $Feature.RevolveDirectionCode
    } else {
        Add-U8  $Buffer $Feature.OperationCode
        Add-U32 $Buffer ([uint32] $Feature.SketchId)
        Add-U8  $Buffer $Feature.ExtentCode
        Add-U8  $Buffer $Feature.DirectionCode
        Add-F64 $Buffer $Feature.Depth
        Add-F64 $Buffer $Feature.Second
    }
    Add-CadSelectionV7 $Buffer $Feature
}

function New-CadPayloadV7 {
    param($Bodies)
    $p = New-ByteBuffer
    $bodyList = @($Bodies)
    Add-U32 $p ([uint32] $bodyList.Count)
    foreach ($body in $bodyList) {
        Add-U64 $p ([uint64] $body.ObjectId)
        Add-U32 $p ([uint32] $body.NextSketchId)
        Add-U32 $p ([uint32] $body.NextFeatureId)
        $sketches = @($body.Sketches)
        Add-U32 $p ([uint32] $sketches.Count)
        foreach ($sketch in $sketches) { Add-CadSketchV6 $p $sketch }
        $features = @($body.Features)
        Add-U32 $p ([uint32] $features.Count)
        foreach ($feature in $features) { Add-CadFeatureV7 $p $feature }
    }
    return $p.ToArray()
}

# A Revolve New Body base on a world-XY root sketch, selecting one region.
function New-CadRevolveBase {
    param([uint32] $ProfileEntityId, [uint32] $AxisEntityId, [uint32] $AxisEdgeLocalIndex,
          [double] $AngleDegrees, [int] $RevolveDirectionCode, [int] $KindCode = 2)
    return [pscustomobject]@{
        FeatureId = 1; KindCode = $KindCode; OperationCode = 1; SketchId = 1
        AxisEntityId = $AxisEntityId; AxisEdgeLocalIndex = $AxisEdgeLocalIndex
        AngleDegrees = $AngleDegrees; RevolveDirectionCode = $RevolveDirectionCode
        SelectionKind = 1; ProfileEntityId = $ProfileEntityId; Regions = (New-CadRegionSelection)
        Faces = @()
    }
}

# CAD REVOLVE: a 1 x 1 m square (entity 1) from u 1..2, v 0..1 on XY, and a
# separate Line (entity 2) on u = 0 from v -1 to v 2 as the axis. The square is
# revolved about the line, edge 0 -- a tube. `$KindCode` 9 constructs the
# unknown-kind refusal from this very fixture.
function New-CadRevolveFile {
    param([double] $AngleDegrees = 360.0, [int] $RevolveDirectionCode = 1,
          [uint32] $AxisEntityId = 2, [int] $KindCode = 2)
    $sketch = New-CadSketchV6 -SketchId 1 -PlacementCode 1 -PlaneCode 1 -NextEntityId 3 `
        -Entities @((New-CadRectangleEntity 1 1.5 0.5 1.0 1.0), (New-CadLineEntity 2 0.0 -1.0 0.0 2.0))
    $base = New-CadRevolveBase -ProfileEntityId 1 -AxisEntityId $AxisEntityId `
        -AxisEdgeLocalIndex 0 -AngleDegrees $AngleDegrees `
        -RevolveDirectionCode $RevolveDirectionCode -KindCode $KindCode
    $body = [pscustomobject]@{ ObjectId = 1; NextSketchId = 2; NextFeatureId = 2
                               Sketches = @($sketch); Features = @($base) }
    $sceneBodies = @([pscustomobject]@{ ObjectId = 1; Transform = $script:IdentityPlacement })
    $scne = New-Section 'SCNE' 1 $true (New-ScenePayload $sceneBodies 2 1)
    $cadb = New-Section 'CADB' 7 $true (New-CadPayloadV7 @($body))
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

# DATA_PACKAGE_SPEC.md 7f promises that the generalized signature IS the 7c one
# for a feature selecting one region without holes, so no stored token of an
# earlier fixture moves. Hold the two implementations here to that before a
# single v5 byte is written.
$cadPlainRectangleLineage = Get-CadPlainRectangleLineage
$cadHoledRectangleLineage = Get-CadHoledRectangleLineage
if ($cadPlainRectangleLineage -ne (Get-CadTopologySignature 1 4 $false)) {
    throw 'The 7f signature of a one-rectangle region disagrees with the 7c signature.'
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
    'cad_region_hole_v5.forge'        = (New-CadRegionHoleFile)
    'cad_feature_add_v5.forge'        = (New-CadFeatureAddFile)
    'cad_feature_cut_v5.forge'        = (New-CadFeatureCutFile)
    'cad_feature_chain_v5.forge'      = (New-CadFeatureChainFile)
    'cad_bad_operation_v5.forge'      = (New-CadFeatureAddFile -OperationCode 9)
    'cad_bad_feature_ref_v5.forge'    = (New-CadFeatureAddFile -SupportFeatureId 7)
    'cad_bad_feature_order_v5.forge'  = (New-CadFeatureAddFile -FeatureId 1)
    'cad_bad_region_v5.forge'         = (New-CadRegionHoleFile -HoleAnchors @(7))
    'cad_sketch_shared_v6.forge'      = (New-CadSketchSharedFile)
    'cad_face_lens_v6.forge'          = (New-CadFaceLensFile)
    'cad_face_protrusion_v6.forge'    = (New-CadFaceProtrusionFile)
    'cad_face_two_circles_v6.forge'   = (New-CadFaceTwoCirclesFile)
    'cad_mixed_selection_v6.forge'    = (New-CadMixedSelectionFile)
    'cad_bad_sketch_ref_v6.forge'     = (New-CadSketchSharedFile -SecondSketchId 7)
    'cad_duplicate_sketch_id_v6.forge' = (New-CadMixedSelectionFile -SecondSketchId 1)
    'cad_bad_selection_kind_v6.forge' = (New-CadFaceLensFile -SelectionKind 9)
    'cad_noncanonical_face_v6.forge'  = (New-CadFaceLensFile -Face (New-CadLensFace -Rotate))
    'cad_unresolved_face_v6.forge'    = (New-CadFaceLensFile -Face (New-CadLensFace -CircleEndOrdinal 2))
    'cad_spline_face_v6.forge'        = (New-CadSplineFaceFile)
    'cad_overlap_face_v6.forge'       = (New-CadOverlapFaceFile)
    'cad_fragment_support_v6.forge'   = (New-CadFragmentSupportFile)
    'cad_revolve_full_v7.forge'       = (New-CadRevolveFile)
    'cad_revolve_partial_v7.forge'    = (New-CadRevolveFile -AngleDegrees 90.0 -RevolveDirectionCode 2)
    'cad_revolve_bad_axis_v7.forge'   = (New-CadRevolveFile -AxisEntityId 7)
    'cad_bad_feature_kind_v7.forge'   = (New-CadRevolveFile -KindCode 9)
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
Write-Host 'Digests of the CADB v5 fixtures (CAD-VERTICAL-SLICE-R1):'
Write-Host ("  cad_region_hole:       {0}" -f ($rows | Where-Object Fixture -eq 'cad_region_hole_v5.forge').Sha256)
Write-Host ("  cad_feature_add:       {0}" -f ($rows | Where-Object Fixture -eq 'cad_feature_add_v5.forge').Sha256)
Write-Host ("  cad_feature_cut:       {0}" -f ($rows | Where-Object Fixture -eq 'cad_feature_cut_v5.forge').Sha256)
Write-Host ("  cad_feature_chain:     {0}" -f ($rows | Where-Object Fixture -eq 'cad_feature_chain_v5.forge').Sha256)
Write-Host ("  cad_bad_operation:     {0}" -f ($rows | Where-Object Fixture -eq 'cad_bad_operation_v5.forge').Sha256)
Write-Host ("  cad_bad_feature_ref:   {0}" -f ($rows | Where-Object Fixture -eq 'cad_bad_feature_ref_v5.forge').Sha256)
Write-Host ("  cad_bad_feature_order: {0}" -f ($rows | Where-Object Fixture -eq 'cad_bad_feature_order_v5.forge').Sha256)
Write-Host ("  cad_bad_region:        {0}" -f ($rows | Where-Object Fixture -eq 'cad_bad_region_v5.forge').Sha256)
Write-Host 'Digests of the CADB v6 fixtures (CAD-V6-S1):'
foreach ($name in @('cad_sketch_shared_v6', 'cad_face_lens_v6', 'cad_face_protrusion_v6',
                    'cad_face_two_circles_v6', 'cad_mixed_selection_v6', 'cad_bad_sketch_ref_v6',
                    'cad_duplicate_sketch_id_v6', 'cad_bad_selection_kind_v6',
                    'cad_noncanonical_face_v6', 'cad_unresolved_face_v6', 'cad_spline_face_v6',
                    'cad_overlap_face_v6', 'cad_fragment_support_v6')) {
    Write-Host ("  {0,-27}{1}" -f ($name + ':'), ($rows | Where-Object Fixture -eq ($name + '.forge')).Sha256)
}
Write-Host 'Digests of the CADB v7 fixtures (CAD-V6-REVOLVE-NEWBODY-E2E-R1):'
foreach ($name in @('cad_revolve_full_v7', 'cad_revolve_partial_v7', 'cad_revolve_bad_axis_v7',
                    'cad_bad_feature_kind_v7')) {
    Write-Host ("  {0,-27}{1}" -f ($name + ':'), ($rows | Where-Object Fixture -eq ($name + '.forge')).Sha256)
}
Write-Host 'Lineage tokens the v5 fixtures carry (7c / 7f signature):'
Write-Host ("  plain rectangle (6 faces): 0x{0:X16}" -f $cadPlainRectangleLineage)
Write-Host ("  holed rectangle (38 faces): 0x{0:X16}" -f $cadHoledRectangleLineage)
Write-Host ("  lens face (CAD-V6-S2, 4 faces): 0x{0:X16}" -f (Get-CadLensLineage))
