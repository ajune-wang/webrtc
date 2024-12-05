<!-- go/cmark -->
<!--* freshness: {owner: 'hta' reviewed: '2024-12-05'} *-->

# Organizational contributors to WebRTC

This document outlines the procedures for cooperating with external (non-Google)
organizational contributors to WebRTC.

Note that this is not covering the case of individual, one-off contributions;
those are adequately covered in other documents.

## Background: Individuals making multiple contributions

The steps outlined in the various docs sketch a strategy:

*   First, contribute something to show understanding of the codebase
*   Then, get bot start rights, so that one can test the contributions before
    asking for review (this right applies only to bots that operate on the open
    source repo)
*   After a number of commits, and demonstrating adequate knowledge of the
    project’s style and structure, one can ask for committer rights, which will
    give the ability to submit code after adequate review (current policy:
    review by two WebRTC project members).

## Organizations making multiple contributions

Sometimes, organizations take on a commitment to contribute to WebRTC on a
longer term basis. In these cases, it is good for all parties to have some
guidelines on how the relationship between the core WebRTC project and the
organization is managed.

We should have the following roles in place:

*   A contact person at the contributing organization \
    This person will be responsible for knowing where the organization is making
    contributions, and why. All contributors from that organization need to be
    known by that contact person; the WebRTC project may redirect queries from
    other people in the org to that person if not already CCed.
*   At least one person with committer rights (or working towards such rights).
    \
    This person will also be a primary reviewer for incoming CLs from the
    organization, ensuring a review is done before the WebRTC project members
    are asked for review. \
    This can be the same as the contact person, or someone different.

When making small contributions like bug fixes, normal review is sufficient.

When asking to add significant functionality (new CC, new codecs, other new
features), the process should include:

*   Specifying why the feature is needed (requirements, conditions for saying
    “it works”, value to the larger community)
*   A design document showing how the feature will be implemented and how it
    will interact with the rest of the WebRTC implementation
*   A plan for who will do the work, and when it’s expected to happen
*   A “match list” of the areas affected by the project and the WebRTC project
    members available to review contributions in those areas. (This can be
    created collaboratively).
*   If the work involves field trials and rollouts on Google properties like
    Meet and Chrome, or other aspects that require “insider” permissions, the
    plan must name a WebRTC project member who will be responsible for managing
    these aspects.

Normally, an ongoing relationship will require some regular cadence of meetings;
a minimum of one hour per quarter should be aimed for, with other meetings as
needed.

## Organizational management of contributed code

For some types of contributions, the value to the core project is not very high,
but some partners find them valuable, and appreciate the binding to the WebRTC
code repository.

In such cases, where the code is self-contained, we can put it into special
directories and declare those directories as “third party contributions” - under
the same licensing conditions as the WebRTC core.

In those cases, the core project will:

*   Build the code as part of the regular bot build system
*   Run tests as appropriate according to defined test suites
*   Disable breaking builds and tests when they break, and file bugs in the
    webrtc bugtracker.

Each such directory should have a configuration file identifying a responsible
owner outside of the WebRTC core team, who will be assigned the bugs pointing at
issues with the contributions.

The core team will apply refactorings and dependency updates on a best-effort
basis, and have the ability to delete contributions that are deemed to be
unmaintained.
