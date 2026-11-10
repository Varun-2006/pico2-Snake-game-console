cmake_minimum_required(VERSION 3.13)

if(NOT DEFINED PICO_SDK_PATH AND DEFINED ENV{PICO_SDK_PATH})
    set(PICO_SDK_PATH "$ENV{PICO_SDK_PATH}")
endif()

if(NOT DEFINED PICO_SDK_PATH)
    message(FATAL_ERROR "PICO_SDK_PATH is not set. Set it to your pico-sdk folder, then run make again.")
endif()

include("${PICO_SDK_PATH}/external/pico_sdk_import.cmake")

project(pico_button_game C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(pico_button_game
    main.c
    src/display_st7789.c
    src/input.c
    src/game.c
    src/sound.c
    src/score_store.c
)
target_include_directories(pico_button_game PRIVATE include)
target_link_libraries(pico_button_game
    pico_stdlib pico_multicore pico_flash pico_util
    hardware_spi hardware_pwm hardware_dma hardware_flash hardware_clocks
)
target_compile_definitions(pico_button_game PRIVATE
    PICO_DEFAULT_SPI=0
    PICO_DEFAULT_SPI_SCK_PIN=18
    PICO_DEFAULT_SPI_TX_PIN=19
)
pico_enable_stdio_usb(pico_button_game 0)
pico_enable_stdio_uart(pico_button_game 0)
pico_override_flash_size(pico_button_game 4190208)
target_link_options(pico_button_game PRIVATE
    "-Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/pico_button_game.map"
)
pico_add_extra_outputs(pico_button_game)
