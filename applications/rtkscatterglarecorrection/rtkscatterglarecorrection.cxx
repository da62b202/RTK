/*=========================================================================
 *
 *  Copyright RTK Consortium
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *         http://www.apache.org/licenses/LICENSE-2.0.txt
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 *=========================================================================*/

#include "rtkscatterglarecorrection_ggo.h"
#include "rtkMacro.h"
#include "rtkGgoFunctions.h"

#include <itkExtractImageFilter.h>
#include <itkSubtractImageFilter.h>
#ifdef RTK_USE_CUDA
    #include "rtkCudaScatterGlareCorrectionImageFilter.h"
#else
    #include "rtkScatterGlareCorrectionImageFilter.h"
#endif

#include "rtkProjectionsReader.h"
#include <itkPasteImageFilter.h>
#include <rtkConstantImageSource.h>
#include <itkImageFileWriter.h>

#include <vector>
#include <algorithm>
#include <string>
#include <chrono>

int main(int argc, char *argv[])
{
  GGO(rtkscatterglarecorrection, args_info);

  typedef float InputPixelType;
  const unsigned int Dimension = 3;
#ifdef RTK_USE_CUDA
  typedef itk::CudaImage< InputPixelType, Dimension > InputImageType;
#else
  typedef itk::Image< InputPixelType, Dimension > InputImageType;
#endif
  
  typedef itk::RegularExpressionSeriesFileNames RegexpType;
  RegexpType::Pointer names = RegexpType::New();
  names->SetDirectory(args_info.path_arg);
  names->SetNumericSort(args_info.nsort_flag);
  names->SetRegularExpression(args_info.regexp_arg);

  typedef rtk::ProjectionsReader< InputImageType > ReaderType;  // Warning: preprocess images
  ReaderType::Pointer reader = ReaderType::New();
  reader->SetFileNames( names->GetFileNames() );
  reader->ComputeLineIntegralOff();
  reader->UpdateOutputInformation();

  // Input projection parameters
  InputImageType::SizeType    sizeInput = reader->GetOutput()->GetLargestPossibleRegion().GetSize();;
  int Nproj = sizeInput[2];

  typedef itk::ExtractImageFilter< InputImageType, InputImageType > ExtractFilterType;
   
  std::vector<float> coef;
  if (args_info.coefficients_given == 2) {
    coef.push_back(args_info.coefficients_arg[0]);
    coef.push_back(args_info.coefficients_arg[1]);
  }
  else {
    coef.push_back(0.0787f);
    coef.push_back(106.244f);
  }
   
#ifdef RTK_USE_CUDA
  typedef rtk::CudaScatterGlareCorrectionImageFilter ScatterCorrectionType;
#else
  typedef rtk::ScatterGlareCorrectionImageFilter<InputImageType, InputImageType, float>   ScatterCorrectionType;
#endif
  ScatterCorrectionType::Pointer SFilter = ScatterCorrectionType::New();
  SFilter->SetTruncationCorrection(0.0);
  SFilter->SetCoefficients(coef);
  
  typedef rtk::ConstantImageSource<InputImageType> ConstantImageSourceType;
  ConstantImageSourceType::Pointer constantSource = ConstantImageSourceType::New();
        
  typedef itk::PasteImageFilter <InputImageType, InputImageType > PasteImageFilterType;
  PasteImageFilterType::Pointer paste = PasteImageFilterType::New();
  paste->SetSourceImage(SFilter->GetOutput());
  paste->SetDestinationImage(constantSource->GetOutput());
   
  std::cout << "Starting processing" << std::endl;
  int projid = 0;
  bool first = true;
  std::vector<int> avgTimings;
  while (projid < Nproj)
  {
      int curBufferSize = std::min(args_info.bufferSize_arg, Nproj - projid);

      InputImageType::RegionType sliceRegionA = reader->GetOutput()->GetLargestPossibleRegion();
      InputImageType::SizeType  sizeA = sliceRegionA.GetSize();
      sizeA[2] = curBufferSize;
      InputImageType::IndexType start = sliceRegionA.GetIndex();
      start[2] = projid;
      InputImageType::RegionType desiredRegionA;
      desiredRegionA.SetSize(sizeA);
      desiredRegionA.SetIndex(start);

      ExtractFilterType::Pointer extract = ExtractFilterType::New();
      extract->SetDirectionCollapseToIdentity();
      extract->SetExtractionRegion(desiredRegionA);
      extract->SetInput(reader->GetOutput());
      extract->Update();
            
      InputImageType::Pointer image = extract->GetOutput();
      image->DisconnectPipeline();

      auto t0 = std::chrono::high_resolution_clock::now();

      SFilter->SetInput(image);
      SFilter->GetOutput()->SetRequestedRegion(image->GetRequestedRegion());
      SFilter->Update();

      auto t1 = std::chrono::high_resolution_clock::now();
      float rrtime = static_cast<float>(std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count()) / static_cast<float>(curBufferSize);

      if (projid > 0) // Because first timing includes filter initialization time
      {
          avgTimings.push_back(rrtime);
      }
      std::cout << "Timing per projection: " << rrtime << " usec" << std::endl;

      InputImageType::Pointer procImage = SFilter->GetOutput();
      procImage->DisconnectPipeline();

      InputImageType::Pointer outImage;
      if (args_info.difference_flag)
      {
          typedef itk::SubtractImageFilter <InputImageType, InputImageType> SubtractImageFilterType;
          SubtractImageFilterType::Pointer subtractFilter = SubtractImageFilterType::New();
          subtractFilter->SetInput1(image);
          subtractFilter->SetInput2(procImage);
          subtractFilter->Update();
          outImage = subtractFilter->GetOutput();
          outImage->DisconnectPipeline();
      }
      else{
          outImage = procImage;
      }

      InputImageType::IndexType current_idx = outImage->GetLargestPossibleRegion().GetIndex();
      current_idx[2] = projid;

      if (first) {
          // Initialization of the output volume
          InputImageType::SizeType    sizeInput = outImage->GetLargestPossibleRegion().GetSize();
          sizeInput[2] = Nproj;
          InputImageType::SpacingType spacingInput = outImage->GetSpacing();
          InputImageType::PointType   originInput = outImage->GetOrigin();
          InputImageType::DirectionType imageDirection;
          imageDirection.SetIdentity();

          constantSource->SetOrigin(originInput);
          constantSource->SetSpacing(spacingInput);
          constantSource->SetDirection(imageDirection);
          constantSource->SetSize(sizeInput);
          constantSource->SetConstant(0.);
          first = false;
      }
      else {
          paste->SetDestinationImage(paste->GetOutput());
      }
      
      paste->SetSourceImage(outImage);
      paste->SetSourceRegion(outImage->GetLargestPossibleRegion());
            
      paste->SetDestinationIndex(current_idx);
      paste->Update();

      projid += curBufferSize;
  }
  
  float sumpp = static_cast<float>(std::accumulate(avgTimings.begin(), avgTimings.end(), 0.f));
  float meanpp = sumpp / static_cast<float>(avgTimings.size());
  std::cout << "Average time per projection [us]: " << meanpp << std::endl;
  
  typedef itk::ImageFileWriter<InputImageType> FileWriterType;
  FileWriterType::Pointer writer = FileWriterType::New();
  if (args_info.output_given) {
    writer->SetFileName(args_info.output_arg);
    writer->SetInput(paste->GetOutput());
    TRY_AND_EXIT_ON_ITK_EXCEPTION( writer->Update() )
  }

  return EXIT_SUCCESS;
}
