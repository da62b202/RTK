# client maintainer: simon.rit@creatis.insa-lyon.fr
set(CTEST_SITE "linux15.dg.creatis.insa-lyon.fr")
set(CTEST_BUILD_NAME "Linux-64bit-gcc-cuda-system-itk420")
set(CTEST_BUILD_CONFIGURATION Release)
set(CTEST_CMAKE_GENERATOR "Unix Makefiles")
set(CTEST_DASHBOARD_ROOT "/tmp/RTK_dashboard")
set(dashboard_binary_name "RTK_lin64_gcc_cuda_system_itk420")
set(ENV{ITK_DIR} "/home/srit/src/itk420/lin64-dg/lib/cmake/ITK-4.2")
set(ENV{CUDA_BIN_PATH} "/usr/bin")
set(ENV{CUDA_LIB_PATH} "/usr/lib64")
set(CTEST_BUILD_FLAGS -j12)

set(ENV{LD_LIBRARY_PATH} "/usr/lib64:$ENV{LD_LIBRARY_PATH}")
set(CONFIGURE_OPTIONS
   -DRTK_USE_CUDA:BOOL=ON
   -DCUDA_CUDA_LIBRARY:PATH=/usr/lib64/libcuda.so
  )

set(ENV{CC} "/home/srit/src/gcc/gcc472-install/bin/gcc")
set(ENV{CXX} "/home/srit/src/gcc/gcc472-install/bin/c++")
set(ENV{LD_LIBRARY_PATH} "/home/srit/src/gcc/gcc472-install/lib64:/home/srit/src/gcc/gcc472-install/lib:$ENV{LD_LIBRARY_PATH}")
set(ENV{PATH} "/home/srit/src/gcc/gcc472-install/bin:/usr/local/bin:/bin:/usr/bin:/usr/local/sbin:/usr/sbin:/sbin:/home/srit/src/gcc/gcc436-install/bin")

include("${CTEST_SCRIPT_DIRECTORY}/rtk_common.cmake")

