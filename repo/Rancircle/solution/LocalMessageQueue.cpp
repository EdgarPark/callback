#include "LocalMessageQueue.h"

LocalMessageQueue::LocalMessageQueue(size_t numThreads)
   : running(false), threadCount(numThreads)
{
}

LocalMessageQueue::~LocalMessageQueue() {
   Stop();
}

void LocalMessageQueue::Start() {
   if (!running) {
      running = true;
      for (size_t i = 0; i < threadCount; ++i) {
         workerThreads.push_back(
            std::make_unique<std::thread>(&LocalMessageQueue::ProcessMessages, this)
         );
      }
   }
}

void LocalMessageQueue::Stop() {
   if (running) {
      {
         std::lock_guard<std::mutex> lock(queueMutex);
         running = false;
      }
      condition.notify_all();

      for (auto& thread : workerThreads) {
         if (thread && thread->joinable()) {
            thread->join();
         }
      }
      workerThreads.clear();
   }
}

void LocalMessageQueue::SetThreadCount(size_t numThreads) {
   if (running) {
      Stop();
   }
   threadCount = numThreads;
}

void LocalMessageQueue::RegisterHandler(MessageId id, MessageHandler handler) {
   std::lock_guard<std::mutex> lock(handlerMutex);
   handlers[id].push_back(handler);
}

void LocalMessageQueue::QueueMessageImpl(MessageId id, const std::vector<Parameter>& params) {
   {
      std::lock_guard<std::mutex> lock(queueMutex);
      messageQueue.push({ id, params });
   }
   condition.notify_one();
}

void LocalMessageQueue::ProcessMessages() {
   while (true) {
      Message msg;
      {
         std::unique_lock<std::mutex> lock(queueMutex);
         condition.wait(lock, [this] {
            return !running || !messageQueue.empty();
            });

         if (!running && messageQueue.empty()) {
            break;
         }

         if (!messageQueue.empty()) {
            msg = std::move(messageQueue.front());
            messageQueue.pop();
         }
      }

      std::lock_guard<std::mutex> lock(handlerMutex);
      auto it = handlers.find(msg.id);
      if (it != handlers.end()) {
         for (const auto& handler : it->second) {
            try {
               handler(msg.params);
            }
            catch (const std::exception& e) {
               // Handle exception (log error, etc.)
            }
         }
      }
   }
}