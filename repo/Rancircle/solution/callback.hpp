#pragma once

#include <functional>
#include <unordered_map>
#include <memory>
#include <any>
#include <tuple>
#include <vector>
#include <stdexcept>
#include <utility>
#include <type_traits>
#include <cstddef> // For size_t
#include <iostream> // For sample output
#include <string>

namespace detail {
   // Base function_traits template
   template<typename F>
   struct function_traits;

   // Specialization for lambdas and functors
   template<typename C>
   struct function_traits : public function_traits<decltype(&C::operator())> {};

   // Specialization for regular functions
   template<typename R, typename... Args>
   struct function_traits<R(Args...)> {
      using return_type = R;
      using args_tuple = std::tuple<std::decay_t<Args>...>;
      static constexpr size_t arity = sizeof...(Args);
   };

   // Specialization for function pointers
   template<typename R, typename... Args>
   struct function_traits<R(*)(Args...)> : public function_traits<R(Args...)> {};

   // Specialization for member function pointers (non-const)
   template<typename C, typename R, typename... Args>
   struct function_traits<R(C::*)(Args...)> {
      using return_type = R;
      using args_tuple = std::tuple<std::decay_t<Args>...>;
      static constexpr size_t arity = sizeof...(Args);
      // Note: Member functions implicitly have the class instance as the first "argument",
      // but function_traits for member pointers typically extract only the explicit arguments.
      // The lambda wrapper approach correctly handles the instance.
   };

   // Specialization for const member function pointers
   template<typename C, typename R, typename... Args>
   struct function_traits<R(C::*)(Args...) const> {
      using return_type = R;
      using args_tuple = std::tuple<std::decay_t<Args>...>;
      static constexpr size_t arity = sizeof...(Args);
   };

   // Helper to decay references
   template<typename F>
   struct function_traits<F&> : public function_traits<F> {};

   template<typename F>
   struct function_traits<F&&> : public function_traits<F> {};

   // Helper to unpack tuple types into template arguments (not directly used in final version, but pattern is similar)
   template<typename T, typename Tuple, size_t... I>
   T unpack_tuple_into_template(Tuple&& tuple, std::index_sequence<I...>);

   // Helper function to call a function with arguments from a std::vector<std::any>
   template<typename Func, typename... Args, size_t... I>
   auto call_with_any_vector_impl(Func& func, const std::vector<std::any>& args, std::index_sequence<I...>) {
      // Ensure correct number of arguments before casting
      if (args.size() != sizeof...(Args)) {
         throw std::runtime_error("Callback invocation failed: incorrect number of arguments.");
      }
      try {
         // Perform std::any_cast for each argument using the known Args types
         // std::any_cast requires an exact type match (after decay)
         return func(std::any_cast<Args>(args[I])...);
      }
      catch (const std::bad_any_cast& e) {
         // Provide a more specific error if any_cast fails
         throw std::runtime_error(std::string("Callback invocation failed: argument type mismatch. ") + e.what());
      }
      catch (const std::out_of_range& e) {
         // Should be caught by size check, but good practice
         throw std::runtime_error(std::string("Callback invocation failed: out of range argument access. ") + e.what());
      }
   }

   // Public entry point for calling with any vector
   template<typename R, typename... Args>
   auto call_with_any_vector(std::function<R(Args...)>& func, const std::vector<std::any>& args) {
      return call_with_any_vector_impl<std::function<R(Args...)>, Args...>(func, args, std::index_sequence_for<Args...>{});
   }
} // namespace detail


class ICallbackBase {
public:
   virtual ~ICallbackBase() = default;
   // This is the type-erased invocation interface
   // Takes arguments as a vector of std::any and returns result as std::any
   virtual std::any invoke(const std::vector<std::any>& args) = 0;
};

class CallbackManager {
public:
   using CallbackId = int;

   // Singleton access
   static std::shared_ptr<CallbackManager> getInstance() {
      static std::shared_ptr<CallbackManager> instance = std::make_shared<CallbackManager>();
      return instance;
   }

   // Private constructor for singleton
   CallbackManager() = default;

   // Derived class to hold the specific function and handle type-safe invocation internally
   // Specialized with the actual return type (R) and argument types (Args...) of the stored function
   template<typename R, typename... Args>
   class Callback final : public ICallbackBase {
   public:
      using CallbackFunction = std::function<R(Args...)>;

      // Constructor takes the specific function (std::function)
      Callback(CallbackFunction&& callback) : m_callback(std::move(callback)) {}
      Callback(const CallbackFunction& callback) : m_callback(callback) {}


      // Implement the type-erased invoke method required by ICallbackBase
      std::any invoke(const std::vector<std::any>& args) override {
         // Use the helper to unpack and cast std::any args to the function's expected types (Args...)
         // and call the stored function (m_callback).
         if constexpr (std::is_same_v<R, void>) {
            detail::call_with_any_vector<void, Args...>(m_callback, args);
            return std::any{}; // Return empty any for void
         }
         else {
            return std::any{ detail::call_with_any_vector<R, Args...>(m_callback, args) };
         }
      }

   private:
      CallbackFunction m_callback; // Stores the actual std::function object
   };

   // --- Registration Methods ---

