#pragma once
#include <iostream>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <any>
#include <tuple>
#include <typeindex>
#include <vector>
#include <type_traits> // Required for std::decay_t, std::is_same_v, etc.

class CallbackBase {
public:
   virtual ~CallbackBase() = default;
   virtual void invokeVoid(const std::vector<std::any>& params) const = 0;
   virtual std::any invoke(const std::vector<std::any>& params) const = 0;
};

template<typename Ret, typename... Args>
class Callback : public CallbackBase {
public:
   explicit Callback(const std::function<Ret(Args...)>& func) : m_func(func) {}

   std::any invoke(const std::vector<std::any>& params) const override {
      if (params.size() != sizeof...(Args)) {
         throw std::runtime_error("Parameter count mismatch");
      }
      return std::any(invokeImpl(params, std::index_sequence_for<Args...>{}));
   }

   void invokeVoid(const std::vector<std::any>& params) const override {
      // Although invoke returns std::any, for void specializations invokeVoid is the primary path
      // This implementation is for non-void callbacks that might be invoked via invokeVoid
      invoke(params); // Call invoke and ignore the result
   }

private:
   std::function<Ret(Args...)> m_func;

   // Helper function to cast std::any to the target parameter type, handling common conversions
   template<typename TargetType>
   TargetType cast_any_to_target(const std::any& a) const {
      try {
         // Attempt direct cast first (handles exact type matches, including const/ref)
         return std::any_cast<TargetType>(a);
      }
      catch (const std::bad_any_cast&) {
         // If direct cast fails, try common conversions

         // Case: Target is std::string or const std::string&, any holds const char* (from literal)
         if constexpr (std::is_same_v<std::decay_t<TargetType>, std::string>) {
            if (a.type() == typeid(const char*)) {
               // Cast const char* out, construct a std::string, then static_cast to TargetType
               // This handles converting const char* to std::string or binding it to const std::string&
               return static_cast<TargetType>(std::string(std::any_cast<const char*>(a)));
            }
         }

         // Add other common conversions here if needed, e.g.:
         // Case: Target is arithmetic type (int, double, float etc.), any holds another arithmetic type
         if constexpr (std::is_arithmetic_v<std::decay_t<TargetType>>) {
            using RawTarget = std::decay_t<TargetType>;
            if (a.type() == typeid(int) && std::is_convertible_v<int, RawTarget>) {
               return static_cast<TargetType>(static_cast<RawTarget>(std::any_cast<int>(a)));
            }
            if (a.type() == typeid(double) && std::is_convertible_v<double, RawTarget>) {
               return static_cast<TargetType>(static_cast<RawTarget>(std::any_cast<double>(a)));
            }
            // ... add conversions for other arithmetic types if necessary
         }


         // If no specific conversion matched, re-throw the original bad_any_cast
         throw;
      }
   }


   template<size_t... Is>
   Ret invokeImpl(const std::vector<std::any>& params, std::index_sequence<Is...>) const {
      // Use the cast_any_to_target helper to get the correct argument types
      // std::tuple_element_t<Is, std::tuple<Args...>> gets the Ith type from the Args... pack
      return m_func(cast_any_to_target<std::tuple_element_t<Is, std::tuple<Args...>>>(params[Is])...);
   }
};

template<typename... Args>
class Callback<void, Args...> : public CallbackBase {
public:
   explicit Callback(const std::function<void(Args...)>& func) : m_func(func) {}

   std::any invoke(const std::vector<std::any>& params) const override {
      // For void return types, invoke just calls invokeVoid and returns an empty any
      invokeImpl(params, std::index_sequence_for<Args...>{});
      return std::any(); // Return empty any for void return
   }

   void invokeVoid(const std::vector<std::any>& params) const override {
      if (params.size() != sizeof...(Args)) {
         throw std::runtime_error("Parameter count mismatch");
      }
      invokeImpl(params, std::index_sequence_for<Args...>{});
   }

private:
   std::function<void(Args...)> m_func;

