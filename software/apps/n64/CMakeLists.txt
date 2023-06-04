# Replace TMDS with 10 bit UART (same baud rate):
# add_definitions(-DDVI_SERIAL_DEBUG=1)
# add_definitions(-DRUN_FROM_CRYSTAL)

add_executable(n64
	config.c
	gfx.c
	joybus.c
	main.c
	osd.c
)

target_compile_definitions(n64 PRIVATE
	DVI_DEFAULT_SERIAL_CONFIG=${DVI_DEFAULT_SERIAL_CONFIG}
	)

target_link_libraries(n64
	pico_stdlib
	pico_multicore
	pico_util
	libdvi
	libsprite
	hardware_pio
)

# Build pio
pico_generate_pio_header(n64 ${CMAKE_CURRENT_LIST_DIR}/joybus.pio)
pico_generate_pio_header(n64 ${CMAKE_CURRENT_LIST_DIR}/n64.pio)


# create map/bin/hex file etc.
pico_add_extra_outputs(n64)
