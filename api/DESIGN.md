# Design considerations

This API is designed to be used in a multithread environment.

The public API functions are designed to be called on any thread, and
will do internal dispatching to the thread where activity needs to happen.

Many of the functions are designed to be used in an asynchronous manner,
where a function is called to initiate an activity, and a callback will
be called when the activity is completed.

Often, even the functions that look like simple functions (such as
information query functions) will need to jump between threads to perform
their function - which means that things may happen on other threads
between calls; writing "increment(x); increment(x)" is not a thread-safe
way to increment X by exactly two, since the increment function may have
jumped to another thread where other activity may have intervened between
the two calls.

# Implementation considerations

This should not directly concern users of the API, but may matter if one
wants to look at the implementation.

Many APIs are defined in terms of a "proxy object", which will do a blocking
dispatch of the function to another thread, and an "implementation object"
which will do the actual
work, but can only be created, invoked and destroyed on its "home thread".

Usually, the classes are named "xxxInterface" (in api/), "xxxProxy" and
"xxx" (not in api/). WebRTC users should only need to depend on the files
in api/. In many cases, the "xxxProxy" and "xxx" classes are subclasses
of "xxxInterface", but this property is an implementation feature only,
and should not be relied upon.