   // Helper function to cast std::any to the target parameter type, handling common conversions
   template<typename TargetType>
   TargetType cast_any_to_target(const std::any& a) const {
      try {
         // Attempt direct cast first (handles exact type matches, including const/ref)
         return std::any_cast<TargetType>(a);
      }
      catch (const std::bad_any_cast&) {
         // If direct cast fails, try common conversions

         // Case: Target is std::string or const std::string&, any holds const char* (from literal)
         if constexpr (std::is_same_v<std::decay_t<TargetType>, std::string>) {
            if (a.type() == typeid(const char*)) {
               // Cast const char* out, construct a std::string, then static_cast to TargetType
               return static_cast<TargetType>(std::string(std::any_cast<const char*>(a)));
            }
         }

         // Add other common conversions here if needed (e.g., arithmetic)
         if constexpr (std::is_arithmetic_v<std::decay_t<TargetType>>) {
            using RawTarget = std::decay_t<TargetType>;
            if (a.type() == typeid(int) && std::is_convertible_v<int, RawTarget>) {
               return static_cast<TargetType>(static_cast<RawTarget>(std::any_cast<int>(a)));
            }
            if (a.type() == typeid(double) && std::is_convertible_v<double, RawTarget>) {
               return static_cast<TargetType>(static_cast<RawTarget>(std::any_cast<double>(a)));
            }
            // ... add conversions for other arithmetic types if necessary
         }


         // If no specific conversion matched, re-throw
         throw;
      }
   }

   template<size_t... Is>
   void invokeImpl(const std::vector<std::any>& params, std::index_sequence<Is...>) const {
      // Use the cast_any_to_target helper to get the correct argument types
      // std::tuple_element_t<Is, std::tuple<Args...>> gets the Ith type from the Args... pack
      m_func(cast_any_to_target<std::tuple_element_t<Is, std::tuple<Args...>>>(params[Is])...);
   }
};

class RxCallbackManager {
public:
   template<typename Ret, typename... Args>
   void registerCallback(const int& id, std::function<Ret(Args...)> func) {
      m_callbacks[id] = std::make_unique<Callback<Ret, Args...>>(func);
   }

   template<typename Ret, typename... Args>
   void registerCallback(const int& id, Ret(*func)(Args...)) {
      registerCallback(id, std::function<Ret(Args...)>(func));
   }

   template<typename C, typename Ret, typename... Args>
   void registerCallback(const int& id, Ret(C::* method)(Args...), C* instance) {
      auto func = [instance, method](Args... args) -> Ret {
         return (instance->*method)(std::forward<Args>(args)...);
      };
      registerCallback(id, std::function<Ret(Args...)>(func));
   }

   template<typename C, typename Ret, typename... Args>
   void registerCallback(const int& id, Ret(C::* method)(Args...) const, const C* instance) {
      auto func = [instance, method](Args... args) -> Ret {
         return (instance->*method)(std::forward<Args>(args)...);
      };
      registerCallback(id, std::function<Ret(Args...)>(func));
   }

   template<typename Ret, typename... Args>
   Ret invoke(const int& id, Args&&... args) {
      auto it = m_callbacks.find(id);
      if (it == m_callbacks.end()) {
         throw std::runtime_error("Callback not found: " + std::to_string(id));
      }

      // Create vector of std::any from forwarded arguments
      std::vector<std::any> params{ std::forward<Args>(args)... };

      // Invoke the callback base method, which will handle casting inside
      std::any result = it->second->invoke(params);

      // Cast the result back to the expected return type
      // This assumes the caller expects the correct return type.
      if constexpr (!std::is_same_v<Ret, void>) {
         try {
            return std::any_cast<Ret>(result);
         }
         catch (const std::bad_any_cast&) {
            throw std::runtime_error("Failed to cast callback return type");
         }
      }
      else {
         // If Ret is void, invoke doesn't return a meaningful value, just check the callback type was indeed void
         if (!result.has_value()) return; // std::any() was returned as expected
         // If result had a value but Ret is void, something is wrong
         throw std::runtime_error("Invoked void callback but received non-empty return value");
      }
   }

   template<typename... Args>
   void invokeVoid(const int& id, Args&&... args) {
      auto it = m_callbacks.find(id);
      if (it == m_callbacks.end()) {
         throw std::runtime_error("Callback not found: " + std::to_string(id));
      }

      // Create vector of std::any from forwarded arguments
      std::vector<std::any> params{ std::forward<Args>(args)... };

      // Invoke the callback base method, which will handle casting inside
      it->second->invokeVoid(params);
   }

   bool hasCallback(const int& id) const {
      return m_callbacks.find(id) != m_callbacks.end();
   }

   void removeCallback(const int& id) {
      m_callbacks.erase(id);
   }

private:
   std::map<int, std::unique_ptr<CallbackBase>> m_callbacks;
};