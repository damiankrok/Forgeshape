package com.forgeshape.app;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Locale;

/**
 * A SECOND, INDEPENDENT reader for the binary glTF the exporter writes.
 *
 * <p><b>Why this exists.</b> An exporter that is only ever read back by its own
 * code proves that it is self-consistent and nothing else. What has to be true
 * is that the bytes are a glTF 2.0 document as the SPECIFICATION describes one,
 * because the reader that matters is somebody else's. So this class is written
 * from the specification — the 12-byte header, the chunk framing, the accessor
 * and bufferView indirection, the required and forbidden properties — and
 * shares not one line with {@code forgeshape_gltf_export.cpp}. It does not call
 * into native code, it does not know what ForgeShape put in the file, and it
 * deliberately re-derives everything (chunk boundaries, buffer lengths, the
 * bounds an accessor declares) rather than trusting a number the writer stated.
 *
 * <p><b>It is a reader, not a matcher.</b> {@link #validate()} answers "is this
 * a well-formed glTF 2.0 binary" and knows nothing about ForgeShape. The tests
 * that assert what ForgeShape put in it do that separately, on top of the model
 * this reader builds. Keeping those apart is what makes a failure legible: a
 * structural problem is the format, a content problem is the product.
 *
 * <p><b>Test-only.</b> Nothing in the product references this class. It uses
 * {@code org.json}, which is part of the Android platform, so it adds no
 * dependency of any kind.
 */
final class GlbDocument {

    private static final int MAGIC = 0x46546C67;      // "glTF", little-endian
    private static final int CHUNK_JSON = 0x4E4F534A; // "JSON"
    private static final int CHUNK_BIN = 0x004E4942;  // "BIN\0"

    private static final int FLOAT = 5126;
    private static final int UNSIGNED_INT = 5125;
    private static final int UNSIGNED_SHORT = 5123;
    private static final int UNSIGNED_BYTE = 5121;
    private static final int MODE_TRIANGLES = 4;

    /** One body as the file presents it: a node, its transform and its geometry. */
    static final class Body {
        String name;
        /**
         * The node's transform as a 4x4, COLUMN-major.
         *
         * <p>Derived, not read: glTF lets a node state its transform either as
         * one {@code matrix} or as any of {@code translation}/{@code rotation}/
         * {@code scale}, and a reader written from the specification has to
         * accept both. The flags below say which form the file actually used,
         * so a test can assert a POLICY — "this exporter writes translation
         * only" — separately from the geometry the transform produces.
         */
        float[] matrix;
        /** True when the node stated a {@code matrix}. */
        boolean hasMatrix;
        /** True when the node stated a {@code translation}. */
        boolean hasTranslation;
        /** True when the node stated a {@code rotation}. */
        boolean hasRotation;
        /** True when the node stated a {@code scale}. */
        boolean hasScale;
        /** The node's translation, zero when it stated none. */
        float[] translation = {0f, 0f, 0f};
        /** Body-local positions, xyz triples, in metres. */
        float[] positions;
        /** Body-local normals, xyz triples. */
        float[] normals;
        int[] indices;
        boolean doubleSided;
        float[] declaredMin;
        float[] declaredMax;

        int vertexCount() {
            return positions.length / 3;
        }

        int triangleCount() {
            return indices.length / 3;
        }

        /** The local position of one vertex. */
        float[] localVertex(int index) {
            return new float[]{positions[index * 3], positions[index * 3 + 1],
                    positions[index * 3 + 2]};
        }

        /**
         * The world position of one vertex.
         *
         * <p>Column-major, column-vector: {@code world = M * local}, which is
         * the convention glTF requires of a node matrix. Written out by hand so
         * that reading this file is enough to check the index arithmetic.
         */
        float[] worldVertex(int index) {
            final float x = positions[index * 3];
            final float y = positions[index * 3 + 1];
            final float z = positions[index * 3 + 2];
            final float[] m = matrix;
            return new float[]{
                    m[0] * x + m[4] * y + m[8] * z + m[12],
                    m[1] * x + m[5] * y + m[9] * z + m[13],
                    m[2] * x + m[6] * y + m[10] * z + m[14]};
        }

