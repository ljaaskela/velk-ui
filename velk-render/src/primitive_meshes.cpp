#include "mesh_builder.h"

#include <velk/api/velk.h>
#include <velk/vector.h>

#include <velk-render/gpu_data.h>

#include <cmath>

namespace velk::impl {

namespace {

// Vertex layout shared by all procedural primitives. Matches the
// `VelkVertex3D` struct in velk.glsl declared with `layout(scalar)`
// so vec3 fields pack tightly (12-byte aligned) instead of being
// rounded up to 16 by std430.
//
//   offset  0..12  position
//   offset 12..24  normal
//   offset 24..32  uv
// Stride = 32 bytes.
constexpr uint32_t kVertex3DStride = 32;

const VertexAttribute kVertex3DAttributes[] = {
    { VertexAttributeSemantic::Position,  VertexAttributeFormat::Float3, 0 },
    { VertexAttributeSemantic::Normal,    VertexAttributeFormat::Float3, 12 },
    { VertexAttributeSemantic::TexCoord0, VertexAttributeFormat::Float2, 24 },
};

struct Vertex3D
{
    float position[3];
    float normal[3];
    float uv[2];
};
static_assert(sizeof(Vertex3D) == kVertex3DStride,
              "Vertex3D must be tightly packed to match GLSL scalar layout");

// Appends one N×N face into the interleaved vertex and index buffers.
// `origin`, `u_axis`, `v_axis` span the [0,1]^2 face in the mesh's
// local unit-bounds space; `normal` is the outward-facing normal.
void append_face(::velk::vector<Vertex3D>& verts,
                 ::velk::vector<uint32_t>& indices,
                 const float origin[3],
                 const float u_axis[3],
                 const float v_axis[3],
                 const float normal[3],
                 uint32_t grid)
{
    const uint32_t base = static_cast<uint32_t>(verts.size());
    const uint32_t side = grid + 1;  // vertices per side

    for (uint32_t vy = 0; vy < side; ++vy) {
        for (uint32_t vx = 0; vx < side; ++vx) {
            float u = static_cast<float>(vx) / static_cast<float>(grid);
            float v = static_cast<float>(vy) / static_cast<float>(grid);
            Vertex3D vert{};  // value-init zeros the .w / trailing pad slots
            for (int i = 0; i < 3; ++i) {
                vert.position[i] = origin[i] + u_axis[i] * u + v_axis[i] * v;
                vert.normal[i] = normal[i];
            }
            vert.uv[0] = u;
            vert.uv[1] = v;
            verts.push_back(vert);
        }
    }

    for (uint32_t vy = 0; vy < grid; ++vy) {
        for (uint32_t vx = 0; vx < grid; ++vx) {
            uint32_t i00 = base + vy * side + vx;
            uint32_t i10 = i00 + 1;
            uint32_t i01 = i00 + side;
            uint32_t i11 = i01 + 1;
            // CCW winding when viewed from outside (normal direction).
            indices.push_back(i00);
            indices.push_back(i10);
            indices.push_back(i11);
            indices.push_back(i00);
            indices.push_back(i11);
            indices.push_back(i01);
        }
    }
}

} // namespace

IMesh::Ptr MeshBuilder::make_cube(uint32_t subdivisions)
{
    const uint32_t grid = subdivisions == 0 ? 1u : subdivisions;

    ::velk::vector<Vertex3D> verts;
    ::velk::vector<uint32_t> indices;

    // Unit cube occupying [0,1]^3. Faces ordered: -X, +X, -Y, +Y, -Z, +Z.
    // Per-face vertices so each face has its own normals/uvs.
    struct FaceDef
    {
        float origin[3];
        float u[3];
        float v[3];
        float n[3];
    };
    const FaceDef faces[] = {
        // -X
        { {0, 0, 0}, {0, 0, 1}, {0, 1, 0}, {-1, 0, 0} },
        // +X
        { {1, 0, 1}, {0, 0, -1}, {0, 1, 0}, {1, 0, 0} },
        // -Y
        { {0, 0, 0}, {1, 0, 0}, {0, 0, 1}, {0, -1, 0} },
        // +Y
        { {0, 1, 1}, {1, 0, 0}, {0, 0, -1}, {0, 1, 0} },
        // -Z
        { {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, 0, -1} },
        // +Z
        { {0, 0, 1}, {1, 0, 0}, {0, 1, 0}, {0, 0, 1} },
    };
    for (const auto& f : faces) {
        append_face(verts, indices, f.origin, f.u, f.v, f.n, grid);
    }

    aabb bounds{};
    bounds.position = { 0.f, 0.f, 0.f };
    bounds.extent = { 1.f, 1.f, 1.f };

    return build({ kVertex3DAttributes, 3 },
                 kVertex3DStride,
                 verts.data(), static_cast<uint32_t>(verts.size()),
                 indices.data(), static_cast<uint32_t>(indices.size()),
                 MeshTopology::TriangleList,
                 bounds);
}

IMesh::Ptr MeshBuilder::make_sphere(uint32_t subdivisions)
{
    const uint32_t segments = subdivisions == 0 ? 16u : subdivisions;
    const uint32_t rings = (segments / 2u) > 2u ? (segments / 2u) : 3u;

    // Unit sphere inscribed in [0,1]^3: radius 0.5, center (0.5, 0.5, 0.5).
    // Using unit-bounds convention so the instance `size` scale maps
    // 1:1 with the element's axis-aligned extent.
    constexpr float kCenter = 0.5f;
    constexpr float kRadius = 0.5f;
    constexpr float kPi = 3.14159265358979323846f;

    ::velk::vector<Vertex3D> verts;
    ::velk::vector<uint32_t> indices;

    // (rings + 1) rings of (segments + 1) vertices each; the seam is
    // duplicated so uvs/normals are per-vertex (no shared-seam
    // wraparound artefacts).
    for (uint32_t r = 0; r <= rings; ++r) {
        float vy = static_cast<float>(r) / static_cast<float>(rings);
        float phi = vy * kPi;
        float sin_phi = std::sin(phi);
        float cos_phi = std::cos(phi);
        for (uint32_t s = 0; s <= segments; ++s) {
            float vx = static_cast<float>(s) / static_cast<float>(segments);
            float theta = vx * kPi * 2.0f;
            float sin_theta = std::sin(theta);
            float cos_theta = std::cos(theta);

            Vertex3D vert{};
            vert.normal[0] = sin_phi * cos_theta;
            vert.normal[1] = cos_phi;
            vert.normal[2] = sin_phi * sin_theta;
            vert.position[0] = kCenter + kRadius * vert.normal[0];
            vert.position[1] = kCenter + kRadius * vert.normal[1];
            vert.position[2] = kCenter + kRadius * vert.normal[2];
            vert.uv[0] = vx;
            vert.uv[1] = vy;
            verts.push_back(vert);
        }
    }

    const uint32_t stride = segments + 1;
    for (uint32_t r = 0; r < rings; ++r) {
        for (uint32_t s = 0; s < segments; ++s) {
            uint32_t i00 = r * stride + s;
            uint32_t i10 = i00 + 1;
            uint32_t i01 = i00 + stride;
            uint32_t i11 = i01 + 1;
            indices.push_back(i00);
            indices.push_back(i01);
            indices.push_back(i11);
            indices.push_back(i00);
            indices.push_back(i11);
            indices.push_back(i10);
        }
    }

    aabb bounds{};
    bounds.position = { 0.f, 0.f, 0.f };
    bounds.extent = { 1.f, 1.f, 1.f };

    return build({ kVertex3DAttributes, 3 },
                 kVertex3DStride,
                 verts.data(), static_cast<uint32_t>(verts.size()),
                 indices.data(), static_cast<uint32_t>(indices.size()),
                 MeshTopology::TriangleList,
                 bounds);
}

IMesh::Ptr MeshBuilder::get_cube(uint32_t subdivisions)
{
    auto it = cube_cache_.find(subdivisions);
    if (it != cube_cache_.end()) {
        return it->second;
    }
    auto mesh = make_cube(subdivisions);
    cube_cache_[subdivisions] = mesh;
    return mesh;
}

IMesh::Ptr MeshBuilder::get_sphere(uint32_t subdivisions)
{
    auto it = sphere_cache_.find(subdivisions);
    if (it != sphere_cache_.end()) {
        return it->second;
    }
    auto mesh = make_sphere(subdivisions);
    sphere_cache_[subdivisions] = mesh;
    return mesh;
}

} // namespace velk::impl
