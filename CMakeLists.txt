cmake_minimum_required(VERSION 3.15)

add_compile_options(-O3 -s -static)

project("png_converter")
add_executable(png_decoder "src/main.c" "src/png.c" "src/dynamic_array.c" "src/huffman_tree.c" "src/bmp.c")
target_link_libraries(png_decoder m)