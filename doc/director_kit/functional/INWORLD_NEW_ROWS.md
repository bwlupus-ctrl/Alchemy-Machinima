# In-world rows added after run 1

This is the fail-closed locality allow-list for controls that become executable
after `run_functional_20261012_093309`. An absent row remains denied in-world.
Each authorization must cite the exact callback or local sink as `file:line`;
owner-family membership alone is not evidence.

| UI | Page | Control | Locality evidence |
|---|---|---|---|

No newly executable row is currently authorized.

## Expected diff procedure

Regenerate the candidate only from run 1's captured generated manifest, then
inspect the fail-closed diff metadata and the candidate rows:

```powershell
python -I scripts/perf/functional_report.py `
  --prepare-manifest doc/director_kit/run_functional_20261012_093309/manifest_generated.json `
  --prepared-output .tmp/inworld_run1_rediff.json --repo-root .

$m = Get-Content .tmp/inworld_run1_rediff.json -Raw | ConvertFrom-Json
$m.meta.inworld_run1_new_rows_denied
$m.entries | Where-Object {
  $_.inworld_allow_evidence -like 'run-1 locality:*' -or
  $_.inworld_allow_evidence -like 'new since run 1*'
} | Select-Object version,page,widget_name,action,inworld_allowed,inworld_allow_evidence
```

For each intended new executable row, inspect its exact registration and full
depth-3 callback chain. Add a table row above only when that chain is local,
using its exact `file:line`; regenerate and confirm only those documented rows
change to `inworld_allowed: true`.
