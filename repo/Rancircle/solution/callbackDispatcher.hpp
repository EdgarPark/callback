#ifndef EVENT_CALLBACK_FUN_H
#define EVENT_CALLBACK_FUN_H

#include <functional>
#include <unordered_map>
#include <vector>
#include <shared_mutex>
#include <string>
#include <cstdint>
#include <stdexcept>
#include <iostream>
#include <any>
#include <thread>

/// �̺�Ʈ Ű Ÿ��
using EventKey = unsigned int;

/// �޽��� ����ü: �̺�Ʈ Ű�� �� ���� ���� ������
struct Message {
   EventKey event;
   std::any  wParam;
   std::any  lParam;
};

/// �ڵ鷯�� ���� �� �������� ����
class HandlerNotFoundException : public std::runtime_error {
public:
   explicit HandlerNotFoundException(const EventKey& event)
      : std::runtime_error("No handlers for event: '" + std::to_string(event) + "'") {}
};

/// �ݹ� �������̽�
class IEventCallback {
public:
   virtual ~IEventCallback() = default;
   /// �޽��� ��� onEvent
   virtual void onEvent(const Message& msg) const = 0;
};

/// std::function ��� ����ó (��Ƽ�� �ݹ� ����)
/// �ݹ��� ���� �����忡�� �񵿱�� �����Ͽ� ���������� ����
class EventCallbackDispatcher : public IEventCallback {
public:
   using CallbackMsg = std::function<void(const Message&)>;

   /// Ư�� �̺�Ʈ�� �޽��� ��� �ݹ� ���
   void registerCallback(const EventKey& event, CallbackMsg cb) {
      std::unique_lock lock(mutex_);
      callbacks_[event].push_back(std::move(cb));
   }

   /// Ư�� �̺�Ʈ�� ��� �ݹ� ����
   void unregisterCallbacks(const EventKey& event) {
      std::unique_lock lock(mutex_);
      callbacks_.erase(event);
   }

   /// �̺�Ʈ �߻�: �޽����� �����Ͽ� ��ϵ� �ݹ��� ���� �����忡�� ����
   void onEvent(const Message& msg) const override {
      std::vector<CallbackMsg> cbs;
      {
         std::shared_lock lock(mutex_);
         auto it = callbacks_.find(msg.event);
         if (it == callbacks_.end() || it->second.empty()) {
            throw HandlerNotFoundException(msg.event);
         }
         // �ݹ� ����Ʈ ����
         cbs = it->second;
      }
      // ����� �ݹ��� ���� ���� �����忡�� ����
      for (const auto& cb : cbs) {
         std::thread([cb, msg]() mutable {
            try {
               cb(msg);
            }
            catch (const std::exception& e) {
               std::cerr << "Callback exception: " << e.what() << std::endl;
            }
            catch (...) {
               std::cerr << "Callback unknown exception" << std::endl;
            }
            }).detach();
      }
   }

   /// ���� �����ε�: wParam(intptr_t), lParam(void*)
   void onEvent(const EventKey& event,
      intptr_t wParam,
      void* lParam) const
   {
      onEvent(Message{ event, wParam, lParam });
   }

   /// ���� �����ε�: wParam(void*), lParam(void*)
   void onEvent(const EventKey& event,
      void* wParam,
      void* lParam) const
   {
      onEvent(Message{ event, wParam, lParam });
   }

   /// ���� �����ε�: wParam(intptr_t), lParam(intptr_t)
   void onEvent(const EventKey& event,
      intptr_t wParam,
      intptr_t lParam) const
   {
      onEvent(Message{ event, wParam, lParam });
   }

private:
   mutable std::shared_mutex                             mutex_;
   std::unordered_map<EventKey, std::vector<CallbackMsg>> callbacks_;
};

#endif