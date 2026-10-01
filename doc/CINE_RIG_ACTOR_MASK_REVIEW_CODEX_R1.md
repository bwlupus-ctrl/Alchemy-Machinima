**GO-after-fixes.** The approach is viable, but these A items block implementation approval. Reviewed against `025ed6763c6`; current HEAD differs only in documents. No edits, builds or commits.

1. **A â€” Suppression check depends on the wrong shader context (Â§5.4).** `shouldSuppressSharedActorFx(owner)` requires the *currently bound shader* to advertise Actor FX (`lldrawpool.cpp:865â€“881`). Calling it at the lighting entry or under a new mask shader does not reliably reproduce G-buffer suppression. A suppressed actor can therefore be redrawn against background depth, marking the set. **Fix:** capture per-view native-draw eligibility during G-buffer submission, or introduce an explicit-context predicate independent of the current program. Apply that eligibility consistently to forward selection too.

2. **A â€” The wrapper removes the vertex entry point (Â§5.5).** `LLGLSLShader::createShader()` passes `mDefines` to **every** listed shader file (`llglslshader.cpp:466â€“469`). Consequently, `addPermutation("main", "actorMaskedMain")` also renames `multiPointLightV.glsl`â€™s `main`; the fragment wrapper cannot repair the vertex stage. This is a deterministic link problem, not merely possible driver rejection. **Fix:** scope renaming to fragment compilation, or supply a corresponding vertex wrapper. Preserve existing program definitions and shader files; the proposed source-edit fallback violates that hard constraint.

3. **A â€” Both collectors can terminate before finding actor lights (Â§Â§5.5, 5.7).** Deferred collection breaks when the world-light count exceeds its limit (`pipeline.cpp:23427â€“23430`). Forward collection breaks after filling six world units (`10722â€“10726`). Actor-only lights later in distance order never reach the proposed collectionsâ€”even with zero competing actor lights. **Fix:** collect actor candidates independently, or continue scanning after world collection closes while preserving exactly the baseline world count, fades and selected units.

4. **A â€” Empty shadow slots outrank actor-only lights (Â§5.6).** The proposed formula assigns a non-rig incumbent tier 1; a null incumbent also has `incumbent_is_rig == false`. Actor candidates at tier 0 therefore cannot fill empty slots. **Fix:** rank null slots below every real candidate and explicitly handle null displaced candidates. This concerns available self-shadow slots, not the accepted loss under contention.

5. **A â€” Slot-bit aliasing breaks isolation (Â§5.1, N12).** A legacy SELF anchor and Group SELF can resolve to different avatars while sharing bit 0 (`alcinelightrig.cpp:1051â€“1064`, `163â€“166`). Their lights then illuminate both subjects. **Fix:** allocate mask bits by unique resolved recipient identity, translating each lightâ€™s recipient set into bits. R8 and any-of remain unchanged; this case needs at most six distinct recipients.

6. **A â€” Forward ownership drops the wearerâ€™s membership (Â§5.7).** For worn animesh that is also a cast subject, both exact and fallback owners can have nonzero bytes. â€œFallback only if exact is zeroâ€ discards the wearerâ€™s lights, contradicting â€œeverything they wearâ€ and potentially disagreeing with mask replay (`llspatialpartition.h:114â€“119`). **Fix:** explicitly union applicable recipient memberships for worn geometry and use the same rule in deferred and forward paths.

7. **A â€” EMPTY is not a trustworthy failure verdict (Â§9.1).** A subjectâ€™s group being â€œnot occludedâ€ does not prove any eligible subject fragment is visible; a bounding box can survive occlusion while the actual geometry is fully hidden. Asynchronous results also need their originating frame/view/subject metadata. **Fix:** reserve EMPTY for independently established visible coverage; otherwise report visibility INCONCLUSIVE. Associate query results with captured visibility state, never current-frame state.

8. **C â€” WP parallelism contradicts file ownership (Â§10).** WP5 shares `alcinelightrig.cpp`, manager headers and tests with WP0/other work, despite the â€œdisjoint filesâ€ claim. Schedule shared-file edits sequentially.

The colour-only clear correctly prevents depth erasure. Fullscreen point parity remains an appropriate validation gate; the accepted B1â€“B7 limits are not findings.


[exited with code 0]
