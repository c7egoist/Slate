//============================================================================================================================================
//                                                               OVERLAYPASS.H
//============================================================================================================================================
// 🧩 The editor overlay's own GPU pass — the grid, the gizmo and (later) wireframe
//    drawn by a dedicated graphics program in a pass of their own, instead of
//    through the interface's ImGui draw lists.
//
//    🔴 WHY A SEPARATE PASS. The interface records everything through ImGui, which
//       tessellates every polyline on the CPU and blends premultiplied alpha —
//       a dense lattice or a high-poly wireframe bogged the frame down, and a
//       low-alpha line over a bright sky read washed out. This pass consumes the
//       CPU-side `OverlayGeometry` record (a few hundred primitives), uploads it
//       when its generation changes, expands lines and dots in the VERTEX SHADER
//       (two CPU points per line, one per dot), and blends straight alpha with no
//       tone mapping — vivid colours, and the CPU never tessellates.
//
//    The pass records INSIDE the host's dynamic-rendering scope, after the
//    interface: the host calls `Record` between `BeginRendering` and `Complete`.
//    It is self-contained — its own pipeline, layout, descriptor set and buffer —
//    following `AtmospherePresentationSurface`'s precedent of owning its objects directly.

#pragma once

#include "Foundation/DeliveryGuarantee.h"
#include "Shared/OverlayGeometry.slang.h"
#include "SlateVulkan/Device/DiagnosticExtension/Api/DiagnosticExtension.h"
#include "SlateVulkan/Device/ShaderCodec/Api/ShaderCodec.h"
#include "SlateVulkan/Device/VulkanExchange/Api/VulkanExchange.h"

#include <vulkan/vulkan.h>

#include <cstdint>

namespace Slate
{

/// 🧩 Owns the overlay pipeline, its descriptor set and its vertex buffer, and records the overlay
///    primitives after the interface within the open dynamic-rendering scope.
/// tag   owning, nonallocating, nonthrowing
class WorkspaceOverlayPass
{
public:

    static constexpr std::uint32_t LineCapacity     = OverlayGeometry::LineLimit;     // [-] - line records
    static constexpr std::uint32_t DotCapacity      = OverlayGeometry::DotLimit;      // [-] - dot records
    static constexpr std::uint32_t TriangleCapacity = OverlayGeometry::TriangleLimit; // [-] - triangle records

    WorkspaceOverlayPass()                              = default;
    WorkspaceOverlayPass(const WorkspaceOverlayPass&)            = delete;
    WorkspaceOverlayPass& operator=(const WorkspaceOverlayPass&) = delete;
    ~WorkspaceOverlayPass();

    /// 🧩 Constructs the pipeline, the descriptor set and the vertex buffer against the active device.
    /// in    Exchange     [-]  the created device; borrowed and outlives this component
    /// in    Naming       [-]  names every object; borrowed and outlives this component
    /// in    Streams      [-]  the lowered shader streams; `WorkspaceOverlayVertex` and `WorkspaceOverlayFragment`
    ///                         are resolved here, borrowed and outlives this component
    /// in    ColourFormat [-]  the display image's format; the pipeline's sole colour attachment
    /// out   Result       [-]  refuses with CapabilityAbsent when no device is active, HostDenied when
    ///                         either shader stream is absent (the build lowered nothing), and
    ///                         ContentUnsupported when the device declines any object
    /// cost  🔴
    /// tag   api, nonthrowing
    Deliver<bool> ConstructWorkspaceOverlayPass(const VulkanExchange&      Exchange,
                            const DiagnosticExtension& Naming,
                            ShaderCodec&               Streams,
                            VkFormat                   ColourFormat);

    /// 🧩 Copies the CPU overlay record into the mapped vertex buffer.
    /// in    Overlay  [-]  the panel's record; lines, dots and triangles are copied into their
    ///                     own regions of the one buffer, converted to the shader's record shapes
    /// note  🔴 Called BETWEEN frames, at most once per generation change: the buffer is
    ///        host-coherent and the previous frame's submission has completed, so the copy is
    ///        visible to the next draw with no barrier of its own.
    /// cost  🚩
    /// tag   api, nonthrowing
    void Upload(const OverlayGeometry& Overlay);

