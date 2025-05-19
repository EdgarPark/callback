#pragma once
#include "messageQueue.h"
#include <queue>
#include <map>
#include <mutex>
#include <thread>
#include <condition_variable>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/ipc.h>
#include <sys/msg.h>
#include <fcntl.h>
#include <sys/stat.h>
#endif

class IPCMessageQueue : public IMessageQueue {

public:
   explicit IPCMessageQueue(const std::string& name, size_t numThreads = 1);
   ~IPCMessageQueue();

   void Start() override;
   void Stop() override;
   void SetThreadCount(size_t numThreads) override;
   void RegisterHandler(MessageId id, MessageHandler handler) override;

protected:
   void QueueMessageImpl(MessageId id, const std::vector<Parameter>& params) override;
   void ProcessMessages();
   bool InitializeIPC();
   void CleanupIPC();
private:
   struct SharedMessage {
      long type;
      MessageId id;
      char data[4096];
      size_t dataSize;
   };

   std::string queueName;
   std::vector<std::unique_ptr<std::thread>> workerThreads;
   std::map<MessageId, std::vector<MessageHandler>> handlers;
   std::mutex handlerMutex;
   bool running;
   size_t threadCount;

#ifdef _WIN32
   HANDLE hMapFile;
   HANDLE hMutex;
   HANDLE hSemaphore;
#else
   int msgId;
   key_t key;
#endif
};