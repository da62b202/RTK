if (ITK_USE_CUDA)

  # With MS compilers on Win64, we need the /bigobj switch, else generated
  # code results in objects with number of sections exceeding object file
  # format.
  # see http://msdn.microsoft.com/en-us/library/ms173499.aspx
  if(CMAKE_CL_64 OR CMAKE_COMPILER_2005)
    add_definitions(/bigobj)
  endif()

  itk_module_test()

  set(ITKCudaCommon-tests
    itkCudaImageTest.cxx
    itkCudaReductionTest.cxx
  )

  CreateTestDriver(ITKCudaCommon "${ITKCudaCommon-Test_LIBRARIES}" "${ITKCudaCommon-tests}")
  itk_add_test(NAME itkCudaImageTest COMMAND ITKCudaCommonTestDriver itkCudaImageTest)
  itk_add_test(NAME itkCudaReductionTest COMMAND ITKCudaCommonTestDriver itkCudaReductionTest)

endif()