        /** The axis-aligned extent of the geometry in body-local metres. */
        float[] localBounds() {
            final float[] out = {Float.MAX_VALUE, Float.MAX_VALUE, Float.MAX_VALUE,
                    -Float.MAX_VALUE, -Float.MAX_VALUE, -Float.MAX_VALUE};
            for (int i = 0; i < positions.length; i += 3) {
                for (int axis = 0; axis < 3; ++axis) {
                    out[axis] = Math.min(out[axis], positions[i + axis]);
                    out[3 + axis] = Math.max(out[3 + axis], positions[i + axis]);
                }
            }
            return out;
        }
    }

    /** Raised for a file so malformed that no model can be built from it. */
    static final class MalformedGlb extends RuntimeException {
        MalformedGlb(String message) {
            super(message);
        }
    }

    private final byte[] bytes;
    private final int declaredLength;
    private final int version;
    private final byte[] jsonChunk;
    private final byte[] bin;
    private final String jsonText;
    private final JSONObject root;
    private final List<Body> bodies = new ArrayList<>();
    private final List<String> problems = new ArrayList<>();

    static GlbDocument parse(byte[] bytes) {
        return new GlbDocument(bytes);
    }

    private GlbDocument(byte[] input) {
        if (input == null || input.length < 12) {
            throw new MalformedGlb("shorter than a GLB header");
        }
        this.bytes = input;
        final ByteBuffer header = ByteBuffer.wrap(input).order(ByteOrder.LITTLE_ENDIAN);
        final int magic = header.getInt(0);
        if (magic != MAGIC) {
            throw new MalformedGlb(String.format(Locale.US, "magic is 0x%08x, not glTF", magic));
        }
        this.version = header.getInt(4);
        this.declaredLength = header.getInt(8);

        // Chunk walk. Every boundary is re-derived from the chunk headers; the
        // total is checked against the file, not assumed from it.
        byte[] json = null;
        byte[] binary = null;
        int offset = 12;
        int chunkCount = 0;
        while (offset + 8 <= input.length) {
            final int chunkLength = header.getInt(offset);
            final int chunkType = header.getInt(offset + 4);
            if (chunkLength < 0 || offset + 8L + chunkLength > input.length) {
                problems.add("chunk " + chunkCount + " runs past the end of the file");
                break;
            }
            if ((chunkLength % 4) != 0) {
                problems.add("chunk " + chunkCount + " length " + chunkLength
                        + " is not 4-byte aligned");
            }
            final byte[] payload = Arrays.copyOfRange(input, offset + 8, offset + 8 + chunkLength);
            if (chunkType == CHUNK_JSON) {
                if (json != null) {
                    problems.add("more than one JSON chunk");
                }
                json = payload;
                // Whatever follows the closing brace is padding, and the
                // specification says padding in a JSON chunk is SPACES. A zero
                // here would still parse in a lenient reader and fail in a
                // strict one, which is the worst kind of wrong.
                int end = payload.length;
                while (end > 0 && payload[end - 1] == 0x20) {
                    end--;
                }
                for (int i = end; i < payload.length; ++i) {
                    if (payload[i] != 0x20) {
                        problems.add("JSON chunk padding must be spaces (0x20)");
                        break;
                    }
                }
                if (end > 0 && payload[end - 1] != '}') {
                    problems.add("JSON chunk does not end in '}' before its padding");
                }
            } else if (chunkType == CHUNK_BIN) {
                if (binary != null) {
                    problems.add("more than one BIN chunk");
                }
                binary = payload;
            } else {
                problems.add(String.format(Locale.US, "unknown chunk type 0x%08x", chunkType));
            }
            offset += 8 + chunkLength;
            chunkCount++;
        }
        if (offset != input.length) {
            problems.add("chunks cover " + offset + " bytes of a " + input.length + " byte file");
        }
        if (json == null) {
            throw new MalformedGlb("no JSON chunk");
        }
        this.jsonChunk = json;
        this.bin = binary == null ? new byte[0] : binary;
        this.jsonText = new String(json, java.nio.charset.StandardCharsets.UTF_8);
        try {
            this.root = new JSONObject(jsonText.trim());
        } catch (JSONException error) {
            throw new MalformedGlb("the JSON chunk is not JSON: " + error.getMessage());
        }
        try {
            build();
        } catch (JSONException error) {
            problems.add("required glTF property missing or wrong type: " + error.getMessage());
        }
    }

