# Design considerations

This API is designed to be used on top of a multithreaded runtime.

The public API functions are designed to be called from a single thread*
(the "client thread"), and can do internal dispatching to the thread
where activity needs to happen.

Many of the functions are designed to be used in an asynchronous manner,
where a function is called to initiate an activity, and a callback will
be called when the activity is completed, or a handler function will
be called on an observer object when interesting events happen.

(QUESTION: Will the callback be called on a random thread, or on the
client thread? Looking at the implementation, and the handlers up in
CHrome, it seems like "random")

Often, even functions that look like simple functions (such as
information query functions) will need to jump between threads to perform
their function - which means that things may happen on other threads
between calls; writing "increment(x); increment(x)" is not a safe
way to increment X by exactly two, since the increment function may have
jumped to another thread that already had a queue of things to handle,
causing large amounts of other activity to have intervened between
the two calls.

(*) The term "thread" is used here to denote any construct that guarantees
sequential execution - other names for such constructs are task runners
and sequenced task queues.

# Implementation considerations

This should not directly concern users of the API, but may matter if one
wants to look at how the WebRTC library is implemented.

Many APIs are defined in terms of a "proxy object", which will do a blocking
dispatch of the function to another thread, and an "implementation object"
which will do the actual
work, but can only be created, invoked and destroyed on its "home thread".

Usually, the classes are named "xxxInterface" (in api/), "xxxProxy" and
"xxx" (not in api/). WebRTC users should only need to depend on the files
in api/. In many cases, the "xxxProxy" and "xxx" classes are subclasses
of "xxxInterface", but this property is an implementation feature only,
and should not be relied upon.

