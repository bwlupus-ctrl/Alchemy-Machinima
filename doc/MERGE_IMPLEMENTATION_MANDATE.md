# MERGE IMPLEMENTATION MANDATE — WRITE CODE, NOT PLANS  (v4)

**On disk:** `I:\alchemy-machinima\doc\MERGE_IMPLEMENTATION_MANDATE.md`
**Run mode:** Codex **`--write` (implement)**, NOT review/research. One Playbook Step per run.
**Shell:** Windows **PowerShell 5.1** (no `grep`, no `&&`; use `Select-String`, `;`, `if ($?)`).

## Canonical facts (verified in-tree — do not re-guess)
- **Integration branch (exists):** `integration/upstream-af0f3bd`, checked out in the sibling worktree
  `I:\alchemy-machinima-integration-upstream-af0f3bd`. Adopt this name everywhere. (`MERGE_PLAYBOOK.md`
  still says `integration/upstream-renderer-af0f3bd`; treat the playbook's git *names* as superseded by
  this line — its step content still governs.)
- **Step 0 is DONE:** `integration/upstream-af0f3bd` HEAD = **`b6658c34420`** = branch
  `checkpoint/00-ledger-adjudicated` (committed `doc/merge_port_ledger.json` — 664 entries / 13,740 lines). The
  pristine base is branch `checkpoint/00-upstream-pristine-af0f3bd`.
- **Next run is Step 1**, and **its base is `checkpoint/00-ledger-adjudicated` (`b6658c34420`)** — NOT
  the pristine checkpoint (that one lacks the ledger).
- Authoritative fork tips: `develop` = `origin/develop` = `47af3df7b1a` (safe; never touched by integration).

## Model (resolves every worktree/patch hazard from the v3 review)
- **All integration work happens on `integration/upstream-af0f3bd` IN ITS SIBLING WORKTREE.** You
  (Codex) implement a step by **committing real code there**. No patch files, no `_merge/` dir, no
  `git apply` dance — those v3 mechanics are removed.
- **The authoritative `develop` worktree (`I:\alchemy-machinima`) is never modified.** The integration
  branch is disposable: if a step is rejected, `git reset` it to the prior checkpoint.
- **Claude reviews and builds.** Claude reads your step as `git -C <sibling> diff <base>..HEAD`, reviews
  it, and (at authorized checkpoints) builds **in the sibling worktree with its own build directory**
  `…-integration-upstream-af0f3bd\build-Windows-vs2026-os` — so the integration source compiles, not
  `develop`. First build is a full configure; later builds are incremental.
- **The specs are inputs, read by absolute path** `I:\alchemy-machinima\doc\MERGE_*.md`. Because they
  are untracked (mutable), **record each spec's SHA-256 in your commit message** and Claude re-checks
  it before building (integrity gate). Author no `.md`.

---

## 0. THE JOB — implement, don't plan
Planning is DONE (six reviewed specs, 0 blocker/0 major). Produce **committed code** on the integration
branch that implements them. A spec, an "approach," a "shape," a bullet plan, a restatement → rejected.
Do not explain what the code would do. Write it.

---

## 1. OUTPUT CONTRACT — per step type (not "one diff" for everything)
| Step | Output contract |
|---|---|
| 0 | **DONE** — branch + `merge_port_ledger.json` committed. Do not redo. |
| **1-9** | **Candidate code committed** to `integration/upstream-af0f3bd` in the sibling worktree. Every symbol verified; compile-closed within reach. **Do NOT create the `checkpoint/<NN>-…` branch yourself** — Claude creates it only after review + build acceptance (§7). |
| 10 | Static adversarial closure: either **no change** (state "no defects; no commit") **or** fix-commits. An empty result here is valid and is NOT a "partial step." |
| 11 | **Claude action:** authorized Release build + in-world acceptance. You produce no commit. |
| 12 | **Claude action, explicit user approval:** advance `develop` to the integration head + backup ref. You produce no commit. |

For the code steps (1-9), every commit must be **real code, real file, real line**; comments document
code, never replace it.

