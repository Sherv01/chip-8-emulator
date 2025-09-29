# CHIP-8 Emulator

A Windows-based CHIP-8 emulator built in C++.

The project comes as a single executable (`chip8.exe`) and is ready to run. You don’t need to build anything unless you want to modify the source.

---

## Features

* Full CHIP-8 instruction set support
* Adjustable emulation speed (Slow, Normal, Fast, Fastest)
* Pause/Resume and Reset functionality
* Keyboard mapping compatible with CHIP-8 keypad
* Fully functional on-screen controls
* Custom button styling
* Custom screen filters available
* Pre-built executable included
  <img width="618" height="419" alt="image" src="https://github.com/user-attachments/assets/5216552f-323d-482a-a203-b7a539ab95b2" />
  <img width="623" height="416" alt="image" src="https://github.com/user-attachments/assets/c213be87-2e36-4aff-ae8c-d278e7ffc7df" />

  


---

## Prerequisites

* **Windows 10/11**
* **Git LFS** (required to download large DLL dependencies included in the repository)

---

## Getting Started

1. Clone the repository with Git LFS enabled:

> ⚠️ IMPORTANT: You **MUST** download Git LFS (Large File Storage) before running these commands because there are large files (~500 MB total) that need to be included when you clone

```bash
git lfs install
git clone https://github.com/Sherv01/chip-8-emulator.git
```

2. Run `chip8.exe` directly.

3. Open a CHIP-8 ROM via the menu and start playing.

---

## Keyboard Mapping

| CHIP-8 Key | Keyboard Key |
| ---------- | ------------ |
| 0x0        | X            |
| 0x1        | 1            |
| 0x2        | 2            |
| 0x3        | 3            |
| 0x4        | Q            |
| 0x5        | W            |
| 0x6        | E            |
| 0x7        | A            |
| 0x8        | S            |
| 0x9        | D            |
| 0xA        | Z            |
| 0xB        | C            |
| 0xC        | 4            |
| 0xD        | R            |
| 0xE        | F            |
| 0xF        | V            |

---

## Notes

* The repository uses **Git LFS** for large DLL files. Make sure to pull with LFS to download them correctly.
* You don’t need to compile anything unless you want to make changes.
