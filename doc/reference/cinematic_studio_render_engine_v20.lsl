// =============================================================================
// CINEMATIC STUDIO: RENDER ENGINE v20
// *** SOLE SOURCE OF TRUTH ***
//
// ARCHITECTURE:
//   gBase      = the pristine setup (raw, pre-transform), cached the moment
//                the setups library answers on 8011.
//   transforms = gTMirror / gTYaw / gTPitch, three plain values.
//   gLights    = DERIVED. computeLive() rebuilds it from gBase x transforms
//                every time anything changes. Nothing accumulates in place,
//                so nothing can drift:
//                  - Mirror twice            = identity
//                  - Orbit +15 then Reset    = exactly the loaded setup
//                  - Switching setups        = keeps current aim (transforms
//                                              re-apply to the new base)
//                RIG_MATH|RESET just zeroes the three transform vars and
//                recomputes from cache. No round trip to the library.
//
//   - 0.9s eased transitions (cubic in/out), shortest-path yaw blending,
//     profile/beam swap at e > 0.5, on = onS || onT during the blend.
//   - Legacy FX streaming (8019/8020/8021) is GONE. FX packs render the
//     prims directly themselves; this engine re-asserts truth on STOP FX
//     and on every setup load.
//   - Ghost-glow fix: glow/fullbright gated on gIsVisible in render() AND
//     cleared in setMeshVis() (alpha alone never hides glow/fullbright).
//   - Every llSetLinkPrimitiveParamsFast float slot is an explicit float
//     literal. Integer literals in float slots are RUNTIME errors that
//     abort the rest of the batch.
//
// MESSAGE CONTRACT:
//   8000 in : SYS|HIDE/SHOW/GAMMA/POWER/REFRESH/SYNC/AUTO_HEIGHT
//             UI|RAD|±0.5  UI|EV|±x/RESET
//             RIG_MATH|MIRROR/ORBIT/PITCH/RESET|val
//             LIGHT_TOGGLE|idx   FX|<name>/STOP FX
//   8000 out: FX|STOP FX  (broadcast whenever 8011 arrives -- a setup load
//             always kills FX; scripts never hear their own llMessageLinked)
//   8001 out: name|radius|masterEV|isOn|height|mirror|yaw|pitch|visible|gamma
//   8011 in : name|radius,y,p,prof,ev,beam,on x4  (25 CSV)  or  name|NOT_FOUND
// =============================================================================

// --- LINKS ---
integer gLinkKey = -1; integer gLinkFill = -1; integer gLinkRim = -1; integer gLinkBg = -1;
integer gLinkOmniKey = -1; integer gLinkOmniFill = -1; integer gLinkOmniRim = -1; integer gLinkOmniBg = -1;

// --- RIG PARAMETERS ---
float gRadius = 1.5; float gMasterEV = 0.0; integer gIsOn = TRUE; integer gUseGamma = TRUE;
float gHeight = 0.6;
integer gIsVisible = TRUE;
string gSetupName = "Manual / Booting";

// --- TRANSFORM STATE (the only mutable aim state) ---
integer gTMirror = FALSE;
float   gTYaw    = 0.0;
float   gTPitch  = 0.0;

// --- TRUTH: pristine setup, raw, pre-transform ---
list gBase = [
    45.0, 35.0, 3,  0.0, 1, 1,
   -45.0,  5.0, 3, -2.0, 1, 1,
  -135.0, 45.0, 4, -0.5, 0, 1,
     0.0,-20.0, 3,  0.0, 0, 0
];

// --- DERIVED live state (what render() draws) ---
list gLights;

// --- TRANSITION ---
list gLightsTarget; list gLightsStart;
float gTransStart = 0.0; float gTransDuration = 0.9;
integer gTransActive = FALSE;
float gRadiusStart; float gRadiusTarget;

string PROJ_TEX = "5748decc-f629-461c-9a36-a35a221fe21f";

