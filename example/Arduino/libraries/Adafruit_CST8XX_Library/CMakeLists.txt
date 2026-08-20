# Adafruit CST8XX Library
# MIT License

cmake_minimum_required(VERSION 3.5)

idf_component_register(SRCS "Adafruit_CST8XX.cpp" INCLUDE_DIRS "." REQUIRES arduino-esp32 Adafruit_BusIO)

project(Adafruit_CST8XX)
