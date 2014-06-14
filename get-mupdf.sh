#!/bin/sh
if [ -d mupdf/.git ]; then
  (cd mupdf ; git pull)
else
  git clone git://git.ghostscript.com/mupdf.git
fi
if [ ! -L mupdf/CMakeLists.txt ]; then
  ln -s ../CMakeLists-fitz.txt mupdf/CMakeLists.txt
fi
if [ ! -L mupdf/cmake ]; then
  ln -s ../cmake mupdf/
fi

