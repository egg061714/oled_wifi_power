# For more information about build system see
# https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/build-system.html
# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)
set(EXTRA_COMPONENT_DIRS components/ssd1306)
set(PROJECT_PARTITION_TABLE_CUSTOM partitions.csv)
set(PROJECT_PARTITION_TABLE_CUSTOM_FILENAME partitions.csv)


include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(oled_wifi_power)