### FORBIDDEN on code steps (any = rejection)
- Authoring/modifying any **`.md`** (reading specs is required and fine; the ledger is `.json`, allowed).
- Weasel-words on added lines: `shape`, `approach`, `would look like`, `sketch`, `pseudo`, `outline`,
  `TODO`, `FIXME`, `placeholder`, `to be determined`, `redefine at re-land`, `the porter should`,
  `one could`, `high-level`, `left as an exercise`.
- A **partial step**: skipping/stubbing/deferring any item the step's Playbook section + matching
  `MERGE_*.md` enumerate (except Step 10, which may legitimately be empty).
- Committing **only** a `BLOCKER:` note with no code, unless every hunk is genuinely blocked (§5).

---

## 2. SCOPE — one Step per run, Playbook 1:1, base = prior checkpoint
Step is chosen by the **run invocation** ("implement Step N"), never by editing this file.

| Run | Playbook Step | Base (checkpoint at end of prior step) | Ends at |
|---|---|---|---|
| 1 | **Upstream renderer core + required fork primitives** | `checkpoint/00-ledger-adjudicated` (`b6658c34420`) | `checkpoint/01-renderer-primitives` |
| 2 | **Render-independent application foundation** | `checkpoint/01-renderer-primitives` | `checkpoint/02-application-foundation` |
| 3 | Committed VCam vertical slice | `checkpoint/02-application-foundation` | `checkpoint/03-vcam-committed` |
| 4 | Clone/ghost vertical slice | `checkpoint/03-vcam-committed` | `checkpoint/04-clones` |
| 5 | Alpha interleaving, forced mask, sidecar | `checkpoint/04-clones` | `checkpoint/05-alpha-mask-sidecar` |
| 6 | Velocity, temporal capture, motion blur, 10-bit/ReShade | `checkpoint/05-alpha-mask-sidecar` | `checkpoint/06-temporal-output` |
| 7 | Projector volumetrics + froxel | `checkpoint/06-temporal-output` | `checkpoint/07-projvol-froxel` |
| 8 | Weather | `checkpoint/07-projvol-froxel` | `checkpoint/08-weather` |
| 9 | Remaining application, UI, assets, hook closure | `checkpoint/08-weather` | `checkpoint/09-application-closure` |
| 10 | Static adversarial closure | `checkpoint/09-...` | `checkpoint/10-static-ready` |

**Steps 1 and 2 are not skippable.** Step 3 (VCam) references renderer primitives (Step 1) and the
application foundation (Step 2); it cannot compile-close until they land first. Read the base's actual
SHA with `git rev-parse <base-branch>` and confirm HEAD is at the base before you start.

---

## 3. GROUNDING (or the code is fiction)
- **Committed fork only** (`47af3df7b1a`) is the port source; upstream base is `af0f3bd1beb`. The sole
  exception is the optional cookie / `alprismcamdriver` WIP slice from `stash@{0}`, NOT in Runs 1-10.
- Every upstream symbol you call resolves at `af0f3bd1beb`; every fork symbol you port resolves at
  `47af3df7b1a`. Prove it with `git grep <sym> <sha>` before writing the call. No invented signatures.

---

## 4. SPEC FIDELITY — verify symbols; conceptual blocks are starting points, not gospel
Do not re-derive the *design* the reviewed specs settled. But you **must**: verify every concrete
symbol/signature against the tree before calling it, and treat any spec block labeled
illustrative/conceptual/"more precise than pseudocode" as a **starting point**. Known examples:
`MERGE_VCAM_ADAPTATION.md` "Concrete VCam port code" (its own note: new member names must match the
final header) and `MERGE_CLONES_ADAPTATION.md` "Conceptual rigged branch". Implement the **real,
verified** code and reconcile every new member/uniform name against the final header you write.

---

## 5. IF YOU HIT A REAL WALL — one line, the only allowed non-code output
```
BLOCKER: <file>:<line> needs <symbol/behavior> — not found at af0f3bd1beb (checked: <the git grep you ran>)
```
Then keep implementing every hunk that is not blocked. Never expand a blocker into prose.

---

