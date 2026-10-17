
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

if(APPLE)
	enable_language(OBJCXX)
endif()

find_package(Qt6 REQUIRED COMPONENTS Core Gui Concurrent Svg Qml Quick Widgets Network)
if(Qt6_VERSION VERSION_GREATER_EQUAL "6.10.0")
	find_package(Qt6 REQUIRED COMPONENTS GuiPrivate)
endif()

# CHIAKI_ENABLE_GUI_WEBENGINE: root CMakeLists.txt (OFF on Linux; App Store passes OFF via ASTRAPLAY_EXTRA_CMAKE_ARGS)
if(CHIAKI_ENABLE_GUI_WEBENGINE)
	# MinGW/MSYS2 Qt6 ships no WebEngine module: degrade to external-browser
	# PSN login instead of failing configure where it is unavailable.
	find_package(Qt6 QUIET COMPONENTS WebEngineQuick)
	if(NOT Qt6WebEngineQuick_FOUND)
		message(STATUS "Qt6 WebEngineQuick not found, building without embedded web engine (PSN login uses external browser)")
		set(CHIAKI_ENABLE_GUI_WEBENGINE OFF)
	endif()
endif()
find_package(Qt6 COMPONENTS DBus)

if(WIN32)
	add_definitions(-DWIN32_LEAN_AND_MEAN)
endif()

set(SOURCE_FILES
	include/exception.h
	src/main.cpp
	include/discoverymanager.h
	src/discoverymanager.cpp
	include/streamsession.h
	src/streamsession.cpp
	include/sessionlog.h
	src/sessionlog.cpp
	include/settings.h
	src/settings.cpp
	include/host.h
	src/host.cpp
	include/embeddedrelayserver.h
	src/embeddedrelayserver.cpp
	res/resources.qrc
	res/license.qrc
	res/relay/relay.qrc
	include/controllermanager.h
	src/controllermanager.cpp
	include/psnaccountid.h
	src/psnaccountid.cpp
	include/psnaccountid_v3.h
	src/psnaccountid_v3.cpp
	include/psntoken.h
	src/psntoken.cpp
	include/cloudstreamingbackend.h
	src/cloudstreamingbackend.cpp
	include/cloudcatalogbackend.h
	src/cloudcatalogbackend.cpp
	include/jsonrequester.h
	src/jsonrequester.cpp
	src/qml/qml.qrc
	include/qmlmainwindow.h
	src/qmlmainwindow.cpp
	include/qmlbackend.h
	src/qmlbackend.cpp
	include/qmlgamesbackend.h
	src/qmlgamesbackend.cpp
	include/qmlcontroller.h
	src/qmlcontroller.cpp
	include/qmlsettings.h
	src/qmlsettings.cpp
	include/qmlsvgprovider.h
	src/qmlsvgprovider.cpp
	include/systemdinhibit.h
	src/systemdinhibit.cpp
	include/gamelauncher.h
	src/gamelauncher.cpp
	include/updatemanager.h
	src/updatemanager.cpp
	include/qrimage.h
	src/qrimage.cpp
	thirdparty/qrcodegen/qrcodegen.cpp
	)
set(RESOURCE_FILES "")

if(APPLE)
	list(APPEND RESOURCE_FILES "astraplay.icns")
elseif(WIN32)
	configure_file(
		${CMAKE_CURRENT_SOURCE_DIR}/astraplay.rc.in
		${CMAKE_CURRENT_SOURCE_DIR}/astraplay.rc
		@ONLY)
	list(APPEND RESOURCE_FILES "astraplay.rc")
endif()

if(CHIAKI_ENABLE_STEAMDECK_NATIVE AND LINUX)
	list(APPEND SOURCE_FILES
		include/sdinputcontext.h
		src/sdinputcontext.cpp
	)
endif()

if(CHIAKI_ENABLE_STEAMWORKS)
	list(APPEND SOURCE_FILES
		include/steamworks/steamworks_wrapper.h
		src/steamworks/steamworks_api.cpp
		include/steamworks/steamworks_cloud_sync.h
		src/steamworks/steamworks_cloud_sync.cpp
	)
