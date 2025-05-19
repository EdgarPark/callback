#pragma once
#include "messageQueue.h"
#include <queue>
#include <mutex>
#include <condition_variable>

class LocalMessageQueue : public IMessageQueue {
public:
   explicit LocalMessageQueue(size_t numThreads = 1);
   ~LocalMessageQueue();

   void Start() override;
   void Stop() override;
   void SetThreadCount(size_t numThreads) override;
   void RegisterHandler(MessageId id, MessageHandler handler) override;

protected:
   void QueueMessageImpl(MessageId id, const std::vector<Parameter>& params) override;
   void ProcessMessages();

private:
   struct Message {
      MessageId id;
      std::vector<Parameter> params;
   };
   std::queue<Message> messageQueue;
   std::map<MessageId, std::vector<MessageHandler>> handlers;
   std::vector<std::unique_ptr<std::thread>> workerThreads;
   std::mutex queueMutex;
   std::mutex handlerMutex;
   std::condition_variable condition;
   bool running;
   size_t threadCount;
};