   // Helper to create the specific Callback instance with correct template arguments (R and Args...)
   template<typename R, typename F, typename... Args>
   std::shared_ptr<ICallbackBase> create_specific_callback(F&& f) {
      // Construct the specific Callback<R, Args...> using the provided function object (f)
      return std::make_shared<Callback<R, Args...>>(std::forward<F>(f));
   }

   // Helper to unpack tuple arguments (obtained from function_traits) and call create_specific_callback
   template<typename R, typename F, typename ArgsTuple, size_t... I>
   std::shared_ptr<ICallbackBase> unpack_and_create(F&& f, std::index_sequence<I...>) {
      // Get the actual argument types from the ArgsTuple using the index sequence I...
      // and pass these types as variadic template arguments to create_specific_callback
      return create_specific_callback<R, F, std::tuple_element_t<I, ArgsTuple>...>(std::forward<F>(f));
   }


   // Generic registration for any callable (lambda, function pointer, functor)
   template<typename F>
   void registerCallback(CallbackId id, F&& f) {
      using traits = detail::function_traits<std::decay_t<F>>;
      using R = typename traits::return_type;
      using ArgsTuple = typename traits::args_tuple; // The types of arguments in a std::tuple
      constexpr size_t Arity = traits::arity;       // Number of arguments

      // Use the helper to unpack the argument types from the tuple and create the specific Callback instance
      m_callbacks[id] = unpack_and_create<R, F, ArgsTuple>(
         std::forward<F>(f),              // The function/callable to register
         std::make_index_sequence<Arity>{} // Generate sequence 0, 1, ..., Arity-1
      );
   }

   // Registration for member function pointers (non-const)
   template<typename T, typename R, typename... Args>
   void registerCallback(CallbackId id, R(T::* memberFunc)(Args...), T* instance) {
      // Create a lambda that wraps the member function call and capture the instance
      // This lambda has the signature R(Args...) matching the member function's parameters
      auto lambda_wrapper = [instance, memberFunc](Args... args) -> R {
         if (!instance) throw std::runtime_error("Cannot invoke member function: instance is null.");
         return (instance->*memberFunc)(std::forward<Args>(args)...);
      };
      // Register the lambda wrapper using the generic registration path
      registerCallback(id, lambda_wrapper);
   }

   // Registration for const member function pointers
   template<typename T, typename R, typename... Args>
   void registerCallback(CallbackId id, R(T::* memberFunc)(Args...) const, T* instance) {
      // Create a lambda that wraps the const member function call and capture the instance
       // This lambda has the signature R(Args...) matching the member function's parameters
      auto lambda_wrapper = [instance, memberFunc](Args... args) -> R {
         if (!instance) throw std::runtime_error("Cannot invoke member function: instance is null.");
         return (instance->*memberFunc)(std::forward<Args>(args)...);
      };
      // Register the lambda wrapper using the generic registration path
      registerCallback(id, lambda_wrapper);
   }


   // --- Invocation Methods ---

   // Invoke a registered callback that returns a value.
   // Caller must specify the expected return type R
   // and provide arguments (Args&&... args) that are compatible with the registered callback's
   // parameters for std::any_cast.
   template<typename R, typename... Args>
   R invoke(CallbackId id, Args&&... args) {
      auto it = m_callbacks.find(id);
      if (it == m_callbacks.end()) {
         // Use a more informative error message
         throw std::runtime_error("Callback not found for id: " + std::to_string(id));
      }

      // Create a vector of std::any from the arguments provided by the caller
      // The Callback object's invoke method will attempt to cast these to the types
      // it expects.
      std::vector<std::any> anyArgs{ std::forward<Args>(args)... };

      // Call the type-erased invoke method on the stored callback object
      // This call handles the unpacking and casting of anyArgs to the function's
      // original arguments and calls the stored function.
      std::any result_any = it->second->invoke(anyArgs);

      // Try to cast the result (which is in a std::any) back to the caller's expected return type R
      // This is where a mismatch in return type between the registered function and
      // the caller's expectation will cause a std::bad_any_cast exception.
      if constexpr (std::is_same_v<R, void>) {
         // If R is void, the result_any is expected to be empty.
         // The call to it->second->invoke() for a void callback is already handled
         // to return std::any{}. No further action needed for the result.
      }
      else {
         try {
            // Attempt the final cast to the type R requested by the caller
            return std::any_cast<R>(result_any);
         }
         catch (const std::bad_any_cast& e) {
            // Catch cast error and provide a user-friendly message
            throw std::runtime_error(std::string("Callback invocation failed: return type mismatch. Expected ") + typeid(R).name() + ", but callback returned incompatible type.");
         }
      }
      // If R is void, control reaches here and the function returns implicitly.
   }

   // Overload for invoking a callback that returns void.
   // The caller does not need to specify a return type template parameter.
   template<typename... Args>
   void invoke(CallbackId id, Args&&... args) {
      // Call the templated invoke method specifying void as the expected return type
      invoke<void, Args...>(id, std::forward<Args>(args)...);
   }

private:
   // The map storing the registered callbacks, type-erased via ICallbackBase
   std::unordered_map<CallbackId, std::shared_ptr<ICallbackBase>> m_callbacks;

   // Disable copy and assignment for singleton
   CallbackManager(const CallbackManager&) = delete;
   CallbackManager& operator=(const CallbackManager&) = delete;
};