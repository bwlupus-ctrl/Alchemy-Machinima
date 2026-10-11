# VC1 follow-ups for the VC4 round

USER DECISION 2026-10-11: **VCam does NOT need undo.** Remove the VCam undo/history integration instead of fixing it:
- Remove the typed ALKitHistory entries for VCam Add/Remove/Bind/Reassign (alkithistory.*, alvcampagemodel.*) and their callers. VCam operations push NO history steps.
- Remove restoreCaptureIdentity / identity-snapshot code that exists only for undo (llprismlens.*), unless something non-undo uses it (the OTS rollback: keep a minimal, transient-tolerant rollback if the OTS add path needs it to stay atomic).
- This makes the recheck's Gate-undo BLOCKER (stray cut after undo, ~:5289-5322), the mGateWasActive MAJOR, the cut-serial-prediction minor, the Preview/Warm restore minor and the replay-pinning minor all MOOT. Verify that no remaining code path reaches them.
- Spec impact: VC1 "typed undo" acceptance and VC5 "successful drops undo/redo as one action" are DROPPED.

Still fix:
- minor: llfloaterdirector.cpp ~2556-2562 reports every offline face as "skipped for permission". Distinguish OFFLINE (not rezzed / still loading) from permission.
- minor: INVALID-geometry quarantines rescan geometry every composite. Cache them like the permission ones (invalidate on object update).