    // -----------------------------------------------------------------------
    // Structure
    // -----------------------------------------------------------------------

    /** Every structural problem found, in the order found. Empty means valid. */
    List<String> validate() {
        return problems;
    }

    String problemSummary() {
        return problems.isEmpty() ? "(none)" : problems.toString();
    }

    int declaredLength() {
        return declaredLength;
    }

    int actualLength() {
        return bytes.length;
    }

    int version() {
        return version;
    }

    String json() {
        return jsonText;
    }

    JSONObject root() {
        return root;
    }

    byte[] bin() {
        return bin;
    }

    byte[] jsonChunk() {
        return jsonChunk;
    }

    List<Body> bodies() {
        return bodies;
    }

    private void build() throws JSONException {
        if (version != 2) {
            problems.add("GLB container version is " + version + ", not 2");
        }
        if (declaredLength != bytes.length) {
            problems.add("header declares " + declaredLength + " bytes, file is " + bytes.length);
        }
        final JSONObject asset = root.optJSONObject("asset");
        if (asset == null || !"2.0".equals(asset.optString("version"))) {
            problems.add("asset.version must be \"2.0\"");
        }
        for (String forbidden : new String[]{"images", "textures", "samplers", "animations",
                "skins", "cameras"}) {
            if (root.has(forbidden)) {
                problems.add("this exporter must not emit " + forbidden);
            }
        }

        final JSONArray buffers = root.optJSONArray("buffers");
        if (buffers == null || buffers.length() != 1) {
            problems.add("a GLB carries exactly one buffer");
            return;
        }
        final JSONObject buffer = buffers.getJSONObject(0);
        if (buffer.has("uri")) {
            problems.add("the single buffer must be the BIN chunk, never an external uri");
        }
        final int bufferLength = buffer.getInt("byteLength");
        if (bufferLength > bin.length) {
            problems.add("buffer declares " + bufferLength + " bytes, BIN chunk holds "
                    + bin.length);
        }

        final JSONArray views = root.optJSONArray("bufferViews");
        final JSONArray accessors = root.optJSONArray("accessors");
        final JSONArray meshes = root.optJSONArray("meshes");
        final JSONArray nodes = root.optJSONArray("nodes");
        if (views == null || accessors == null || meshes == null || nodes == null) {
            problems.add("a document with geometry needs bufferViews, accessors, meshes"
                    + " and nodes");
            return;
        }
        for (int i = 0; i < views.length(); ++i) {
            final JSONObject view = views.getJSONObject(i);
            if (view.optInt("buffer", -1) != 0) {
                problems.add("bufferView " + i + " does not point at buffer 0");
            }
            final long start = view.optLong("byteOffset", 0L);
            final long length = view.getLong("byteLength");
            if (start < 0 || length < 0 || start + length > bufferLength) {
                problems.add("bufferView " + i + " runs past the buffer");
            }
            if ((start % 4) != 0) {
                problems.add("bufferView " + i + " starts at " + start + ", not 4-byte aligned");
            }
        }

        // Scene coverage: every node the scene names must exist, and every node
        // must carry a mesh — a node with nothing in it is a promise the file
        // does not keep.
        final int sceneIndex = root.optInt("scene", -1);
        final JSONArray scenes = root.optJSONArray("scenes");
        if (sceneIndex < 0 || scenes == null || sceneIndex >= scenes.length()) {
            problems.add("the document must name a default scene");
            return;
        }
        final JSONArray sceneNodes = scenes.getJSONObject(sceneIndex).optJSONArray("nodes");
        if (sceneNodes == null) {
            problems.add("the default scene lists no nodes");
            return;
        }

        final JSONArray materials = root.optJSONArray("materials");
        for (int i = 0; i < sceneNodes.length(); ++i) {
            final int nodeIndex = sceneNodes.getInt(i);
            if (nodeIndex < 0 || nodeIndex >= nodes.length()) {
                problems.add("scene names node " + nodeIndex + ", which does not exist");
                continue;
            }
            final JSONObject node = nodes.getJSONObject(nodeIndex);
            if (node.has("children")) {
                problems.add("this exporter emits a flat scene; node " + nodeIndex
                        + " has children");
            }
            final Body body = new Body();
            body.name = node.optString("name", "");
            body.hasMatrix = node.has("matrix");
            body.hasTranslation = node.has("translation");
            body.hasRotation = node.has("rotation");
            body.hasScale = node.has("scale");
            // glTF forbids mixing the two forms; either is legal alone.
            if (body.hasMatrix
                    && (body.hasTranslation || body.hasRotation || body.hasScale)) {
                problems.add("node " + nodeIndex + " mixes a matrix with TRS properties");
            }
            if (body.hasMatrix) {
                body.matrix = readFloats(node.optJSONArray("matrix"), 16);
                if (body.matrix == null) {
                    problems.add("node " + nodeIndex + " has a matrix that is not 16 elements");
                    body.matrix = identity();
                }
                body.translation = new float[]{body.matrix[12], body.matrix[13], body.matrix[14]};
            } else {
                final float[] translation = body.hasTranslation
                        ? readFloats(node.optJSONArray("translation"), 3) : new float[]{0f, 0f, 0f};
                if (translation == null) {
                    problems.add("node " + nodeIndex + " has a translation that is not 3 elements");
                    body.translation = new float[]{0f, 0f, 0f};
                } else {
                    body.translation = translation;
                }
                // Rotation and scale are read only so the derived matrix is
                // right for a file that uses them; this exporter writes
                // neither, and the test asserts that separately.
                final float[] rotation = body.hasRotation
                        ? readFloats(node.optJSONArray("rotation"), 4) : null;
                final float[] scale = body.hasScale
                        ? readFloats(node.optJSONArray("scale"), 3) : null;
                if (body.hasRotation && rotation == null) {
                    problems.add("node " + nodeIndex + " has a rotation that is not a quaternion");
                }
                if (body.hasScale && scale == null) {
                    problems.add("node " + nodeIndex + " has a scale that is not 3 elements");
                }
                body.matrix = composeTrs(body.translation, rotation, scale);
            }
            final int meshIndex = node.optInt("mesh", -1);
            if (meshIndex < 0 || meshIndex >= meshes.length()) {
                problems.add("node " + nodeIndex + " does not reference a mesh");
                continue;
            }
            final JSONArray primitives =
                    meshes.getJSONObject(meshIndex).optJSONArray("primitives");
            if (primitives == null || primitives.length() != 1) {
                problems.add("mesh " + meshIndex + " must hold exactly one primitive");
                continue;
            }
            final JSONObject primitive = primitives.getJSONObject(0);
            if (primitive.optInt("mode", MODE_TRIANGLES) != MODE_TRIANGLES) {
                problems.add("mesh " + meshIndex + " is not made of triangles");
            }
            final JSONObject attributes = primitive.getJSONObject("attributes");
            if (!attributes.has("POSITION") || !attributes.has("NORMAL")) {
                problems.add("mesh " + meshIndex + " must have POSITION and NORMAL");
                continue;
            }
            final int positionAccessor = attributes.getInt("POSITION");
            body.positions = readAccessor(accessors, views, positionAccessor, "VEC3", FLOAT);
            body.normals = readAccessor(accessors, views, attributes.getInt("NORMAL"),
                    "VEC3", FLOAT);
            final JSONObject positions = accessors.getJSONObject(positionAccessor);
            body.declaredMin = readFloats(positions.optJSONArray("min"), 3);
            body.declaredMax = readFloats(positions.optJSONArray("max"), 3);
            if (body.declaredMin == null || body.declaredMax == null) {
                problems.add("a POSITION accessor must declare min and max");
            }
            if (!primitive.has("indices")) {
                problems.add("mesh " + meshIndex + " must be indexed");
                continue;
            }
            body.indices = readIndices(accessors, views, primitive.getInt("indices"));
            if (body.positions == null || body.normals == null || body.indices == null) {
                continue;
            }
            if (body.normals.length != body.positions.length) {
                problems.add("mesh " + meshIndex + " has one normal per vertex or it has a bug");
            }
            if ((body.indices.length % 3) != 0) {
                problems.add("mesh " + meshIndex + " has " + body.indices.length
                        + " indices, which is not whole triangles");
            }
            final int vertexCount = body.positions.length / 3;
            for (int index : body.indices) {
                if (index < 0 || index >= vertexCount) {
                    problems.add("mesh " + meshIndex + " indexes vertex " + index
                            + " of " + vertexCount);
                    break;
                }
            }
            checkDeclaredBounds(meshIndex, body);
            final int materialIndex = primitive.optInt("material", -1);
            if (materialIndex >= 0) {
                if (materials == null || materialIndex >= materials.length()) {
                    problems.add("mesh " + meshIndex + " names material " + materialIndex
                            + ", which does not exist");
                } else {
                    body.doubleSided =
                            materials.getJSONObject(materialIndex).optBoolean("doubleSided", false);
                }
            }
            bodies.add(body);
        }
    }

