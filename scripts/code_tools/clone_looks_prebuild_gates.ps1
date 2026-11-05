# Dot-source AFTER prebuild_guard.ps1 defines $kitPy and BEFORE its foreach.
# Kept in the repository so every worktree gets the permanent safety gate.
# clone_looks_gl_check includes the runtime fixtures and real-SL safety suite.
$kitPy += @(
  @{ Label = 'Director ALL OFF contract'; File = 'scripts\code_tools\director_all_off_check.py'; PyArgs = @('--check') }
  @{ Label = 'Director ALL OFF planted fixtures'; File = 'scripts\code_tools\director_all_off_check.py'; PyArgs = @('--selftest') }
  @{ Label = 'Clone looks GL and real SL safety gate'; File = 'scripts\code_tools\clone_looks_gl_check.py'; PyArgs = @() }
  @{ Label = 'Clone rest frame numeric and upload contract'; File = 'scripts\code_tools\clone_rest_frame_check.py'; PyArgs = @('--source-only') }
)
