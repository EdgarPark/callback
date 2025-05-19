#include "IPCMessageQueue.h"
#include <sstream>

IPCMessageQueue::IPCMessageQueue(const std::string& name, size_t numThreads)
   : queueName(name), running(false), threadCount(numThreads)
{
#ifdef _WIN32
   hMapFile = NULL;
   hMutex = NULL;
   hSemaphore = NULL;
#else
   msgId = -1;
#endif
}

IPCMessageQueue::~IPCMessageQueue() {
   Stop();
}

bool IPCMessageQueue::InitializeIPC() {
#ifdef _WIN32
   // Windows Named Mutex
   hMutex = CreateMutexA(NULL, FALSE, (queueName + "_mutex").c_str());
   if (hMutex == NULL) return false;

   // Windows Named Semaphore
   hSemaphore = CreateSemaphoreA(NULL, 0, 1000, (queueName + "_sem").c_str());
   if (hSemaphore == NULL) {
      CleanupIPC();
      return false;
   }

   // Windows Memory Mapped File
   hMapFile = CreateFileMappingA(
      INVALID_HANDLE_VALUE,
      NULL,
      PAGE_READWRITE,
      0,
      sizeof(SharedMessage),
      queueName.c_str()
   );
   if (hMapFile == NULL) {
      CleanupIPC();
      return false;
}
   return true;
#else
   key = ftok(queueName.c_str(), 65);
   msgId = msgget(key, IPC_CREAT | 0666);
   return (msgId != -1);
#endif
}

void IPCMessageQueue::CleanupIPC() {
#ifdef _WIN32
   if (hMapFile) {
      CloseHandle(hMapFile);
      hMapFile = NULL;
   }
   if (hMutex) {
      CloseHandle(hMutex);
      hMutex = NULL;
   }
   if (hSemaphore) {
      CloseHandle(hSemaphore);
      hSemaphore = NULL;
   }
#else
   if (msgId != -1) {
      msgctl(msgId, IPC_RMID, NULL);
      msgId = -1;
   }
#endif
}

void IPCMessageQueue::ProcessMessages() {
   SharedMessage msg;

   while (running) {
#ifdef _WIN32
      DWORD waitResult = WaitForSingleObject(hSemaphore, 100);
      if (waitResult == WAIT_OBJECT_0) {
         WaitForSingleObject(hMutex, INFINITE);
         LPVOID pBuf = MapViewOfFile(hMapFile, FILE_MAP_READ, 0, 0, sizeof(SharedMessage));

         if (pBuf) {
            memcpy(&msg, pBuf, sizeof(SharedMessage));
            UnmapViewOfFile(pBuf);

            ReleaseMutex(hMutex);

            // Deserialize parameters
            std::stringstream ss(std::string(msg.data, msg.dataSize));
            std::vector<Parameter> params;
            size_t paramCount;
            char delimiter;

            ss >> paramCount >> delimiter;

            for (size_t i = 0; i < paramCount; ++i) {
               size_t typeHash;
               char colon;
               ss >> typeHash >> colon;

               if (typeHash == typeid(int).hash_code()) {
                  int value;
                  ss >> value >> delimiter;
                  params.push_back(value);
               }
               else if (typeHash == typeid(float).hash_code()) {
                  float value;
                  ss >> value >> delimiter;
                  params.push_back(value);
               }
               else if (typeHash == typeid(double).hash_code()) {
                  double value;
                  ss >> value >> delimiter;
                  params.push_back(value);
               }
               else if (typeHash == typeid(std::string).hash_code()) {
                  std::string value;
                  std::getline(ss, value, ';');
                  params.push_back(value);
               }
            }

            // Process message
            std::lock_guard<std::mutex> lock(handlerMutex);
            auto it = handlers.find(msg.id);
            if (it != handlers.end()) {
               for (const auto& handler : it->second) {
                  try {
                     handler(params);
                  }
                  catch (const std::exception& e) {
                     // Handle exception (log error, etc.)
                  }
               }
            }
         }
      }
#else
      if (msgrcv(msgId, &msg, sizeof(SharedMessage) - sizeof(long), 0, IPC_NOWAIT) != -1) {
         // Deserialize and process message (same as Windows version)
         std::stringstream ss(std::string(msg.data, msg.dataSize));
         std::vector<Parameter> params;
         size_t paramCount;
         char delimiter;

         ss >> paramCount >> delimiter;

         for (size_t i = 0; i < paramCount; ++i) {
            size_t typeHash;
            char colon;
            ss >> typeHash >> colon;

            if (typeHash == typeid(int).hash_code()) {
               int value;
               ss >> value >> delimiter;
               params.push_back(value);
            }
            else if (typeHash == typeid(float).hash_code()) {
               float value;
               ss >> value >> delimiter;
               params.push_back(value);
            }
            else if (typeHash == typeid(double).hash_code()) {
               double value;
               ss >> value >> delimiter;
               params.push_back(value);
            }
            else if (typeHash == typeid(std::string).hash_code()) {
               std::string value;
               std::getline(ss, value, ';');
               params.push_back(value);
            }
         }

         // Process message
         std::lock_guard<std::mutex> lock(handlerMutex);
         auto it = handlers.find(msg.id);
         if (it != handlers.end()) {
            for (const auto& handler : it->second) {
               try {
                  handler(params);
               }
               catch (const std::exception& e) {
                  // Handle exception (log error, etc.)
               }
            }
         }
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
#endif
   }
}

void IPCMessageQueue::Start() {
   if (!running) {
      if (!InitializeIPC()) {
         throw std::runtime_error("Failed to initialize IPC");
      }

      running = true;
      for (size_t i = 0; i < threadCount; ++i) {
         workerThreads.push_back(
            std::make_unique<std::thread>(&IPCMessageQueue::ProcessMessages, this)
         );
      }
   }
}

void IPCMessageQueue::Stop() {
   if (running) {
      running = false;
      for (auto& thread : workerThreads) {
         if (thread && thread->joinable()) {
            thread->join();
         }
      }
      workerThreads.clear();
      CleanupIPC();
   }
}

void IPCMessageQueue::SetThreadCount(size_t numThreads) {
   if (running) {
      Stop();
   }
   threadCount = numThreads;
}

void IPCMessageQueue::RegisterHandler(MessageId id, MessageHandler handler) {
   std::lock_guard<std::mutex> lock(handlerMutex);
   handlers[id].push_back(handler);
}

void IPCMessageQueue::QueueMessageImpl(MessageId id, const std::vector<Parameter>& params) {
   SharedMessage msg;
   msg.type = 1;
   msg.id = id;

   std::stringstream ss;
   ss << params.size() << ";";

   for (const auto& param : params) {
      std::visit([&ss](const auto& value) {
         ss << typeid(value).hash_code() << ":" << value << ";";
         }, param);
   }

   std::string serialized = ss.str();
   msg.dataSize = serialized.size();
   memcpy(msg.data, serialized.c_str(), serialized.size());

#ifdef _WIN32
   if (hMapFile && hMutex && hSemaphore) {
      WaitForSingleObject(hMutex, INFINITE);
      LPVOID pBuf = MapViewOfFile(hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SharedMessage));
      if (pBuf) {
         memcpy(pBuf, &msg, sizeof(SharedMessage));
         UnmapViewOfFile(pBuf);
         ReleaseSemaphore(hSemaphore, 1, NULL);
      }
      ReleaseMutex(hMutex);
   }
#else
   if (msgId != -1) {
      msgsnd(msgId, &msg, sizeof(SharedMessage) - sizeof(long), 0);
   }
#endif
}