list PROFILES = ["2700K Incan", <1.00,0.55,0.15>, "3200K Tung", <1.00,0.92,0.72>, "4500K Neut", <1.00,1.00,0.95>, "5600K Day", <0.85,0.92,1.00>, "6500K Cool", <0.70,0.85,1.00>, "8000K Moon", <0.45,0.55,1.00>, "10000K Sky", <0.3,0.4,1.0>, "Full CTO", <1.00,0.65,0.05>, "Full CTB", <0.30,0.60,1.00>, "Plus Green", <0.60,1.00,0.60>, "Magenta", <1.00,0.20,0.80>, "Rosco Red", <1.00,0.02,0.02>, "Congo Blue", <0.05,0.05,1.00>, "Deep Amber", <1.0, 0.4, 0.0>, "Emerald", <0.0, 1.0, 0.1>, "Cyber Pink", <1.00,0.05,0.80>, "Sci-Fi Cyan", <0.05,0.85,1.00>, "Golden Hour", <1.00,0.40,0.05>, "Vaporwave", <0.8, 0.0, 1.0>, "Blood Red", <0.6, 0.0, 0.0>, "Deep Space", <0.0, 0.0, 0.2>, "Toxic Waste", <0.2, 1.0, 0.0>, "Neon Purple", <0.5, 0.0, 1.0>, "Pure White", <1.00,1.00,1.00>];
list BEAMS = ["Standard", 1.5, 1.0, "Softbox", 2.8, 1.5, "Snoot", 0.2, 0.5];

// =============================================================================
// HELPERS
// =============================================================================

float wrap180(float a) {
    while (a > 180.0) a -= 360.0;
    while (a <= -180.0) a += 360.0;
    return a;
}

float ease(float t) {
    if (t < 0.0) return 0.0;
    if (t > 1.0) return 1.0;
    if (t < 0.5) return 4.0 * t * t * t;
    float f = (2.0 * t) - 2.0;
    return 0.5 * f * f * f + 1.0;
}

// Derive the live light state from truth: base x (mirror -> +yaw -> +pitch).
list computeLive() {
    list out = []; integer i;
    for (i = 0; i < 4; i++) {
        float y = llList2Float(gBase, i*6 + 0);
        if (gTMirror) y = -y;
        y = wrap180(y + gTYaw);
        float pa = llList2Float(gBase, i*6 + 1) + gTPitch;
        if (pa > 85.0) pa = 85.0; if (pa < -85.0) pa = -85.0;
        out += [ y, pa,
                 llList2Integer(gBase, i*6 + 2),
                 llList2Float(gBase, i*6 + 3),
                 llList2Integer(gBase, i*6 + 4),
                 llList2Integer(gBase, i*6 + 5) ];
    }
    return out;
}

findLinks() {
    gLinkKey = -1; gLinkFill = -1; gLinkRim = -1; gLinkBg = -1;
    gLinkOmniKey = -1; gLinkOmniFill = -1; gLinkOmniRim = -1; gLinkOmniBg = -1;
    integer i; for (i = 1; i <= llGetNumberOfPrims(); i++) {
        string n = llGetLinkName(i);
        if (n == "RED") gLinkKey = i; else if (n == "GREEN") gLinkFill = i;
        else if (n == "BLUE") gLinkRim = i; else if (n == "YELLOW") gLinkBg = i;
        else if (n == "OMNI_RED") gLinkOmniKey = i; else if (n == "OMNI_GREEN") gLinkOmniFill = i;
        else if (n == "OMNI_BLUE") gLinkOmniRim = i; else if (n == "OMNI_YELLOW") gLinkOmniBg = i;
    }
}

announce() {
    llOwnerSay("Render Engine v20 online. Links K/F/R/B=" + (string)gLinkKey + "/" + (string)gLinkFill
        + "/" + (string)gLinkRim + "/" + (string)gLinkBg
        + " Omni=" + (string)gLinkOmniKey + "/" + (string)gLinkOmniFill + "/" + (string)gLinkOmniRim + "/" + (string)gLinkOmniBg
        + " | Free mem: " + (string)llGetFreeMemory());
}

