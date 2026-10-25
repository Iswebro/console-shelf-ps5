# Native prototype work

The native target is C/C++ with SDL2. The HTML/JavaScript preview remains a design reference and a fast way to test interactions. It is not the runtime shipped to the PS5. The emulator still runs as its own app, so the preview's choice of language does not change emulator performance.

The first native component is a C++17 catalogue and browsing model. It has no renderer, console filesystem access or launch side effects. Hardware names, icon paths and suggested cores are generated from the preview catalogue with `node tools/build-native-catalog.js`; demo games and installed-status labels are intentionally omitted. Runtime adapters must supply actual discovery evidence. An existing game file can make a console visible without making its launch button available.

Run `npm run test:native` on Windows with an existing Visual Studio C++ installation. The script configures that compiler only in its child process and writes to ignored `.cache/native-host/`. It installs no packages. On Linux with a C++17 compiler:

```sh
mkdir -p .cache/native-host
c++ -std=c++17 -Wall -Wextra -Werror -I native/include native/src/model.cpp native/tests/model_tests.cpp -o .cache/native-host/model-tests
.cache/native-host/model-tests
```

This host check is not evidence of a working PS5 executable. The PS5 SDK, renderer, controller input, persistence, discovery adapters, launcher and packaging are the next components in the [prototype milestone](../docs/PROTOTYPE-PLAN.md). No PS5 download has been released yet.
