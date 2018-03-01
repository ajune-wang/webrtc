/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "ARDAppClientNative.h"

#import "WebRTC/RTCAudioTrack.h"
#import "WebRTC/RTCCameraVideoCapturer.h"
#import "WebRTC/RTCConfiguration.h"
#import "WebRTC/RTCFileLogger.h"
#import "WebRTC/RTCFileVideoCapturer.h"
#import "WebRTC/RTCIceServer.h"
#import "WebRTC/RTCLogging.h"
#import "WebRTC/RTCMediaConstraints.h"
#import "WebRTC/RTCMediaStream.h"
#import "WebRTC/RTCRtpSender.h"
#import "WebRTC/RTCTracing.h"
#import "WebRTC/RTCVideoCodecFactory.h"
#import "WebRTC/RTCVideoSource.h"
#import "WebRTC/RTCVideoTrack.h"

#import "ARDAppEngineClient.h"
#import "ARDJoinResponse.h"
#import "ARDMessageResponse.h"
#import "ARDSettingsModel.h"
#import "ARDSignalingMessage.h"
#import "ARDTURNClient+Internal.h"
#import "ARDUtilities.h"
#import "ARDWebSocketChannel.h"
#import "NSString+StdString.h"
#import "RTCConfiguration+Private.h"
#import "RTCIceCandidate+JSON.h"
#import "RTCIceCandidate+Private.h"
#import "RTCLegacyStatsReport+Private.h"
#import "RTCMediaConstraints+Private.h"
#import "RTCPeerConnection+Private.h"
#import "RTCSessionDescription+JSON.h"
#import "RTCSessionDescription+Private.h"
#import "RTCVideoRendererAdapter+Private.h"
#import "RTCVideoSource+Private.h"

#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "api/audio_codecs/builtin_audio_encoder_factory.h"
#include "api/jsepicecandidate.h"
#include "api/peerconnectioninterface.h"
#include "modules/audio_device/include/audio_device.h"
#include "modules/audio_processing/include/audio_processing.h"
#include "rtc_base/scoped_ref_ptr.h"
#include "sdk/objc/Framework/Native/api/video_decoder_factory.h"
#include "sdk/objc/Framework/Native/api/video_encoder_factory.h"
#include "sdk/objc/Framework/Native/api/video_renderer.h"

static NSString *const kARDIceServerRequestUrl = @"https://appr.tc/params";

static NSString *const kARDAppClientErrorDomain = @"ARDAppClient";
static NSInteger const kARDAppClientErrorUnknown = -1;
static NSInteger const kARDAppClientErrorRoomFull = -2;
static NSInteger const kARDAppClientErrorCreateSDP = -3;
static NSInteger const kARDAppClientErrorSetSDP = -4;
static NSInteger const kARDAppClientErrorInvalidClient = -5;
static NSInteger const kARDAppClientErrorInvalidRoom = -6;
static NSString *const kARDMediaStreamId = @"ARDAMS";
static NSString *const kARDAudioTrackId = @"ARDAMSa0";
static NSString *const kARDVideoTrackId = @"ARDAMSv0";
static NSString *const kARDVideoTrackKind = @"video";

// TODO(tkchin): Add these as UI options.
static BOOL const kARDAppClientEnableTracing = NO;
static BOOL const kARDAppClientEnableRtcEventLog = YES;
static int64_t const kARDAppClientAecDumpMaxSizeInBytes = 5e6;      // 5 MB.
static int64_t const kARDAppClientRtcEventLogMaxSizeInBytes = 5e6;  // 5 MB.
static int const kKbpsMultiplier = 1000;

@interface ARDAppClientNative ()

- (void)peerConnectionOnSignalingChange:(webrtc::PeerConnectionInterface::SignalingState)newState;
- (void)peerConnectionOnAddStream:(rtc::scoped_refptr<webrtc::MediaStreamInterface>)stream;
- (void)peerConnectionOnRemoveStream:(rtc::scoped_refptr<webrtc::MediaStreamInterface>)stream;
- (void)peerConnectionOnRenegotiationNeeded;
- (void)peerConnectionOnIceConnectionChange:
        (webrtc::PeerConnectionInterface::IceConnectionState)newState;
- (void)peerConnectionOnIceGatheringChange:
        (webrtc::PeerConnectionInterface::IceGatheringState)newState;
- (void)peerConnectionOnIceCandidate:(RTCIceCandidate *)candidate;
- (void)peerConnectionOnIceCandidatesRemoved:(const std::vector<cricket::Candidate> &)candidates;
- (void)peerConnectionOnDataChannel:(rtc::scoped_refptr<webrtc::DataChannelInterface>)dataChannel;

- (void)createSessionDescriptionOnSuccess:(webrtc::SessionDescriptionInterface *)desc;
- (void)createSessionDescriptionOnFailure:(const std::string &)error;

- (void)setSessionDescriptionOnSuccess;
- (void)setSessionDescriptionOnFailure:(const std::string &)error;

- (void)statsOnComplete:(const webrtc::StatsReports &)reports;

@end

// We need a proxy to NSTimer because it causes a strong retain cycle. When
// using the proxy, |invalidate| must be called before it properly deallocs.
@interface ARDTimerProxy : NSObject

- (instancetype)initWithInterval:(NSTimeInterval)interval
                         repeats:(BOOL)repeats
                    timerHandler:(void (^)(void))timerHandler;
- (void)invalidate;

@end

@implementation ARDTimerProxy {
  NSTimer *_timer;
  void (^_timerHandler)(void);
}

