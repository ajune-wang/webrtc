<?% config.freshness.owner = 'hta' %?>
<?% config.freshness.reviewed = '2021-05-31' %?>

# Basic concepts and primitives

## Time

Internally, time is represent using the [webrtc::Timestamp](1) class. This represents
time with a resolution of one microsecond, using a 64-bit integer, and provides
converters to milliseconds or seconds as needed.

The epoch is not specified (because we can't always know if the system clock is
correct), but whenever an absolute epoch is needed, the Unix time
epoch (Jan 1, 1970 at 0:00 GMT) is used.

NOTE: There are parts of the codebase that don't use Timestamp. They need to
be updated.

## Threads

All execution happens on an [rtc::Thread](2), which is a subclass of
[webrtc::TaskQueueBase](3). This offers primitives for posting tasks,
with or without delay, and may contain a SocketServer for processing I/O.

## Synchronization primitives

### PostTask and thread-guarded variables

The preferred method for synchronization is to post tasks between threads,
and to let each thread take care of its own variables. All variables in
classes intended to be used with multiple threads should therefore be
annotated with RTC_GUARDED_BY(thread).

For classes used with only one thread, the recommended pattern is to let
them own a webrtc::SequenceChecker (conventionally named sequence_checker_)
and let all variables be RTC_GUARDED_BY(sequence_checker_).

Member variables marked const do not need to be guarded, since they never
change. (But note that they may point to objects that can change!)

### Synchronization primitves to be used when needed

When it is absolutely necessary to let one thread wait for another thread
to do something, Thread::Invoke can be used. This function is DISCOURAGED,
since it leads to performance issues, but is currently still widespread.

When it is absolutely necessary to access one variable from multiple threads,
the webrtc::Mutex can be used. Such variables MUST be marked up with
RTC_GUARDED_BY(mutex), to allow static analysis that lessens the chance of
deadlocks or unintended consequences.

### Synchronization primitives that are being removed
The following non-exhaustive list of synchronization primitives are
in the (slow) process of being removed from the codebase.

* sigslot. Use [webrtc::CallbackList](4) instead, or, when there's only one
  signal consumer, a single std::function.
  
* AsyncInvoker.



[1](https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/api/units/timestamp.h;drc=b95d90b78a3491ef8e8aa0640dd521515ec881ca;l=29)
[2]https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/rtc_base/thread.h;drc=1107751b6f11c35259a1c5c8a0f716e227b7e3b4;l=194
[3]https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/api/task_queue/task_queue_base.h;drc=1107751b6f11c35259a1c5c8a0f716e227b7e3b4;l=25
[4]https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/rtc_base/callback_list.h;drc=54b91412de3f579a2d5ccdead6e04cc2cc5ca3a1;l=162
