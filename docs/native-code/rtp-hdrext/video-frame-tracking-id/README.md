# Video Frame Tracking Id

The Video Frame Tracking Id extension is meant for media quality testing
purpose and advanced video algorithms. It tracks a 32-bit video frame number id
field from the sender to the receiver to gather referenced base media quality
metrics such as PSNR or SSIM.

Features like frame invalidation can use this information to avoid sending IDR frames, this will help reduce frame size in already lossy network.

Currently packets lost does not indicate how many frames lost on network, with this frames lost on network can be computed.

Potentially this can be used for FEC enhancements like Reed-Solomon coding.

The frame number id is monotonically increasing, and may wrap.

Contact <jleconte@google.com> for more info.
Contact <aheggestad@nvidia.com> for more info.

**Name:** "Video Frame Tracking Id"

**Formal name:**
<http://www.webrtc.org/experiments/rtp-hdrext/video-frame-tracking-id>

**Status:** This extension is defined to allow for media quality testing
and advanced video algorithms.


### Data layout overview
     1-byte header + 4 bytes of data:

      0                   1                   2                   3
      0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     |  ID   | L=3   |       video-frame-tracking-id (bit 0-23)      |
     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     |  ... (24-31)  |
     +-+-+-+-+-+-+-+-+

Notes: The extension SHOULD be present only in the first packet of each frame.
If attached to other packets it can be ignored.
