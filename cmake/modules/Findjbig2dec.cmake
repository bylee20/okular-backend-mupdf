# - Try to find the jbig2dec library
# Once done this will define
#
#  JBIG2DEC_FOUND - system has jbig2dec
#  JBIG2DEC_INCLUDE_DIRS - the jbig2dec include directories
#  JBIG2DEC_LIBRARIES - Link these to use jbig2dec

# Copyright (c) 2012, Pino Toscano <pino@kde.org>
#
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.

if (JBIG2DEC_LIBRARIES AND JBIG2DEC_INCLUDE_DIRS)

  set(JBIG2DEC_FOUND TRUE)

else (JBIG2DEC_LIBRARIES AND JBIG2DEC_INCLUDE_DIRS)

  find_path(JBIG2DEC_INCLUDE_DIRS jbig2.h)
  find_library(JBIG2DEC_LIBRARIES jbig2dec)

  include(FindPackageHandleStandardArgs)
  find_package_handle_standard_args(jbig2dec DEFAULT_MSG JBIG2DEC_LIBRARIES JBIG2DEC_INCLUDE_DIRS)

endif (JBIG2DEC_LIBRARIES AND JBIG2DEC_INCLUDE_DIRS)
