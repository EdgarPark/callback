// solution.cpp : 이 파일에는 'main' 함수가 포함됩니다. 거기서 프로그램 실행이 시작되고 종료됩니다.
//
#include "callbackDispatcher.hpp"
#include <iostream>
#include "sample.h"
using namespace std;

 class AsyncHandler {
 public:
     void handleIntVoid(const Message& msg) {
        std::cout << "[" << std::this_thread::get_id() << "] [Member Fun] Handling EVENT_ASYNC_INT_VOID (" << msg.event << ")..." << std::endl;
        try {
           // onEvent 오버로드에서 intptr_t, void*로 전달된 데이터를
           // std::any_cast를 통해 원래 타입으로 다시 가져옵니다.
           auto code = std::any_cast<intptr_t>(msg.wParam);
           auto data = std::any_cast<void*>(msg.lParam);
           std::cout << "[" << std::this_thread::get_id() << "] [Member Fun] code=" << code
              << ", data=" << data << std::endl;
        }
        catch (const std::bad_any_cast& e) {
           std::cerr << "[" << std::this_thread::get_id() << "] [Member Fun] Error casting data for EVENT_ASYNC_INT_VOID: " << e.what() << std::endl;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(30)); // 작업 시뮬레이션
        std::cout << "[" << std::this_thread::get_id() << "] [Member Fun] Finished handling EVENT_ASYNC_INT_VOID." << std::endl;
     }
     void handleVoidVoid(const Message& msg) {
         auto wp = std::any_cast<void*>(msg.wParam);
         auto lp = std::any_cast<void*>(msg.lParam);
         std::cout << "[Member Async Void-Void] wParam=" << wp
                   << ", lParam=" << lp << std::endl;
     }
     void handleIntInt(const Message& msg) {
         auto a = std::any_cast<intptr_t>(msg.wParam);
         auto b = std::any_cast<intptr_t>(msg.lParam);
         std::cout << "[Member Async Int-Int] a=" << a
                   << ", b=" << b << std::endl;
     }
 };
 enum MyEvents {
    // ... 다른 이벤트들 ...
    EVENT_ASYNC_INT_VOID = 2001, // intptr_t, void* 타입 인자를 기대하는 이벤트
    // ... 다른 이벤트들 ...
    EVENT_WITHOUT_HANDLER = 9999
 };
int main()
{
   EventCallbackDispatcher dispatcher;
   AsyncHandler handler;
   dispatcher.registerCallback(MyEvents::EVENT_ASYNC_INT_VOID,
      [&handler](const Message& msg) {
         handler.handleIntVoid(msg);
      });
   dispatcher.onEvent(MyEvents::EVENT_ASYNC_INT_VOID, (intptr_t)12345, (void*)0xABCDEF01);
   std::this_thread::sleep_for(std::chrono::seconds(1));  
   
   try {
      VideoProcessor processor;
      VideoStreamHandler handler;
      handler.handleStream();
   }
   catch (const std::exception& e) {
      std::cerr << "Error: " << e.what() << std::endl;
      return 1;
   }

   RxCallbackManager callbackManager;
   RxRtspClientService rtspService;

   // 클래스 멤버 함수 등록
   callbackManager.registerCallback(1, &RxRtspClientService::onVideo, &rtspService);
   callbackManager.registerCallback(2, &RxRtspClientService::processData, &rtspService);

   // 람다 함수 등록
   callbackManager.registerCallback(3,
      std::function<int(int, int)>([](int a, int b) -> int {
         std::cout << "Lambda called with a=" << a << ", b=" << b << std::endl;
         return a + b;
         }));

    //일반 함수 등록
   std::function<void(const std::string&)> normalFunc = [](const std::string& message) -> void {
      std::cout << "Normal function called: " << message << std::endl;
   };
   callbackManager.registerCallback(4, normalFunc);
   

   // 다른 클래스에서 콜백 사용
   CallbackUser callbackUser(callbackManager);

   // 콜백 호출
   callbackUser.triggerVideoCallback(1920, 1080, "H.264");
   int result = callbackUser.triggerProcessDataCallback("TestData", 0.75);
   std::cout << "Process data result: " << result << std::endl;

   // 직접 호출
    callbackManager.invokeVoid(4, "Hello from main");
   int sum = callbackManager.invoke<int>(3, 10, 20);
   std::cout << "Lambda result: " << sum << std::endl;

   return 0;
}