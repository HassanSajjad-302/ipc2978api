This has code samples for communication between compiler and build-system
using named pipes.
The code covers connection establishment and communication.
Error handling is however not implemented.
The code can be tested using CMake targets
```BuildSystemTest``` and ```CompilerTest```.

```BuildSystemTest``` needs to be run first.
Since it is the process that will create the compiler process.
It has ```std::this_thread::sleep_for``` call to test against the
synchronization bugs.
It is after the ```IPCManagerBS``` creation,
since the build-system will launch a new compilation only after the
creation of this manager.

Current, the repo only has code for Windows.
Soon it will be enhanced for POSIX-compliant operating systems.

It will also have code for shared memory / memory mapped file
for both POSIX and Windows.