## 6. SELF-VERIFY BEFORE YOU RETURN — PowerShell 5.1 (the reviewer re-runs these)
Run from the sibling worktree `I:\alchemy-machinima-integration-upstream-af0f3bd`:
```powershell
# 6a. Clean worktree BEFORE you start (HEAD exactly at the base, nothing dirty) AND AFTER you commit
#     (nothing left uncommitted = nothing omitted from the reviewed range):
git rev-parse --abbrev-ref HEAD          # -> integration/upstream-af0f3bd
git status --porcelain                    # BEFORE editing: no output; git rev-parse HEAD == <base-branch>
# ...implement, then commit...
git status --porcelain                    # AFTER committing: no output (everything is in the commit)
git rev-parse HEAD; git rev-parse <base-branch>   # HEAD must now differ (you committed)

# 6b. Review the step's changes as a range diff (no patch files, handles binaries natively):
git diff --stat <base-branch>..HEAD
git diff --check <base-branch>..HEAD      # -> no output (no whitespace/format errors)

# 6c. No weasel-words on added lines:
git diff <base-branch>..HEAD | Select-String -Pattern '^\+' |
  Select-String -Pattern 'shape|approach|would look like|sketch|pseudo|outline|TODO|FIXME|placeholder|to be determined|redefine at re-land|the porter should|one could|high-level|left as an exercise'
#   -> no output

# 6d. No .md changed this step:
git diff --name-only <base-branch>..HEAD | Select-String -Pattern '\.md$'    # -> no output

# 6e. Every new upstream symbol you introduced resolves at the upstream base:
git grep <each_new_upstream_symbol> af0f3bd1beb                              # -> found

# 6f. Spec + mandate integrity — compute at run START, re-verify UNCHANGED right before you commit,
#     and paste the run-start hashes into the commit message. All SIX specs PLUS this mandate:
Get-FileHash -Algorithm SHA256 `
  I:\alchemy-machinima\doc\MERGE_CONTRACT_DIFF.md, I:\alchemy-machinima\doc\MERGE_VCAM_ADAPTATION.md, `
  I:\alchemy-machinima\doc\MERGE_CLONES_ADAPTATION.md, I:\alchemy-machinima\doc\MERGE_PIPELINE_PLAN.md, `
  I:\alchemy-machinima\doc\MERGE_SHADER_DRIFT.md, I:\alchemy-machinima\doc\MERGE_PLAYBOOK.md, `
  I:\alchemy-machinima\doc\MERGE_IMPLEMENTATION_MANDATE.md
#   -> if any hash changed between run-start and commit, ABORT (a governing doc moved under you).
```
If 6c/6d print anything, you shipped prose or touched a doc — fix it before returning.

---

## 7. BUILD GATES (Claude's job; Playbook-aligned per the v3 review)
Builds run **only with explicit user authorization**, in the **sibling worktree's own**
`build-Windows-vs2026-os`, never the `develop` cache. **Codex never builds — Claude does.**
| After | Build |
|---|---|
| Step 0 | Upstream baseline Release build — confirm pristine upstream compiles before any fork code. |
| Steps 1-2 | Static unless the landed CMake units are dependency-closed; build when closed. |
| **Steps 3-9** | **Release + startup build after each applied vertical slice.** |
| Step 10 | Static closure (adversarial), no new build required. |
| Step 11 | Full Release + in-world acceptance (authorized). |
**Candidate → checkpoint acceptance.** Your step commit is a *candidate*; the branch tip stays an
unaccepted candidate until Claude's review passes and (at a buildable step) the build passes. **Only
then does Claude create `checkpoint/<NN>-…`** at that commit. Never create the checkpoint yourself.

**Failed-build repair runs.** If an authorized build fails, the next run is **`Step N-fixK`**: add fix
commits onto the *same* candidate (base = the *same* prior checkpoint, not a new one). No
`checkpoint/<NN>-…` is created until the build passes. A build failure is your bug to fix — not a note,
not a "known limitation."

**Restated:** commit candidate code to the integration branch per step; Claude reviews the range diff,
builds (on authorization) in the integration worktree, and stamps the checkpoint only on acceptance.
Write the code.
