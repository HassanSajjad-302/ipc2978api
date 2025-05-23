This has code samples for communication between compiler and build-system
using named pipes.
The code covers connection establishment and communication.
Error handling is however not implemented.

The code can be tested using CMake targets
```BuildSystemTest``` and ```CompilerTest```.

```BuildSystemTest``` needs to be run first.
As build-system is the process that will create the compiler process.
It has ```std::this_thread::sleep_for``` call to test against the
synchronization bugs.
It is after the ```IPCManagerBS``` creation,
since the build-system will launch a new compilation only after the
creation of this manager.

Shared memory file was also tested.

API is fully cross-platform.
Current, the repo only has backend for Windows.
Soon it will be enhanced for POSIX-compliant operating systems.

Compiler needs to do the following steps to support this.

1) Link Against IPCManagerCompiler.cpp, Manager.cpp.
   These include IPCManagerCompiler.hpp and Manager.hpp.
2) Add a noScanIPC command-line option.
3) Add a findInclude command-line option.
   Can be only provided if 1 is provided.
4) Reserve global memory for IPCManagerCompiler object.
   Construct only if noScanIPC is set.
   It is constructed with the object-file path
   that is provided on the command-line.
5) If the compiler stumbles on import, or,
   #include statement in translate-include or find-include mode,
   use this function to receive the module
   ```BTCModule receiveBTCModule(const CTBModule &moduleName```,
   and this ```BTCNonModule receiveBTCNonModule(const CTBNonModule &nonModule)```
   to receive a non-module.
6) If ```BTCModule``` is received, or,
   ```BTCNonModule::isHeaderUnit == true```,
   read BMI file and its deps using
   ```string_view readSharedMemoryBMIFile(const BMIFile &file)```.
7) Do not print output and error-output but store it.
   Send it with
   ```void sendCTBLastMessage(const CTBLastMessage &lastMessage);```.
   If there is a BMI output,
   send it with
   ```void sendCTBLastMessage(const CTBLastMessage &lastMessage, const string &bmiFile, const string &filePath);```
8) That's all.

In total, compiler needs to use 5 functions of
```IPCCompilerManager``` plus the constructor.
Later on, this paper and my build-system will be further enhanced
to support clang 2-step compilation,
https://lists.isocpp.org/sg15/2023/11/2106.php and
https://isocpp.org/files/papers/P3057R0.html#hash-of-declarations-solution.

