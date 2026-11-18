# Team division and branches

| Member | Branch | Owned files |
|---|---|---|
| 1 | `feature/rfid` | `drivers/rfid/`, `tests/RFID_Test/` |
| 2 | `feature/water-level` | `drivers/water/`, `tests/WaterLevel_Test/` |
| 3 | `feature/potentiometer` | `drivers/potentiometer/`, `tests/Potentiometer_Test/` |
| 4 | `feature/pumps` | `drivers/motor/`, `tests/Pump_Test/` |
| 5 | `feature/lcd-buttons` | `drivers/display/`, `drivers/buttons/`, `tests/LCDButtons_Test/` |
| 6 | `feature/camera-ocr` | `drivers/uv/`, `drivers/camera/`, `tests/CameraOCR_Test/` |
| 7 | `feature/state-machine` | `src/`, `include/`, root `CMakeLists.txt`, `tests/Integration_Test/` |

Create each branch from `main`, work only in owned files, build and test locally, commit, push, and open a pull request. Resolve conflicts before merging and keep `main` buildable. No branches are created automatically by this skeleton.