// =============================================================================
// RENDERING
// =============================================================================

setMeshVis(float alpha) {
    // Glow and fullbright render even at alpha 0.0 -- clear them explicitly
    // or transparent prims leave ghost halos.
    list b = [];
    if (gLinkKey > 0)      b += [PRIM_LINK_TARGET, gLinkKey,      PRIM_COLOR, ALL_SIDES, <1.0,1.0,1.0>, alpha, PRIM_GLOW, 5, 0.0, PRIM_FULLBRIGHT, 5, FALSE];
    if (gLinkFill > 0)     b += [PRIM_LINK_TARGET, gLinkFill,     PRIM_COLOR, ALL_SIDES, <1.0,1.0,1.0>, alpha, PRIM_GLOW, 5, 0.0, PRIM_FULLBRIGHT, 5, FALSE];
    if (gLinkRim > 0)      b += [PRIM_LINK_TARGET, gLinkRim,      PRIM_COLOR, ALL_SIDES, <1.0,1.0,1.0>, alpha, PRIM_GLOW, 5, 0.0, PRIM_FULLBRIGHT, 5, FALSE];
    if (gLinkBg > 0)       b += [PRIM_LINK_TARGET, gLinkBg,       PRIM_COLOR, ALL_SIDES, <1.0,1.0,1.0>, alpha, PRIM_GLOW, 5, 0.0, PRIM_FULLBRIGHT, 5, FALSE];
    if (gLinkOmniKey > 0)  b += [PRIM_LINK_TARGET, gLinkOmniKey,  PRIM_COLOR, ALL_SIDES, <1.0,1.0,1.0>, alpha];
    if (gLinkOmniFill > 0) b += [PRIM_LINK_TARGET, gLinkOmniFill, PRIM_COLOR, ALL_SIDES, <1.0,1.0,1.0>, alpha];
    if (gLinkOmniRim > 0)  b += [PRIM_LINK_TARGET, gLinkOmniRim,  PRIM_COLOR, ALL_SIDES, <1.0,1.0,1.0>, alpha];
    if (gLinkOmniBg > 0)   b += [PRIM_LINK_TARGET, gLinkOmniBg,   PRIM_COLOR, ALL_SIDES, <1.0,1.0,1.0>, alpha];
    if (llGetListLength(b) > 0) llSetLinkPrimitiveParamsFast(LINK_SET, b);
}

