# Native keyboard effects

The Room core owns the Windows Raw Input subscription. Effects request a bounded
listener only while a keyboard-reactive effect is running and enabled. This is
an optional secondary API; the Plugin API 5 controller layout is unchanged.
There is no helper process and no JavaScript or browser renderer.

The service reads physical Set 1 make/break events, ignores typematic repeats,
and clears state when a device disappears, the computer resumes, or the last
listener stops. It does not translate keys into characters, inject input, or
store or publish typing history. Each listener consumes at most 64 recent events
with a 250 ms deadline. Registration conflicts with another in-process keyboard
consumer are reported instead of replacing that consumer.

Effects match the Windows physical-device container of the Raw Input interface
to the controller's HID interface. VID/PID alone is not used as identity. Standard
OpenRGB English key names identify physical positions, so an AZERTY A produces
the physical Q position without translating through a text layout. Unknown or
renamed key identities are ignored; Fn keys not exposed by Windows cannot fire
an event. Two near-simultaneous reports from different HID collections of one
physical device are coalesced. Separate keyboards remain distinct.

Visual Map exports value-only keyboard geometry built from the same exact LED
sampling plan that routes colors. It includes placement, scale, rotation, flips,
segments and global LED indexes. Disabled routes expose no input points. The
effects' crop/rotation transform is applied after the map transform. No geometry
is inferred from the lower-resolution virtual LED compatibility grid.

Rainbow Tap stores at most 64 spatial impulses for five seconds. Ring age and
integrated wave speed are separate: setting speed to zero stops growth while
expiration continues; changing speed never moves a ring discontinuously.
Neon Nebula uses the same input history and native double-buffered GPU feedback.
Preview clicks also create impulses. Keyboard input can be disabled per effect.

Validation distinguishes pure event tests, production GPU tests, Windows device
identity queries, and a physical keyboard test. The first three do not establish
that a user observed the correct optical output. See deployment validation for
the latest physical confirmation.

Windows API references: [Raw Input registration](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-registerrawinputdevices)
and [physical keyboard data](https://learn.microsoft.com/en-us/windows/win32/api/winuser/ns-winuser-rawkeyboard).
