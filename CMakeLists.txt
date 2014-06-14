project(okular-mupdf)

cmake_minimum_required(VERSION 2.6)

find_package(KDE4 REQUIRED)
find_package(Okular REQUIRED)

include_directories(
  ${KDE4_INCLUDES} ${QT_INCLUDES}
  ${CMAKE_CURRENT_BINARY_DIR}
  ${OKULAR_INCLUDE_DIR}
  mupdf/fitz
  mupdf/pdf
)

add_subdirectory(mupdf)


set(okularGenerator_mupdf_SRCS
  qmupdf.cpp
  generator_mupdf.cpp
)

kde4_add_plugin(okularGenerator_mupdf ${okularGenerator_mupdf_SRCS})

target_link_libraries(okularGenerator_mupdf ${OKULAR_LIBRARIES} ${KDE4_KDEUI_LIBS} ${QT_QTXML_LIBRARY} mupdf-okular)

install(TARGETS okularGenerator_mupdf DESTINATION ${PLUGIN_INSTALL_DIR})

install(FILES libokularGenerator_mupdf.desktop okularMupdf.desktop DESTINATION ${SERVICES_INSTALL_DIR})
install(FILES okularApplication_mupdf.desktop DESTINATION ${XDG_APPS_INSTALL_DIR})

