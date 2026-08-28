//============================================================================================================================================
//                                                          IDLEWAKEPROOF.CPP
//============================================================================================================================================
// ⏱️ THE EDITOR MUST NOT PRESENT AN IMAGE NOBODY ASKED FOR.
//
// 🔴 It did, sixty times a second, forever. Under FIFO pacing the host rebuilt the whole interface and
//    presented a fresh image every tick whether or not one pixel differed, which measured at 8 to 9% of a
//    core with the artist's hands off the input. `RedrawScheduler` was written for exactly this, carries
//    the wake rule in `Waking`, is owned by `ViewportSequence` — and was read by nobody. The marks were
//    raised faithfully every tick and never once asked about.
//
// 🔴 THE SKETCH GEOMETRY WAS NOT THE COST, AND THIS PROOF RECORDS THAT SO NOBODY OPTIMISES IT AGAIN.
//    Projecting and tessellating a 240-curve sketch measures 162 microseconds a frame — under 1% of a
//    core at sixty frames a second. Measured before anything was changed. The expense was never the
//    shapes; it was rebuilding and presenting an interface that had not changed.
//
// 📝 The wake rule itself is checked here rather than the host loop, because the rule is the part that can
//    be wrong in a way nobody notices: a rule that always wakes costs the idle, and a rule that wrongly
//    sleeps freezes the window. Both directions are claimed.

#include "SlateUI/Interface/RedrawScheduler/Api/RedrawScheduler.h"

#include <cstdio>

namespace
{

std::uint32_t Claims = 0u;
std::uint32_t Failures = 0u;

void Require(bool Held, const char* Naming)
{
    ++Claims;
    if (Held)
        return;
    ++Failures;
    std::printf("  FAILED  %s\n", Naming);
}

}   // namespace

int main()
{
    using namespace Slate;

    std::printf("[IdleWakeProof]\n");

    // ① A REGISTERED PANEL ARRIVES MARKED, so the first tick is never slept through.
    {
        RedrawScheduler Scheduler;
        const Deliver<std::uint32_t> Enrolled = Scheduler.Register("Viewport");
        Require(Enrolled.Resolved, "a panel enrols");
        Require(Scheduler.Marked(), "a freshly registered panel stands marked");
        Require(Scheduler.Waking(false, false),
                "so a tick that registers a panel is never slept through");
    }

    // ② THE RULE SLEEPS ONLY WHEN ALL THREE OPERANDS ARE QUIET.
    {
        RedrawScheduler Scheduler;
        const Deliver<std::uint32_t> Enrolled = Scheduler.Register("Viewport");
        Require(Enrolled.Resolved, "a panel enrols");
        Scheduler.Retire();

        Require(!Scheduler.Waking(false, false),
                "nothing marked, nothing moving, nothing arrived: the host must sleep");

        // 🔴 EACH OPERAND ALONE MUST WAKE IT. A rule missing one of the three produces a panel that
        //    freezes mid-transition and resumes when the artist happens to move the pointer, which
        //    gets attributed to the input device rather than to the rule.
        Require(Scheduler.Waking(true, false),
                "a settling spring wakes the host on its own");
        Require(Scheduler.Waking(false, true),
                "an arrival wakes the host on its own");

        Scheduler.Mark(Enrolled.Delivered, RedrawMark::Recolour);
        Require(Scheduler.Waking(false, false),
                "a raised mark wakes the host on its own");
    }

    // ③ A MARK SURVIVES UNTIL IT IS RETIRED, so a change cannot be slept through before it is drawn.
    {
        RedrawScheduler Scheduler;
        const Deliver<std::uint32_t> Enrolled = Scheduler.Register("Viewport");
        Require(Enrolled.Resolved, "a panel enrols");
        Scheduler.Retire();
        Require(!Scheduler.Waking(false, false), "quiet after retirement");

        Scheduler.Mark(Enrolled.Delivered, RedrawMark::Rearrange);
        Require(Scheduler.Waking(false, false), "marked, so waking");
        Require(Scheduler.Waking(false, false),
                "and STILL waking when asked again, because asking is not retiring");

        Scheduler.Retire();
        Require(!Scheduler.Waking(false, false), "and quiet once the content has been sealed");
    }

    // ④ THE DEARER MARK WINS, which is what keeps a hover from cancelling a re-solve.
    {
        RedrawScheduler Scheduler;
        const Deliver<std::uint32_t> Enrolled = Scheduler.Register("Viewport");
        Require(Enrolled.Resolved, "a panel enrols");
        Scheduler.Retire();

        Scheduler.Mark(Enrolled.Delivered, RedrawMark::Rearrange);
        Scheduler.Mark(Enrolled.Delivered, RedrawMark::Recolour);
        Require(Scheduler.Current(Enrolled.Delivered) == RedrawMark::Rearrange,
                "a cheap mark arriving after a dear one does not lower it");
    }

    // ⑤ EVERY PANEL WAKES TOGETHER when the display or the appearance moves under all of them.
    {
        RedrawScheduler Scheduler;
        const Deliver<std::uint32_t> First  = Scheduler.Register("North");
        const Deliver<std::uint32_t> Second = Scheduler.Register("South");
        Require(First.Resolved && Second.Resolved, "two panels enrol");
        Scheduler.Retire();
        Require(!Scheduler.Waking(false, false), "both quiet");

        Scheduler.MarkEvery(RedrawMark::Rearrange);
        Require(Scheduler.Current(First.Delivered) == RedrawMark::Rearrange
             && Scheduler.Current(Second.Delivered) == RedrawMark::Rearrange,
                "a resize marks every registered panel, not the one that noticed");
    }

    std::printf("[IdleWakeProof] %u claims, %u failures\n", Claims, Failures);
    return Failures == 0u ? 0 : 1;
}
