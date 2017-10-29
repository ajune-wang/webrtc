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

1. Unlike with the other directories listed in the table, the API-ness
   of the `api` directory is recursive—its subdirectories (and their
   subdirectories, etc.) are also API directories.

2. For historical reasons, `rtc_base` currently contains a mix of
   files intended to be part of the API and files never intended to be
   part of the API. Procedurally, the entire directory will be treated
   as part of the API with regards to the procedure for incompatible
   changes, but be aware that quite a lot of such incompatible changes
   are planned in the form of moving stuff out of this directory, and
   thus out of the API.

While the files, types, functions, macros, build targets, etc. in
these directories will sometimes undergo incompatible changes, such
changes will be announced in advance to
[discuss-webrtc@googlegroups.com][discuss-webrtc], and a migration
path will be provided.

[discuss-webrtc]: https://groups.google.com/forum/#!forum/discuss-webrtc

In the directories not listed in the table above (including
subdirectores of the listed directories, unless noted in the table),
incompatible changes may happen at any time, and are not announced.

## All these worlds are yours—except Europa

In the API headers, or in files included by the API headers, there are
types, functions, namespaces, etc. that have `impl` or `internal` in
their names (in various styles, such as `CamelCaseImpl`,
`snake_case_impl`). They are not part of the API, and may change
incompatibly at any time; do not use them.
