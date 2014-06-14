project(okular-mupdf)

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules ${CMAKE_MODULE_PATH})
cmake_minimum_required(VERSION 2.6)

find_package(PkgConfig REQUIRED)
pkg_check_modules(OPENJPEG2 REQUIRED libopenjp2)
pkg_check_modules(OPENSSL REQUIRED openssl)

find_package(KDE4 REQUIRED)
find_package(Okular REQUIRED)
find_package(ZLIB REQUIRED)
find_package(Freetype REQUIRED)
find_package(jbig2dec REQUIRED)
find_package(JPEG REQUIRED)

include_directories(
  ${KDE4_INCLUDES} ${QT_INCLUDES}
  ${CMAKE_CURRENT_BINARY_DIR}
  ${OKULAR_INCLUDE_DIR}
)

set(okularGenerator_mupdf_SRCS
  qmupdf.cpp
  generator_mupdf.cpp
)

kde4_add_plugin(okularGenerator_mupdf ${okularGenerator_mupdf_SRCS})

target_link_libraries(okularGenerator_mupdf
	${OKULAR_LIBRARIES} ${KDE4_KDEUI_LIBS} ${QT_QTXML_LIBRARY}
	${ZLIB_LIBRARIES} ${FREETYPE_LIBRARIES} ${JBIG2DEC_LIBRARIES}
	${JPEG_LIBRARIES} ${OPENJPEG2_LIBRARIES} ${OPENSSL_LIBRARIES}
	mupdf mujs
)

install(TARGETS okularGenerator_mupdf DESTINATION ${PLUGIN_INSTALL_DIR})

install(FILES libokularGenerator_mupdf.desktop okularMupdf.desktop DESTINATION ${SERVICES_INSTALL_DIR})
install(FILES okularApplication_mupdf.desktop DESTINATION ${XDG_APPS_INSTALL_DIR})