    /// 🧩 Records the overlay primitives inside the open dynamic-rendering scope, clipped to one
    ///    viewport leaf's box.
    /// in    Command  [-]  the recording, between `vkCmdBeginRendering` and `vkCmdEndRendering`
    /// in    Width    [px] the display extent the viewport state is set against
    /// in    Height   [px]
    /// in    ClipX0 [px]  the viewport leaf's box — the scissor is set to
    /// in    ClipY0 [px]
    /// in    ClipX1 [px]
    /// in    ClipY1 [px]
    ///                     it, so the grid, the axes and the gizmo NEVER paint over the outliner,
    ///                     the properties or any other panel; they are drawn only inside the
    ///                     viewport leaf that produced the geometry
    /// note  🔴 The lines draw as `4 × count`, the dots as `4 × count`, the triangles as
    ///        `3 × count` — the vertex stage expands by `SV_VertexID`, so nothing here tessellates.
    /// cost  ✔️
    /// tag   api, nonallocating, nonthrowing
    void Record(VkCommandBuffer Command, std::uint32_t Width, std::uint32_t Height,
                float ClipX0, float ClipY0, float ClipX1, float ClipY1);

    /// 🧩 Whether the pass stands — what the host tests before it records the GPU overlay, and what
    ///    decides whether the interface-drawn fallback (same geometry, drawn through the recording
    ///    surface) must stand in instead.
    /// note  🔴 The pass refuses when the build lowered no shaders; the host then draws the fallback
    ///        so the grid, the axes and the gizmo are visible even without the GPU pass.
    /// cost  ✔️
    /// tag   api, nonallocating, nonthrowing
    bool Standing() const
    {
        return DeviceEdge != nullptr && OverlayPipeline != VK_NULL_HANDLE;
    }

    /// 🧩 Destroys every object and forgets the device handles, ahead of a device rebuild.
    /// cost  🔴
    /// tag   api, nonthrowing
    void Reclaim();

private:

    const VulkanExchange*    DeviceEdge = nullptr;   // [-] - borrowed; never owned
    const DiagnosticExtension* NamingEdge = nullptr; // [-] - borrowed; never owned

    VkBuffer                 VertexBuffer   = VK_NULL_HANDLE;   // [-] - lines, dots, triangles, one buffer
    VkDeviceMemory           VertexMemory   = VK_NULL_HANDLE;   // [-]
    std::uint8_t*            MappedSlot     = nullptr;          // [-] - persistent mapping

    VkDescriptorSetLayout    OverlayLayout  = VK_NULL_HANDLE;   // [-] - three storage bindings
    VkDescriptorPool         OverlayPool    = VK_NULL_HANDLE;   // [-]
    VkDescriptorSet          OverlaySet     = VK_NULL_HANDLE;   // [-]

    VkPipelineLayout         OverlayPipelineLayout = VK_NULL_HANDLE;   // [-] - the set + the push constant
    VkPipeline               OverlayPipeline       = VK_NULL_HANDLE;   // [-]

    std::uint32_t            LineBytes      = 0u;               // [B] - the line region's extent
    std::uint32_t            DotBytes       = 0u;               // [B] - the dot region's extent
    std::uint32_t            TriangleBytes  = 0u;               // [B] - the triangle region's extent

    // 📐 The analytic ground's pose, taken from the uploaded record. It is a
    //    camera rather than a vertex count, so it lives beside the counts and not
    //    in the vertex buffer.
    OverlayGroundPose        OverlayGround        = {};

    std::uint32_t            OverlayLineCount     = 0u;         // [-] - the uploaded record's counts
    std::uint32_t            OverlayDotCount      = 0u;         // [-]
    std::uint32_t            OverlayTriangleCount = 0u;         // [-]
};

} // namespace Slate
