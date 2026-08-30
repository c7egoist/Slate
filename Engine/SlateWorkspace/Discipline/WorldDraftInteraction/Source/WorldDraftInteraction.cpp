//============================================================================================================================================
//                                                    WORLDDRAFTINTERACTION.CPP
//============================================================================================================================================

#include "SlateWorkspace/Discipline/WorldDraftInteraction/Api/WorldDraftInteraction.h"

#include "SlateWorkspace/Discipline/TransformSequence/Api/TransformSequence.h"

#include <cmath>

namespace Slate
{

namespace
{

bool SamePickIdentity(const WorldPick& Left,
                      const WorldPick& Right)
{
    if (Left.Subject != Right.Subject)
        return false;

    switch (Left.Subject)
    {
        case WorldPickSubject::Point:
            return Left.Point.IssuedIndex == Right.Point.IssuedIndex;
        case WorldPickSubject::Control:
            return Left.Control.IssuedIndex == Right.Control.IssuedIndex;
        case WorldPickSubject::Curve:
            return Left.Curve.IssuedIndex == Right.Curve.IssuedIndex;
        case WorldPickSubject::Loop:
            return Left.Loop.IssuedIndex == Right.Loop.IssuedIndex;
        case WorldPickSubject::None:
            return true;
    }
    return false;
}

GizmoHandle ResolveUniversalGizmoHandle(const GizmoScreenBasis& Screen,
                                        float PointerX,
                                        float PointerY)
{
    const GizmoHandle Move = ResolveGizmoHandle(Screen, TransformManner::Move, PointerX, PointerY);
    if (Move == GizmoHandle::MoveFree)
        return Move;

    const GizmoHandle Scale = ResolveGizmoHandle(Screen, TransformManner::Scale, PointerX, PointerY);
    if (Scale != GizmoHandle::None)
        return Scale;

    if (Move != GizmoHandle::None)
        return Move;

    return ResolveGizmoHandle(Screen, TransformManner::Rotate, PointerX, PointerY);
}

WorldPick ResolveActiveSelection(const WorldPick& SemanticSelection)
{
    return SemanticSelection;
}

} // namespace

bool RefreshWorldDraftPick(const WorldDraftStructure& Declared,
                           WorldPick& Pick)
{
    if (!Pick.Standing())
        return false;

    switch (Pick.Subject)
    {
        case WorldPickSubject::Point:
            return ResolveWorldDraftPointPosition(Declared, Pick.Point, Pick.Position);

        case WorldPickSubject::Control:
        {
            std::vector<WorldControlPlacement> Controls;
            if (!Pick.Curve.Assigned() || !ResolveWorldDraftControls(Declared, Pick.Curve, Controls))
                return false;
            for (const WorldControlPlacement& Control : Controls)
                if (Control.Name.IssuedIndex == Pick.Control.IssuedIndex)
                {
                    Pick.Position = Control.Position;
                    return true;
                }
            return false;
        }

        case WorldPickSubject::Curve:
            return ResolveWorldCurvePivot(Declared, Pick.Curve, Pick.Position);

        case WorldPickSubject::Loop:
            return ResolveWorldLoopPivot(Declared, Pick.Loop, Pick.Position);

        case WorldPickSubject::None:
            return false;
    }

    return false;
}

void DriveWorldDraftSelectionAndTransform(const PlaneExtent& Extent,
                                          const PointerCondition& Pointer,
                                          const TextInputCondition& TextInput,
                                          const SelectionOptions& Selection,
                                          const GizmoOptions& Gizmo,
                                          const ResolvedCamera& Camera,
                                          WorldDraftStructure& Declared,
                                          WorldPick& SemanticSelection,
                                          WorldPick& HoveredSelection,
                                          WorldDraftTransformSession& Transform,
                                          bool& PointerTaken,
                                          double SessionMilliseconds,
                                          double& LastGPressedMilliseconds,
                                          GizmoHandle* HoveredHandle)
{
    HoveredSelection = {};
    if (HoveredHandle != nullptr)
        *HoveredHandle = GizmoHandle::None;

    if (Extent.Encloses(Pointer.PositionX, Pointer.PositionY))
        ResolveWorldDraftPickForElement(Declared, Camera, Extent,
                                        Pointer.PositionX, Pointer.PositionY,
                                        Selection.ResolvedTolerance(),
                                        Selection.Element, HoveredSelection);

    WorldPick ActiveSelection = ResolveActiveSelection(SemanticSelection);
    RefreshWorldDraftPick(Declared, ActiveSelection);
    if (SemanticSelection.Standing())
        RefreshWorldDraftPick(Declared, SemanticSelection);

    GizmoHandle ResolvedHandle = GizmoHandle::None;
    if (Gizmo.Shown && !Transform.Engaged() && ActiveSelection.Standing())
    {
        GizmoScreenBasis Screen = {};
        if (ResolveGizmoScreenBasis(Camera, Extent, ActiveSelection.Position, Screen))
            ResolvedHandle = ResolveUniversalGizmoHandle(Screen, Pointer.PositionX, Pointer.PositionY);
    }
    if (HoveredHandle != nullptr)
        *HoveredHandle = ResolvedHandle;

    if (!PointerTaken && !Transform.Engaged() && Pointer.ContactPressed &&
        ResolvedHandle == GizmoHandle::None && HoveredSelection.Standing())
    {
        SemanticSelection = HoveredSelection;
        PointerTaken = true;
        ActiveSelection = SemanticSelection;
    }

    const TransformCommandIntake Command =
        ResolveTransformCommand(TextInput.Intake, TextInput.IntakeCount, Transform.Engaged(), Transform.Manner());

    if (!Transform.Engaged() && ActiveSelection.Standing() && Extent.Encloses(Pointer.PositionX, Pointer.PositionY))
    {
        if (ResolvedHandle == GizmoHandle::None && !PointerTaken && Pointer.ContactHeld &&
            !Pointer.ContactPressed && (std::fabs(Pointer.TravelX) + std::fabs(Pointer.TravelY)) > 0.0f &&
            HoveredSelection.Standing() && SamePickIdentity(HoveredSelection, ActiveSelection))
        {
            PointerTaken = StartWorldDraftTransformSession(Declared, Camera, Extent,
                                                           Pointer.PositionX, Pointer.PositionY,
                                                           ActiveSelection,
                                                           TransformRestriction::Free,
                                                           false, Transform, true);
        }

        if (ResolvedHandle != GizmoHandle::None && Pointer.ContactPressed)
        {
            if (ResolveHandleManner(ResolvedHandle) == TransformManner::Move)
            {
                PointerTaken = StartWorldDraftTransformSession(Declared, Camera, Extent,
                                                               Pointer.PositionX, Pointer.PositionY,
                                                               ActiveSelection,
                                                               ResolveHandleRestriction(ResolvedHandle),
                                                               false, Transform, true);
            }
        }
        else if (Command.StartRequested && Command.StartManner == TransformManner::Move)
        {
            const bool Slide = ResolveSlideRequested(Command.MoveTapCount,
                                                     SessionMilliseconds,
                                                     LastGPressedMilliseconds,
                                                     ActiveSelection.Curve.Assigned());
            if (Command.MoveTapCount > 0u)
                LastGPressedMilliseconds = SessionMilliseconds;
            PointerTaken = StartWorldDraftTransformSession(Declared, Camera, Extent,
                                                           Pointer.PositionX, Pointer.PositionY,
                                                           ActiveSelection,
                                                           Slide ? TransformRestriction::Curve
                                                                 : TransformRestriction::Free,
                                                           Slide, Transform, false);
        }
    }

    if (Transform.Engaged())
    {
        const bool SlideRequested = ResolveSlideRequested(Command.MoveTapCount,
                                                          SessionMilliseconds,
                                                          LastGPressedMilliseconds,
                                                          Transform.Target.Curve.Assigned());
        if (Command.MoveTapCount > 0u)
            LastGPressedMilliseconds = SessionMilliseconds;

        if (SlideRequested)
        {
            Transform.Restriction() = TransformRestriction::Curve;
            Transform.SlideAlongCurve() = true;
        }
        else if (Command.RestrictionRequested)
        {
            Transform.Restriction() = Command.Restriction;
            Transform.SlideAlongCurve() = false;
        }

        if (Command.NumericAppend[0] != '\0')
            AppendTransformNumericRun(Transform.Standing.Numeric, TransformNumericLimit, Command.NumericAppend);
        if (TextInput.BackspacePressed)
            RetractTransformCommand(Transform.Standing);
        if (TextInput.DeletePressed)
            ClearTransformNumeric(Transform.Standing);

        if (TextInput.CancelPressed)
        {
            CancelWorldDraftTransformSession(Declared, Transform);
            RefreshWorldDraftPick(Declared, SemanticSelection);
            PointerTaken = true;
        }
        else
        {
            UpdateWorldDraftTransformSession(Camera, Extent,
                                             Pointer.PositionX, Pointer.PositionY,
                                             Declared, Transform);
            RefreshWorldDraftPick(Declared, SemanticSelection);
            PointerTaken = true;

            if (Transform.AwaitingRelease)
            {
                if (Pointer.ContactReleased)
                    CommitWorldDraftTransformSession(Transform);
            }
            else if (TextInput.AcceptPressed || Pointer.ContactPressed)
            {
                CommitWorldDraftTransformSession(Transform);
            }
        }
    }
}

} // namespace Slate
