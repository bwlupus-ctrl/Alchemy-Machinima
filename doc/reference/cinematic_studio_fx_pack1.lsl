// =============================================================================
// CINEMATIC STUDIO: MOTION FX COPROCESSOR (PACK 1 - CORE / ORGANIC)
// *** DIRECT RENDER ARCHITECTURE ***  v1.1 - STAGED POSES
//
// v1.1: resetState() (which cures leftover-light ghosting between effects)
// exposed that several effects never set their own light positions/profiles
// and were silently relying on poses inherited from the previous FX or
// setup. Those effects collapsed to yaw 0 / pitch 0 / profile 0. Every
// effect now declares a complete starting pose in startFX:
//   - Club Strobe: quad yaw spread 45/-45/135/-135
//   - Explosion:   quad spread, low/high pitch mix
//   - Supernova:   quad spread, high pitch (light from above)
//   - Villain Reveal: key dead front (rises from below), rim behind,
//     cool profiles (6500K key / Congo Blue rim)
//   - Swinging Lamp: 3200K tungsten profile
//   - Parachute Flare: Full CTO (orange flare) on both lights
//   - Car Pass: tail-light phase now Rosco Red (was Magenta) -- revert
//     pr0/pr1 to 10 in the cycle<35 branch if you prefer the old look.
//
// This pack no longer streams frames to the engine. It renders the light
// prims DIRECTLY via llSetLinkPrimitiveParamsFast, restoring the property
// of the original (pre-coprocessor) engine where each FX tick computed and
// rendered atomically in a single script event:
//   - Zero inter-script frame messages -> no event queue, nothing stale,
//     nothing to flow-control. The 8020/8021/8019 channels are unused.
//   - Only the 4 PROJECTOR prims move per frame (same moving-prim budget
//     as the old flawless 4-light engine). OMNI prims get their light
//     params every frame but reposition only ~1x per second.
//   - Wall-clock motion math (llGetTime) so timer jitter never lurches.
//   - Delta suppression: prims whose pose/params did not change get no
//     update rules at all (dead phases in Shooting Star, Car Pass etc.
//     cost nothing).
//
// State sync: mirrors the engine's 8001 broadcast
//   setup|radius|masterEV|isOn|height|mirrored|yaw|pitch|visible|gamma
// and requests one at startup via "SYS|SYNC".
//
// The engine remains the sole renderer for static setups; on STOP FX or a
// setup load the engine re-renders and this pack goes idle.
// =============================================================================

list MY_FX = [
    "Police Sirens", "Club Strobe", "Fire Flicker", "Streetlight", "Neon Pulse", "Paparazzi",
    "TV Screen", "Underwater", "UFO Abduction", "Haunted Flicker", "RGB Gamer", "Disco Ball",
    "Warning Alert", "Matrix Drop", "Thunderstorm", "Searchlight", "Heartbeat", "Movie Projector",
    "Warp Tunnel", "Fairy Woods", "Short Circuit", "Red Alert Pulse", "Aurora Borealis",
    "Cyber Scanner", "Shooting Star", "Elevator Fault", "Car Pass", "Explosion", "Supernova",
    "Stage Debut", "Swinging Lamp", "Parachute Flare", "Villain Reveal"
];

float TICK = 0.06;           // ~16fps for position-animated effects

string PROJ_TEX = "5748decc-f629-461c-9a36-a35a221fe21f";

// Profile colors only (indices match the engine's PROFILES list)
list PCOLORS = [<1.00,0.55,0.15>, <1.00,0.92,0.72>, <1.00,1.00,0.95>, <0.85,0.92,1.00>,
                <0.70,0.85,1.00>, <0.45,0.55,1.00>, <0.3,0.4,1.0>,   <1.00,0.65,0.05>,
                <0.30,0.60,1.00>, <0.60,1.00,0.60>, <1.00,0.20,0.80>, <1.00,0.02,0.02>,
                <0.05,0.05,1.00>, <1.0,0.4,0.0>,    <0.0,1.0,0.1>,    <1.00,0.05,0.80>,
                <0.05,0.85,1.00>, <1.00,0.40,0.05>, <0.8,0.0,1.0>,    <0.6,0.0,0.0>,
                <0.0,0.0,0.2>,    <0.2,1.0,0.0>,    <0.5,0.0,1.0>,    <1.00,1.00,1.00>];
// fov, falloff pairs: Standard, Softbox, Snoot
list BEAMS = [1.5, 1.0, 2.8, 1.5, 0.2, 0.5];

// --- LINKS (found by this script directly) ---
integer gLnK = -1; integer gLnF = -1; integer gLnR = -1; integer gLnB = -1;
integer gLnOK = -1; integer gLnOF = -1; integer gLnOR = -1; integer gLnOB = -1;

