# VELRATH

Competitive multiplayer 3D Action RPG built with Unreal Engine C++.

## Local baseline

- Unreal Engine 5.7.4
- Visual Studio with the Game development with C++ workload
- Windows SDK 10.0.26100.0
- Git LFS

The Gameplay Ability System and Enhanced Input plugins are enabled in the project.

## Commands

Run these commands from PowerShell in the repository root:

```powershell
& .\Scripts\GenerateProjectFiles.ps1
& .\Scripts\Build.ps1
& .\Scripts\RunEditor.ps1
& .\Scripts\Test.ps1
```

Build other targets explicitly when needed:

```powershell
& .\Scripts\Build.ps1 -Target Game -Configuration Development
```

The Epic Games Launcher distribution used by this workspace builds the Editor
and Game targets, but rejects dedicated Server targets. Building
`VELRATHServer` requires an Unreal Engine 5.7.4 source build (or an installed
build produced from source with server support). Do not run the Server target
with the current Launcher engine. A listen server can still be developed with
the Game target while that infrastructure is prepared.

Generated Unreal and IDE files are intentionally excluded from Git.