- (instancetype)initWithInterval:(NSTimeInterval)interval
                         repeats:(BOOL)repeats
                    timerHandler:(void (^)(void))timerHandler {
  NSParameterAssert(timerHandler);
  if (self = [super init]) {
    _timerHandler = timerHandler;
    _timer = [NSTimer scheduledTimerWithTimeInterval:interval
                                              target:self
                                            selector:@selector(timerDidFire:)
                                            userInfo:nil
                                             repeats:repeats];
  }
  return self;
}

- (void)invalidate {
  [_timer invalidate];
}

- (void)timerDidFire:(NSTimer *)timer {
  _timerHandler();
}

@end

namespace webrtc {

// Peer connection observer

class AppClientPeerConnectionObserver : public PeerConnectionObserver {
 public:
  AppClientPeerConnectionObserver(ARDAppClientNative *appClient);
  virtual ~AppClientPeerConnectionObserver();

  void OnSignalingChange(PeerConnectionInterface::SignalingState new_state) override;

  void OnAddStream(rtc::scoped_refptr<MediaStreamInterface> stream) override;

  void OnRemoveStream(rtc::scoped_refptr<MediaStreamInterface> stream) override;

  void OnDataChannel(rtc::scoped_refptr<DataChannelInterface> data_channel) override;

  void OnRenegotiationNeeded() override;

  void OnIceConnectionChange(PeerConnectionInterface::IceConnectionState new_state) override;

  void OnIceGatheringChange(PeerConnectionInterface::IceGatheringState new_state) override;

  void OnIceCandidate(const IceCandidateInterface *candidate) override;

  void OnIceCandidatesRemoved(const std::vector<cricket::Candidate> &candidates) override;

 private:
  __weak ARDAppClientNative *app_client_;
};

AppClientPeerConnectionObserver::AppClientPeerConnectionObserver(ARDAppClientNative *appClient) {
  app_client_ = appClient;
}

AppClientPeerConnectionObserver::~AppClientPeerConnectionObserver() {
  app_client_ = nil;
}

void AppClientPeerConnectionObserver::OnSignalingChange(
    PeerConnectionInterface::SignalingState new_state) {
  [app_client_ peerConnectionOnSignalingChange:new_state];
}

void AppClientPeerConnectionObserver::OnAddStream(rtc::scoped_refptr<MediaStreamInterface> stream) {
  [app_client_ peerConnectionOnAddStream:stream];
}

void AppClientPeerConnectionObserver::OnRemoveStream(
    rtc::scoped_refptr<MediaStreamInterface> stream) {
  [app_client_ peerConnectionOnRemoveStream:stream];
}

void AppClientPeerConnectionObserver::OnDataChannel(
    rtc::scoped_refptr<DataChannelInterface> data_channel) {
  [app_client_ peerConnectionOnDataChannel:data_channel];
}

void AppClientPeerConnectionObserver::OnRenegotiationNeeded() {
  [app_client_ peerConnectionOnRenegotiationNeeded];
}

void AppClientPeerConnectionObserver::OnIceConnectionChange(
    PeerConnectionInterface::IceConnectionState new_state) {
  [app_client_ peerConnectionOnIceConnectionChange:new_state];
}

void AppClientPeerConnectionObserver::OnIceGatheringChange(
    PeerConnectionInterface::IceGatheringState new_state) {
  [app_client_ peerConnectionOnIceGatheringChange:new_state];
}

void AppClientPeerConnectionObserver::OnIceCandidate(const IceCandidateInterface *candidate) {
  RTCIceCandidate *iceCandidate = [[RTCIceCandidate alloc] initWithNativeCandidate:candidate];
  [app_client_ peerConnectionOnIceCandidate:iceCandidate];
}

void AppClientPeerConnectionObserver::OnIceCandidatesRemoved(
    const std::vector<cricket::Candidate> &candidates) {
  [app_client_ peerConnectionOnIceCandidatesRemoved:candidates];
}

// Create session description observer

class AppClientCreateSessionDescriptionObserver : public CreateSessionDescriptionObserver {
 public:
  AppClientCreateSessionDescriptionObserver(ARDAppClientNative *appClient) {
    app_client_ = appClient;
  }

  ~AppClientCreateSessionDescriptionObserver() { app_client_ = nil; }

  void OnSuccess(SessionDescriptionInterface *desc) override {
    [app_client_ createSessionDescriptionOnSuccess:desc];
  }

  void OnFailure(const std::string &error) override {
    [app_client_ createSessionDescriptionOnFailure:error];
  }

 private:
  __weak ARDAppClientNative *app_client_;
};

// Set session description observer

class AppClientSetSessionDescriptionObserver : public SetSessionDescriptionObserver {
 public:
  AppClientSetSessionDescriptionObserver(ARDAppClientNative *appClient) { app_client_ = appClient; }

  ~AppClientSetSessionDescriptionObserver() { app_client_ = nil; }

  void OnSuccess() override { [app_client_ setSessionDescriptionOnSuccess]; }

  void OnFailure(const std::string &error) override {
    [app_client_ setSessionDescriptionOnFailure:error];
  }

 private:
  __weak ARDAppClientNative *app_client_;
};

// Stats observer

class AppClientStatsObserver : public StatsObserver {
 public:
  AppClientStatsObserver(ARDAppClientNative *appClient) { app_client_ = appClient; }

  ~AppClientStatsObserver() { app_client_ = nil; }

  void OnComplete(const StatsReports &reports) override { [app_client_ statsOnComplete:reports]; }

 private:
  __weak ARDAppClientNative *app_client_;
};

}  // namespace webrtc

