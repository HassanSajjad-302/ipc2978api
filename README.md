# IPC2978

IPC2978 provides the compiler/build-system protocol for module and header-unit
dependencies. Wire lengths and counts retain their original `uint32_t` format;
local byte counts and parsing offsets use `uint64_t`. Responses identify each
BMI by its file path; the compiler obtains its size from the completed file.

Completed BMI files are opened and mapped by the compiler process when a
response is received. The build system publishes the file path and completion
information; it does not own a cross-process mapping or send a final mapping
acknowledgement. On Unix the compiler uses a file mapping, and on Windows it
opens the file with compatible sharing flags and creates its own file mapping.

The library can be copied into a consumer with:

```sh
python3 tools/copy_library.py \
  --include-dir <consumer>/include \
  --source-dir <consumer>/src
```

Run `python3 clang/lib/IPC2978/setup.py` from the LLVM repository, or
`python3 hconfigure/setup.py` from the HMake repository, to refresh their
copies. Both scripts use the sibling `ipc2978api` repository by default;
`--source /path/to/ipc2978api` selects another checkout. Unchanged files retain
their timestamps to avoid unnecessary rebuilds.

Create an `IPCManagerCompiler` for the compiler session and call
`findResponse(logicalName, FileType)` when resolving a dependency. Module and
header-unit responses expose their mapped contents in `response.bmiContents`.
Ordinary header responses provide a path. The manager caches responses and
mappings, so repeated paths and aliases reuse the same mapping.
`findBMIContents(filePath)` looks up an already mapped BMI; it does not open a
file or send an IPC request.

BMI contents remain mapped until the compiler process exits, when the operating
system releases the views. Destroying a manager does not unmap them. Response
paths and logical names are separate: they are string views into manager-owned
storage, so keep the manager alive while using them. Initialize mock-file mode
once on a fresh manager with `readEntriesFromFile`; a second attempt is rejected,
including after the first attempt failed, and does not invalidate existing views.

The build system should send a BMI only after its producer succeeds and keep
the completed file available and immutable until all consumers finish. No BMI
size or mapping acknowledgement is transmitted. Removing the old size field
changes the response layout, so refresh and rebuild the compiler and build
system together before using them.

`IPCManagerBS::receiveMessage` parses compiler requests after the build system
removes the framing. HMake writes responses through its own event loop. The
standalone tests use `TestBuildSystem` to send the same response format over
their test pipes; that sender is test support rather than a production API.

Build and run the standalone protocol and mapping tests with:

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

`BuildSystemTest` launches `CompilerTest` and compares the received dependency
cache with the sent responses. `MappingTest` covers independent consumers,
process-lifetime views, aliases, invalid files, one-time mock-file loading, and
wire encoding. Mapping checks run in child processes so their views are released
before the parent removes the test files.

`ClangTest` simulates the build system and requires a Clang rebuilt with the
same IPC2978 library and wire format as the test. Copying the library sources
does not update an existing compiler binary. For the sibling LLVM checkout
with an already configured `llvm/my-fork` build, run:

```sh
python3 ../llvm-project/clang/lib/IPC2978/setup.py
cmake --build ../llvm-project/llvm/my-fork --target clang
cmake -S . -B build \
  -DIPC2978_CLANG_EXECUTABLE="$PWD/../llvm-project/llvm/my-fork/bin/clang"
cmake --build build
ctest --test-dir build --output-on-failure
```

Setting `IPC2978_CLANG_EXECUTABLE` adds `ClangTest` to CTest, with its generated
sources and build outputs in `build/ClangTest-work`. It also sets the default
compiler when running the test directly. A command-line argument overrides it:

```sh
mkdir -p build/ClangTest-work
cd build/ClangTest-work
../ClangTest /absolute/path/to/rebuilt/clang
```

Without a configured path or argument, the test looks for `./clang`
(`.\clang.exe` on Windows) for compatibility. The test is also copied to Clang's
unit tests, where it selects the compiler from `LLVM_TOOLS_BINARY_DIR`.
