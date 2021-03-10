project(ocr_system CXX C)

option(WITH_STATIC_LIB "Compile demo with static/shared library, default use static."   ON)


SET(PADDLE_LIB "" CACHE PATH "Location of libraries")
SET(OPENCV_DIR "" CACHE PATH "Location of libraries")
SET(BM_LIB "" CACHE PATH "Location of libraries")

set(DEMO_NAME "ocr_system")

macro(safe_set_static_flag)
    foreach(flag_var
        CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE
        CMAKE_CXX_FLAGS_MINSIZEREL CMAKE_CXX_FLAGS_RELWITHDEBINFO)
      if(${flag_var} MATCHES "/MD")
        string(REGEX REPLACE "/MD" "/MT" ${flag_var} "${${flag_var}}")
      endif(${flag_var} MATCHES "/MD")
    endforeach(flag_var)
endmacro()

if(NOT DEFINED PADDLE_LIB)
  message(FATAL_ERROR "please set PADDLE_LIB with -DPADDLE_LIB=/path/paddle/lib")
endif()

if(NOT DEFINED OPENCV_DIR)
    message(FATAL_ERROR "please set OPENCV_DIR with -DOPENCV_DIR=/path/opencv")
endif()

if(NOT DEFINED BM_LIB)
    message(FATAL_ERROR "please set BM_LIB with -DBM_LIB=/path/lib")
endif()

find_package(OpenCV REQUIRED PATHS ${OPENCV_DIR}/share/OpenCV NO_DEFAULT_PATH)
include_directories("${PADDLE_LIB}/paddle/include")
link_directories("${PADDLE_LIB}/paddle/lib")
include_directories(${OpenCV_INCLUDE_DIRS})

include_directories("${BM_LIB}/include/bmruntime")
include_directories("${BM_LIB}/include/bmlib")
include_directories("${BM_LIB}/include/decode")
link_directories("${BM_LIB}/lib/bmnn/pcie")
link_directories("${BM_LIB}/lib/decode/x86")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -o3 ${FLAG_OPENMP} -std=c++11")
set(CMAKE_STATIC_LIBRARY_PREFIX "")
message("cmake cxx flags" ${CMAKE_CXX_FLAGS})

include_directories("${PADDLE_LIB}/third_party/install/protobuf/include")
include_directories("${PADDLE_LIB}/third_party/install/glog/include")
include_directories("${PADDLE_LIB}/third_party/install/gflags/include")
include_directories("${PADDLE_LIB}/third_party/install/xxhash/include")
include_directories("${PADDLE_LIB}/third_party/install/zlib/include")
include_directories("${PADDLE_LIB}/third_party/boost")
include_directories("${PADDLE_LIB}/third_party/eigen3")

include_directories("${CMAKE_SOURCE_DIR}/")

link_directories("${PADDLE_LIB}/third_party/install/zlib/lib")
link_directories("${PADDLE_LIB}/third_party/install/protobuf/lib")
link_directories("${PADDLE_LIB}/third_party/install/glog/lib")
link_directories("${PADDLE_LIB}/third_party/install/gflags/lib")
link_directories("${PADDLE_LIB}/third_party/install/xxhash/lib")
link_directories("${PADDLE_LIB}/paddle/lib")

set(MATH_LIB ${PADDLE_LIB}/third_party/install/openblas/lib/libopenblas${CMAKE_STATIC_LIBRARY_SUFFIX})

# Note: libpaddle_inference_api.so/a must put before libpaddle_fluid.so/a
if(WITH_STATIC_LIB)
  set(DEPS
      ${PADDLE_LIB}/paddle/lib/libpaddle_fluid${CMAKE_STATIC_LIBRARY_SUFFIX})
else()
  set(DEPS
      ${PADDLE_LIB}/paddle/lib/libpaddle_fluid${CMAKE_SHARED_LIBRARY_SUFFIX})
endif()


set(DEPS ${DEPS} ${MATH_LIB}
  glog gflags protobuf z xxhash)
  
if(EXISTS "${PADDLE_LIB}/third_party/install/snappystream/lib")
  set(DEPS ${DEPS} snappystream)
endif()
if (EXISTS "${PADDLE_LIB}/third_party/install/snappy/lib")
  set(DEPS ${DEPS} snappy)
endif()


set(EXTERNAL_LIB "-ldl -lrt -lgomp -lz -lm -lpthread")
set(DEPS ${DEPS} ${EXTERNAL_LIB})

set(BM_LIBS "-lbmcpu -lbmcv -lbmlib -lbmrt -lusercpu -lbmjpuapi -lbmjpulite")
set(DEPS ${DEPS} ${BM_LIBS})


set(DEPS ${DEPS} ${OpenCV_LIBS})

AUX_SOURCE_DIRECTORY(./src SRCS)
add_executable(${DEMO_NAME} ${SRCS})

target_link_libraries(${DEMO_NAME} ${DEPS})

