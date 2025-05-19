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

/// 이벤트 키 타입
using EventKey = unsigned int;

/// 메시지 구조체: 이벤트 키와 두 개의 임의 데이터
struct Message {
   EventKey event;
   std::any  wParam;
   std::any  lParam;
};

/// 핸들러가 없을 때 던져지는 예외
class HandlerNotFoundException : public std::runtime_error {
public:
   explicit HandlerNotFoundException(const EventKey& event)
      : std::runtime_error("No handlers for event: '" + std::to_string(event) + "'") {}
};

/// 콜백 인터페이스
class IEventCallback {
public:
   virtual ~IEventCallback() = default;
   /// 메시지 기반 onEvent
   virtual void onEvent(const Message& msg) const = 0;
};

/// std::function 기반 디스패처 (멀티플 콜백 지원)
/// 콜백을 별도 스레드에서 비동기로 실행하여 독립적으로 수행
class EventCallbackDispatcher : public IEventCallback {
public:
   using CallbackMsg = std::function<void(const Message&)>;

   /// 특정 이벤트에 메시지 기반 콜백 등록
   void registerCallback(const EventKey& event, CallbackMsg cb) {
      std::unique_lock lock(mutex_);
      callbacks_[event].push_back(std::move(cb));
   }

   /// 특정 이벤트의 모든 콜백 해제
   void unregisterCallbacks(const EventKey& event) {
      std::unique_lock lock(mutex_);
      callbacks_.erase(event);
   }

   /// 이벤트 발생: 메시지를 생성하여 등록된 콜백을 별도 스레드에서 실행
   void onEvent(const Message& msg) const override {
      std::vector<CallbackMsg> cbs;
      {
         std::shared_lock lock(mutex_);
         auto it = callbacks_.find(msg.event);
         if (it == callbacks_.end() || it->second.empty()) {
            throw HandlerNotFoundException(msg.event);
         }
         // 콜백 리스트 복사
         cbs = it->second;
      }
      // 복사된 콜백을 각각 독립 스레드에서 실행
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

   /// 편의 오버로드: wParam(intptr_t), lParam(void*)
   void onEvent(const EventKey& event,
      intptr_t wParam,
      void* lParam) const
   {
      onEvent(Message{ event, wParam, lParam });
   }

   /// 편의 오버로드: wParam(void*), lParam(void*)
   void onEvent(const EventKey& event,
      void* wParam,
      void* lParam) const
   {
      onEvent(Message{ event, wParam, lParam });
   }

   /// 편의 오버로드: wParam(intptr_t), lParam(intptr_t)
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