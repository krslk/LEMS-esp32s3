idf_component_register(
  SRCS
  "led.c" # 组件的源文件（相对路径）
  INCLUDE_DIRS
  "." # 头文件目录（供其他文件引用）
  "../../main"
  REQUIRES
  esp_driver_gpio # 依赖的其他组件（如 GPIO 驱动）
)
