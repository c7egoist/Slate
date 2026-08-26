//============================================================================================================================================
//                                                   SHAREDVIEWPORTHOSTBRIDGE.H
//============================================================================================================================================
// 🧩 Shared host-side viewport support used by EditorHost, PaintHost and ParametricSketchHost.
//    The hosts stay standalone executables; this header keeps their common runtime decisions in one place.

#pragma once

#include "Foundation/DeliveryGuarantee.h"
#include "SlateDocument/Format/WorkspaceSceneActivation/Api/WorkspaceSceneActivation.h"
#include "SlateUI/Interface/ContentBrowserPanel/Api/ContentBrowserPanel.h"
#include "SlateUI/Interface/InterfaceExchange/Api/RecordingSurface.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <string>

namespace Slate
{

//------------------------------------------------------------------------------------------------------------------------
//                                                  HOST FEATURE SEPARATION
//------------------------------------------------------------------------------------------------------------------------

// 📝 Compile-time feature switches. Hosts pass one of these masks into the shared helpers rather than
//    cloning the helper bodies. The standalone boundaries remain explicit: Paint does not ask for CAD,
//    ParametricSketch does not ask for texture-paint layer editing, and Editor may ask for both.
constexpr std::uint32_t SharedViewportFeatureSceneEnvironment = 1u << 0u;
constexpr std::uint32_t SharedViewportFeatureCodexScene       = 1u << 1u;
constexpr std::uint32_t SharedViewportFeatureCadSketch        = 1u << 2u;
constexpr std::uint32_t SharedViewportFeaturePaintWorkspace   = 1u << 3u;

#if defined(SLATE_EDITOR_HOST)
constexpr std::uint32_t SharedViewportHostFeatures = SharedViewportFeatureSceneEnvironment |
                                                     SharedViewportFeatureCodexScene |
                                                     SharedViewportFeatureCadSketch |
                                                     SharedViewportFeaturePaintWorkspace;
#elif defined(SLATE_PAINT_HOST)
constexpr std::uint32_t SharedViewportHostFeatures = SharedViewportFeatureSceneEnvironment |
                                                     SharedViewportFeatureCodexScene |
                                                     SharedViewportFeaturePaintWorkspace;
#elif defined(SLATE_PARAMETRIC_SKETCH_HOST)
constexpr std::uint32_t SharedViewportHostFeatures = SharedViewportFeatureCodexScene |
                                                     SharedViewportFeatureCadSketch;
#else
constexpr std::uint32_t SharedViewportHostFeatures = 0u;
#endif

enum class SharedViewportOrientation : std::uint32_t
{
    None = 0u,
    Top,
    Bottom,
    Front,
    Back,
    Right,
    Left,
    Iso
};

struct SharedViewportBasis
{
    double Right[3] = { 1.0, 0.0, 0.0 };
    double Up[3] = { 0.0, 1.0, 0.0 };
    double Forward[3] = { 0.0, 0.0, 1.0 };
};

struct SharedViewportCameraSeed
{
    double Position[3] = { 0.0, 1.2, -4.0 };
    double YawDegrees = 0.0;
    double PitchDegrees = -8.0;
    double FieldOfViewDegrees = 60.0;
};

inline SharedViewportCameraSeed SharedViewportDefaultCamera()
{
    return SharedViewportCameraSeed{};
}

inline bool SharedViewportHasFeature(std::uint32_t FeatureMask)
{
    return (SharedViewportHostFeatures & FeatureMask) != 0u;
}

inline SharedViewportBasis SharedViewportBasisFromYawPitch(double YawDegrees, double PitchDegrees)
{
    const double Yaw = YawDegrees * 3.14159265358979323846 / 180.0;
    const double Pitch = PitchDegrees * 3.14159265358979323846 / 180.0;
    const double CosP = std::cos(Pitch);
    const double SinP = std::sin(Pitch);
    const double SinY = std::sin(Yaw);
    const double CosY = std::cos(Yaw);

    SharedViewportBasis Basis;
    Basis.Forward[0] = CosP * SinY;
    Basis.Forward[1] = SinP;
    Basis.Forward[2] = CosP * CosY;
    Basis.Right[0] = CosY;
    Basis.Right[1] = 0.0;
    Basis.Right[2] = -SinY;
    Basis.Up[0] = -SinP * SinY;
    Basis.Up[1] = CosP;
    Basis.Up[2] = -SinP * CosY;
    return Basis;
}

inline const char* SharedViewportOrientationName(SharedViewportOrientation Orientation)
{
    switch (Orientation)
    {
        case SharedViewportOrientation::Top: return "Top";
        case SharedViewportOrientation::Bottom: return "Bottom";
        case SharedViewportOrientation::Front: return "Front";
        case SharedViewportOrientation::Back: return "Back";
        case SharedViewportOrientation::Right: return "Right";
        case SharedViewportOrientation::Left: return "Left";
        case SharedViewportOrientation::Iso: return "Iso";
        case SharedViewportOrientation::None: return "";
    }
    return "";
}

inline void SharedViewportOrientationPreset(SharedViewportOrientation Orientation,
                                            double& YawDegrees,
                                            double& PitchDegrees)
{
    switch (Orientation)
    {
        case SharedViewportOrientation::Top:    YawDegrees = 0.0;   PitchDegrees = 80.0;  break;
        case SharedViewportOrientation::Bottom: YawDegrees = 0.0;   PitchDegrees = -80.0; break;
        case SharedViewportOrientation::Front:  YawDegrees = 0.0;   PitchDegrees = 0.0;   break;
        case SharedViewportOrientation::Back:   YawDegrees = 180.0; PitchDegrees = 0.0;   break;
        case SharedViewportOrientation::Right:  YawDegrees = 90.0;  PitchDegrees = 0.0;   break;
        case SharedViewportOrientation::Left:   YawDegrees = -90.0; PitchDegrees = 0.0;   break;
        case SharedViewportOrientation::Iso:    YawDegrees = 52.0;  PitchDegrees = 24.0;  break;
        case SharedViewportOrientation::None: break;
    }
}

inline double SharedViewportCameraDepth(const SharedViewportBasis& Basis, const double Axis[3])
{
    // The HTML reference (References/Cad/js/viewport3d.js::vp3Basis/vp3Gizmo) uses
    // camera-forward = target - eye for depth ordering. Editor/Paint yaw-pitch stores
    // the eye ray in Basis.Forward, while Parametric passes an already camera-forward
    // frame, so the public gizmo projection always normalizes depth through this helper
    // instead of letting each host guess front/back differently.
    return -(Axis[0] * Basis.Forward[0] + Axis[1] * Basis.Forward[1] + Axis[2] * Basis.Forward[2]);
}

inline void SharedViewportOrientationPoint(const SharedViewportBasis& Basis,
                                           const PlaneExtent& Extent,
                                           const double Axis[3],
                                           float& X,
                                           float& Y,
                                           double& Depth)
{
    constexpr float Radius = 34.0f;
    const float CentreX = Extent.MaximumX - 70.0f;
    const float CentreY = Extent.MinimumY + 58.0f;
    const double SX = Axis[0] * Basis.Right[0] + Axis[1] * Basis.Right[1] + Axis[2] * Basis.Right[2];
    const double SY = Axis[0] * Basis.Up[0] + Axis[1] * Basis.Up[1] + Axis[2] * Basis.Up[2];
    Depth = SharedViewportCameraDepth(Basis, Axis);
    X = CentreX + static_cast<float>(SX) * Radius;
    Y = CentreY - static_cast<float>(SY) * Radius;
}

inline void RecordSharedViewportOrientationGizmo(RecordingSurface& Surface,
                                                 const PlaneExtent& Extent,
                                                 const SharedViewportBasis& Basis)
{
    struct AxisRecord
    {
        double Axis[3];
        ThemeToken Colour;
        bool Positive;
        const char* Label;
    };

    const AxisRecord Axes[6] =
    {
        { {  1.0,  0.0,  0.0 }, ThemeToken{ 0xFCu, 0x5Au, 0x5Au, 255u }, true,  "X" },
        { { -1.0,  0.0,  0.0 }, ThemeToken{ 0xFCu, 0x5Au, 0x5Au, 255u }, false, ""  },
        { {  0.0,  1.0,  0.0 }, ThemeToken{ 0x7Bu, 0xD6u, 0x6Au, 255u }, true,  "Y" },
        { {  0.0, -1.0,  0.0 }, ThemeToken{ 0x7Bu, 0xD6u, 0x6Au, 255u }, false, ""  },
        { {  0.0,  0.0,  1.0 }, ThemeToken{ 0x5Au, 0x8Bu, 0xFCu, 255u }, true,  "Z" },
        { {  0.0,  0.0, -1.0 }, ThemeToken{ 0x5Au, 0x8Bu, 0xFCu, 255u }, false, ""  },
    };

    struct ProjectedAxis
    {
        AxisRecord Axis;
        float X;
        float Y;
        double Depth;
    };

    ProjectedAxis Projected[6] = {};
    for (std::uint32_t Index = 0u; Index < 6u; ++Index)
    {
        Projected[Index].Axis = Axes[Index];
        SharedViewportOrientationPoint(Basis, Extent, Axes[Index].Axis,
                                       Projected[Index].X, Projected[Index].Y, Projected[Index].Depth);
    }

    std::sort(Projected, Projected + 6u, [](const ProjectedAxis& Left, const ProjectedAxis& Right)
    {
        return Left.Depth < Right.Depth;
    });

    const float CentreX = Extent.MaximumX - 70.0f;
    const float CentreY = Extent.MinimumY + 58.0f;
    Surface.Confine(Extent);
    for (const ProjectedAxis& Point : Projected)
    {
        if (!Point.Axis.Positive)
            continue;
        const float X[2] = { CentreX, Point.X };
        const float Y[2] = { CentreY, Point.Y };
        Surface.Polyline(X, Y, 2u, ThemeToken{ 255u, 255u, 255u, 36u }, 2.0f);
    }

    for (const ProjectedAxis& Point : Projected)
    {
        const float Radius = Point.Axis.Positive ? 9.0f : 7.0f;
        if (Point.Axis.Positive)
        {
            ThemeToken Fill = Point.Axis.Colour;
            Fill.Opacity = Point.Depth > 0.0 ? 255u : 190u;
            Surface.Medallion(Point.X, Point.Y, Radius, Fill);
            Surface.TextRun(Point.X - 3.2f, Point.Y - 5.1f,
                            ThemeToken{ 0u, 0u, 0u, 218u }, Point.Axis.Label, 10.0f, 0.0f, true);
        }
        else
        {
            Surface.Medallion(Point.X, Point.Y, Radius, ThemeToken{ 10u, 12u, 16u, 230u });
            Surface.Medallion(Point.X, Point.Y, Radius - 3.0f, Point.Axis.Colour);
        }
    }
    Surface.Release();
}

inline void SharedViewportProjectAxisPoint(const SharedViewportBasis& Basis,
                                           const double Axis[3],
                                           float Scale,
                                           float& X,
                                           float& Y,
                                           double& Depth)
{
    const double SX = Axis[0] * Basis.Right[0] + Axis[1] * Basis.Right[1] + Axis[2] * Basis.Right[2];
    const double SY = Axis[0] * Basis.Up[0] + Axis[1] * Basis.Up[1] + Axis[2] * Basis.Up[2];
    Depth = SharedViewportCameraDepth(Basis, Axis);
    X = static_cast<float>(SX) * Scale;
    Y = -static_cast<float>(SY) * Scale;
}

inline SharedViewportOrientation HitSharedViewportOrientationGizmo(const PlaneExtent& Extent,
                                                                   const SharedViewportBasis& Basis,
                                                                   float PointerX,
                                                                   float PointerY)
{
    struct HitAxis
    {
        double Axis[3];
        SharedViewportOrientation Orientation;
    };

    const HitAxis Axes[6] =
    {
        { {  1.0,  0.0,  0.0 }, SharedViewportOrientation::Right },
        { { -1.0,  0.0,  0.0 }, SharedViewportOrientation::Left },
        { {  0.0,  1.0,  0.0 }, SharedViewportOrientation::Top },
        { {  0.0, -1.0,  0.0 }, SharedViewportOrientation::Bottom },
        { {  0.0,  0.0,  1.0 }, SharedViewportOrientation::Front },
        { {  0.0,  0.0, -1.0 }, SharedViewportOrientation::Back },
    };

    SharedViewportOrientation Best = SharedViewportOrientation::None;
    float BestDistance = 13.0f * 13.0f;
    for (const HitAxis& Axis : Axes)
    {
        float X = 0.0f;
        float Y = 0.0f;
        double Depth = 0.0;
        SharedViewportOrientationPoint(Basis, Extent, Axis.Axis, X, Y, Depth);
        static_cast<void>(Depth);
        const float DX = PointerX - X;
        const float DY = PointerY - Y;
        const float Distance = DX * DX + DY * DY;
        if (Distance < BestDistance)
        {
            BestDistance = Distance;
            Best = Axis.Orientation;
        }
    }
    return Best;
}

inline void RecordSharedViewportCadCube(RecordingSurface& Surface,
                                        const PlaneExtent& Extent,
                                        const SharedViewportBasis& Basis)
{
    struct FaceRecord
    {
        SharedViewportOrientation Orientation;
        double Normal[3];
        ThemeToken Colour;
        const char* Label;
        float Corners[8];
        float CentreX;
        float CentreY;
        double Depth;
    };

    const float CentreX = Extent.MaximumX - 70.0f;
    const float CentreY = Extent.MinimumY + 58.0f;
    const float Scale = 28.0f;
    const double AxisX[3] = { 1.0, 0.0, 0.0 };
    const double AxisY[3] = { 0.0, 1.0, 0.0 };
    const double AxisZ[3] = { 0.0, 0.0, 1.0 };
    float VX = 0.0f, VY = 0.0f, UX = 0.0f, UY = 0.0f, WX = 0.0f, WY = 0.0f;
    double DX = 0.0, DY = 0.0, DZ = 0.0;
    SharedViewportProjectAxisPoint(Basis, AxisX, Scale, VX, VY, DX);
    SharedViewportProjectAxisPoint(Basis, AxisY, Scale, UX, UY, DY);
    SharedViewportProjectAxisPoint(Basis, AxisZ, Scale, WX, WY, DZ);
    const float OriginX = CentreX - (VX + UX + WX) * 0.5f;
    const float OriginY = CentreY - (VY + UY + WY) * 0.5f;

    const auto Point = [&](float XMul, float YMul, float ZMul, float& X, float& Y)
    {
        X = OriginX + VX * XMul + UX * YMul + WX * ZMul;
        Y = OriginY + VY * XMul + UY * YMul + WY * ZMul;
    };
    const auto FillFace = [&](FaceRecord& Face,
                              float A0, float A1, float A2,
                              float B0, float B1, float B2,
                              float C0, float C1, float C2,
                              float D0, float D1, float D2)
    {
        Point(A0, A1, A2, Face.Corners[0], Face.Corners[1]);
        Point(B0, B1, B2, Face.Corners[2], Face.Corners[3]);
        Point(C0, C1, C2, Face.Corners[4], Face.Corners[5]);
        Point(D0, D1, D2, Face.Corners[6], Face.Corners[7]);
        Face.CentreX = (Face.Corners[0] + Face.Corners[2] + Face.Corners[4] + Face.Corners[6]) * 0.25f;
        Face.CentreY = (Face.Corners[1] + Face.Corners[3] + Face.Corners[5] + Face.Corners[7]) * 0.25f;
        Face.Depth = SharedViewportCameraDepth(Basis, Face.Normal);
    };

    FaceRecord Faces[6] =
    {
        { SharedViewportOrientation::Right,  {  1.0,  0.0,  0.0 }, ThemeToken{ 0xFCu, 0x5Au, 0x5Au, 150u }, "Right" },
        { SharedViewportOrientation::Left,   { -1.0,  0.0,  0.0 }, ThemeToken{ 0xFCu, 0x5Au, 0x5Au, 110u }, "Left"  },
        { SharedViewportOrientation::Top,    {  0.0,  1.0,  0.0 }, ThemeToken{ 0xF8u, 0xFAu, 0xFCu, 150u }, "Top"   },
        { SharedViewportOrientation::Bottom, {  0.0, -1.0,  0.0 }, ThemeToken{ 0xD8u, 0xDEu, 0xEAu, 105u }, "Bottom"},
        { SharedViewportOrientation::Front,  {  0.0,  0.0,  1.0 }, ThemeToken{ 0x5Bu, 0x8Cu, 0xFFu, 150u }, "Front" },
        { SharedViewportOrientation::Back,   {  0.0,  0.0, -1.0 }, ThemeToken{ 0x5Bu, 0x8Cu, 0xFFu, 110u }, "Back"  },
    };

    FillFace(Faces[0], 1,0,0, 1,1,0, 1,1,1, 1,0,1);
    FillFace(Faces[1], 0,0,0, 0,0,1, 0,1,1, 0,1,0);
    FillFace(Faces[2], 0,1,0, 0,1,1, 1,1,1, 1,1,0);
    FillFace(Faces[3], 0,0,0, 1,0,0, 1,0,1, 0,0,1);
    FillFace(Faces[4], 0,0,1, 1,0,1, 1,1,1, 0,1,1);
    FillFace(Faces[5], 0,0,0, 0,1,0, 1,1,0, 1,0,0);

    std::sort(Faces, Faces + 6u, [](const FaceRecord& Left, const FaceRecord& Right)
    {
        return Left.Depth > Right.Depth;
    });

    const auto DrawFaceLabel = [&](const FaceRecord& Face)
    {
        // CAD cube labels are vector strokes projected into the actual face parallelogram. They are not
        // screen-space TextRun overlays; every stroke endpoint is interpolated from the face's corners so
        // the word lives on the same projected 3D face in perspective/orthographic views.
        const auto FacePoint = [&](float U, float V, float& X, float& Y)
        {
            const float Ax = Face.Corners[0];
            const float Ay = Face.Corners[1];
            const float Bx = Face.Corners[2];
            const float By = Face.Corners[3];
            const float Dx = Face.Corners[6];
            const float Dy = Face.Corners[7];
            X = Ax + (Bx - Ax) * U + (Dx - Ax) * V;
            Y = Ay + (By - Ay) * U + (Dy - Ay) * V;
        };
        const auto Stroke = [&](float X0, float Y0, float X1, float Y1)
        {
            float SX0 = 0.0f, SY0 = 0.0f, SX1 = 0.0f, SY1 = 0.0f;
            FacePoint(X0, Y0, SX0, SY0);
            FacePoint(X1, Y1, SX1, SY1);
            const float Xs[2] = { SX0, SX1 };
            const float Ys[2] = { SY0, SY1 };
            Surface.Polyline(Xs, Ys, 2u, ThemeToken{ 16u, 18u, 24u, 225u }, 1.25f);
        };
        const auto Glyph = [&](char C, float X, float Y, float W, float H)
        {
            const auto L = [&](float X0, float Y0, float X1, float Y1)
            {
                Stroke(X + X0 * W, Y + Y0 * H, X + X1 * W, Y + Y1 * H);
            };
            switch (C)
            {
                case 'A': L(0,1,0.5f,0); L(1,1,0.5f,0); L(0.22f,0.55f,0.78f,0.55f); break;
                case 'B': L(0,0,0,1); L(0,0,0.72f,0); L(0.72f,0,0.9f,0.22f); L(0.9f,0.22f,0.72f,0.48f); L(0,0.48f,0.72f,0.48f); L(0.72f,0.48f,0.9f,0.74f); L(0.9f,0.74f,0.72f,1); L(0.72f,1,0,1); break;
                case 'C': L(1,0.12f,0.82f,0); L(0.82f,0,0.18f,0); L(0.18f,0,0,0.2f); L(0,0.2f,0,0.8f); L(0,0.8f,0.18f,1); L(0.18f,1,0.82f,1); L(0.82f,1,1,0.88f); break;
                case 'E': L(1,0,0,0); L(0,0,0,1); L(0,0.5f,0.78f,0.5f); L(0,1,1,1); break;
                case 'F': L(0,0,0,1); L(0,0,1,0); L(0,0.5f,0.78f,0.5f); break;
                case 'G': L(1,0.14f,0.82f,0); L(0.82f,0,0.18f,0); L(0.18f,0,0,0.2f); L(0,0.2f,0,0.8f); L(0,0.8f,0.18f,1); L(0.18f,1,0.82f,1); L(0.82f,1,1,0.82f); L(1,0.82f,1,0.58f); L(1,0.58f,0.58f,0.58f); break;
                case 'H': L(0,0,0,1); L(1,0,1,1); L(0,0.5f,1,0.5f); break;
                case 'I': L(0,0,1,0); L(0.5f,0,0.5f,1); L(0,1,1,1); break;
                case 'K': L(0,0,0,1); L(1,0,0,0.5f); L(0,0.5f,1,1); break;
                case 'L': L(0,0,0,1); L(0,1,1,1); break;
                case 'M': L(0,1,0,0); L(0,0,0.5f,0.55f); L(0.5f,0.55f,1,0); L(1,0,1,1); break;
                case 'N': L(0,1,0,0); L(0,0,1,1); L(1,1,1,0); break;
                case 'O': L(0.18f,0,0.82f,0); L(0.82f,0,1,0.18f); L(1,0.18f,1,0.82f); L(1,0.82f,0.82f,1); L(0.82f,1,0.18f,1); L(0.18f,1,0,0.82f); L(0,0.82f,0,0.18f); L(0,0.18f,0.18f,0); break;
                case 'P': L(0,1,0,0); L(0,0,0.78f,0); L(0.78f,0,1,0.24f); L(1,0.24f,0.78f,0.5f); L(0.78f,0.5f,0,0.5f); break;
                case 'R': L(0,1,0,0); L(0,0,0.78f,0); L(0.78f,0,1,0.24f); L(1,0.24f,0.78f,0.5f); L(0.78f,0.5f,0,0.5f); L(0.45f,0.5f,1,1); break;
                case 'T': L(0,0,1,0); L(0.5f,0,0.5f,1); break;
                default: break;
            }
        };
        const char* Text = Face.Label;
        std::uint32_t Count = 0u;
        while (Text[Count] != '\0') ++Count;
        const float Gap = 0.018f;
        const float LetterW = std::min(0.135f, (0.78f - Gap * static_cast<float>(Count > 0u ? Count - 1u : 0u)) / std::max(1.0f, static_cast<float>(Count)));
        const float Height = LetterW * 1.5f;
        const float Total = LetterW * static_cast<float>(Count) + Gap * static_cast<float>(Count > 0u ? Count - 1u : 0u);
        const float StartX = 0.5f - Total * 0.5f;
        const float StartY = 0.5f - Height * 0.5f;
        for (std::uint32_t Index = 0u; Index < Count; ++Index)
            Glyph(Text[Index], StartX + static_cast<float>(Index) * (LetterW + Gap), StartY, LetterW, Height);
    };

    Surface.Confine(Extent);
    for (const FaceRecord& Face : Faces)
    {
        const float T0[6] = { Face.Corners[0], Face.Corners[1], Face.Corners[2], Face.Corners[3], Face.Corners[4], Face.Corners[5] };
        const float T1[6] = { Face.Corners[0], Face.Corners[1], Face.Corners[4], Face.Corners[5], Face.Corners[6], Face.Corners[7] };
        Surface.Tongue(T0, 3u, Face.Colour);
        Surface.Tongue(T1, 3u, Face.Colour);
        const float X0[2] = { Face.Corners[0], Face.Corners[2] }; const float Y0[2] = { Face.Corners[1], Face.Corners[3] };
        const float X1[2] = { Face.Corners[2], Face.Corners[4] }; const float Y1[2] = { Face.Corners[3], Face.Corners[5] };
        const float X2[2] = { Face.Corners[4], Face.Corners[6] }; const float Y2[2] = { Face.Corners[5], Face.Corners[7] };
        const float X3[2] = { Face.Corners[6], Face.Corners[0] }; const float Y3[2] = { Face.Corners[7], Face.Corners[1] };
        Surface.Polyline(X0, Y0, 2u, ThemeToken{ 255u, 255u, 255u, 190u }, 1.2f);
        Surface.Polyline(X1, Y1, 2u, ThemeToken{ 255u, 255u, 255u, 190u }, 1.2f);
        Surface.Polyline(X2, Y2, 2u, ThemeToken{ 255u, 255u, 255u, 190u }, 1.2f);
        Surface.Polyline(X3, Y3, 2u, ThemeToken{ 255u, 255u, 255u, 190u }, 1.2f);

        if (Face.Depth <= 0.25)
            DrawFaceLabel(Face);
    }
    Surface.Release();
}

inline SharedViewportOrientation HitSharedViewportCadCube(const PlaneExtent& Extent,
                                                          const SharedViewportBasis& Basis,
                                                          float PointerX,
                                                          float PointerY)
{
    const float CentreX = Extent.MaximumX - 70.0f;
    const float CentreY = Extent.MinimumY + 58.0f;
    const float Scale = 28.0f;
    const double Axis[6][3] =
    {
        {  1.0,  0.0,  0.0 }, { -1.0,  0.0,  0.0 },
        {  0.0,  1.0,  0.0 }, {  0.0, -1.0,  0.0 },
        {  0.0,  0.0,  1.0 }, {  0.0,  0.0, -1.0 }
    };
    const SharedViewportOrientation Orientation[6] =
    {
        SharedViewportOrientation::Right, SharedViewportOrientation::Left,
        SharedViewportOrientation::Top, SharedViewportOrientation::Bottom,
        SharedViewportOrientation::Front, SharedViewportOrientation::Back
    };

    SharedViewportOrientation Best = SharedViewportOrientation::None;
    float BestDistance = 24.0f * 24.0f;
    for (std::uint32_t Index = 0u; Index < 6u; ++Index)
    {
        float X = 0.0f;
        float Y = 0.0f;
        double Depth = 0.0;
        SharedViewportProjectAxisPoint(Basis, Axis[Index], Scale * 0.58f, X, Y, Depth);
        X += CentreX;
        Y += CentreY;
        const float DX = PointerX - X;
        const float DY = PointerY - Y;
        const float Distance = DX * DX + DY * DY;
        if (Distance < BestDistance)
        {
            BestDistance = Distance;
            Best = Orientation[Index];
        }
    }
    return Best;
}

inline void RecordSharedViewportGizmo(RecordingSurface& Surface,
                                      const PlaneExtent& Extent,
                                      const SharedViewportBasis& Basis,
                                      bool CadMode)
{
    if (CadMode)
        RecordSharedViewportCadCube(Surface, Extent, Basis);
    else
        RecordSharedViewportOrientationGizmo(Surface, Extent, Basis);
}

inline SharedViewportOrientation HitSharedViewportGizmo(const PlaneExtent& Extent,
                                                        const SharedViewportBasis& Basis,
                                                        float PointerX,
                                                        float PointerY,
                                                        bool CadMode)
{
    return CadMode ? HitSharedViewportCadCube(Extent, Basis, PointerX, PointerY)
                   : HitSharedViewportOrientationGizmo(Extent, Basis, PointerX, PointerY);
}

//------------------------------------------------------------------------------------------------------------------------
//                                                    ENGINE CONTENT
//------------------------------------------------------------------------------------------------------------------------

inline std::filesystem::path ResolveEngineContentRoot(const std::filesystem::path& ExecutablePath)
{
    const auto Standing = [](const std::filesystem::path& Candidate)
    {
        return std::filesystem::exists(Candidate / "WhiteTeaService.codex") ||
               std::filesystem::exists(Candidate / "FontArchives");
    };

    const std::filesystem::path Starts[3] =
    {
        std::filesystem::current_path() / "EngineContent",
        ExecutablePath.parent_path() / "EngineContent",
        ExecutablePath.parent_path().parent_path() / "EngineContent"
    };

    for (const std::filesystem::path& Candidate : Starts)
        if (Standing(Candidate))
            return Candidate.lexically_normal();

    std::filesystem::path Walk = std::filesystem::current_path();
    for (std::uint32_t Step = 0u; Step < 8u; ++Step)
    {
        const std::filesystem::path Candidate = Walk / "EngineContent";
        if (Standing(Candidate))
            return Candidate.lexically_normal();

        if (!Walk.has_parent_path() || Walk.parent_path() == Walk)
            break;
        Walk = Walk.parent_path();
    }

    return (std::filesystem::current_path() / "EngineContent").lexically_normal();
}

//------------------------------------------------------------------------------------------------------------------------
//                                                    CODEX ACTIVATION
//------------------------------------------------------------------------------------------------------------------------

struct SharedCodexActivation
{
    bool                    Requested = false;
    bool                    Resolved = false;
    ActivatedWorkspaceScene Scene = {};
    Refusal                 Error = { RefusalReason::CapabilityAbsent, "no codex activation was requested" };
    std::filesystem::path   ScenePath = {};
};

inline bool ContentRecordIsCodexScene(const ContentRecord& Record)
{
    if (Record.Archive != ContentArchive::Arrangement || Record.Extension == nullptr)
        return false;

    return std::string(Record.Extension) == ".codex" || std::string(Record.Extension) == "codex";
}

inline void CenterActivatedSceneAtWorldOrigin(ActivatedWorkspaceScene& Scene)
{
    bool Any = false;
    double Minimum[3] = { 0.0, 0.0, 0.0 };
    double Maximum[3] = { 0.0, 0.0, 0.0 };
    for (const CodexSceneEntry& Entry : Scene.Workspace.Scene)
    {
        if (Entry.Subject != CodexSceneSubject::Geometry)
            continue;
        if (Entry.Naming.find("Floor") != std::string::npos)
            continue;
        if (!Any)
        {
            for (std::uint32_t Axis = 0u; Axis < 3u; ++Axis)
                Minimum[Axis] = Maximum[Axis] = Entry.Position[Axis];
            Any = true;
            continue;
        }
        for (std::uint32_t Axis = 0u; Axis < 3u; ++Axis)
        {
            Minimum[Axis] = std::min(Minimum[Axis], Entry.Position[Axis]);
            Maximum[Axis] = std::max(Maximum[Axis], Entry.Position[Axis]);
        }
    }
    if (!Any)
        return;

    const double Centre[3] =
    {
        (Minimum[0] + Maximum[0]) * 0.5,
        (Minimum[1] + Maximum[1]) * 0.5,
        (Minimum[2] + Maximum[2]) * 0.5
    };

    for (CodexSceneEntry& Entry : Scene.Workspace.Scene)
    {
        if (Entry.Subject != CodexSceneSubject::Geometry)
            continue;
        if (Entry.Naming.find("Floor") == std::string::npos)
            for (std::uint32_t Axis = 0u; Axis < 3u; ++Axis)
                Entry.Position[Axis] -= Centre[Axis];
    }
    for (ActivatedGeometryEntry& Entry : Scene.Geometry)
    {
        if (Entry.Entry.Subject != CodexSceneSubject::Geometry)
            continue;
        if (Entry.Entry.Naming.find("Floor") == std::string::npos)
            for (std::uint32_t Axis = 0u; Axis < 3u; ++Axis)
                Entry.Entry.Position[Axis] -= Centre[Axis];
    }
}

inline SharedCodexActivation ConsumeSharedCodexActivation(ContentBrowserConfiguration& Applied,
                                                          const ContentLibrary& Library,
                                                          const std::filesystem::path& EngineContentRoot)
{
    SharedCodexActivation Result;

    if (Applied.ActivationRequested >= Library.RecordCount)
        return Result;

    Result.Requested = true;
    const ContentRecord& Requested = Library.Records[Applied.ActivationRequested];
    Applied.ActivationRequested = ContentLibrary::AbsentIndex;

    if (!ContentRecordIsCodexScene(Requested))
    {
        Result.Error = { RefusalReason::ContentUnsupported, "the selected content record is not a codex scene" };
        return Result;
    }

    const std::string Extension = Requested.Extension != nullptr && Requested.Extension[0] == '.'
                                ? std::string(Requested.Extension)
                                : "." + std::string(Requested.Extension != nullptr ? Requested.Extension : "");
    Result.ScenePath = EngineContentRoot / (std::string(Requested.Naming) + Extension);

    WorkspaceSceneActivation Activating;
    Deliver<ActivatedWorkspaceScene> Activated = Activating.Open(Result.ScenePath.string(), EngineContentRoot.string());
    if (!Activated.Resolved)
    {
        Result.Error = Activated.Error;
        return Result;
    }

    ActivatedWorkspaceScene Loaded = Activated.Resolve();
    CenterActivatedSceneAtWorldOrigin(Loaded);
    Result.Scene = Loaded;
    Result.Resolved = true;
    Result.Error = {};
    return Result;
}

} // namespace Slate