// --- ENGINE STATE MIRROR (from 8001 sync) ---
float gRadius = 1.5; float gMasterEV = 0.0; integer gIsOn = 1; float gHeight = 0.6;
integer gMirror = 0; float gGYaw = 0.0; float gGPitch = 0.0; integer gVisible = 1; integer gGamma = 1;
float gDistEV = 0.0;

// --- RENDER CACHES (delta suppression) ---
list gOY = [-999.0,-999.0,-999.0,-999.0];   // last rendered yaw (post-transform)
list gOP = [-999.0,-999.0,-999.0,-999.0];   // last rendered pitch (post-transform)
list gOPr = [-1,-1,-1,-1];                  // last profile
list gOE = [-999.0,-999.0,-999.0,-999.0];   // last intensity
list gOM = [-1,-1,-1,-1];                   // last modifier
list gOO = [-1,-1,-1,-1];                   // last on flag
integer gDirty = TRUE;                      // force full refresh
integer gFrameN = 0;                        // frame counter (omni cadence)
list gBatch;                                // rule accumulator for current frame

// --- FX STATE ---
string gActiveFX = "NONE";
integer gFXStep = 0;         // per-tick counter (discrete/beat effects)
float gInterval = 0.1;       // original design cadence of the active effect
float gT0 = 0.0;             // wall-clock start of the active effect
integer gLastIS = -1;        // last whole virtual step (smooth effects)
integer gDropIdx = -1;       // Matrix Drop cycle tracker

float y0; float p0; integer pr0; float ev0; integer m0; integer on0;
float y1; float p1; integer pr1; float ev1; integer m1; integer on1;
float y2; float p2; integer pr2; float ev2; integer m2; integer on2;
float y3; float p3; integer pr3; float ev3; integer m3; integer on3;

registerFX() {
    llMessageLinked(LINK_SET, 8002, "FX_REG|" + llDumpList2String(MY_FX, "|"), NULL_KEY);
}

findLinks() {
    gLnK = -1; gLnF = -1; gLnR = -1; gLnB = -1; gLnOK = -1; gLnOF = -1; gLnOR = -1; gLnOB = -1;
    integer i; for (i = 1; i <= llGetNumberOfPrims(); i++) {
        string n = llGetLinkName(i);
        if (n == "RED") gLnK = i; else if (n == "GREEN") gLnF = i;
        else if (n == "BLUE") gLnR = i; else if (n == "YELLOW") gLnB = i;
        else if (n == "OMNI_RED") gLnOK = i; else if (n == "OMNI_GREEN") gLnOF = i;
        else if (n == "OMNI_BLUE") gLnOR = i; else if (n == "OMNI_YELLOW") gLnOB = i;
    }
}

resetState() {
    y0=0.0; p0=0.0; pr0=0; ev0=0.0; m0=0; on0=0;
    y1=0.0; p1=0.0; pr1=0; ev1=0.0; m1=0; on1=0;
    y2=0.0; p2=0.0; pr2=0; ev2=0.0; m2=0; on2=0;
    y3=0.0; p3=0.0; pr3=0; ev3=0.0; m3=0; on3=0;
    gFXStep = 0; gLastIS = -1; gDropIdx = -1; gDirty = TRUE; gFrameN = 0;
}

float wrap180(float a) {
    while (a > 180.0) a -= 360.0;
    while (a <= -180.0) a += 360.0;
    return a;
}

float fmodf(float a, float m) {
    return a - (m * (float)llFloor(a / m));
}

