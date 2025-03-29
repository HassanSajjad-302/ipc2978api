
#include "Messages.hpp"

void CTBLastMessage::from(Manager &manager, char (&buffer)[4096], uint64_t &bytesRead, uint64_t &bytesProcessed)
{
    exitStatus = manager.readBoolFromPipe(buffer, bytesRead, bytesProcessed);
    if (exitStatus != EXIT_SUCCESS)
    {
        return;
    }
    hasLogicalName = manager.readBoolFromPipe(buffer, bytesRead, bytesProcessed);
    headerFiles = manager.readVectorOfStringFromPipe(buffer, bytesRead, bytesProcessed);
    output = manager.readStringFromPipe(buffer, bytesRead, bytesProcessed);
    errorOutput = manager.readStringFromPipe(buffer, bytesRead, bytesProcessed);
    if (hasLogicalName)
    {
        logicalName = manager.readStringFromPipe(buffer, bytesRead, bytesProcessed);
    }
    outputFilePaths = manager.readVectorOfMaybeMappedFileFromPipe(buffer, bytesRead, bytesProcessed);
}