endif()

if(APPLE)
	list(APPEND SOURCE_FILES
		include/macMicPermission.h
		src/macMicPermission.m
		include/macWakeSleep.h
		src/macWakeSleep.cpp
	)
elseif(WIN32)
	list(APPEND SOURCE_FILES
		include/windowsWakeSleep.h
		src/windowsWakeSleep.cpp
	)
endif()

add_executable(astraplay
		${RESOURCE_FILES}
		${SOURCE_FILES}
)
target_include_directories(astraplay PRIVATE include thirdparty)

add_library(placebo-libav-impl src/libav_impl.c)
target_link_libraries(placebo-libav-impl PkgConfig::LIBPLACEBO FFMPEG::avcodec FFMPEG::avutil FFMPEG::avformat)
target_link_libraries(astraplay PkgConfig::LIBPLACEBO placebo-libav-impl)

target_link_libraries(astraplay chiaki-lib)
if(CHIAKI_ENABLE_CLI)
	add_definitions(-DCHIAKI_ENABLE_CLI)
	target_link_libraries(astraplay chiaki-cli-lib)
endif()

target_link_libraries(astraplay Qt::Core Qt::Gui Qt::GuiPrivate Qt::Concurrent Qt::Svg Qt::Qml Qt::Quick Qt::Widgets Qt::Network)
if (${Qt6DBus_FOUND} AND NOT WIN32 AND NOT APPLE)
	target_link_libraries(astraplay Qt::DBus)
	target_compile_definitions(astraplay PRIVATE CHIAKI_HAVE_DBUS)
endif()
if(CHIAKI_ENABLE_GUI_WEBENGINE AND Qt6WebEngineQuick_FOUND)
	target_link_libraries(astraplay Qt::WebEngineQuick)
	target_compile_definitions(astraplay PRIVATE CHIAKI_HAVE_WEBENGINE)
endif()
target_link_libraries(astraplay SDL2::SDL2)
if(CHIAKI_ENABLE_SPEEX)
  target_link_libraries(astraplay PkgConfig::SpeexDSP)
  target_compile_definitions(astraplay PRIVATE CHIAKI_GUI_ENABLE_SPEEX)
endif()
if(CHIAKI_GUI_ENABLE_SDL_GAMECONTROLLER)
	target_compile_definitions(astraplay PRIVATE CHIAKI_GUI_ENABLE_SDL_GAMECONTROLLER)
endif()
if(CHIAKI_ENABLE_SETSU)
	target_link_libraries(astraplay setsu)
	target_compile_definitions(astraplay PRIVATE CHIAKI_GUI_ENABLE_SETSU)
endif()
if(CHIAKI_ENABLE_STEAMDECK_NATIVE)
	target_link_libraries(astraplay sdeck)
	target_compile_definitions(astraplay PRIVATE QT_STATICPLUGIN CHIAKI_GUI_ENABLE_STEAMDECK_NATIVE)
endif()
if(CHIAKI_ENABLE_STEAM_SHORTCUT)
	target_link_libraries(astraplay cpp-steam-tools)
	target_compile_definitions(astraplay PRIVATE CHIAKI_GUI_ENABLE_STEAM_SHORTCUT)
endif()
if(CHIAKI_ENABLE_STEAMWORKS)
	target_link_libraries(astraplay steamworks)
	target_compile_definitions(astraplay PRIVATE CHIAKI_ENABLE_STEAMWORKS)
endif()
if(CHIAKI_IS_MAC_APPSTORE)
	target_compile_definitions(astraplay PRIVATE CHIAKI_IS_MAC_APPSTORE)
	target_sources(astraplay PRIVATE
		include/appreviewmanager.h
		src/appreviewmanager.cpp
		src/appreviewbridge.mm
	)
	target_link_libraries(astraplay "-framework StoreKit")
endif()
if(NOT CHIAKI_ENABLE_FFMPEG_DECODER)
	message(FATAL_ERROR "AstraPlay GUI requires CHIAKI_ENABLE_FFMPEG_DECODER=ON")
