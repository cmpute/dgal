# Copyright (c) 2020 Jacob Zhong
# 
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
# 
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
# 
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.
# 

cmake_minimum_required(VERSION 3.4)
project(dgal CXX)

option(DGAL_PYTHON_BINDING "Build python binding for the DGAL" ON)

if (DGAL_PYTHON_BINDING)
    get_filename_component(PDIR ${CMAKE_SOURCE_DIR} DIRECTORY)
    include_directories(${PDIR})
    find_package(pybind11 2.2 REQUIRED)

    pybind11_add_module(dgal geometry_binding.cpp)
    # install(TARGETS geometry DESTINATION ${CMAKE_INSTALL_PREFIX}/python)
    install(TARGETS dgal DESTINATION ${CMAKE_SOURCE_DIR})

endif ()

install(
    FILES geometry.hpp geometry_grad.hpp
    DESTINATION ${CMAKE_INSTALL_PREFIX}/include/dgal
)