    /**
     * The declared min/max must be the data's own bounds.
     *
     * <p>Recomputed here rather than compared to what the writer intended: a
     * consumer that culls or frames the scene by these numbers is misled by a
     * wrong one exactly as badly as by wrong geometry, and nothing else in the
     * file would reveal it.
     */
    private void checkDeclaredBounds(int meshIndex, Body body) {
        if (body.declaredMin == null || body.declaredMax == null) {
            return;
        }
        final float[] bounds = body.localBounds();
        for (int axis = 0; axis < 3; ++axis) {
            if (Math.abs(bounds[axis] - body.declaredMin[axis]) > 1e-6f
                    || Math.abs(bounds[3 + axis] - body.declaredMax[axis]) > 1e-6f) {
                problems.add("mesh " + meshIndex + " axis " + axis + ": accessor declares ["
                        + body.declaredMin[axis] + ", " + body.declaredMax[axis]
                        + "] but the data is [" + bounds[axis] + ", " + bounds[3 + axis] + "]");
                return;
            }
        }
    }

    // -----------------------------------------------------------------------
    // Accessor plumbing, resolved from the specification's own indirection
    // -----------------------------------------------------------------------

    private float[] readAccessor(JSONArray accessors, JSONArray views, int index,
                                 String type, int componentType) throws JSONException {
        if (index < 0 || index >= accessors.length()) {
            problems.add("accessor " + index + " does not exist");
            return null;
        }
        final JSONObject accessor = accessors.getJSONObject(index);
        if (!type.equals(accessor.optString("type"))) {
            problems.add("accessor " + index + " is " + accessor.optString("type")
                    + ", expected " + type);
            return null;
        }
        if (accessor.optInt("componentType") != componentType) {
            problems.add("accessor " + index + " has the wrong component type");
            return null;
        }
        final int count = accessor.getInt("count");
        final int components = "VEC3".equals(type) ? 3 : 1;
        final ByteBuffer data = slice(accessors, views, accessor, index,
                (long) count * components * 4);
        if (data == null) {
            return null;
        }
        final float[] out = new float[count * components];
        for (int i = 0; i < out.length; ++i) {
            out[i] = data.getFloat();
        }
        return out;
    }

