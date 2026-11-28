# Saved Windows Debug executable

`HerelinkGCS.exe` is the Windows x64 Debug executable with the CH5 emergency
disarm countdown, stored with Git LFS. The user confirmed CH5 operation on
2026-11-28. Its SHA-256, build timestamp and validation results
are recorded in [manifest.json](manifest.json). The source changes are in the
same commit. This snapshot was successfully launched through the existing
Debug launcher on 2026-11-20.

Use the established command from the repository root on the configured PC:

```powershell
.\custom\build-debug.cmd run
```

The launcher uses `build-herelink-debug/Debug/HerelinkGCS.exe`, configures the
Qt/GStreamer environment and writes diagnostic logs. It refuses to start a
second GCS instance. This saved EXE is a Debug artifact, not a portable installer;
it requires the project's existing Windows runtime dependencies and Debug
runtime layout. Running it directly is not the supported launch method.

To restore this exact executable after fetching the commit, safely close GCS
first and run from the repository root:

```powershell
git lfs pull --include="custom/artifacts/windows-debug/HerelinkGCS.exe"
Get-FileHash .\custom\artifacts\windows-debug\HerelinkGCS.exe -Algorithm SHA256
# Compare the hash with manifest.json before replacing the build executable.
Copy-Item .\custom\artifacts\windows-debug\HerelinkGCS.exe .\build-herelink-debug\Debug\HerelinkGCS.exe
.\custom\build-debug.cmd run
```

The existing configured build/runtime directory must be present. For a fresh
development setup, follow [the build instructions](../../README.md#windows-commands)
and build normally first. Runtime DLLs, user settings, aircraft parameters and
personal logs are not bundled with this snapshot.

See [RC control diagnosis](../../RC_CONTROL_DIAGNOSIS.md) for the control paths,
known test/lint limitations and the remaining hardware checks.
