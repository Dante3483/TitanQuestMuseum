# Building

From a normal Windows command prompt, in this project:

```bat
build.bat
tools\test.bat
```

MSVC Build Tools with the x86 desktop C++ workload and Windows SDK are required.
`tools/find_vcvars.bat` locates vcvars32 through vswhere. Do not run an x64 developer
prompt. No dependency downloads or reference rebuild are needed for the ASI.
MinHook's MIT sources are included in third_party.

The build compiles C++17 x86 Release, /MT, /O2, /Oy-, /utf-8, /W4 /WX, /GR-;
MinHook C uses hde32. Link is DLL /MACHINE:X86 /SAFESEH /DYNAMICBASE /NXCOMPAT,
with PDB/MAP symbols. The hook frame disassembly check is mandatory.

Outputs:

- `dist/TitanQuestMuseum.asi`: latest compile-verified plugin to copy.
- `dist/TitanQuestMuseum.pdb`: matching debugging symbols; keep locally.
- `dist/TitanQuestMuseum.previous.bin` and `.previous.pdb`: previous successful build.
- `build/staging`: linker output; never install directly from here.
- `build/TitanQuestMuseum.map`, `build/hooks.dis`: debugging/frame evidence.

Publishing stages and verifies the new file, then atomically replaces the dist
binary, retaining the previous one under a non-ASI extension. Failed compilation,
linking, frame checks or publication leave the previous published ASI available.

## Offline checks

`tools/test.bat` runs suites sequentially because they share output directories:
bindings, config, journal, proto, store, tooltip, search, viewgate, catalogue,
museum and import. Logs/output files are in build. The viewgate suite keeps the
exhaustive safety simulation; legacy pad tests were replaced by new UI tests.
Museum checks cover section layout, shared hit rectangles, local text sizing,
fixed statistics anchoring, no page labels, wheel arithmetic and view restoration.
Import checks verify byte-exact copies, reference preservation and no overwrite.

Bindings and catalogue use the installed game's files **read only**, found via
Steam registry or `TQ_GAME_DIR` (directory holding TQ.exe). They do not launch
or deploy into the game. Catalogue/proto require data/oracle; to regenerate it
from the unchanged reference C++ implementation:

```bat
tools\build_reference_oracle.bat
```

This oracle helper requires the supplied v4.1 project as this project's parent.
Normal Museum builds are standalone. The preexisting all-items fixture tests
format compatibility for all 1588 original journal records; it is not installed.

For native text proof, using an available Python 3 runtime:

```bat
python tools\verify_native_text.py
```

No test writes a Titan Quest save, installed plugin or reference source file.