    private int[] readIndices(JSONArray accessors, JSONArray views, int index)
            throws JSONException {
        if (index < 0 || index >= accessors.length()) {
            problems.add("index accessor " + index + " does not exist");
            return null;
        }
        final JSONObject accessor = accessors.getJSONObject(index);
        if (!"SCALAR".equals(accessor.optString("type"))) {
            problems.add("an index accessor must be SCALAR");
            return null;
        }
        final int componentType = accessor.optInt("componentType");
        final int width;
        switch (componentType) {
            case UNSIGNED_BYTE:
                width = 1;
                break;
            case UNSIGNED_SHORT:
                width = 2;
                break;
            case UNSIGNED_INT:
                width = 4;
                break;
            default:
                problems.add("index component type " + componentType + " is not an unsigned"
                        + " integer type glTF allows for indices");
                return null;
        }
        final int count = accessor.getInt("count");
        final ByteBuffer data = slice(accessors, views, accessor, index, (long) count * width);
        if (data == null) {
            return null;
        }
        final int[] out = new int[count];
        for (int i = 0; i < count; ++i) {
            switch (width) {
                case 1:
                    out[i] = data.get() & 0xFF;
                    break;
                case 2:
                    out[i] = data.getShort() & 0xFFFF;
                    break;
                default:
                    out[i] = data.getInt();
                    break;
            }
        }
        return out;
    }

