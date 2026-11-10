---
name: minimover
description: Convert XYZ da Vinci .3w and G-code files with miniMover on Linux.
---

# miniMover

- `minimover-qt models --json` lists printer model indices.
- `minimover-qt convert INPUT.gcode OUTPUT.3w --model INDEX --json` creates a printer-specific file.
- `minimover-qt convert INPUT.3w OUTPUT.gcode --json` decodes a file.
- Output files must not exist; originals are preserved. Exit status is 0 on success, 1 on failure.
- Paths passed to the legacy converter must be shorter than 260 bytes.
- Never guess the printer model or issue physical-printer commands without the user's confirmation.
- Converting a file does not prove it is safe to print. The operator must verify the printer, material, toolpath, and clear build area.
- `minimover-qt open` opens the interactive interface. Wi-Fi support is experimental.
