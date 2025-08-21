# CSE_CST328 Library API Reference

Version 0.1, +05:30 09:01:36 PM 18-02-2025, Tuesday

## Index

- [CSE\_CST328 Library API Reference](#cse_cst328-library-api-reference)
  - [Index](#index)
  - [Dependencies](#dependencies)
  - [Configuration](#configuration)
  - [Constants](#constants)
  - [Classes](#classes)
  - [Class `TS_Point`](#class-ts_point)
    - [Variables](#variables)
    - [`TS_Point()`](#ts_point)
  - [Class `CSE_CST328`](#class-cse_cst328)
    - [Variables](#variables-1)
    - [`CSE_CST328()`](#cse_cst328)
    - [`begin()`](#begin)
    - [`readData()`](#readdata)
    - [`fastReadData()`](#fastreaddata)
    - [`getTouches()`](#gettouches)
    - [`isTouched()`](#istouched)
    - [`getPoint()`](#getpoint)
    - [`setRotation()`](#setrotation)
    - [`getRotation()`](#getrotation)
    - [`getWidth()`](#getwidth)
    - [`getHeight()`](#getheight)
    - [`writeRegister8()`](#writeregister8)
    - [`readRegister8()`](#readregister8)
    - [`readRegister32()`](#readregister32)
    - [`write16()`](#write16)


## Dependencies

- None

## Configuration

The debug output is controlled by the `DEBUG_SERIAL` macro.

## Constants

All of the constants are defined in the [`CSE_CST328_Constants.h`](../src/CSE_CST328_Constants.h) file.

## Classes

- `CSE_CST328` - The main class for wrapping the data and functions of the library.
- `TS_Point` - A class to store touch points.

## Class `TS_Point`

### Variables

#### Public

- `uint8_t touchId` : ID of the touch point. Initializes to `0`.
- `int16_t x` : X coordinate. Initializes to `0`.
- `int16_t y` : Y coordinate. Initializes to `0`.
- `int16_t z` : Z coordinate. Often used for indicating pressure. Initializes to `0`.
- `uint8_t state` : State of the touch point (touched = 1, not touched = 0). Initializes to `0`.

### `TS_Point()`

Constructor for the `TS_Point` class. There are two overloads.

- `TS_Point()` : Initializes all variables to `0`.
- `TS_Point (int16_t x, int16_t y, int16_t z, uint8_t id)` : Initializes all variables.

#### Syntax 1

```cpp
TS_Point (void);
```

Initializes all variables to `0`.

##### Parameters

- None

##### Returns

- `TS_Point` object.

#### Syntax 2

```cpp
TS_Point (int16_t x, int16_t y, int16_t z, uint8_t id);
```

Accepts 4 initial parameters.

##### Parameters

- `x` : X coordinate.
- `y` : Y coordinate.
- `z` : Z coordinate.
- `id` : Unique finger ID for the point.

##### Returns

- `TS_Point` object.

## Class `CSE_CST328`

### Variables

#### Public

- `uint16_t defWidth` - Default width of the screen. This is used when rotating the screen.
- `uint16_t defHeight` - Default height of the screen. This is used when rotating the screen.
- `uint16_t width` - Current width of the screen.
- `uint16_t height` - Current height of the screen.
- `uint8_t rotation` - Current rotation of the screen.
- `TS_Point touchPoints [5]` - Array of 5 touch points.

#### Private

- `TwoWire *wireInstance` - A pointer to the I2C bus.
- `int8_t pinReset` - Chip reset pin.
- `int8_t pinInterrupt` - Chip interrupt pin.
- `bool inited` - Flag to indicate if the library has been initialized.

### `CSE_CST328()`

Constructor for the `CSE_CST328` class.

#### Syntax

```cpp
CSE_CST328 (uint16_t width, uint16_t height, TwoWire *i2c = &Wire, int8_t pinRst = -1, int8_t pinIrq = -1);
```

Default width and height are set to the initial `width` and `height`. The `touchPoints` array is initialized with all 0s, but with 0~4 `touchId`s. The default `rotation` is set to 0 and the `inited` flag is set to `false`.

##### Parameters

- `width` : Width of the screen.
- `height` : Height of the screen.
- `i2c` : Pointer to the I2C bus.
- `pinRst` : Chip reset pin. Defaults to `-1` for no reset pin.
- `pinIrq` : Chip interrupt pin. Defaults to `-1` for no interrupt pin.

##### Returns

- `CSE_CST328` object.

### `begin()`

Initializes the library and the touch panel. This must be called first before using any other functions in the library. If the library is already initialized, this function will return immediately. The Wire instance is also initialized by default. If the reset pin is present (not -1), a reset sequence will be performed. If the interrupt pin is present (not -1), the pin is set to `INPUT_PULLUP`.

The chip is detected by reading the constant `0xCACA` value at the `REG_CST328_INFO_3` register. If the value is not found, the initialization will fail. After successful reading, the chip is put to normal mode.

#### Syntax

```cpp
bool begin();
```

##### Parameters

- None

##### Returns

- `bool` : `true` if initialization was successful, `false` otherwise.

### `readData()`

Reads the entire set of touch information registers from the CST328. The total number of registers is 27 bytes. The read data is saved to the `touchPoints` array after conversions and rotations. Since this function reads all data at a time, this can be slower. If you want to read the registers faster, try [`fastReadData()`](#fastreaddata).

#### Syntax

```cpp
void readData();
```

##### Parameters

- None

##### Returns

- None.

### `fastReadData()`

This reads the set of registers for a single finger or touch point. You can pass the id of the finger (0~4) to read the data. This operation is faster than [`readData()`](#readdata). Data is saved to the `touchPoints` array after applying conversions and rotations to the read data. Other touch points are unchanged. Since this function only reads one touch point at a time, the data of other touch points in the `touchPoints` array may be outdated.

#### Syntax

```cpp
void fastReadData (uint8_t id = 0);
```

##### Parameters

- `id` : The finger ID of the point as per the data sheet. This is the index of the finger data in the `touchPoints` array. For 5 touch points, the range is 0~4.

##### Returns

- None.

### `getTouches()`

Returns the number of active touches. Even though there is a dedicated register (`0xD005`) for reporting the number of touches, the register is not updated when the touches are lifted. That means, you can not rely on the register value to determine the number of active touches. To solve this problem, `getTouches()` reads all touch data from the chip and check the touch state of all points individually. The total count is then returned.

If you do not want to find the number of touches this way, you can try reading the register `0xD005` directly.

#### Syntax

```cpp
uint8_t getTouches();
```

##### Parameters

- None

##### Returns

- `uint8_t` : Number of active touches.

### `isTouched()`

Checks if the touch panel is touched or a specific touch point is being touched. There are two overloads.

#### Syntax 1

```cpp
bool isTouched();
```

Check if the screen is being touched by checking all touch points.

##### Parameters

- None

##### Returns

- `bool` : `true` if the screen is being touched, `false` otherwise.

#### Syntax 2

```cpp
bool isTouched (uint8_t id = 0);
```

Check if a specific touch point is being touched.

##### Parameters

- `id` : The finger ID of the point as per the data sheet. This is the index of the finger data in the `touchPoints` array. For 5 touch points, the range is 0~4.

##### Returns

- `bool` : `true` if the touch point is being touched, `false` otherwise.

### `getPoint()`

Returns the coordinates of the touch point. You need to send the finger ID to get the data. The data in the `touchPoints` array is returned. New data is not read from the chip while doing this. You need to manually call [`readData()`](#readdata) or [`fastReadData()`](#fastreaddata) to update the data in the array. This is done so to give you control of when to read the data from the chip.

#### Syntax

```cpp
TS_Point getPoint (uint8_t id = 0);
```

##### Parameters

- `id` : The finger ID of the point as per the data sheet. This is the index of the finger data in the `touchPoints` array. For 5 touch points, the range is 0~4.

##### Returns

- `TS_Point` object.

### `setRotation()`

Sets the rotation of the touch panel. The rotation of the touch panel is relative to the width and height you set during initialization. This function sets the rotation and updates the `width` and `height` variables accordingly.

If you think that the touch panel coordinates are rotated from the rotation of your screen, try the other possible rotation values. One of them will match your screen rotation.

#### Syntax

```cpp
void setRotation (uint8_t rotation);
```

##### Parameters

- `rotation` : Possible values are 0, 1, 2, 3.

##### Returns

- `uint8_t` : Current rotation. The possible values are 0, 1, 2, 3.

### `getRotation()`

Returns the current rotation of the touch panel.

#### Syntax

```cpp
uint8_t getRotation();
```

##### Parameters

- None

##### Returns

- `uint8_t` : Current rotation. The possible values are 0, 1, 2, 3.

### `getWidth()`

Returns the current width of the touch panel. If you apply rotations, the width and height will be different. The default width and height are always stored in the `defWidth` and `defHeight` variables.

#### Syntax

```cpp
uint16_t getWidth();
```

##### Parameters

- None

##### Returns

- `uint16_t` : Current width.

### `getHeight()`

Returns the current height of the touch panel. If you apply rotations, the width and height will be different. The default width and height are always stored in the `defWidth` and `defHeight` variables.

#### Syntax

```cpp
uint16_t getHeight();
```

##### Parameters

- None

##### Returns

- `uint16_t` : Current height.

### `writeRegister8()`

Writes a single byte to a register in the chip. Since CST328 have all 16-bit register addresses, you need to send the 16-bit addresses.

#### Syntax

```cpp
void writeRegister8 (uint16_t reg, uint8_t val);
```

##### Parameters

- `reg` : 16-bit address of the register.
- `val` : Data to be written to the register.

##### Returns

- None.

### `readRegister8()`

Reads a single byte from a register in the chip. Since CST328 have all 16-bit register addresses, you need to send the 16-bit addresses.

#### Syntax

```cpp
uint8_t readRegister8 (uint16_t reg);
```

##### Parameters

- `reg` : 16-bit address of the register.

##### Returns

- `uint8_t` : Data read from the register.

### `readRegister32()`

Reads a 32-bit value (four bytes) from a register in the chip. Since CST328 have all 16-bit register addresses, you need to send the 16-bit addresses.

#### Syntax

```cpp
uint32_t readRegister32 (uint16_t reg);
```

##### Parameters

- `reg` : 16-bit address of the register.

##### Returns

- `uint32_t` : Data read from the registers.

### `write16()`

This is a special function that simply writes the register address as specified in the datasheet. It does not accept any values. Since CST328 have all 16-bit register addresses, you need to send the 16-bit addresses.

#### Syntax

```cpp
void write16 (uint16_t reg);
```

##### Parameters

- `reg` : 16-bit address of the register.

##### Returns

- None.

