# ESP32-S3 2.1-inch course examples

对应 `1.28Examples` 的六个课程案例，功能保持一致，已适配 480×480 ST7701 RGB 屏、CST8XX 触摸、PCF8574 电源/复位控制和 2.1 英寸板卡引脚。每个目录都是可直接在 Arduino IDE 打开的独立草图。

硬件映射：每个课程目录都包含独立的 `Board21.h` 和 `Board21.cpp`，显示 RGB 引脚及时序定义在该课程目录内；I2C 为 GPIO38/39；触摸地址 0x15；旋钮 A/B 为 GPIO42/4；背光 GPIO6；板载 LED GPIO43；USB 使用 ESP32-S3 原生 USB。各课程可直接在自己的目录中打开，不依赖其他课程目录的板卡支持文件。

