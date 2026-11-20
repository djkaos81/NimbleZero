NimbleZero display test kit (for rc10, either variant)                       2026-11-19
======================================================================================
BUILD 4 (same day).  Results so far on hardware (unit 1), and what this build adds:
  Builds 1-3: sync pin works (61.8 Hz); orientation variant 2; with panel scan order + sync
           + cached buffers + background sending the bars run at 61.8 frames/s without
           tearing, also when the program needs 14 ms per frame; Doom 61 frames/s.
           That covered programs that draw to the display device directly.
  Build 4 is about everything else - launcher, console, M5Stack apps - which draw into
  /dev/fb0:
    - small changes are sent as small pieces in scan order (before: whole frames only);
      pieces of 1/16 of the screen or more start on the sync pulse;
    - the kernel passes /dev/fb0 changes on only every 50 ms (20 per second); a new switch
      shortens that to 16 ms;
    - "install.sh boot-default on" makes the smooth setting active from power-on, so the
      whole system uses it (and /dev/fb0 gets a cached buffer, which only happens at start).
  NOT yet run on hardware (emulator: module load, settings file, test pictures; build
  machine: 12000 random partial updates compared with full sends, all four variants).

  Update:   scp -O -r nz-kit zero@<address>:     (again, overwrites)
            sudo sh nz-kit/install.sh install ; sudo reboot
  Part 1:   sudo sh /usr/lib/nz-kit/nz-panel.sh auto          (saved in panel-auto.txt)
            Tests A-D draw into /dev/fb0 over the launcher's picture: A original, B smooth
            at the kernel's pace, C smooth at 16 ms, D a small blinking block; E is the
            bars test from build 3.  Bars solid?  Anything in the wrong place or mirrored?
  Part 2:   sudo sh nz-kit/install.sh boot-default on ; sudo reboot
            Use the device normally for a while: launcher, Options, console (type some
            text), Wi-Fi/Store screens, an M5Stack app if one is installed, Doom.
            Look for: anything drawn in the wrong place, left-over pieces, mirrored or
            missing parts, flicker, sluggish keys.  Then once more:
            sudo sh /usr/lib/nz-kit/nz-panel.sh auto          (rename the first file before)
  Back to switches-off at start:  sudo sh nz-kit/install.sh boot-default off ; sudo reboot
  If the screen is unusable after Part 2 and SSH does not answer: the nz-kit-restore file
  on NZBOOT (see "Putting everything back") also removes this setting.
  The steps below are the original instructions; steps 3-5 are replaced by the lines above.

Goal: 60 frames/s without tearing.  This kit measures how close the panel gets and lets
you look at the result with moving test bars and with Doom.

NOTHING IN THIS KIT HAS RUN ON A CARDPUTER ZERO.  In the emulator (which has no LCD, no
SPI and no sync pin) these were checked: the install and restore scripts, that the test
driver loads into the rc10 kernel and shows its switches, the safety net (with the restart
replaced by a message), the test pictures, and Doom on the console framebuffer including
keys and the timed demo.  The driver's new display path, the sync pin and the "native"
pixel order are untested until you run them.

What gets installed
  - a test build of the LCD driver (the vendor driver plus switches; with every switch at
    0 - the state after each restart - it uses the original code path)
  - boot settings: 100 MHz panel clock (you measured 60.6 frames/s there, picture clean)
    and the line that tells the driver the panel's sync output is on GPIO 5.  The vendor
    left that line commented out; whether the pin is really wired is the first thing the
    test reports.  The driver only listens on that pin.
  - Doom (doomgeneric, GPL-2.0) with the Freedoom Phase 1 game data (BSD-style licence) in
    /home/zero/doom, and a launcher entry "Doom (Freedoom)".  No sound.
  Changed on the system partition: the driver file, /usr/lib/nz-kit/, /etc/init.d/S01nz-kit.
  Changed on NZBOOT: three files in overlays/, config.txt (a copy of the old one is kept
  as config-before-nz-kit.txt).

Steps (use your test card; rc10 booted, SSH working)
  1. From the PC, in the folder that contains nz-kit:
         scp -O -r nz-kit zero@<address>:
     (-O: the device has plain scp, no SFTP server.  About 30 MB.)
  2. On the device:
         ssh zero@<address>
         sudo sh nz-kit/install.sh install
         sudo reboot
  3. Look at the screen: launcher as usual?  Then:
         sudo sh /usr/lib/nz-kit/nz-panel.sh auto
     It runs six 5-second tests (A-F) with moving bars.  Watch each one and note where the
     bars look broken, stepped or sheared, and where they look solid.  Send me everything
     it prints plus those notes.
  4.     sudo sh /usr/lib/nz-kit/nz-panel.sh orient
     Shows a test card four times, 6 seconds each.  Tell me which variant number shows the
     F upright with RED top-left, GREEN top-right, BLUE bottom-left, WHITE bottom-right.
     (Tests C, E and F in step 3 use variant 2, which is what the vendor settings imply.
     If another variant is the right one, the bars in those tests are mirrored; the timing
     numbers are still valid.)
  5. Doom, timed:   sudo sh /usr/lib/nz-kit/nz-panel.sh doom
     Plays the built-in demo as fast as the display accepts frames and prints the rate.
     To time it with a particular setting, set it first, e.g.
         sudo sh /usr/lib/nz-kit/nz-panel.sh set 1 2 1 0
     and go back with   ... nz-panel.sh stock   (or restart).
  6. Doom, played: start "Doom (Freedoom)" from the launcher.  The yellow number top right
     is frames sent per second (the game itself advances 35 times per second; the program
     sends frames as fast as the display takes them).
         F / X      forward / back            Z / C     turn left / right
         , / .      step left / right         Shift     run
         Enter or L fire (Enter also selects in menus)
         Space or K open doors, use           Tab       map
         1-7        weapons                   Esc or `  menu
         0          frame counter on/off      - / =     smaller / larger view
     Quit from the menu (Esc, Quit Game, y), or Ctrl+Alt+Backspace.
     Typing names with F, X, Z or C in them (save games) is not possible in this build.

Putting everything back
     sudo sh nz-kit/install.sh restore ; sudo reboot
  If the screen stays dark and SSH does not answer: put the card in the PC, create an empty
  file named  nz-kit-restore  on NZBOOT, boot the device and wait two minutes (it restarts
  once by itself).  If that does not help, reflash rc10: your data partition is kept.

Folder "source": the driver patch, the Doom platform layer, the test picture program and
the exact doomgeneric source used (GPL-2.0).  FREEDOOM-COPYING.txt is Freedoom's licence.