render() {
    list batch = [];
    integer i;
    float distEV = llLog(gRadius / 1.5) / 0.6931472;
    float gMult = 0.05; if (!gIsVisible) gMult = 0.0;

    for (i = 0; i < 4; i++) {
        integer linkProj = gLinkKey; integer linkOmni = gLinkOmniKey;
        if (i == 1) { linkProj = gLinkFill; linkOmni = gLinkOmniFill; }
        else if (i == 2) { linkProj = gLinkRim; linkOmni = gLinkOmniRim; }
        else if (i == 3) { linkProj = gLinkBg; linkOmni = gLinkOmniBg; }

        float yaw = llList2Float(gLights, i*6 + 0);
        float pitch = llList2Float(gLights, i*6 + 1);
        float rY = yaw * DEG_TO_RAD; float rP = pitch * DEG_TO_RAD;
        float cosP = llCos(rP);

        vector orb = <gRadius * cosP * llCos(rY), gRadius * cosP * llSin(rY), gRadius * llSin(rP)>;
        vector p_proj = orb + <0.0, 0.0, gHeight>;

        float omniPitch = pitch - 45.0;
        if (omniPitch < -85.0) omniPitch = -85.0;
        float oCos = llCos(omniPitch * DEG_TO_RAD);
        vector p_omni = <gRadius * oCos * llCos(rY), gRadius * oCos * llSin(rY), gRadius * llSin(omniPitch * DEG_TO_RAD)> + <0.0, 0.0, gHeight>;

        integer prIdx = llList2Integer(gLights, i*6 + 2) * 2 + 1;
        vector col = <1.0, 1.0, 1.0>;
        if (prIdx < llGetListLength(PROFILES)) col = llList2Vector(PROFILES, prIdx);
        if (gUseGamma) col = <llPow(col.x, 2.2), llPow(col.y, 2.2), llPow(col.z, 2.2)>;

        float intensity = llPow(2.0, llList2Float(gLights, i*6 + 3) + gMasterEV + distEV);
        integer on = llList2Integer(gLights, i*6 + 5) && gIsOn && (intensity > 0.001);
        integer fb = on && gIsVisible;

        if (linkProj > 0) {
            integer bIdx = llList2Integer(gLights, i*6 + 4);
            float fov = llList2Float(BEAMS, bIdx*3 + 1);
            float fall = llList2Float(BEAMS, bIdx*3 + 2);
            batch += [PRIM_LINK_TARGET, linkProj,
                PRIM_POS_LOCAL, p_proj,
                PRIM_ROT_LOCAL, llRotBetween(<0.0, 0.0, -1.0>, -llVecNorm(orb)),
                PRIM_POINT_LIGHT, on, col, intensity, gRadius * 2.2, fall,
                PRIM_PROJECTOR, PROJ_TEX, fov, 0.0, 0.0,
                PRIM_GLOW, 5, gMult * intensity,
                PRIM_FULLBRIGHT, 5, fb];
        }
        if (linkOmni > 0) {
            float omniIntens = intensity * 0.45;
            integer omniOn = on && (omniIntens > 0.001);
            batch += [PRIM_LINK_TARGET, linkOmni,
                PRIM_POS_LOCAL, p_omni,
                PRIM_ROT_LOCAL, ZERO_ROTATION,
                PRIM_POINT_LIGHT, omniOn, col, omniIntens, gRadius * 1.5, 0.75,
                PRIM_PROJECTOR, NULL_KEY, 0.0, 0.0, 0.0,
                PRIM_GLOW, 5, 0.0,
                PRIM_FULLBRIGHT, 5, FALSE];
        }
    }
    if (llGetListLength(batch) > 0) llSetLinkPrimitiveParamsFast(LINK_SET, batch);
}

// 8001 broadcast + hover text. The single point everything else syncs from.
syncState() {
    llMessageLinked(LINK_SET, 8001, gSetupName + "|" + (string)gRadius + "|" + (string)gMasterEV
        + "|" + (string)gIsOn + "|" + (string)gHeight + "|" + (string)gTMirror
        + "|" + (string)gTYaw + "|" + (string)gTPitch + "|" + (string)gIsVisible
        + "|" + (string)gUseGamma, NULL_KEY);
    llSetText("", <1.0,1.0,1.0>, 0.0);

    integer i;
    for (i = 0; i < 4; i++) {
        integer linkProj = gLinkKey;
        if (i == 1) linkProj = gLinkFill; else if (i == 2) linkProj = gLinkRim; else if (i == 3) linkProj = gLinkBg;
        if (linkProj > 0) {
            if (!gIsVisible) {
                llSetLinkPrimitiveParamsFast(linkProj, [PRIM_TEXT, "", <1.0,1.0,1.0>, 0.0]);
            } else {
                string role = "KEY"; vector tc = <1.0, 0.4, 0.4>;
                if (i == 1) { role = "FILL"; tc = <0.4, 1.0, 0.4>; }
                else if (i == 2) { role = "RIM"; tc = <0.4, 0.7, 1.0>; }
                else if (i == 3) { role = "BG"; tc = <1.0, 1.0, 0.4>; }

                integer prStrIdx = llList2Integer(gLights, i*6 + 2) * 2;
                string profName = "Custom";
                if (prStrIdx < llGetListLength(PROFILES)) profName = llList2String(PROFILES, prStrIdx);

                float finalEV = llList2Float(gLights, i*6 + 3) + gMasterEV;
                integer on = llList2Integer(gLights, i*6 + 5) && gIsOn;

                string txt = role + "\n" + profName + "\nEV: " + (string)finalEV;
                if (!on) txt += "\n[OFF]";
                if (i == 0) txt = "🎬 " + gSetupName + "\n---\n" + txt;
                llSetLinkPrimitiveParamsFast(linkProj, [PRIM_TEXT, txt, tc, 1.0]);
            }
        }
    }
}

