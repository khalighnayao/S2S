# Project Guidelines

These instructions apply to code under `Sources/`.

## Code Style
- Use only C++2003 language features.
- Never use post-C++2003 syntax or library features.
- Do not introduce post-C++2003 syntax (for example: `auto`, lambdas, range-for, `nullptr`, `override`).
- Keep one primary class per file.
- Keep include guards and `#pragma once` for headers.
- Follow formatting/style from `Sources/.editorconfig`.
- The DLL must remain compliant with the existing build chain.
- Do not update, replace, or modernize the build chain/toolchain.

## Architecture
- `CvGameCoreDLL` is the DLL entrypoint. See `Sources/CvGameCoreDLL.cpp`.
- Core engine classes are generally `Cv*`; Python-facing wrappers are generally `Cy*`.
- AI work typically centers on `CvPlayerAI`, `CvCityAI`, `CvUnitAI`, and `CvTeamAI`.
- For AI overview, see `Sources/Mainpage.dox`.

## Build And Test
- Full dev bootstrap + DLL build: `DevSetup.bat`.
- Build DLL from toolchain: `Tools/_MakeDLL.bat Release build deploy`.
- CI-style XML validation: `Tools/XmlValidator.exe -a`.
- Optional callback validation: `Tools/XMLTools/verify-python-callbacks.py`.

## Conventions
- Prefer minimal, local changes in large core files.
- Preserve save compatibility by default; for intentional save breaks, coordinate and mark with `@SAVEBREAK` where relevant.
- If C++ changes affect XML/Python interfaces, validate related XML and callback references.

## Pitfalls
- Build dependencies are legacy and strict (VC++ toolkit + bundled deps in `Build/deps/`); avoid modern compiler assumptions.
- `DevSetup.bat` warns that `Mods/Caveman2Cosmos` can be replaced/symlinked; keep edits in the git workspace.
- Some mod/runtime behavior depends on local tooling and setup scripts; avoid assuming clean-room runtime behavior without validation.

## Reference Docs
- Setup flow: `DevSetup.bat`
- CI flow: `appveyor.yml`
- Source formatting policy: `Sources/.editorconfig`
- AI overview: `Sources/Mainpage.dox`
- Save-break coordination notes: `Notes for the next breaking of save game compatability cycle.txt`