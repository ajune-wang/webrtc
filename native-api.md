# API header files

As a user of the WebRTC library, you may use headers and build files
in the following directories:

Directory                                  | Notes
-------------------------------------------|------
`api`                                      | (1)
`common_audio/include`                     |
`media`                                    |
`modules/audio_coding/include`             |
`modules/audio_device/include`             |
`modules/audio_processing/include`         |
`modules/bitrate_controller/include`       |
`modules/congestion_controller/include`    |
`modules/include`                          |
`modules/remote_bitrate_estimator/include` |
`modules/rtp_rtcp/include`                 |
`modules/rtp_rtcp/source`                  |
`modules/utility/include`                  |
`modules/video_coding/codecs/h264/include` |
`modules/video_coding/codecs/i420/include` |
`modules/video_coding/codecs/vp8/include`  |
`modules/video_coding/codecs/vp9/include`  |
`modules/video_coding/include`             |
`pc`                                       |
`rtc_base`                                 | (2)
`system_wrappers/include`                  |
`voice_engine/include`                     |

*Notes*

1. Generally, the entries in this table are non-recursive; i.e., a
   subdirectory of an API directory is not an API directory unless it
   too is explicitly listed in the table. However, the `api` directory
   is an exception to that rule&mdash;the entire subtree rooted there
   is included in the API.

2. For historical reasons, `rtc_base` currently contains a mix of
   files intended to be part of the API and files never intended to be
   part of the API. We are in the process of cleaning this up, and
   while doing so we will follow the normal procedure for making API
   changes when that seems to make sense, but for changes to code that
   is &ldquot;obviously&rdquot; not part of the API we may choose to
   bypass the red tape.

While the files, types, functions, macros, build targets, etc. in
these directories will sometimes undergo incompatible changes, such
changes will be announced in advance to
[discuss-webrtc@googlegroups.com][discuss-webrtc], and a migration
path will be provided.

[discuss-webrtc]: https://groups.google.com/forum/#!forum/discuss-webrtc

In the directories not listed in the table above (including
subdirectores of the listed directories, unless noted in the table),
incompatible changes may happen at any time, and are not announced.

## All these worlds are yours&mdash;except Europa

In the API headers, or in files included by the API headers, there are
types, functions, namespaces, etc. that have `impl` or `internal` in
their names (in various styles, such as `CamelCaseImpl`,
`snake_case_impl`). They are not part of the API, and may change
incompatibly at any time; do not use them.
