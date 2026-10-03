# Run by FetchContent in the Tesseract source directory: drop the
# configure-time Leptonica TIFF probe (see Tesseract.cmake).
file(READ CMakeLists.txt content)
string(REPLACE "check_leptonica_tiff_support()" "# check_leptonica_tiff_support() (disabled by Glimpse)" content "${content}")
file(WRITE CMakeLists.txt "${content}")