// ---------------------------------------------------------------------------
// DIRECT RENDER CORE
// Appends update rules for one light pair into gBatch. Position rules only
// when the transformed pose changed; light-param rules only when intensity/
// color/modifier/on changed. Omni positions move only on cadence frames.
// ---------------------------------------------------------------------------
doLight(integer i, integer lnP, integer lnO, float yaw, float pitch, integer pr, float ev, integer m, integer on) {
    if (lnP < 1 && lnO < 1) return;

    float cY = yaw; if (gMirror) cY = -cY; cY += gGYaw;
    float cP = pitch + gGPitch;
    float inten = llPow(2.0, ev + gMasterEV + gDistEV);
    integer isOn = on && (inten > 0.001);

    integer posChg = gDirty;
    if (llList2Float(gOY, i) != cY || llList2Float(gOP, i) != cP) posChg = TRUE;
    integer visChg = gDirty;
    if (llList2Integer(gOPr, i) != pr || llList2Float(gOE, i) != inten ||
        llList2Integer(gOM, i) != m || llList2Integer(gOO, i) != isOn) visChg = TRUE;
    if (!posChg && !visChg) return;

    if (posChg) { gOY = llListReplaceList(gOY, [cY], i, i); gOP = llListReplaceList(gOP, [cP], i, i); }
    if (visChg) {
        gOPr = llListReplaceList(gOPr, [pr], i, i); gOE = llListReplaceList(gOE, [inten], i, i);
        gOM = llListReplaceList(gOM, [m], i, i); gOO = llListReplaceList(gOO, [isOn], i, i);
    }

    vector col = <1,1,1>;
    if (pr >= 0 && pr < 24) col = llList2Vector(PCOLORS, pr);
    if (gGamma) col = <llPow(col.x, 2.2), llPow(col.y, 2.2), llPow(col.z, 2.2)>;
    float gMult = 0.05; if (!gVisible) gMult = 0.0;
    integer fb = isOn && gVisible;
    float rYaw = cY * DEG_TO_RAD; float rPit = cP * DEG_TO_RAD;

    if (lnP > 0) {
        gBatch += [PRIM_LINK_TARGET, lnP];
        if (visChg) gBatch += [PRIM_POINT_LIGHT, isOn, col, inten, gRadius*2.2, llList2Float(BEAMS, m*2+1),
                              PRIM_PROJECTOR, PROJ_TEX, llList2Float(BEAMS, m*2), 0.0, 0.0,
                              PRIM_GLOW, 5, gMult*inten, PRIM_FULLBRIGHT, 5, fb];
        if (posChg) {
            float cosP = llCos(rPit);
            vector orb = <gRadius*cosP*llCos(rYaw), gRadius*cosP*llSin(rYaw), gRadius*llSin(rPit)>;
            gBatch += [PRIM_POS_LOCAL, orb + <0,0,gHeight>, PRIM_ROT_LOCAL, llRotBetween(<0,0,-1>, -llVecNorm(orb))];
        }
    }
    if (lnO > 0) {
        float oInt = inten * 0.45;
        integer oOn = isOn && (oInt > 0.001);
        gBatch += [PRIM_LINK_TARGET, lnO];
        if (visChg) gBatch += [PRIM_POINT_LIGHT, oOn, col, oInt, gRadius*1.5, 0.75];
        // Omnis are soft bounce fill: reposition only ~1x/sec (or on full
        // refresh) to keep the moving-prim budget at the 4 projectors.
        if (posChg && (gDirty || (gFrameN & 15) == 0)) {
            float oPit = cP - 45.0; if (oPit < -85.0) oPit = -85.0;
            float ocos = llCos(oPit*DEG_TO_RAD);
            vector po = <gRadius*ocos*llCos(rYaw), gRadius*ocos*llSin(rYaw), gRadius*llSin(oPit*DEG_TO_RAD)>;
            gBatch += [PRIM_POS_LOCAL, po + <0,0,gHeight>, PRIM_ROT_LOCAL, ZERO_ROTATION];
        }
    }
}

renderFrame() {
    if (!gIsOn) return;
    gBatch = [];
    doLight(0, gLnK, gLnOK, y0, p0, pr0, ev0, m0, on0);
    doLight(1, gLnF, gLnOF, y1, p1, pr1, ev1, m1, on1);
    doLight(2, gLnR, gLnOR, y2, p2, pr2, ev2, m2, on2);
    doLight(3, gLnB, gLnOB, y3, p3, pr3, ev3, m3, on3);
    gDirty = FALSE;
    gFrameN++;
    if (llGetListLength(gBatch) > 0) llSetLinkPrimitiveParamsFast(LINK_SET, gBatch);
    gBatch = [];
}

