# Dot-source AFTER prebuild_guard.ps1 defines $kitPy and BEFORE its foreach.
# Kept in the repository so every worktree gets the permanent safety gate.
# clone_looks_gl_check includes the runtime fixtures and real-SL safety suite.
$kitPy += @(
  @{ Label = 'Clone looks GL and real SL safety gate'; File = 'scripts\code_tools\clone_looks_gl_check.py'; PyArgs = @() }
)
