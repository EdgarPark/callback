#pragma once

#include "messageQueue.h"
#include "LocalMessageQueue.h"
#include "IPCMessageQueue.h"
#include <memory>

class MessageQueueFactory {
public:
   static std::unique_ptr<IMessageQueue> CreateMessageQueue(
      bool useIPC = false,
      const std::string& ipcName = "",
      size_t numThreads = 1
   ) {
      if (useIPC) {
         return std::make_unique<IPCMessageQueue>(ipcName, numThreads);
      }
      return std::make_unique<LocalMessageQueue>(numThreads);
   }
};