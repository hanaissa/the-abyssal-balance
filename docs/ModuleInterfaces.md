# Module interfaces

Public contracts live in each module header and use `AppStatus` for readiness/error reporting. Shared types are centralized in `include/AppTypes.h` and `include/AppEvents.h`.

The initial files are interface stubs. `APP_NOT_READY` and `APP_UNSUPPORTED` are deliberate and must not be treated as working hardware implementations.
