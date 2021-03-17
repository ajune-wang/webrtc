# Video Frame Tracking Id

The Video Frame Tracking Id extension is used to track webrtc::VideoFrame id
field from the sender to the receiver. It is used to gather referenced base media
quality metrics such as PSNR or SSIM.
Contact <jleconte@google.com> for more info.

**Name:** "Video Frame Tracking Id"

**Formal name:** <http://www.webrtc.org/experiments/rtp-hdrext/video-frame-tracking-id>

**Status:** This extension is defined here to allow for media quality testing.
It is enabled by using a field trial and shouldn't be used in any other context.

### Data layout overview
     1-byte header + 2 bytes of data:

      0                   1                   2
      0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3
     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     |  ID   | L=1   |    video-frame-tracking-id    |
     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

Notes: Extension shoud be present only in the first packet of each frames. If
attached to other packets it can be ignored.