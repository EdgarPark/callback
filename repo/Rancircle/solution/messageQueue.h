#pragma once
#include <vector>
#include <any>
#include <functional>
#include <memory>
#include <variant>
#include <map>

class IMessageQueue {
public:
   using MessageId = int;
   using Parameter = std::variant<int, float, double, std::string>;
   using MessageHandler = std::function<void(const std::vector<Parameter>&)>;

   virtual ~IMessageQueue() = default;
   virtual void Start() = 0;
   virtual void Stop() = 0;
   virtual void SetThreadCount(size_t numThreads) = 0;
   virtual void RegisterHandler(MessageId id, MessageHandler handler) = 0;

   void QueueMessage(MessageId id) {
      QueueMessageImpl(id, {});
   }

   template<typename T>
   void QueueMessage(MessageId id, T p1) {
      std::vector<Parameter> params{ Parameter(p1) };
      QueueMessageImpl(id, params);
   }

   template<typename T1, typename T2>
   void QueueMessage(MessageId id, T1 p1, T2 p2) {
      std::vector<Parameter> params{ Parameter(p1), Parameter(p2) };
      QueueMessageImpl(id, params);
   }

   template<typename T1, typename T2, typename T3>
   void QueueMessage(MessageId id, T1 p1, T2 p2, T3 p3) {
      std::vector<Parameter> params{ Parameter(p1), Parameter(p2), Parameter(p3) };
      QueueMessageImpl(id, params);
   }

   template<typename T1, typename T2, typename T3, typename T4>
   void QueueMessage(MessageId id, T1 p1, T2 p2, T3 p3, T4 p4) {
      std::vector<Parameter> params{ Parameter(p1), Parameter(p2), Parameter(p3), Parameter(p4) };
      QueueMessageImpl(id, params);
   }

protected:
   virtual void QueueMessageImpl(MessageId id, const std::vector<Parameter>& params) = 0;
};