@implementation ARDAppClientNative {
  RTCFileLogger *_fileLogger;
  ARDTimerProxy *_statsTimer;
  ARDSettingsModel *_settings;
  RTCVideoSource *_source;

  std::unique_ptr<rtc::Thread> _networkThread;
  std::unique_ptr<rtc::Thread> _workerThread;
  std::unique_ptr<rtc::Thread> _signalingThread;
  rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> _factory;
  rtc::scoped_refptr<webrtc::PeerConnectionInterface> _peerConnection;
  rtc::scoped_refptr<webrtc::VideoTrackInterface> _localVideoTrack;
  rtc::scoped_refptr<webrtc::VideoTrackInterface> _remoteVideoTrack;
  std::unique_ptr<webrtc::AppClientPeerConnectionObserver> _observer;
  std::unique_ptr<webrtc::MediaConstraints> _nativeConstraints;
  std::unique_ptr<rtc::VideoSinkInterface<webrtc::VideoFrame>> _nativeRenderer;
}

@synthesize shouldGetStats = _shouldGetStats;
@synthesize state = _state;
@synthesize delegate = _delegate;
@synthesize roomServerClient = _roomServerClient;
@synthesize channel = _channel;
@synthesize loopbackChannel = _loopbackChannel;
@synthesize turnClient = _turnClient;
@synthesize messageQueue = _messageQueue;
@synthesize isTurnComplete = _isTurnComplete;
@synthesize hasReceivedSdp = _hasReceivedSdp;
@synthesize roomId = _roomId;
@synthesize clientId = _clientId;
@synthesize isInitiator = _isInitiator;
@synthesize iceServers = _iceServers;
@synthesize webSocketURL = _websocketURL;
@synthesize webSocketRestURL = _websocketRestURL;
@synthesize defaultPeerConnectionConstraints = _defaultPeerConnectionConstraints;
@synthesize isLoopback = _isLoopback;

- (instancetype)init {
  return [self initWithDelegate:nil];
}

- (instancetype)initWithDelegate:(id<ARDAppClientDelegate>)delegate {
  if (self = [super init]) {
    _networkThread = rtc::Thread::CreateWithSocketServer();
    BOOL result = _networkThread->Start();
    NSAssert(result, @"Failed to start network thread.");

    _workerThread = rtc::Thread::Create();
    result = _workerThread->Start();
    NSAssert(result, @"Failed to start worker thread.");

    _signalingThread = rtc::Thread::Create();
    result = _signalingThread->Start();
    NSAssert(result, @"Failed to start signaling thread.");

    _roomServerClient = [[ARDAppEngineClient alloc] init];
    _delegate = delegate;
    NSURL *turnRequestURL = [NSURL URLWithString:kARDIceServerRequestUrl];
    _turnClient = [[ARDTURNClient alloc] initWithURL:turnRequestURL];
    [self configure];
  }
  return self;
}

// TODO(tkchin): Provide signaling channel factory interface so we can recreate
// channel if we need to on network failure. Also, make this the default public
// constructor.
- (instancetype)initWithRoomServerClient:(id<ARDRoomServerClient>)rsClient
                        signalingChannel:(id<ARDSignalingChannel>)channel
                              turnClient:(id<ARDTURNClient>)turnClient
                                delegate:(id<ARDAppClientDelegate>)delegate {
  NSParameterAssert(rsClient);
  NSParameterAssert(channel);
  NSParameterAssert(turnClient);
  if (self = [super init]) {
    _roomServerClient = rsClient;
    _channel = channel;
    _turnClient = turnClient;
    _delegate = delegate;
    [self configure];
  }
  return self;
}

- (void)configure {
  _messageQueue = [NSMutableArray array];
  _iceServers = [NSMutableArray array];
  _fileLogger = [[RTCFileLogger alloc] init];
  [_fileLogger start];
}

- (void)dealloc {
  self.shouldGetStats = NO;
  [self disconnect];
}

- (void)setShouldGetStats:(BOOL)shouldGetStats {
  if (_shouldGetStats == shouldGetStats) {
    return;
  }
  if (shouldGetStats) {
    __weak ARDAppClientNative *weakSelf = self;
    _statsTimer = [[ARDTimerProxy alloc]
        initWithInterval:1
                 repeats:YES
            timerHandler:^{
              rtc::scoped_refptr<webrtc::AppClientStatsObserver> observer(
                  new rtc::RefCountedObject<webrtc::AppClientStatsObserver>(self));
              _peerConnection->GetStats(
                  observer, nullptr, webrtc::PeerConnectionInterface::kStatsOutputLevelDebug);
            }];
  } else {
    [_statsTimer invalidate];
    _statsTimer = nil;
  }
  _shouldGetStats = shouldGetStats;
}

- (void)setState:(ARDAppClientState)state {
  if (_state == state) {
    return;
  }
  _state = state;
  [_delegate appClient:self didChangeState:_state];
}

