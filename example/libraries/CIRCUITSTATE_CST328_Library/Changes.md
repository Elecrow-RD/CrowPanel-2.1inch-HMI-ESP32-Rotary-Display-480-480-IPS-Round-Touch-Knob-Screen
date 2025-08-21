

#
### **+05:30 10:02:38 PM 18-02-2025, Tuesday**

  - Added [API](/docs/API.md) documentation.

#
### **+05:30 08:57:27 PM 18-02-2025, Tuesday**

  - Updated constants file.
  - Updated all macro instances.
  - Updated Readme.

#
### **+05:30 11:29:05 PM 17-02-2025, Monday**

  - Added new `Read-Touch-Interrupt.ino` example.
    - Tested and working.
  - Updated Readme.

#
### **+05:30 07:37:25 PM 17-02-2025, Monday**

  - Added `Fast-Read-Touch-Polling` example.
    - This now uses the `fastReadData()` to read a single touch point data.
  - Added `fastReadData()` function.
    - This is faster than `readData()`.
  - Added `isTouched (uint8_t id = 0)` overload.
    - This will only check for the provided touch point for activity.
    - If no parameters are passed, all touch points are checked.

#
### **+05:30 06:18:01 PM 17-02-2025, Monday**

  - Updated `Read-Touch-Polling` example.
    - Removed unnecessary code.
    - Added more documentation.
  - Added `Test` example for internal testing.
  - Updated Readme.
    - Added Adafruit FT6206 library link.
    - Added links to PlatformIO.

#
### **+05:30 09:11:26 PM 16-02-2025, Sunday**

  - Added Arduino library specification files.
  - Updated function documentation.
  - Updated Readme.
  - Updated `touchPoints` array initialization.
  - Added new `inited` variable.
  - `begin()` now returns if already initialized.
  - Moved register write and read functions to `private` scope.

#
### **+05:30 04:38:52 PM 16-02-2025, Sunday**

  - Reading multiple touch points working.
  - Initial commit.
  - New Version `v0.1`.