// Smooth effects: gInterval is the original design cadence (the math is
// scaled to it), but the actual timer runs at TICK and positions are
// computed from wall-clock elapsed time.
startFX(string fx) {
    resetState();
    gActiveFX = fx;
    gT0 = llGetTime();

    if (fx == "Police Sirens") { y0=45; p0=10; pr0=11; on0=1; m0=0; y1=-45; p1=10; pr1=12; on1=1; m1=0; gInterval=0.15; llSetTimerEvent(0.15); }
    else if (fx == "Fire Flicker") { y0=20; p0=-20; pr0=13; on0=1; m0=1; y1=-20; p1=-15; pr1=17; on1=1; m1=1; gInterval=0.2; llSetTimerEvent(0.2); }
    else if (fx == "Club Strobe") { y0=45; p0=30; y1=-45; p1=30; y2=135; p2=30; y3=-135; p3=30; on0=1; on1=1; on2=1; on3=1; gInterval=0.1; llSetTimerEvent(0.1); }
    else if (fx == "Neon Pulse") { y0=45; p0=15; pr0=15; on0=1; m0=1; y1=-45; p1=15; pr1=16; on1=1; m1=1; gInterval=0.1; llSetTimerEvent(0.1); }
    else if (fx == "Streetlight") { on0=1; pr0=3; m0=1; gInterval=0.2; llSetTimerEvent(TICK); }
    else if (fx == "Paparazzi") { pr0=5; m0=2; pr1=5; m1=2; pr2=5; m2=2; pr3=5; m3=2; gInterval=0.1; llSetTimerEvent(0.1); }
    else if (fx == "TV Screen") { y0=0; p0=0; on0=1; m0=1; gInterval=0.2; llSetTimerEvent(0.2); }
    else if (fx == "Underwater") { y0=45; p0=45; pr0=16; on0=1; m0=1; y1=-45; p1=-20; pr1=12; on1=1; m1=1; gInterval=0.2; llSetTimerEvent(TICK); }
    else if (fx == "UFO Abduction") { y0=0; p0=85; pr0=14; on0=1; m0=2; ev0=1.0; gInterval=0.1; llSetTimerEvent(TICK); }
    else if (fx == "Haunted Flicker") { y0=20; p0=15; pr0=0; on0=1; m0=0; ev0=-1.5; gInterval=0.1; llSetTimerEvent(0.1); }
    else if (fx == "RGB Gamer") {
        on0=1; m0=1; p0=15; ev0=0.5; y0=45; on1=1; m1=1; p1=15; ev1=0.5; y1=-45;
        on2=1; m2=1; p2=15; ev2=0.5; y2=135; on3=1; m3=1; p3=15; ev3=0.5; y3=-135; gInterval=0.2; llSetTimerEvent(TICK);
    }
    else if (fx == "Disco Ball") { on0=1; m0=2; ev0=1; y0=0; on1=1; m1=2; ev1=1; y1=90; on2=1; m2=2; ev2=1; y2=180; on3=1; m3=2; ev3=1; y3=-90; gInterval=0.1; llSetTimerEvent(TICK); }
    else if (fx == "Warning Alert") { y0=0; p0=15; pr0=11; on0=1; m0=1; ev0=1.0; gInterval=0.1; llSetTimerEvent(TICK); }
    else if (fx == "Matrix Drop") { y0=0; p0=85; pr0=9; on0=1; m0=2; ev0=0.5; gInterval=0.15; llSetTimerEvent(TICK); }
    else if (fx == "Thunderstorm") { gInterval=0.05; llSetTimerEvent(0.05); }
    else if (fx == "Searchlight") { on0=1; pr0=4; m0=2; p0=15; ev0=1; gInterval=0.1; llSetTimerEvent(TICK); }
    else if (fx == "Heartbeat") { on0=1; pr0=11; m0=1; p0=-10; y0=0; gInterval=0.1; llSetTimerEvent(0.1); }
    else if (fx == "Movie Projector") { on0=1; m0=1; y0=0; p0=5; gInterval=0.1; llSetTimerEvent(0.1); }
    else if (fx == "Warp Tunnel") {
        on0=1; m0=0; ev0=1.5; y0=45; pr0=16; on1=1; m1=0; ev1=1.5; y1=-45; pr1=15;
        on2=1; m2=0; ev2=1.5; y2=135; pr2=12; on3=1; m3=0; ev3=1.5; y3=-135; pr3=16; gInterval=0.05; llSetTimerEvent(TICK);
    }
    else if (fx == "Fairy Woods") { on0=1; m0=1; y0=45; pr0=14; on1=1; m1=1; y1=-60; pr1=10; on2=1; m2=1; y2=180; pr2=16; gInterval=0.2; llSetTimerEvent(0.2); }
    else if (fx == "Short Circuit") { on0=1; pr0=2; m0=0; y0=0; p0=45; gInterval=0.05; llSetTimerEvent(0.05); }
    else if (fx == "Red Alert Pulse") { on0=1; pr0=11; m0=0; y0=45; p0=15; on1=1; pr1=11; m1=0; y1=-45; p1=15; gInterval=0.1; llSetTimerEvent(0.1); }
    else if (fx == "Aurora Borealis") { on0=1; m0=0; y0=45; pr0=9; on1=1; m1=0; y1=-45; pr1=16; on2=1; m2=0; y2=180; pr2=12; gInterval=0.2; llSetTimerEvent(TICK); }
    else if (fx == "Cyber Scanner") { on0=1; pr0=16; m0=2; y0=0; ev0=1.0; gInterval=0.1; llSetTimerEvent(TICK); }
    else if (fx == "Shooting Star") { y0=-115.0; p0=60.0; pr0=3; m0=0; on0=0; ev0=-10.0; gInterval=0.05; llSetTimerEvent(TICK); }
    else if (fx == "Elevator Fault") {
        y0=0.0; p0=78.0; pr0=2; m0=3; on0=1; ev0=0.2; y1=90.0; p1=78.0; pr1=2; m1=3; on1=1; ev1=0.2;
        y2=180.0; p2=78.0; pr2=2; m2=3; on2=1; ev2=0.2; y3=-90.0; p3=78.0; pr3=2; m3=3; on3=1; ev3=0.2; gInterval=0.1; llSetTimerEvent(0.1);
    }
    else if (fx == "Car Pass") { y0=100.0; p0=12.0; pr0=3; m0=0; on0=0; ev0=-10.0; y1=108.0; p1=12.0; pr1=2; m1=0; on1=0; ev1=-10.0; gInterval=0.07; llSetTimerEvent(TICK); }
    else if (fx == "Explosion") { y0=45.0; p0=25.0; y1=-45.0; p1=25.0; y2=135.0; p2=40.0; y3=-135.0; p3=40.0; on0=1; on1=1; on2=1; on3=1; pr0=3; pr1=3; pr2=3; pr3=3; gInterval=0.08; llSetTimerEvent(0.08); }
    else if (fx == "Supernova") { y0=45.0; p0=50.0; y1=-45.0; p1=50.0; y2=135.0; p2=50.0; y3=-135.0; p3=50.0; on0=1; on1=1; on2=1; on3=1; pr0=0; pr1=0; pr2=0; pr3=0; gInterval=0.08; llSetTimerEvent(0.08); }
    else if (fx == "Stage Debut") { gInterval=0.15; llSetTimerEvent(0.15); }
    else if (fx == "Swinging Lamp") { on0=1; y0=0.0; p0=55.0; pr0=1; m0=0; ev0=0.5; gInterval=0.07; llSetTimerEvent(TICK); }
    else if (fx == "Parachute Flare") { on0=1; on1=1; p0=85.0; y0=0.0; pr0=7; m0=0; p1=82.0; y1=0.0; pr1=7; m1=0; ev0=1.0; gInterval=0.12; llSetTimerEvent(TICK); }
    else if (fx == "Villain Reveal") { y0=0.0; p0=-60.0; pr0=4; m0=0; on0=0; y1=180.0; p1=25.0; pr1=12; m1=0; on1=0; ev0=-10.0; ev1=-10.0; gInterval=0.12; llSetTimerEvent(TICK); }
}