- (void)connectToRoomWithId:(NSString *)roomId
                   settings:(ARDSettingsModel *)settings
                 isLoopback:(BOOL)isLoopback {
  NSParameterAssert(roomId.length);
  NSParameterAssert(_state == kARDAppClientStateDisconnected);
  _settings = settings;
  _isLoopback = isLoopback;
  self.state = kARDAppClientStateConnecting;

  RTCDefaultVideoDecoderFactory *decoderFactory = [[RTCDefaultVideoDecoderFactory alloc] init];
  RTCDefaultVideoEncoderFactory *encoderFactory = [[RTCDefaultVideoEncoderFactory alloc] init];
  encoderFactory.preferredCodec = [settings currentVideoCodecSettingFromStore];

  std::unique_ptr<webrtc::VideoDecoderFactory> videoDecoderFactory =
      webrtc::ObjCToNativeVideoDecoderFactory(decoderFactory);
  std::unique_ptr<webrtc::VideoEncoderFactory> videoEncoderFactory =
      webrtc::ObjCToNativeVideoEncoderFactory(encoderFactory);

  _factory = webrtc::CreatePeerConnectionFactory(_networkThread.get(),
                                                 _workerThread.get(),
                                                 _signalingThread.get(),
                                                 nullptr,  // audioDeviceModule
                                                 webrtc::CreateBuiltinAudioEncoderFactory(),
                                                 webrtc::CreateBuiltinAudioDecoderFactory(),
                                                 std::move(videoEncoderFactory),
                                                 std::move(videoDecoderFactory),
                                                 nullptr,  // audio mixer
                                                 nullptr /* audioProcessingModule */);

#if defined(WEBRTC_IOS)
  if (kARDAppClientEnableTracing) {
    NSString *filePath = [self documentsFilePathForFileName:@"webrtc-trace.txt"];
    RTCStartInternalCapture(filePath);
  }
#endif

  // Request TURN.
  __weak ARDAppClientNative *weakSelf = self;
  [_turnClient requestServersWithCompletionHandler:^(NSArray *turnServers, NSError *error) {
    if (error) {
      RTCLogError("Error retrieving TURN servers: %@", error.localizedDescription);
    }
    ARDAppClientNative *strongSelf = weakSelf;
    [strongSelf.iceServers addObjectsFromArray:turnServers];
    strongSelf.isTurnComplete = YES;
    [strongSelf startSignalingIfReady];
  }];

  // Join room on room server.
  [_roomServerClient joinRoomWithRoomId:roomId
                             isLoopback:isLoopback
                      completionHandler:^(ARDJoinResponse *response, NSError *error) {
                        ARDAppClientNative *strongSelf = weakSelf;
                        if (error) {
                          [strongSelf.delegate appClient:strongSelf didError:error];
                          return;
                        }
                        NSError *joinError =
                            [[strongSelf class] errorForJoinResultType:response.result];
                        if (joinError) {
                          RTCLogError(@"Failed to join room:%@ on room server.", roomId);
                          [strongSelf disconnect];
                          [strongSelf.delegate appClient:strongSelf didError:joinError];
                          return;
                        }
                        RTCLog(@"Joined room:%@ on room server.", roomId);
                        strongSelf.roomId = response.roomId;
                        strongSelf.clientId = response.clientId;
                        strongSelf.isInitiator = response.isInitiator;
                        for (ARDSignalingMessage *message in response.messages) {
                          if (message.type == kARDSignalingMessageTypeOffer ||
                              message.type == kARDSignalingMessageTypeAnswer) {
                            strongSelf.hasReceivedSdp = YES;
                            [strongSelf.messageQueue insertObject:message atIndex:0];
                          } else {
                            [strongSelf.messageQueue addObject:message];
                          }
                        }
                        strongSelf.webSocketURL = response.webSocketURL;
                        strongSelf.webSocketRestURL = response.webSocketRestURL;
                        [strongSelf registerWithColliderIfReady];
                        [strongSelf startSignalingIfReady];
                      }];
}

- (void)disconnect {
  if (_state == kARDAppClientStateDisconnected) {
    return;
  }
  if (self.hasJoinedRoomServerRoom) {
    [_roomServerClient leaveRoomWithRoomId:_roomId clientId:_clientId completionHandler:nil];
  }
  if (_channel) {
    if (_channel.state == kARDSignalingChannelStateRegistered) {
      // Tell the other client we're hanging up.
      ARDByeMessage *byeMessage = [[ARDByeMessage alloc] init];
      [_channel sendMessage:byeMessage];
    }
    // Disconnect from collider.
    _channel = nil;
  }
  _clientId = nil;
  _roomId = nil;
  _isInitiator = NO;
  _hasReceivedSdp = NO;
  _messageQueue = [NSMutableArray array];
  _localVideoTrack = nil;
#if defined(WEBRTC_IOS)
  _factory->StopAecDump();
  _peerConnection->StopRtcEventLog();
#endif
  _peerConnection->Close();
  _peerConnection = nullptr;
  self.state = kARDAppClientStateDisconnected;
#if defined(WEBRTC_IOS)
  if (kARDAppClientEnableTracing) {
    RTCStopInternalCapture();
  }
#endif
}

#pragma mark - ARDSignalingChannelDelegate

- (void)channel:(id<ARDSignalingChannel>)channel didReceiveMessage:(ARDSignalingMessage *)message {
  switch (message.type) {
    case kARDSignalingMessageTypeOffer:
    case kARDSignalingMessageTypeAnswer:
      // Offers and answers must be processed before any other message, so we
      // place them at the front of the queue.
      _hasReceivedSdp = YES;
      [_messageQueue insertObject:message atIndex:0];
      break;
    case kARDSignalingMessageTypeCandidate:
    case kARDSignalingMessageTypeCandidateRemoval:
      [_messageQueue addObject:message];
      break;
    case kARDSignalingMessageTypeBye:
      // Disconnects can be processed immediately.
      [self processSignalingMessage:message];
      return;
  }
  [self drainMessageQueueIfReady];
}

- (void)channel:(id<ARDSignalingChannel>)channel didChangeState:(ARDSignalingChannelState)state {
  switch (state) {
    case kARDSignalingChannelStateOpen:
      break;
    case kARDSignalingChannelStateRegistered:
      break;
    case kARDSignalingChannelStateClosed:
    case kARDSignalingChannelStateError:
      // TODO(tkchin): reconnection scenarios. Right now we just disconnect
      // completely if the websocket connection fails.
      [self disconnect];
      break;
  }
}

