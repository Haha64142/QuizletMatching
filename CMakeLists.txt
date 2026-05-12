cmake_minimum_required(VERSION 3.10)

project(QuizletMatching)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CPACK_GENERATOR "ZIP")
include(CPack)

add_executable(QuizletMatching src/main.cpp)
target_link_libraries(QuizletMatching PRIVATE tesseract)

install(TARGETS QuizletMatching DESTINATION .)

install(DIRECTORY C:/msys64/ucrt64/share/tessdata/ DESTINATION .)
install(
  FILES C:/msys64/ucrt64/bin/libgcc_s_seh-1.dll
        C:/msys64/ucrt64/bin/libwinpthread-1.dll
        C:/msys64/ucrt64/bin/libstdc++-6.dll
        C:/msys64/ucrt64/bin/libtesseract-5.5.dll
        C:/msys64/ucrt64/bin/libarchive-13.dll
        C:/msys64/ucrt64/bin/libcurl-4.dll
        C:/msys64/ucrt64/bin/libgomp-1.dll
        C:/msys64/ucrt64/bin/libleptonica-6.dll
        C:/msys64/ucrt64/bin/libb2-1.dll
        C:/msys64/ucrt64/bin/libbz2-1.dll
        C:/msys64/ucrt64/bin/libcrypto-3-x64.dll
        C:/msys64/ucrt64/bin/libexpat-1.dll
        C:/msys64/ucrt64/bin/libiconv-2.dll
        C:/msys64/ucrt64/bin/liblz4.dll
        C:/msys64/ucrt64/bin/liblzma-5.dll
        C:/msys64/ucrt64/bin/zlib1.dll
        C:/msys64/ucrt64/bin/libzstd.dll
        C:/msys64/ucrt64/bin/libbrotlidec.dll
        C:/msys64/ucrt64/bin/libidn2-0.dll
        C:/msys64/ucrt64/bin/libnghttp2-14.dll
        C:/msys64/ucrt64/bin/libnghttp3-9.dll
        C:/msys64/ucrt64/bin/libngtcp2-16.dll
        C:/msys64/ucrt64/bin/libngtcp2_crypto_ossl-0.dll
        C:/msys64/ucrt64/bin/libpsl-5.dll
        C:/msys64/ucrt64/bin/libssh2-1.dll
        C:/msys64/ucrt64/bin/libssl-3-x64.dll
        C:/msys64/ucrt64/bin/libbrotlicommon.dll
        C:/msys64/ucrt64/bin/libintl-8.dll
        C:/msys64/ucrt64/bin/libunistring-5.dll
        C:/msys64/ucrt64/bin/libgif-7.dll
        C:/msys64/ucrt64/bin/libjpeg-8.dll
        C:/msys64/ucrt64/bin/libopenjp2-7.dll
        C:/msys64/ucrt64/bin/libpng16-16.dll
        C:/msys64/ucrt64/bin/libtiff-6.dll
        C:/msys64/ucrt64/bin/libwebp-7.dll
        C:/msys64/ucrt64/bin/libwebpmux-3.dll
        C:/msys64/ucrt64/bin/libdeflate.dll
        C:/msys64/ucrt64/bin/libjbig-0.dll
        C:/msys64/ucrt64/bin/libLerc.dll
        C:/msys64/ucrt64/bin/libsharpyuv-0.dll
  DESTINATION .)
