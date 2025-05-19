#pragma once

// Message ID definitions
enum MessageId {
   MSG_NONE = 0,
   MSG_UPDATE = 1,
   MSG_PROCESS = 2,
   MSG_CONTROL = 3
};

// Simple macro definitions
#define CALL_MESSAGE_0(q, id) (q)->QueueMessage(id)
#define CALL_MESSAGE_1(q, id, a1) (q)->QueueMessage(id, a1)
#define CALL_MESSAGE_2(q, id, a1, a2) (q)->QueueMessage(id, a1, a2)
#define CALL_MESSAGE_3(q, id, a1, a2, a3) (q)->QueueMessage(id, a1, a2, a3)
#define CALL_MESSAGE_4(q, id, a1, a2, a3, a4) (q)->QueueMessage(id, a1, a2, a3, a4)

// Arguments counting
#define VA_NARGS_IMPL(_1, _2, _3, _4, _5, N, ...) N
#define VA_NARGS(...) VA_NARGS_IMPL(__VA_ARGS__, 4, 3, 2, 1, 0)

// Concatenation helper
#define CONCAT_(a, b) a##b
#define CONCAT(a, b) CONCAT_(a, b)

// The main macro that selects the appropriate version
#define CALL_MESSAGE_IMPL(q, id, N, ...) CONCAT(CALL_MESSAGE_, N)(q, id, __VA_ARGS__)
#define CALL_MESSAGE(q, id, ...) CALL_MESSAGE_IMPL(q, id, VA_NARGS(__VA_ARGS__), __VA_ARGS__)

// Convenience macro for direct calls
#define MSG_CALL(q, id, ...) CALL_MESSAGE(q, id, __VA_ARGS__)