#pragma mark - PeerConnectionObserver
// Callbacks for this observer occur on non-main thread and need to be
// dispatched back to main queue as needed.

- (void)peerConnectionOnSignalingChange:(webrtc::PeerConnectionInterface::SignalingState)newState {
  RTCLog(@"Signaling state changed: %ld", (long)newState);
}

- (void)peerConnectionOnAddStream:(rtc::scoped_refptr<webrtc::MediaStreamInterface>)stream {
  dispatch_async(dispatch_get_main_queue(), ^{
    webrtc::VideoTrackVector videoTracks = stream->GetVideoTracks();
    webrtc::AudioTrackVector audioTracks = stream->GetAudioTracks();

    RTCLog(
        @"Received %lu video tracks and %lu audio tracks", videoTracks.size(), audioTracks.size());
    if (videoTracks.size() && _remoteVideoTrack != videoTracks.front()) {
      id<RTCVideoRenderer> renderer = [_delegate remoteVideoRenderer];
      _nativeRenderer = webrtc::ObjCToNativeVideoRenderer(renderer);

      if (_remoteVideoTrack) {
        _remoteVideoTrack->RemoveSink(_nativeRenderer.get());
      }

      _remoteVideoTrack = videoTracks.front();
      _remoteVideoTrack->AddOrUpdateSink(_nativeRenderer.get(), rtc::VideoSinkWants());

      [_delegate appClientDidReceiveRemoteVideoTrack:self];
    }
  });
}

- (void)peerConnectionOnRemoveStream:(rtc::scoped_refptr<webrtc::MediaStreamInterface>)stream {
  RTCLog(@"Stream was removed.");
}

- (void)peerConnectionOnRenegotiationNeeded {
  RTCLog(@"WARNING: Renegotiation needed but unimplemented.");
}

- (void)peerConnectionOnIceConnectionChange:
        (webrtc::PeerConnectionInterface::IceConnectionState)newState {
  RTCLog(@"ICE state changed: %ld", (long)newState);
  dispatch_async(dispatch_get_main_queue(), ^{
    RTCIceConnectionState state = [RTCPeerConnection iceConnectionStateForNativeState:newState];
    [_delegate appClient:self didChangeConnectionState:state];
  });
}

- (void)peerConnectionOnIceGatheringChange:
        (webrtc::PeerConnectionInterface::IceGatheringState)newState {
  RTCLog(@"ICE gathering state changed: %ld", (long)newState);
}

- (void)peerConnectionOnIceCandidate:(RTCIceCandidate *)candidate {
  dispatch_async(dispatch_get_main_queue(), ^{
    ARDICECandidateMessage *message = [[ARDICECandidateMessage alloc] initWithCandidate:candidate];
    [self sendSignalingMessage:message];
  });
}

- (void)peerConnectionOnIceCandidatesRemoved:(const std::vector<cricket::Candidate> &)candidates {
  dispatch_async(dispatch_get_main_queue(), ^{
    NSMutableArray *ice_candidates = [NSMutableArray arrayWithCapacity:candidates.size()];
    for (const auto &candidate : candidates) {
      std::unique_ptr<webrtc::JsepIceCandidate> candidate_wrapper(
          new webrtc::JsepIceCandidate(candidate.transport_name(), -1, candidate));
      RTCIceCandidate *ice_candidate =
          [[RTCIceCandidate alloc] initWithNativeCandidate:candidate_wrapper.get()];
      [ice_candidates addObject:ice_candidate];
    }

    ARDICECandidateRemovalMessage *message =
        [[ARDICECandidateRemovalMessage alloc] initWithRemovedCandidates:ice_candidates];
    [self sendSignalingMessage:message];
  });
}

- (void)peerConnectionOnDataChannel:(rtc::scoped_refptr<webrtc::DataChannelInterface>)dataChannel {
}

#pragma mark - Create session description observer
// Callbacks for this observer occur on non-main thread and need to be
// dispatched back to main queue as needed.

- (void)createSessionDescriptionOnSuccess:(webrtc::SessionDescriptionInterface *)desc {
  dispatch_async(dispatch_get_main_queue(), ^{
    rtc::scoped_refptr<webrtc::AppClientSetSessionDescriptionObserver> observer(
        new rtc::RefCountedObject<webrtc::AppClientSetSessionDescriptionObserver>(self));
    _peerConnection->SetLocalDescription(observer, desc);

    RTCSessionDescription *sdp = [[RTCSessionDescription alloc] initWithNativeDescription:desc];
    ARDSessionDescriptionMessage *message =
        [[ARDSessionDescriptionMessage alloc] initWithDescription:sdp];
    [self sendSignalingMessage:message];
    [self setMaxBitrateForPeerConnectionVideoSender];
  });
}

- (void)createSessionDescriptionOnFailure:(const std::string &)error {
  dispatch_async(dispatch_get_main_queue(), ^{
    RTCLogError(@"Failed to create session description.");
    [self disconnect];
    NSDictionary *userInfo = @{
      NSLocalizedDescriptionKey : @"Failed to create session description.",
    };
    NSError *sdpError = [[NSError alloc] initWithDomain:kARDAppClientErrorDomain
                                                   code:kARDAppClientErrorCreateSDP
                                               userInfo:userInfo];
    [_delegate appClient:self didError:sdpError];
  });
}

#pragma mark - Set session description observer

