<?% config.freshness.owner = 'minyue' %?>
<?% config.freshness.reviewed = '2021-04-13' %?>

# The WebRTC Audio Coding Module

WebRTC audio coding module handles both audio sending and receiving.

* Audio Sending
Audio frames, each of which should always contain 10 ms worth of data, are provided to the audio coding module through `Add10MsData()`. The audio coding module uses a provided audio encoder to encoded audio frames and deliver the data to a pre-registered audio packetization callback, which is supposed to wrap the encoded audio into RTP packets and send them over a transport. Built-in audio codecs are included the `codecs` folder.


* Audio Receiving
Audio packets are provided to the audio coding module through `IncomingPacket()`, and are processed by an audio jitter buffer (NetEq), which includes decoding of the packets. Audio decoders are provided by an audio decoder factory. Decoded audio samples should be queried by calling `PlayoutData10Ms()`.