// =============================================================================
// TRANSITIONS
// =============================================================================

startTransition(list target, float newRadius) {
    gLightsStart = gLights;
    gLightsTarget = target;
    gRadiusStart = gRadius;
    gRadiusTarget = newRadius;
    gTransStart = llGetTime();
    gTransActive = TRUE;
    llSetTimerEvent(0.05);
}

// Recompute live state from truth and ease toward it.
applyTruth(float newRadius) {
    if (newRadius < 0.5) newRadius = 0.5;
    startTransition(computeLive(), newRadius);
}

stepTransition() {
    float t = (llGetTime() - gTransStart) / gTransDuration;
    if (t >= 1.0) {
        gLights = gLightsTarget; gRadius = gRadiusTarget;
        gTransActive = FALSE; llSetTimerEvent(0.0);
        render(); syncState();
        return;
    }
    float e = ease(t);
    list blended = []; integer i;
    for (i = 0; i < 4; i++) {
        float yawS = llList2Float(gLightsStart, i*6 + 0);
        float yawT = llList2Float(gLightsTarget, i*6 + 0);
        float dYaw = wrap180(yawT - yawS);

        float pitchS = llList2Float(gLightsStart, i*6 + 1); float pitchT = llList2Float(gLightsTarget, i*6 + 1);
        float evS = llList2Float(gLightsStart, i*6 + 3); float evT = llList2Float(gLightsTarget, i*6 + 3);

        integer profS = llList2Integer(gLightsStart, i*6 + 2); integer profT = llList2Integer(gLightsTarget, i*6 + 2);
        integer modS = llList2Integer(gLightsStart, i*6 + 4); integer modT = llList2Integer(gLightsTarget, i*6 + 4);
        integer onS = llList2Integer(gLightsStart, i*6 + 5); integer onT = llList2Integer(gLightsTarget, i*6 + 5);

        integer prof = profS; if (e > 0.5) prof = profT;
        integer mod = modS; if (e > 0.5) mod = modT;
        integer on = onS || onT;   // stay on during the blend if on in either state

        blended += [ yawS + (dYaw * e), pitchS + (pitchT - pitchS) * e, prof, evS + (evT - evS) * e, mod, on ];
    }
    gLights = blended;
    gRadius = gRadiusStart + (gRadiusTarget - gRadiusStart) * e;
    render();
}

// =============================================================================
// MAIN
// =============================================================================