- (void)setSessionDescriptionOnSuccess {
  dispatch_async(dispatch_get_main_queue(), ^{
    // If we're answering and we've just set the remote offer we need to create
    // an answer and set the local description.
    if (!_isInitiator && !_peerConnection->local_description()) {
      RTCMediaConstraints *constraints = [self defaultAnswerConstraints];

      rtc::scoped_refptr<webrtc::AppClientCreateSessionDescriptionObserver> observer(
          new rtc::RefCountedObject<webrtc::AppClientCreateSessionDescriptionObserver>(self));
      _peerConnection->CreateAnswer(observer, constraints.nativeConstraints.get());
    }
  });
}

- (void)setSessionDescriptionOnFailure:(const std::string &)error {
  dispatch_async(dispatch_get_main_queue(), ^{
    RTCLogError(@"Failed to set session description.");
    [self disconnect];
    NSDictionary *userInfo = @{
      NSLocalizedDescriptionKey : @"Failed to set session description.",
    };
    NSError *sdpError = [[NSError alloc] initWithDomain:kARDAppClientErrorDomain
                                                   code:kARDAppClientErrorSetSDP
                                               userInfo:userInfo];
    [_delegate appClient:self didError:sdpError];
  });
}

#pragma mark - Stats observer

- (void)statsOnComplete:(const webrtc::StatsReports &)reports {
  dispatch_async(dispatch_get_main_queue(), ^{
    NSMutableArray *stats = [NSMutableArray arrayWithCapacity:reports.size()];
    for (const auto *report : reports) {
      RTCLegacyStatsReport *statsReport =
          [[RTCLegacyStatsReport alloc] initWithNativeReport:*report];
      [stats addObject:statsReport];
    }

    [_delegate appClient:self didGetStats:stats];
  });
}

#pragma mark - Private

#if defined(WEBRTC_IOS)

- (NSString *)documentsFilePathForFileName:(NSString *)fileName {
  NSParameterAssert(fileName.length);
  NSArray *paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
  NSString *documentsDirPath = paths.firstObject;
  NSString *filePath = [documentsDirPath stringByAppendingPathComponent:fileName];
  return filePath;
}

#endif

- (BOOL)hasJoinedRoomServerRoom {
  return _clientId.length;
}

