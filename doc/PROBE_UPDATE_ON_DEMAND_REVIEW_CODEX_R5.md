Read-only verification at `84545d8b841`; these are design findings against existing code, not implementation validation.

1. **OK — §4.10.** History-first eviction, early settling, and the bulk deadline prevent perpetual postponement. Teardown and identity mismatch explicitly emit retained bounds before erasure/reset.

2. **OK — §§4.4, 4.6.** Monotonic operation stamps and strict `lastFaceOp > stamp` exclude same-frame pre-blur captures. Sculpt readiness matches `llvolume.cpp:1963,3283`.

3. **BUG — §4.11:690–694.** Replacement re-arms at serial N, but rejects a new irradiance pass starting later in that same frame because acceptance requires `start > N`. In On-change mode, the new probe can finish irradiance/radiance and converge, leaving the barrier waiting until timeout (`alcineliveproberefresh.h:546–551`). Distinguish pre-arm starts from post-arm starts within the frame, or defer the replacement’s first pass. Identity binding and Every-frame tracking themselves are correct.

4. **BUG — §4.5.6:443–449.** With 8193 processed lights, no **whole-probe** recount fits the 8192-pair budget: either progress stops or the budget is exceeded. Moreover, continuously re-dirtied priority probes can indefinitely starve regular recounts. Pending-as-over-cap protects pending probes’ event delivery, but does not establish bounded recount progress. Specify resumable pair-level work and starvation-free scheduling. Distant-spot tracking and retained over-cap history otherwise address R4.

5. **BUG — T54 (§10.1).** Stop a key already folded into bulk while another bulk key keeps moving: delivery can take 10 seconds, contradicting “stopped members settle within `kQmin`.” Separate tracked-key and bulk expectations. T55–T58 and S18 otherwise cover their stated cases; add recount progress tests for item 4.

**Must fix: 3, 4, and T54’s contradictory expectation.**


[exited with code 0]
