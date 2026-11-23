# Team division and branches

| Member | Branch | Owned files |
|---|---|---|
| 1 | `feature/rfid` | `drivers/rfid/` |
| 2 | `feature/water-level` | `drivers/water/` |
| 3 | `feature/potentiometer` | `drivers/potentiometer/` |
| 4 | `feature/pumps` | `drivers/motor/` |
| 5 | `feature/lcd-buttons` | `drivers/display/`, `drivers/buttons/` |
| 6 | `feature/camera-ocr` | `drivers/uv/`, `drivers/camera/` |
| 7 | `feature/state-machine` | `src/`, `include/`, root `CMakeLists.txt` |

Create each branch from `main`, work only in owned files, build and test locally, commit, push, and open a pull request. Resolve conflicts before merging and keep `main` buildable. No branches are created automatically by this skeleton.