    private ByteBuffer slice(JSONArray accessors, JSONArray views, JSONObject accessor,
                             int index, long byteLength) throws JSONException {
        final int viewIndex = accessor.optInt("bufferView", -1);
        if (viewIndex < 0 || viewIndex >= views.length()) {
            problems.add("accessor " + index + " does not name a bufferView");
            return null;
        }
        final JSONObject view = views.getJSONObject(viewIndex);
        final long viewStart = view.optLong("byteOffset", 0L);
        final long viewLength = view.getLong("byteLength");
        final long start = viewStart + accessor.optLong("byteOffset", 0L);
        if (start < 0 || start + byteLength > viewStart + viewLength
                || start + byteLength > bin.length) {
            problems.add("accessor " + index + " reads past its bufferView");
            return null;
        }
        if (byteLength > viewLength) {
            problems.add("accessor " + index + " is larger than its bufferView");
            return null;
        }
        final ByteBuffer buffer = ByteBuffer.wrap(bin, (int) start, (int) byteLength);
        buffer.order(ByteOrder.LITTLE_ENDIAN);
        return buffer;
    }

    private static float[] readFloats(JSONArray array, int expected) {
        if (array == null || array.length() != expected) {
            return null;
        }
        final float[] out = new float[expected];
        for (int i = 0; i < expected; ++i) {
            out[i] = (float) array.optDouble(i, Double.NaN);
        }
        return out;
    }

    private static float[] identity() {
        return new float[]{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    }

    /**
     * T * R * S as one column-major 4x4, the order glTF specifies for a node's
     * TRS properties. A missing rotation or scale is the identity.
     *
     * <p>The quaternion is {@code (x, y, z, w)} — glTF's order, with w LAST —
     * expanded here by hand rather than through any library, so this reader
     * stays free of anything the writer could also be using.
     */
    private static float[] composeTrs(float[] translation, float[] rotation, float[] scale) {
        final float x = rotation == null ? 0f : rotation[0];
        final float y = rotation == null ? 0f : rotation[1];
        final float z = rotation == null ? 0f : rotation[2];
        final float w = rotation == null ? 1f : rotation[3];
        final float sx = scale == null ? 1f : scale[0];
        final float sy = scale == null ? 1f : scale[1];
        final float sz = scale == null ? 1f : scale[2];
        final float[] m = new float[16];
        m[0] = (1 - 2 * (y * y + z * z)) * sx;
        m[1] = (2 * (x * y + z * w)) * sx;
        m[2] = (2 * (x * z - y * w)) * sx;
        m[4] = (2 * (x * y - z * w)) * sy;
        m[5] = (1 - 2 * (x * x + z * z)) * sy;
        m[6] = (2 * (y * z + x * w)) * sy;
        m[8] = (2 * (x * z + y * w)) * sz;
        m[9] = (2 * (y * z - x * w)) * sz;
        m[10] = (1 - 2 * (x * x + y * y)) * sz;
        m[12] = translation[0];
        m[13] = translation[1];
        m[14] = translation[2];
        m[15] = 1f;
        return m;
    }
}
