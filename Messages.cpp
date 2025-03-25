
#include "Messages.hpp"

void CTBModule::from(Manager &manager, char (&buffer)[BUFFERSIZE], uint64_t &bytesRead, uint64_t &bytesProcessed)
{
    manager.readStringFromPipe(moduleName, buffer, bytesRead, bytesProcessed);
}