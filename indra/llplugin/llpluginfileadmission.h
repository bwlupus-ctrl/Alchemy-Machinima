/** Launch and media verdict are separate clocks; notifications never extend them. */
#ifndef LL_LLPLUGINFILEADMISSION_H
#define LL_LLPLUGINFILEADMISSION_H
struct LLPluginFileAdmission
{
    double launchDeadline = 0., verdictDeadline = 0.;
    bool playerOpened = false;
    void queued(double now, double launchAllowance)
    { launchDeadline = now + launchAllowance; verdictDeadline = 0.; playerOpened = false; }
    void opened(double now)
    { playerOpened = true; if (!verdictDeadline) verdictDeadline = now + 10.; }
    void resumed(double now, int previous, int desired, int verdict)
    {
        // A player rebuilt while paused starts decoding only on this resume.
        // Admitted files and duplicate Playing notifications keep their state.
        if (playerOpened && previous != 2 && desired == 2 && verdict == 0) verdictDeadline = now + 10.;
    }
    bool expired(double now) const
    { const double deadline = verdictDeadline ? verdictDeadline : launchDeadline; return deadline > 0. && now >= deadline; }
    int verdict(int current, int desired, double now) const
    { return desired == 2 && current == 0 && expired(now) ? -1 : current; }
};
#endif
