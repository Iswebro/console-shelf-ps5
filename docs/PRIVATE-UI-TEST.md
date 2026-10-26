# Private isShelf menu testing

This development mode lets a PC send one menu action to isShelf and retrieve a fresh
1920 x 1080 PNG from the console's renderer. It uses the owner's existing `ftpsrv`
and fixed `ui-test-*` files directly in isShelf's existing `/data/console-shelf`
folder. It requires no PSN sign-in.

It is an action, image, inspection loop rather than continuous video. The PNG shows
the frame isShelf rendered. HDMI capture is needed to check what the display path
actually shows. This mode controls isShelf's menus only; remote Confirm actions
that would launch another app are refused. The physical controller retains its
normal behaviour. Emulator control is outside this mode.

## Build and enable

The ordinary PS5 build omits the bridge. Build a private executable explicitly:

```powershell
node tools/build-ps5.js build_ps5.py --standard-helper-runtime --ui-test-bridge
```

This command builds locally; it does not contact or install on a PS5. The standard
helper runtime flag retains the owner's existing accepted launch configuration;
it is unrelated to remote test control. The test build carries a marker in the
executable and its structural report. Public release preparation rejects that marker.

Install only through the existing private candidate and rollback procedure, preserving
the current program and preferences. After opening isShelf and letting discovery
finish, click **L3 + R3 together on the same controller**. A persistent header shows
that test control is enabled. Click both again to disable it. Control expires after
ten minutes and must be enabled again locally after every app restart.

## Use from the PC

Keep `ftpsrv` available on the trusted local network. Run the Python standard-library
client with the console's LAN IPv4 address and a fresh local evidence directory:

```powershell
python tools/ui_test_client.py --host <console-LAN-IPv4> --action snapshot --output-dir .cache/ui-review
python tools/ui_test_client.py --host <console-LAN-IPv4> --action right --output-dir .cache/ui-review
python tools/ui_test_client.py --host <console-LAN-IPv4> --action confirm --output-dir .cache/ui-review
```

The client saves each verified PNG, acknowledgement and local SHA-256 in a new
directory. Inspect the returned image before choosing the next action. Available
actions are `snapshot`, `up`, `down`, `left`, `right`, `confirm`, `back`, `settings`,
`layout`, `favorite`, `previous-tab`, `tab` and `stop`. Direction commands move once.
Back is a tap and cannot become a hold-to-close gesture. Settings changes save through
isShelf's existing settings code, so testing a setting can change the owner's preferences.

The transport uses only fixed prefixed filenames. It creates no private subdirectory
that would prevent the FTP service from accessing the exchange under another user ID.
The app accepts a
bounded request for the current session, exact next sequence and observed UI revision.
Physical controller input invalidates old observations. The app refuses actions during
loading, closure or a launch handoff. Images are captured on request after rendering
and frame submission, with a short delay for menu motion to settle.

The session identifier prevents old requests from replaying after restart; it is not
authentication. Anyone who already has write access through the owner's FTP service
can write the control files while control is enabled. Local enablement, a visible
indicator, the time limit and the command allowlist define this private mode's scope.

After a timeout or lost connection, the action outcome may be unknown. **Do not resend
the action.** Use the client's read-only `--inspect` recovery option to retrieve the
last response. Each action is sent at most once, and the response binds its sequence
to the PNG's byte count and FNV-1a digest. Local SHA-256 records identify saved evidence.

## PC verification

```powershell
npm run test:native
npm run build:native-host
python tools/test_ui_test_client.py
python tools/test_ui_test_integration.py
python tools/test_ps5_release_inputs.py
```

The integration test runs the real SDL event loop with `--headless --ui-test` in a
new local data folder. It checks navigation and new images, details/settings, persistence,
stale and duplicate commands, stopping control while leaving the app open, and the
disabled default. FTP transport tests use a local fixture. These checks establish PC
behaviour. Console activation, FTP exchange, responsiveness and physical display
remain unverified until tested on the PS5.
