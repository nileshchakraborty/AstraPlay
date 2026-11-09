
add_library(munit "${CMAKE_CURRENT_SOURCE_DIR}/munit/munit.c")
target_include_directories(munit PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/munit")

add_executable(astraplay-unit
		main.c
		http.c
		rpcrypt.c
		gkcrypt.c
		takion.c
		seqnum.c
		keystate.c
		reorderqueue.c
		fec.c
		test_log.c
		test_log.h
		bitstream.c
		regist.c
		cloudcatalog_merge.c
		cloudsession_kamaji.c
		ctrl_lifecycle.c
		pschord.c
		congestioncontrol_test.c
		controller_test.c)

target_link_libraries(astraplay-unit chiaki-lib munit)
if(TARGET PkgConfig::json-c)
	target_link_libraries(astraplay-unit PkgConfig::json-c)
endif()

add_test(unit astraplay-unit)
add_custom_target(chiaki-unit DEPENDS astraplay-unit)