default {
    state_entry() {
        findLinks();
        gLights = computeLive();
        render(); syncState();
        announce();
    }

    changed(integer c) {
        if (c & CHANGED_LINK) { findLinks(); render(); }
    }

    timer() { if (gTransActive) stepTransition(); }

    link_message(integer s, integer num, string str, key id) {

        // ------------------ SETUP DATA FROM LIBRARY ------------------
        if (num == 8011) {
            // A setup load always kills FX. Scripts don't hear their own
            // llMessageLinked, so this only reaches the packs + Director.
            llMessageLinked(LINK_SET, 8000, "FX|STOP FX", NULL_KEY);

            list parts = llParseString2List(str, ["|"], []);
            gSetupName = llList2String(parts, 0);
            string data = llList2String(parts, 1);

            if (data == "NOT_FOUND") {
                // Custom/manual name: keep current truth, just resync.
                gLights = computeLive();
                render(); syncState();
                return;
            }

            list vals = llParseString2List(data, [","], []);
            float newRad = (float)llList2String(vals, 0);

            // Cache the PRISTINE setup -- raw, pre-transform.
            list nb = []; integer i;
            for (i = 0; i < 4; i++) {
                integer base = 1 + (i * 6);
                nb += [ (float)llList2String(vals, base),
                        (float)llList2String(vals, base + 1),
                        (integer)llList2String(vals, base + 2),
                        (float)llList2String(vals, base + 3),
                        (integer)llList2String(vals, base + 4),
                        (integer)llList2String(vals, base + 5) ];
            }
            gBase = nb;
            applyTruth(newRad);   // transforms re-apply to the new base
            syncState();
            return;
        }

        // ------------------ COMMANDS ------------------
        if (num != 8000) return;
        list p = llParseString2List(str, ["|"], []);
        string cmd = llList2String(p, 0);

        if (cmd == "SYS") {
            string action = llList2String(p, 1);
            if (action == "HIDE") { gIsVisible = FALSE; setMeshVis(0.0); render(); syncState(); }
            else if (action == "SHOW") { gIsVisible = TRUE; setMeshVis(1.0); render(); syncState(); }
            else if (action == "GAMMA") { gUseGamma = !gUseGamma; render(); syncState(); }
            else if (action == "POWER") { gIsOn = !gIsOn; render(); syncState(); }
            else if (action == "REFRESH") { findLinks(); render(); syncState(); announce(); }
            else if (action == "SYNC") { syncState(); }
            else if (action == "AUTO_HEIGHT") {
                // Aim the rig's vertical center at the owner's upper body.
                vector av = llList2Vector(llGetObjectDetails(llGetOwner(), [OBJECT_POS]), 0);
                if (av != ZERO_VECTOR) {
                    vector here = llGetPos();
                    float h = (av.z - here.z) + 0.4;
                    if (h < 0.0) h = 0.0; if (h > 3.0) h = 3.0;
                    gHeight = h;
                    gSetupName = "Manual Edit";
                    applyTruth(gRadius); syncState();
                }
            }
        }
        else if (cmd == "FX") {
            if (llList2String(p, 1) == "STOP FX") {
                // Re-assert truth: an FX pack has been writing arbitrary
                // poses to the prims.
                gTransActive = FALSE; llSetTimerEvent(0.0);
                gLights = computeLive();
                render(); syncState();
            } else {
                // An FX is starting: stand down so the pack owns the prims.
                gTransActive = FALSE; llSetTimerEvent(0.0);
            }
        }
        else if (cmd == "UI") {
            string t = llList2String(p, 1);
            float newRad = gRadius;
            if (t == "RAD") { newRad += (float)llList2String(p, 2); if (newRad < 0.5) newRad = 0.5; }
            else if (t == "EV") {
                if (llList2String(p, 2) == "RESET") gMasterEV = 0.0;
                else gMasterEV += (float)llList2String(p, 2);
            }
            gSetupName = "Manual Edit";
            applyTruth(newRad); syncState();
        }
        else if (cmd == "RIG_MATH") {
            string t = llList2String(p, 1);
            float val = (float)llList2String(p, 2);
            if (t == "MIRROR") gTMirror = !gTMirror;
            else if (t == "ORBIT") gTYaw = wrap180(gTYaw + val);
            else if (t == "PITCH") {
                gTPitch += val;
                if (gTPitch > 85.0) gTPitch = 85.0; if (gTPitch < -85.0) gTPitch = -85.0;
            }
            else if (t == "RESET") { gTYaw = 0.0; gTPitch = 0.0; gTMirror = FALSE; }
            gSetupName = "Manual Aim";
            if (t == "RESET") gSetupName = "Angles Reset";
            applyTruth(gRadius); syncState();   // recomputed from cached truth: no round trip
        }
        else if (cmd == "LIGHT_TOGGLE") {
            integer idx = (integer)llList2String(p, 1);
            if (idx >= 0 && idx <= 3) {
                integer cur = llList2Integer(gBase, idx*6 + 5);
                gBase = llListReplaceList(gBase, [!cur], idx*6 + 5, idx*6 + 5);
                applyTruth(gRadius); syncState();
            }
        }
    }
}