default {
    state_entry() {
        llOwnerSay("Motion Coprocessor: Pack 1 (Core/Organic, Direct Render v1.1) Online.");
        findLinks();
        registerFX();
        llMessageLinked(LINK_SET, 8000, "SYS|SYNC", NULL_KEY);
    }

    changed(integer c) { if (c & CHANGED_LINK) findLinks(); }

    link_message(integer sender_num, integer num, string str, key id) {
        if (num == 8001) {
            // Mirror engine state. Any change can affect positions/intensity,
            // so force a full refresh on the next frame.
            list q = llParseString2List(str, ["|"], []);
            gRadius = (float)llList2String(q, 1); gMasterEV = (float)llList2String(q, 2);
            gIsOn = (integer)llList2String(q, 3); gHeight = (float)llList2String(q, 4);
            gMirror = (integer)llList2String(q, 5); gGYaw = (float)llList2String(q, 6);
            gGPitch = (float)llList2String(q, 7);
            if (llGetListLength(q) > 8) gVisible = (integer)llList2String(q, 8);
            if (llGetListLength(q) > 9) gGamma = (integer)llList2String(q, 9);
            if (gRadius < 0.1) gRadius = 0.1;
            gDistEV = llLog(gRadius / 1.5) / 0.6931472;
            gDirty = TRUE;
            return;
        }
        if (num == 8003 && str == "UI_READY") registerFX();
        else if (num == 8000) {
            list p = llParseString2List(str, ["|"], []);
            if (llList2String(p, 0) == "FX") {
                string fx = llList2String(p, 1);
                if (fx == "NONE" || fx == "STOP FX") {
                    gActiveFX = "NONE"; llSetTimerEvent(0.0);
                }
                else if (~llListFindList(MY_FX, [fx])) { startFX(fx); }
                else { gActiveFX = "NONE"; llSetTimerEvent(0.0); }
            }
        }
    }

    timer() {
        // Discrete/beat effects advance one step per tick (original cadence).
        gFXStep++;
        // Smooth effects use wall-clock virtual time: fs is the fractional
        // "step number" the original math was designed around, so all the
        // original speed constants carry over unchanged.
        float elapsed = llGetTime() - gT0;
        float fs = elapsed / gInterval;
        integer is = (integer)fs;
        float t = fs;

        // ---------- DISCRETE / BEAT EFFECTS (original cadence) ----------
        if (gActiveFX == "Police Sirens") { if (gFXStep % 2 == 0) { ev0=1.0; ev1=-10.0; } else { ev0=-10.0; ev1=1.0; } }
        else if (gActiveFX == "Fire Flicker") { ev0 = -0.5 + (llFrand(1.5) - 0.75); ev1 = -1.0 + (llFrand(1.0) - 0.5);   }
        else if (gActiveFX == "Club Strobe") {
            p0=30+llFrand(30); if(llFrand(1)>0.6){pr0=7+(integer)llFrand(16); ev0=0.5+llFrand(1); on0=1;} else ev0=-10;
            p1=30+llFrand(30); if(llFrand(1)>0.6){pr1=7+(integer)llFrand(16); ev1=0.5+llFrand(1); on1=1;} else ev1=-10;
            p2=30+llFrand(30); if(llFrand(1)>0.6){pr2=7+(integer)llFrand(16); ev2=0.5+llFrand(1); on2=1;} else ev2=-10;
            p3=30+llFrand(30); if(llFrand(1)>0.6){pr3=7+(integer)llFrand(16); ev3=0.5+llFrand(1); on3=1;} else ev3=-10;
        }
        else if (gActiveFX == "Neon Pulse") {
            // ORGANIC MATH: Light 2 pulses slightly slower to drift out of phase
            ev0 = llSin(t * 0.4) * 1.5;
            ev1 = llSin((t + 5.0) * 0.35) * 1.5;
        }
        else if (gActiveFX == "Paparazzi") {
            if(llFrand(1)>0.8){y0=llFrand(360)-180.0; p0=llFrand(60); ev0=2.0; on0=1;}else on0=0;
            if(llFrand(1)>0.8){y1=llFrand(360)-180.0; p1=llFrand(60); ev1=2.0; on1=1;}else on1=0;
            if(llFrand(1)>0.8){y2=llFrand(360)-180.0; p2=llFrand(60); ev2=2.0; on2=1;}else on2=0;
            if(llFrand(1)>0.8){y3=llFrand(360)-180.0; p3=llFrand(60); ev3=2.0; on3=1;}else on3=0;
        }
        else if (gActiveFX == "TV Screen") { if (llFrand(1.0) > 0.5) pr0=4; else pr0=6; ev0 = -1.0 + llFrand(2.0); }
        else if (gActiveFX == "Haunted Flicker") { if (llFrand(1.0) > 0.9) ev0=-10.0; else ev0=-1.5+llFrand(0.3); }
        else if (gActiveFX == "Thunderstorm") { if(llFrand(1.0)>0.9){on0=1; ev0=2.0+llFrand(1.0); y0=llFrand(360.0)-180.0; p0=10.0+llFrand(60.0); pr0=4;}else ev0=-10.0; }
        else if (gActiveFX == "Heartbeat") { integer beat = gFXStep % 15; if (beat==0 || beat==3) ev0=1.5; else ev0=-3.0; }
        else if (gActiveFX == "Movie Projector") { ev0=-1.0+llFrand(2.5); if(llFrand(1.0)>0.5) pr0=3; else pr0=2; }
        else if (gActiveFX == "Fairy Woods") {
            // ORGANIC MATH: Broken synchronicity across the lights
            ev0 = llSin(t * 0.1);
            ev1 = llCos((t + 3.0) * 0.13);
            ev2 = llSin((t + 7.0) * 0.07);
        }
        else if (gActiveFX == "Short Circuit") { if(llFrand(1.0)>0.8) ev0=1.5+llFrand(1.0); else if(llFrand(1.0)>0.6) ev0=-1.0; else ev0=-10.0; }
        else if (gActiveFX == "Red Alert Pulse") { float throb = llSin(t*0.2); ev0=throb*2.0; ev1=throb*2.0; }
        else if (gActiveFX == "Elevator Fault") {
            ev0 = 0.2 + llFrand(0.05); ev1 = 0.2 + llFrand(0.05); ev2 = 0.2 + llFrand(0.05); ev3 = 0.2 + llFrand(0.05);
            if (gFXStep % 15 == 0) { ev0 = -10.0; ev1 = -10.0; ev2 = -10.0; ev3 = -10.0; }
            if (llFrand(1.0) > 0.96) {
                integer bad = (integer)llFrand(4.0);
                if (bad == 0) ev0 = -10.0; else if (bad == 1) ev1 = -10.0; else if (bad == 2) ev2 = -10.0; else ev3 = -10.0;
            }
        }
        else if (gActiveFX == "Explosion") {
            integer beat = gFXStep % 65;
            if (beat == 0) { on0=1; on1=1; on2=1; on3=1; pr0=3; pr1=3; pr2=3; pr3=3; }
            if (beat < 3) { ev0=2.0; ev1=2.0; ev2=2.0; ev3=2.0; }
            else if (beat < 12) {
                float timeNorm = (float)(beat - 3) / 9.0; float e = 2.0 - timeNorm * 3.5;
                ev0=e; ev1=e*0.8; ev2=e*0.6; ev3=e*0.4; pr0=13; pr1=13; pr2=0; pr3=0;
            } else if (beat < 45) {
                float timeNorm = (float)(beat - 12) / 33.0; float base = -0.5 - timeNorm * 2.5;
                ev0 = base + llFrand(0.5); ev1 = base - 0.4 + llFrand(0.3); on2=0; on3=0; pr0=0; pr1=0;
            } else { on0=1; ev0=-10.0; on1=0; on2=0; on3=0; pr0=0; if (llFrand(1.0) > 0.95) ev0 = -3.0 + llFrand(1.5); }
        }
        else if (gActiveFX == "Supernova") {
            integer beat = gFXStep % 90;
            if (beat == 0) { on0=1; on1=1; on2=1; on3=1; pr0=0; pr1=0; pr2=0; pr3=0; ev0=-2.0; ev1=-2.0; ev2=-2.0; ev3=-2.0; }
            if (beat < 50) {
                float timeNorm = (float)beat / 50.0; float e = -2.0 + timeNorm * 4.0; ev0=e; ev1=e; ev2=e; ev3=e;
                integer warmShift = (integer)(timeNorm * 4.0); pr0=warmShift; pr1=warmShift; pr2=warmShift; pr3=warmShift;
            } else if (beat < 53) { ev0=2.0; ev1=2.0; ev2=2.0; ev3=2.0; pr0=3; pr1=3; pr2=3; pr3=3; }
            else if (beat < 65) {
                float timeNorm = (float)(beat - 53) / 12.0; float e = 2.0 - timeNorm * 13.0; if (e < -10.0) e = -10.0; ev0=e; ev1=e; ev2=e; ev3=e;
            } else if (beat < 75) { on2=0; on3=0; ev0=-3.0; pr0=5; ev1=-3.5; pr1=5; }
            else { ev0=-10.0; ev1=-10.0; }
        }
        else if (gActiveFX == "Stage Debut") {
            integer beat = gFXStep % 80;
            if (beat == 0) { on0=0; on1=0; on2=0; on3=0; }
            else if (beat == 5) { on3=1; y3=0.0; p3=-15.0; pr3=1; m3=0; ev3=-0.5; }
            else if (beat == 12) { on0=1; y0=45.0; p0=45.0; pr0=3; m0=2; ev0=0.4; }
            else if (beat >= 16 && beat <= 20) { on1=1; y1=-40.0; p1=25.0; pr1=2; m1=3; ev1 = -2.0 + ((float)(beat - 16) / 5.0) * 1.5; }
            else if (beat == 22) { on2=1; y2=150.0; p2=40.0; pr2=1; m2=2; ev2=-0.5; }
            else if (beat > 25 && beat < 70) { ev0 = 0.4 + llSin((float)beat * 0.04) * 0.1; }
            else if (beat >= 70) { on0=0; on1=0; on2=0; on3=0; }
        }

        // ---------- SMOOTH EFFECTS (wall-clock continuous) ----------
        else if (gActiveFX == "Streetlight") {
            float cycle = fmodf(fs, 40.0);
            if (cycle < 20.0) { y0 = 0.0; p0 = 30.0 + ((cycle / 20.0) * 55.0); } else { y0 = 180.0; p0 = 85.0 - (((cycle - 20.0) / 20.0) * 55.0); }
            ev0 = ((p0 - 30.0) / 55.0 * 2.0) - 1.0;
        }
        else if (gActiveFX == "Underwater") {
            // ORGANIC MATH: The two caustics undulate on completely different frequency bands
            ev0 = llSin(t * 0.2);
            p0 = 45.0 + (llCos(t * 0.15) * 10.0);
            ev1 = llSin((t * 0.22) + 2.0) - 0.5;
            p1 = -20.0 + (llCos((t * 0.17) + 1.0) * 5.0);
        }
        else if (gActiveFX == "UFO Abduction") { y0 = wrap180(15.0 * fs); ev0 = 1.0 + (llSin(fs * 0.5) * 0.5); }
        else if (gActiveFX == "RGB Gamer") {
            y0 = wrap180(45.0 + 5.0*fs);   pr0 = 7 + (is % 16);
            y1 = wrap180(-45.0 + 5.0*fs);  pr1 = 7 + ((is + 4) % 16);
            y2 = wrap180(135.0 + 5.0*fs);  pr2 = 7 + ((is + 8) % 16);
            y3 = wrap180(-135.0 + 5.0*fs); pr3 = 7 + ((is + 12) % 16);
        }
        else if (gActiveFX == "Disco Ball") {
            y0 = wrap180(0.0   + 20.0*fs); p0 = 30.0 + (llSin(t*0.3)*30.0);
            y1 = wrap180(90.0  + 20.0*fs); p1 = 30.0 + (llSin((t+1.0)*0.3)*30.0);
            y2 = wrap180(180.0 + 20.0*fs); p2 = 30.0 + (llSin((t+2.0)*0.3)*30.0);
            y3 = wrap180(-90.0 + 20.0*fs); p3 = 30.0 + (llSin((t+3.0)*0.3)*30.0);
            if (is != gLastIS && (is % 5) == 0) {
                pr0 = 7+(integer)llFrand(16); pr1 = 7+(integer)llFrand(16);
                pr2 = 7+(integer)llFrand(16); pr3 = 7+(integer)llFrand(16);
            }
        }
        else if (gActiveFX == "Warning Alert") { y0 = wrap180(25.0 * fs); }
        else if (gActiveFX == "Matrix Drop") {
            float dropLen = 170.0 / 15.0;            // steps per full drop
            float ph = fmodf(fs, dropLen);
            p0 = 85.0 - (15.0 * ph);
            integer dIdx = (integer)(fs / dropLen);
            if (dIdx != gDropIdx) { gDropIdx = dIdx; y0 = llFrand(180.0) - 90.0; }
        }
        else if (gActiveFX == "Searchlight") { y0 = llSin(t*0.1) * 90.0; }
        else if (gActiveFX == "Warp Tunnel") {
            p0 = 85.0 - (fmodf(fs,        20.0) / 20.0 * 170.0);
            p1 = 85.0 - (fmodf(fs +  5.0, 20.0) / 20.0 * 170.0);
            p2 = 85.0 - (fmodf(fs + 10.0, 20.0) / 20.0 * 170.0);
            p3 = 85.0 - (fmodf(fs + 15.0, 20.0) / 20.0 * 170.0);
        }
        else if (gActiveFX == "Aurora Borealis") {
            // ORGANIC MATH: Swelling completely out of phase
            p0 = 45.0 + llSin(t * 0.05) * 20.0;
            p1 = 45.0 + llCos((t + 1.0) * 0.06) * 20.0;
            p2 = 45.0 + llSin((t + 2.0) * 0.04) * 20.0;
        }
        else if (gActiveFX == "Cyber Scanner") { p0 = llSin(t*0.15) * 60.0; }
        else if (gActiveFX == "Shooting Star") {
            float phase = fmodf(fs, 40.0);
            if (phase < 10.0) { y0 = -115.0 + phase * 23.0; p0 = 55.0 + llSin(phase * 0.3142) * 25.0; ev0 = 1.5 - phase * 0.05; on0 = 1; }
            else { on0 = 0; }
        }
        else if (gActiveFX == "Car Pass") {
            float cycle = fmodf(fs, 55.0);
            if (cycle < 18.0) {
                float timeNorm = cycle / 18.0; y0 = 100.0 - timeNorm * 210.0; y1 = y0 + 8.0;
                on0=1; ev0=0.9; pr0=3; on1=1; ev1=0.7; pr1=2; on2=0; on3=0;
            } else if (cycle < 21.0) { on0=0; on1=0; on2=0; on3=0; }
            else if (cycle < 35.0) {
                float timeNorm = (cycle - 21.0) / 14.0; y0 = -110.0 - timeNorm * 20.0; y1 = y0 + 8.0;
                on0=1; ev0=0.4 - timeNorm*0.6; pr0=11; on1=1; ev1=0.3 - timeNorm*0.5; pr1=11; on2=0; on3=0;
            } else { on0=0; on1=0; on2=0; on3=0; }
        }
        else if (gActiveFX == "Swinging Lamp") {
            float beat = fmodf(fs, 230.0);
            if (beat < 200.0) {
                on0 = 1;
                float amp = 60.0 * llPow(0.978, beat);
                y0 = llSin(beat * 0.18) * amp; p0 = 75.0 - (amp / 60.0) * 30.0;
                float absY = y0; if (absY < 0.0) absY = -absY; ev0 = 0.5 - (absY / 60.0) * 0.2 + llFrand(0.04);
            } else if (beat < 215.0) { on0 = 1; y0 = 0.0; p0 = 75.0; ev0 = 0.5; }
            else { on0 = 0; }
        }
        else if (gActiveFX == "Parachute Flare") {
            float beat = fmodf(fs, 110.0);
            if (beat < 80.0) {
                on0 = 1; on1 = 1;
                float descent = beat / 80.0; p0 = 85.0 - descent * 80.0; if (p0 < 5.0) p0 = 5.0;
                y0 = llSin(beat * 0.18) * 15.0; ev0 = (1.0 - descent) * 1.2 + llFrand(0.15);
                p1 = p0 + 8.0; if (p1 > 85.0) p1 = 85.0; y1 = y0 * 0.5; ev1 = (1.0 - descent) * 0.3 - 0.5 + llFrand(0.1);
                if (p0 <= 8.0) { ev0 = llFrand(2.0) - 10.0; ev1 = -10.0; }
            } else { on0=0; on1=0; }
        }
        else if (gActiveFX == "Villain Reveal") {
            float beat = fmodf(fs, 100.0);
            if (beat < 20.0) { on0=0; on1=0; ev0=-10.0; ev1=-10.0; }
            else if (beat < 50.0) {
                on0=1; float timeNorm = (beat - 20.0) / 30.0; p0 = -60.0 + (timeNorm * 40.0); ev0 = -2.0 + (timeNorm * 2.5);
            } else {
                on0=1; p0 = -20.0 + llSin(beat * 0.08) * 5.0; ev0 = 0.3 + llSin(beat * 0.1) * 0.2;
                on1=1; float rimFade = (beat - 50.0) / 20.0; if (rimFade > 1.0) rimFade = 1.0; ev1 = -10.0 + (rimFade * 9.5);
            }
        }

        gLastIS = is;
        if (gActiveFX != "NONE" && gActiveFX != "STOP FX") renderFrame();
    }
}
