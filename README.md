# Insta360 M5StickC Remote (Forked & Enhanced)

<img src="demo.jpg" alt="M5StickC Plus2 displaying multi-camera dashboard" width="500">

This project is a fork of the excellent work by theserialhobbyist: [https://github.com/theserialhobbyist/insta360_m5StickC_remote/](https://github.com/theserialhobbyist/insta360_m5StickC_remote/)

It provides an enhanced remote control experience for Insta360 cameras using M5Stack devices (M5StickC, M5StickC Plus, and M5StickC Plus2).

Tested with Insta360 X5 and Ace Pro 2.

### Key Enhancements & Differences from Original:

- **Multi-Camera Connection Stability:** Improved BLE advertising and connection ID tracking for more reliable dual-camera connections.
- **Robust Recording Synchronization:** Implemented smart shutter logic that uses unicast commands to ensure both cameras are always in sync (e.g., if one is recording and the other isn't, a single click will stop the active one, and the next click will start both together).
- **Intuitive Dashboard UI:**
  - Clean, redesigned main screen displaying large status indicators and short names (e.g., "X5", "Ace") for both connected cameras.
  - Integrated remote battery level display.
  - Clear recording status with visual red dots for each active camera.
  - Optimized, non-flickering recording timer.
- **Streamlined Controls:**
  - **Short Press (Button A):** Triggers shutter (start/stop recording).
  - **Long Press (Button A):** Powers on (Wake) disconnected cameras or powers off (Sleep) connected cameras.
- **Dynamic Layout & Persistence:**
  - Added a menu option to toggle between Horizontal (side-by-side) and Vertical (top-bottom) dashboard layouts.
  - Physical screen rotation applied automatically for Vertical mode, allowing for vertical device mounting.
  - Layout preference is saved and loaded from device memory.
- **Refactored Menu System:**
  - A dedicated "Pairing" menu (accessible via Button B) for connecting cameras and changing settings.
  - Removed redundant and unused screens from the navigation flow (Mode, Sleep, Wake, Screen Off, individual Connect screens).
- **Polished User Feedback:** Centralized and consistent visual messages for command execution ("SENT", "SYNC!") and device states ("SLEEPING...", "Waking...").

### Details

Details at [serialhobbyism.com](https://serialhobbyism.com/open-source-diy-remote-for-insta360-cameras)

Demonstration on YouTube: https://youtube.com/shorts/_0BtUzAN8ro?feature=share