endif()
if(APPLE)
	target_link_libraries(astraplay "-framework AVFoundation" "-framework IOKit")
endif()

# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fanalyzer")
if(CHIAKI_IS_MAC_APPSTORE AND DEFINED ASTRAPLAY_MACOS_BUILD_NUMBER AND NOT "${ASTRAPLAY_MACOS_BUILD_NUMBER}" STREQUAL "")
	set(MACOSX_BUNDLE_BUILD_VERSION "${ASTRAPLAY_MACOS_BUILD_NUMBER}")
else()
	set(MACOSX_BUNDLE_BUILD_VERSION "${CHIAKI_VERSION}")
endif()

if(APPLE)
	set_target_properties(astraplay PROPERTIES
			OUTPUT_NAME AstraPlay
			MACOSX_BUNDLE TRUE
			MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/MacOSXBundleInfo.plist.in"
			MACOSX_BUNDLE_BUNDLE_NAME AstraPlay
			MACOSX_BUNDLE_EXECUTABLE_NAME AstraPlay
			MACOSX_BUNDLE_SHORT_VERSION_STRING "${CHIAKI_VERSION}"
			MACOSX_BUNDLE_BUNDLE_VERSION "${MACOSX_BUNDLE_BUILD_VERSION}"
			MACOSX_BUNDLE_COPYRIGHT "thestr4ng3r (AGPLv3)"
			MACOSX_BUNDLE_GUI_IDENTIFIER "io.github.nileshchakraborty.astraplay"
			MACOSX_BUNDLE_ICON_FILE astraplay.icns
			RESOURCE "${RESOURCE_FILES}")
	add_custom_command(TARGET astraplay POST_BUILD
			COMMAND codesign --force --deep --sign - "$<TARGET_BUNDLE_DIR:astraplay>"
			COMMENT "Ad-hoc signing AstraPlay.app bundle for macOS local network permission compatibility")
else()
	set_target_properties(astraplay PROPERTIES
			OUTPUT_NAME astraplay)
endif()

if(WIN32)
	# Allow disabling WIN32_EXECUTABLE for debugging (e.g., fastbuild.bat sets CHIAKI_ENABLE_CONSOLE=ON)
	if(NOT CHIAKI_ENABLE_CONSOLE)
		set_target_properties(astraplay PROPERTIES WIN32_EXECUTABLE TRUE)
	endif()
endif()

install(TARGETS astraplay
		RUNTIME DESTINATION bin
		BUNDLE DESTINATION bin)
# [ASTRAPLAY BEGIN] Install metadata under the canonical Flatpak/AppStream ID.
install(FILES io.github.nileshchakraborty.AstraPlay.desktop DESTINATION share/applications)
install(FILES astraplay-512.png DESTINATION share/icons/hicolor/512x512/apps
		RENAME astraplay.png)
install(FILES io.github.nileshchakraborty.AstraPlay.metainfo.xml DESTINATION share/metainfo)
# [ASTRAPLAY END]

if(CHIAKI_ENABLE_TESTS)
	enable_testing()
	find_package(Qt6 REQUIRED COMPONENTS Test)
	add_executable(chiaki-qt-test
		test/embeddedrelayserver_bdd.cpp
		include/embeddedrelayserver.h
		src/embeddedrelayserver.cpp
	)
	target_include_directories(chiaki-qt-test PRIVATE include)
	target_link_libraries(chiaki-qt-test PRIVATE Qt::Core Qt::Network Qt::Test)
	add_test(NAME qt-embedded-relay-bdd COMMAND chiaki-qt-test)

	add_executable(chiaki-updater-test
		test/updatemanager_test.cpp
		include/updatemanager.h
		src/updatemanager.cpp
	)
	target_include_directories(chiaki-updater-test PRIVATE include ${CMAKE_SOURCE_DIR}/lib/include ${CMAKE_BINARY_DIR}/lib/include)
	target_link_libraries(chiaki-updater-test PRIVATE Qt::Core Qt::Gui Qt::Network Qt::Test)
	add_test(NAME qt-updater-test COMMAND chiaki-updater-test)
endif()
