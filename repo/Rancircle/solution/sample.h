#pragma once
#include "callback.hpp"
#include <iostream>
#include <string>
#include "callbackMng.hpp"

// ����� ���� ������ ����
struct VideoFrame {
   int frameId;
   std::string data;

   VideoFrame(int id, std::string d) : frameId(id), data(std::move(d)) {}
};

struct ProcessResult {
   bool success;
   std::string message;

   ProcessResult(bool s, std::string msg) : success(s), message(std::move(msg)) {}
};

// ���� ó�� Ŭ����
class VideoProcessor {
public:
   VideoProcessor() {
      auto manager = CallbackManager::getInstance();
      manager->registerCallback(1, &VideoProcessor::processFrame, this);
   }

   ProcessResult processFrame(const VideoFrame& frame) {
      std::cout << "Processing frame " << frame.frameId << ": " << frame.data << std::endl;
      return ProcessResult(true, "Frame processed successfully");
   }
};

// ���� ��Ʈ�� �ڵ鷯
class VideoStreamHandler {
public:
   void handleStream() {
      auto manager = CallbackManager::getInstance();

      // ���� �ݹ� ���
      manager->registerCallback(2, [](const ProcessResult& result) -> bool {
         std::cout << "Post-processing result: " << result.message << std::endl;
         return result.success;
         });

      // ������ ���� �� ó��
      VideoFrame frame(1, "Test video data");

      // ����� ��ȯ Ÿ�� ����
      ProcessResult result = manager->invoke<ProcessResult>(1, frame);
      bool postProcessResult = manager->invoke<bool>(2, result);

      std::cout << "Final result: " << (postProcessResult ? "Success" : "Failure") << std::endl;
   }
};

class RxRtspClientService {
public:
   RxRtspClientService() : m_frameCount(0) {}

   void onVideo(int width, int height, const std::string& format) {
      std::cout << "Video frame received: " << width << "x" << height
         << " format: " << format << " (frame #" << ++m_frameCount << ")" << std::endl;
   }

   int processData(const std::string& data, double quality) {
      std::cout << "Processing data: " << data << " with quality: " << quality << std::endl;
      return static_cast<int>(data.length() * quality);
   }

private:
   int m_frameCount;
};

// ��� ���ø� ���� �ݹ� ��� Ŭ����
class CallbackUser {
public:
   CallbackUser(RxCallbackManager& callbackManager) : m_callbackManager(callbackManager) {}

   void triggerVideoCallback(int width, int height, const std::string& format) {
      std::cout << "CallbackUser: Triggering video callback..." << std::endl;
      m_callbackManager.invokeVoid(1, width, height, format);
   }

   int triggerProcessDataCallback(const std::string& data, double quality) {
      std::cout << "CallbackUser: Triggering process data callback..." << std::endl;
      return m_callbackManager.invoke<int>(2, data, quality);
   }

private:
   RxCallbackManager& m_callbackManager;
};