// Begins the peer connection connection process if we have both joined a room
// on the room server and tried to obtain a TURN server. Otherwise does nothing.
// A peer connection object will be created with a stream that contains local
// audio and video capture. If this client is the caller, an offer is created as
// well, otherwise the client will wait for an offer to arrive.
- (void)startSignalingIfReady {
  if (!_isTurnComplete || !self.hasJoinedRoomServerRoom) {
    return;
  }
  self.state = kARDAppClientStateConnected;

  // Create peer connection.
  RTCMediaConstraints *constraints = [self defaultPeerConnectionConstraints];
  RTCConfiguration *configuration = [[RTCConfiguration alloc] init];
  configuration.iceServers = _iceServers;

  std::unique_ptr<webrtc::PeerConnectionInterface::RTCConfiguration> config(
      [configuration createNativeConfiguration]);
  _observer.reset(new webrtc::AppClientPeerConnectionObserver(self));
  _nativeConstraints = constraints.nativeConstraints;
  CopyConstraintsIntoRtcConfiguration(_nativeConstraints.get(), config.get());
  _peerConnection = _factory->CreatePeerConnection(*config, nullptr, nullptr, _observer.get());

  // Create AV senders.
  [self createMediaSenders];
  if (_isInitiator) {
    // Send offer.
    rtc::scoped_refptr<webrtc::AppClientCreateSessionDescriptionObserver> observer(
        new rtc::RefCountedObject<webrtc::AppClientCreateSessionDescriptionObserver>(self));
    _peerConnection->CreateOffer(observer, [self defaultOfferConstraints].nativeConstraints.get());
  } else {
    // Check if we've received an offer.
    [self drainMessageQueueIfReady];
  }
#if defined(WEBRTC_IOS)
  // Start event log.
  if (kARDAppClientEnableRtcEventLog) {
    NSString *filePath = [self documentsFilePathForFileName:@"webrtc-rtceventlog"];

    int fd = open(filePath.UTF8String, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    if (fd < 0) {
      RTCLogError(@"Error opening file: %@. Error: %d", filePath, errno);
    } else if (!_peerConnection->StartRtcEventLog(fd, kARDAppClientRtcEventLogMaxSizeInBytes)) {
      RTCLogError(@"Failed to start event logging.");
    }
  }

  // Start aecdump diagnostic recording.
  if ([_settings currentCreateAecDumpSettingFromStore]) {
    NSString *filePath = [self documentsFilePathForFileName:@"webrtc-audio.aecdump"];

    int fd = open(filePath.UTF8String, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    if (fd < 0) {
      RTCLogError(@"Error opening file: %@. Error: %d", filePath, errno);
    } else if (!_factory->StartAecDump(fd, kARDAppClientAecDumpMaxSizeInBytes)) {
      RTCLogError(@"Failed to start aec dump.");
    }
  }
#endif
}

// Processes the messages that we've received from the room server and the
// signaling channel. The offer or answer message must be processed before other
// signaling messages, however they can arrive out of order. Hence, this method
// only processes pending messages if there is a peer connection object and
// if we have received either an offer or answer.
- (void)drainMessageQueueIfReady {
  if (!_peerConnection || !_hasReceivedSdp) {
    return;
  }
  for (ARDSignalingMessage *message in _messageQueue) {
    [self processSignalingMessage:message];
  }
  [_messageQueue removeAllObjects];
}

// Processes the given signaling message based on its type.
- (void)processSignalingMessage:(ARDSignalingMessage *)message {
  NSParameterAssert(_peerConnection || message.type == kARDSignalingMessageTypeBye);
  switch (message.type) {
    case kARDSignalingMessageTypeOffer:
    case kARDSignalingMessageTypeAnswer: {
      ARDSessionDescriptionMessage *sdpMessage = (ARDSessionDescriptionMessage *)message;
      RTCSessionDescription *description = sdpMessage.sessionDescription;

      rtc::scoped_refptr<webrtc::AppClientSetSessionDescriptionObserver> observer(
          new rtc::RefCountedObject<webrtc::AppClientSetSessionDescriptionObserver>(self));
      _peerConnection->SetRemoteDescription(observer, description.nativeDescription);
      break;
    }
    case kARDSignalingMessageTypeCandidate: {
      ARDICECandidateMessage *candidateMessage = (ARDICECandidateMessage *)message;
      std::unique_ptr<const webrtc::IceCandidateInterface> iceCandidate(
          candidateMessage.candidate.nativeCandidate);
      _peerConnection->AddIceCandidate(iceCandidate.get());
      break;
    }
    case kARDSignalingMessageTypeCandidateRemoval: {
      ARDICECandidateRemovalMessage *candidateMessage = (ARDICECandidateRemovalMessage *)message;
      std::vector<cricket::Candidate> candidates;
      for (RTCIceCandidate *iceCandidate in candidateMessage.candidates) {
        std::unique_ptr<const webrtc::IceCandidateInterface> candidate(
            iceCandidate.nativeCandidate);
        if (candidate) {
          candidates.push_back(candidate->candidate());
          // Need to fill the transport name from the sdp_mid.
          candidates.back().set_transport_name(candidate->sdp_mid());
        }
      }
      if (!candidates.empty()) {
        _peerConnection->RemoveIceCandidates(candidates);
      }
      break;
    }
    case kARDSignalingMessageTypeBye:
      // Other client disconnected.
      // TODO(tkchin): support waiting in room for next client. For now just
      // disconnect.
      [self disconnect];
      break;
  }
}

// Sends a signaling message to the other client. The caller will send messages
// through the room server, whereas the callee will send messages over the
// signaling channel.
- (void)sendSignalingMessage:(ARDSignalingMessage *)message {
  if (_isInitiator) {
    __weak ARDAppClientNative *weakSelf = self;
    [_roomServerClient sendMessage:message
                         forRoomId:_roomId
                          clientId:_clientId
                 completionHandler:^(ARDMessageResponse *response, NSError *error) {
                   ARDAppClientNative *strongSelf = weakSelf;
                   if (error) {
                     [strongSelf.delegate appClient:strongSelf didError:error];
                     return;
                   }
                   NSError *messageError =
                       [[strongSelf class] errorForMessageResultType:response.result];
                   if (messageError) {
                     [strongSelf.delegate appClient:strongSelf didError:messageError];
                     return;
                   }
                 }];
  } else {
    [_channel sendMessage:message];
  }
}

- (void)setMaxBitrateForPeerConnectionVideoSender {
  std::vector<rtc::scoped_refptr<webrtc::RtpSenderInterface>> nativeSenders(
      _peerConnection->GetSenders());
  for (const auto &nativeSender : nativeSenders) {
    if (nativeSender->track() &&
        nativeSender->track()->kind() == [NSString stdStringForString:kARDVideoTrackKind]) {
      [self setMaxBitrate:[_settings currentMaxBitrateSettingFromStore]
           forVideoSender:nativeSender];
    }
  }
}

- (void)setMaxBitrate:(NSNumber *)maxBitrate
       forVideoSender:(rtc::scoped_refptr<webrtc::RtpSenderInterface>)sender {
  if (maxBitrate.intValue <= 0) {
    return;
  }

  webrtc::RtpParameters parametersToModify = sender->GetParameters();
  for (auto &encoding : parametersToModify.encodings) {
    encoding.max_bitrate_bps = rtc::Optional<int>(maxBitrate.intValue * kKbpsMultiplier);
  }
  if (!sender->SetParameters(parametersToModify).ok()) {
    RTCLogError(@"RtpSender: Failed to set parameters");
  }
}

- (void)createMediaSenders {
  RTCMediaConstraints *constraints = [self defaultMediaAudioConstraints];

  std::unique_ptr<webrtc::MediaConstraints> nativeConstraints;
  if (constraints) {
    nativeConstraints = constraints.nativeConstraints;
  }
  rtc::scoped_refptr<webrtc::AudioSourceInterface> source =
      _factory->CreateAudioSource(nativeConstraints.get());
  std::string trackId = [NSString stdStringForString:kARDAudioTrackId];
  rtc::scoped_refptr<webrtc::AudioTrackInterface> track =
      _factory->CreateAudioTrack(trackId, source);
  std::string streamId = [NSString stdStringForString:kARDMediaStreamId];
  rtc::scoped_refptr<webrtc::MediaStreamInterface> stream =
      _factory->CreateLocalMediaStream(streamId);
  stream->AddTrack(track);

  _localVideoTrack = [self createLocalVideoTrack];
  if (_localVideoTrack) {
    stream->AddTrack(_localVideoTrack);
  }

  if (!_peerConnection->AddStream(stream)) {
    RTCLogError(@"Failed to add stream");
  }
}

- (rtc::scoped_refptr<webrtc::VideoTrackInterface>)createLocalVideoTrack {
  if ([_settings currentAudioOnlySettingFromStore]) {
    return nil;
  }

  _source = [[RTCVideoSource alloc] initWithSignalingThread:_signalingThread.get()
                                               workerThread:_workerThread.get()];
#if !TARGET_IPHONE_SIMULATOR
  RTCCameraVideoCapturer *capturer = [[RTCCameraVideoCapturer alloc] initWithDelegate:_source];
  [_delegate appClient:self didCreateLocalCapturer:capturer];

#else
#if defined(__IPHONE_11_0) && (__IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_11_0)
  if (@available(iOS 10, *)) {
    RTCFileVideoCapturer *fileCapturer = [[RTCFileVideoCapturer alloc] initWithDelegate:source];
    [_delegate appClient:self didCreateLocalFileCapturer:fileCapturer];
  }
#endif
#endif

  std::string nativeId = [NSString stdStringForString:kARDVideoTrackId];
  // TODO(andersc): source.nativeVideoSource should be `ObjCToNativeVideoSource(source)`
  rtc::scoped_refptr<webrtc::VideoTrackInterface> track =
      _factory->CreateVideoTrack(nativeId, _source.nativeVideoSource);

  return track;
}

#pragma mark - Collider methods

- (void)registerWithColliderIfReady {
  if (!self.hasJoinedRoomServerRoom) {
    return;
  }
  // Open WebSocket connection.
  if (!_channel) {
    _channel = [[ARDWebSocketChannel alloc] initWithURL:_websocketURL
                                                restURL:_websocketRestURL
                                               delegate:self];
    if (_isLoopback) {
      _loopbackChannel =
          [[ARDLoopbackWebSocketChannel alloc] initWithURL:_websocketURL restURL:_websocketRestURL];
    }
  }
  [_channel registerForRoomId:_roomId clientId:_clientId];
  if (_isLoopback) {
    [_loopbackChannel registerForRoomId:_roomId clientId:@"LOOPBACK_CLIENT_ID"];
  }
}

#pragma mark - Defaults

- (RTCMediaConstraints *)defaultMediaAudioConstraints {
  NSString *valueLevelControl = [_settings currentUseLevelControllerSettingFromStore] ?
      kRTCMediaConstraintsValueTrue :
      kRTCMediaConstraintsValueFalse;
  NSDictionary *mandatoryConstraints = @{kRTCMediaConstraintsLevelControl : valueLevelControl};
  RTCMediaConstraints *constraints =
      [[RTCMediaConstraints alloc] initWithMandatoryConstraints:mandatoryConstraints
                                            optionalConstraints:nil];
  return constraints;
}

- (RTCMediaConstraints *)defaultAnswerConstraints {
  return [self defaultOfferConstraints];
}

- (RTCMediaConstraints *)defaultOfferConstraints {
  NSDictionary *mandatoryConstraints =
      @{@"OfferToReceiveAudio" : @"true", @"OfferToReceiveVideo" : @"true"};
  RTCMediaConstraints *constraints =
      [[RTCMediaConstraints alloc] initWithMandatoryConstraints:mandatoryConstraints
                                            optionalConstraints:nil];
  return constraints;
}

- (RTCMediaConstraints *)defaultPeerConnectionConstraints {
  if (_defaultPeerConnectionConstraints) {
    return _defaultPeerConnectionConstraints;
  }
  NSString *value = _isLoopback ? @"false" : @"true";
  NSDictionary *optionalConstraints = @{@"DtlsSrtpKeyAgreement" : value};
  RTCMediaConstraints *constraints =
      [[RTCMediaConstraints alloc] initWithMandatoryConstraints:nil
                                            optionalConstraints:optionalConstraints];
  return constraints;
}

#pragma mark - Errors

+ (NSError *)errorForJoinResultType:(ARDJoinResultType)resultType {
  NSError *error = nil;
  switch (resultType) {
    case kARDJoinResultTypeSuccess:
      break;
    case kARDJoinResultTypeUnknown: {
      error = [[NSError alloc] initWithDomain:kARDAppClientErrorDomain
                                         code:kARDAppClientErrorUnknown
                                     userInfo:@{
                                       NSLocalizedDescriptionKey : @"Unknown error.",
                                     }];
      break;
    }
    case kARDJoinResultTypeFull: {
      error = [[NSError alloc] initWithDomain:kARDAppClientErrorDomain
                                         code:kARDAppClientErrorRoomFull
                                     userInfo:@{
                                       NSLocalizedDescriptionKey : @"Room is full.",
                                     }];
      break;
    }
  }
  return error;
}

+ (NSError *)errorForMessageResultType:(ARDMessageResultType)resultType {
  NSError *error = nil;
  switch (resultType) {
    case kARDMessageResultTypeSuccess:
      break;
    case kARDMessageResultTypeUnknown:
      error = [[NSError alloc] initWithDomain:kARDAppClientErrorDomain
                                         code:kARDAppClientErrorUnknown
                                     userInfo:@{
                                       NSLocalizedDescriptionKey : @"Unknown error.",
                                     }];
      break;
    case kARDMessageResultTypeInvalidClient:
      error = [[NSError alloc] initWithDomain:kARDAppClientErrorDomain
                                         code:kARDAppClientErrorInvalidClient
                                     userInfo:@{
                                       NSLocalizedDescriptionKey : @"Invalid client.",
                                     }];
      break;
    case kARDMessageResultTypeInvalidRoom:
      error = [[NSError alloc] initWithDomain:kARDAppClientErrorDomain
                                         code:kARDAppClientErrorInvalidRoom
                                     userInfo:@{
                                       NSLocalizedDescriptionKey : @"Invalid room.",
                                     }];
      break;
  }
  return error;
}

@end
