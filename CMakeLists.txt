option(WITH_STATIC_LIB "Compile demo with static/shared library, default use static."   ON)
option(SOC_MODE "Compile demo with pcie or soc mode, default use pcie."   OFF)


SET(PADDLE_LIB "" CACHE PATH "Location of libraries")
SET(OPENCV_DIR "" CACHE PATH "Location of libraries")
SET(BM_LIB "" CACHE PATH "Location of libraries")

if(SOC_MODE)
  set (CMAKE_CXX_COMPILER "aarch64-linux-gnu-g++")
  set (CMAKE_C_COMPILER "aarch64-linux-gnu-gcc")
  add_definitions(-DSOC_MODE)
endif()

project(ocr_system CXX C)

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

if(NOT DEFINED BM_LIB)
  message(FATAL_ERROR "please set BM_LIB with -DBM_LIB=/path/lib")
endif()

include_directories("${PADDLE_LIB}/paddle/include")
link_directories("${PADDLE_LIB}/paddle/lib")
include_directories(${OpenCV_INCLUDE_DIRS})

include_directories("${BM_LIB}/include/bmruntime")
include_directories("${BM_LIB}/include/bmlib")
include_directories("${BM_LIB}/include/decode")
include_directories("${BM_LIB}/include/ffmpeg")
include_directories("${BM_LIB}/include/opencv/opencv4")

if(SOC_MODE)
  link_directories("${BM_LIB}/lib/bmnn/soc")
  link_directories("${BM_LIB}/lib/decode/soc")
  link_directories("${BM_LIB}/lib/opencv/soc")
  link_directories("${BM_LIB}/lib/ffmpeg/soc")
else()
  link_directories("${BM_LIB}/lib/bmnn/pcie")
  link_directories("${BM_LIB}/lib/decode/x86")
  link_directories("${BM_LIB}/lib/opencv/x86")
  link_directories("${BM_LIB}/lib/ffmpeg/x86")
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -o3 ${FLAG_OPENMP} -std=c++11")
set(CMAKE_STATIC_LIBRARY_PREFIX "")
message("cmake cxx flags" ${CMAKE_CXX_FLAGS})

if(SOC_MODE)
  include_directories("${CMAKE_SOURCE_DIR}/")
  link_directories("${PADDLE_LIB}/paddle/lib")
  set(EXTERNAL_LIB "-ldl -lrt -lgomp -lz -lm -lpthread")
  set(DEPS ${DEPS} ${EXTERNAL_LIB})
else()
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
endif()

# Note: libpaddle_inference_api.so/a must put before libpaddle_fluid.so/a
if(WITH_STATIC_LIB)
  if(SOC_MODE)
    set(DEPS
    #${PADDLE_LIB}/paddle/lib/libpaddle_api_light_bundled${CMAKE_STATIC_LIBRARY_SUFFIX}
    ${PADDLE_LIB}/paddle/lib/libpaddle_api_full_bundled${CMAKE_STATIC_LIBRARY_SUFFIX})
  else()
    set(DEPS
      ${PADDLE_LIB}/paddle/lib/libpaddle_fluid${CMAKE_STATIC_LIBRARY_SUFFIX})
  endif()
else()
  set(DEPS
      ${PADDLE_LIB}/paddle/lib/libpaddle_fluid${CMAKE_SHARED_LIBRARY_SUFFIX})
endif()

if(SOC_MODE)
  set(EXTERNAL_LIB "-ldl -lrt -lgomp -lm -lpthread")
else()
  set(DEPS ${DEPS} ${MATH_LIB}
        glog gflags protobuf z xxhash)

  if(EXISTS "${PADDLE_LIB}/third_party/install/snappystream/lib")
    set(DEPS ${DEPS} snappystream)
  endif()
  if (EXISTS "${PADDLE_LIB}/third_party/install/snappy/lib")
    set(DEPS ${DEPS} snappy)
  endif()
  set(EXTERNAL_LIB "-ldl -lrt -lgomp -lz -lm -lpthread")
endif()

set(DEPS ${DEPS} ${EXTERNAL_LIB})

set(BM_LIBS "-lbmcpu -lbmcv -lbmlib -lbmrt -lusercpu -lbmjpuapi -lbmjpulite")
set(DEPS ${DEPS} ${BM_LIBS})

set(BM_OpenCV_LIBS "-lopencv_core -lopencv_imgproc -lopencv_imgcodecs \
      -lopencv_videoio -lavcodec -lswresample -lswscale -lavformat -lavutil")
set(DEPS ${DEPS} ${BM_OpenCV_LIBS})


AUX_SOURCE_DIRECTORY(./src SRCS)
add_executable(${DEMO_NAME} ${SRCS})

target_link_libraries(${DEMO_NAME} ${DEPS})

