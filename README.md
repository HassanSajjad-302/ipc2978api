This has code samples for communication between compiler and build-system
using named pipes on Windows and unix sockets on unix.
The code covers connection establishment, communication,
BMI File Memory mapping, sharing, reading and clearance.

The code can be tested using CMake targets
```BuildSystemTest``` and ```CompilerTest```.

```BuildSystemTest``` needs to be run first.
As build-system is the process that will create the compiler process.
Test also checks for build-system and compiler process synchronization bugs.

API is fully cross-platform.

The Compiler needs to do the following steps to support this.

1) Copy files in `include` and `src` directories.
2) Build & Link against the source files.
2) Add a noScanIPC command-line option.
3) If noScanIPC option is provided, initialize the `IPCCompiler` instance.
4) Add a findInclude command-line option.
   Can be only provided if 1 is provided.
5) If the compiler stumbles on import, or,
   #include statement in translate-include or find-include mode,
   use this function to receive the module
   ```BTCModule receiveBTCModule(const CTBModule &moduleName```,
   and this ```BTCNonModule receiveBTCNonModule(const CTBNonModule &nonModule)```
   to receive a non-module.
   By passing the required parameters, compiler conveys the build-system of
   its requirements.
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

In total, compiler needs to use five functions of
```IPCCompilerManager``` plus the constructor.
Later on, this paper and my build-system will be further enhanced
to support clang 2-step compilation,
https://lists.isocpp.org/sg15/2023/11/2106.php and
https://isocpp.org/files/papers/P3057R0.html#hash-of-declarations-solution.

