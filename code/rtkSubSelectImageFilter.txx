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
#ifndef __rtkSubSelectImageFilter_txx
#define __rtkSubSelectImageFilter_txx

#include "rtkSubSelectImageFilter.h"

#include <itkImageRegionIterator.h>
#include <itkImageRegionConstIterator.h>

#include "math.h"

namespace rtk
{

template<typename ProjectionStackType>
SubSelectImageFilter<ProjectionStackType>::SubSelectImageFilter()
{
  m_OutputGeometry = GeometryType::New();
}

template<typename ProjectionStackType>
void SubSelectImageFilter<ProjectionStackType>::SetInputProjectionStack(const ProjectionStackType* Projections)
{
  this->SetNthInput(0, const_cast<ProjectionStackType*>(Projections));
}

template<typename ProjectionStackType>
typename ProjectionStackType::ConstPointer SubSelectImageFilter<ProjectionStackType>::GetInputProjectionStack()
{
  return static_cast< const ProjectionStackType * >
          ( this->itk::ProcessObject::GetInput(0) );
}

template<typename ProjectionStackType>
void SubSelectImageFilter<ProjectionStackType>::GenerateInputRequestedRegion()
{
  // call the superclass' implementation of this method
  Superclass::GenerateInputRequestedRegion();

  // get pointer to the input
  typename Superclass::InputImagePointer inputPtr =
          const_cast< ProjectionStackType * >( this->GetInput() );

  if ( inputPtr )
  {
  // request the region of interest
  inputPtr->SetRequestedRegionToLargestPossibleRegion();
  }
}

template<typename ProjectionStackType>
void SubSelectImageFilter<ProjectionStackType>::GenerateOutputInformation()
{
  unsigned int Dimension = this->GetInput(0)->GetImageDimension();
  typename ProjectionStackType::RegionType outputLargestPossibleRegion = this->GetInput(0)->GetLargestPossibleRegion();
  outputLargestPossibleRegion.SetSize(Dimension-1, m_NbSelectedProjs);

  this->GetOutput()->SetLargestPossibleRegion(outputLargestPossibleRegion);

  // Update output geometry
  // NOTE : The output geometry must be computed here, not in the GenerateData(),
  // because downstream forward and backprojection filters will need this geometry
  // to compute their output information and input requested region
  m_OutputGeometry->Clear();
  for(unsigned long i=0; i < m_SelectedProjections.size(); i++)
    {
    if (m_SelectedProjections[i])
      {
      m_OutputGeometry->AddProjectionInRadians( m_InputGeometry->GetSourceToIsocenterDistances()[i],
                                                m_InputGeometry->GetSourceToDetectorDistances()[i],
                                                m_InputGeometry->GetGantryAngles()[i],
                                                m_InputGeometry->GetProjectionOffsetsX()[i],
                                                m_InputGeometry->GetProjectionOffsetsY()[i],
                                                m_InputGeometry->GetOutOfPlaneAngles()[i],
                                                m_InputGeometry->GetInPlaneAngles()[i],
                                                m_InputGeometry->GetSourceOffsetsX()[i],
                                                m_InputGeometry->GetSourceOffsetsY()[i]);
      }
    }
}

template<typename ProjectionStackType>
typename rtk::ThreeDCircularProjectionGeometry::Pointer SubSelectImageFilter<ProjectionStackType>::GetOutputGeometry()
{
  return m_OutputGeometry;
}

template<typename ProjectionStackType>
void SubSelectImageFilter<ProjectionStackType>::GenerateData()
{
  unsigned int Dimension = this->GetInput(0)->GetImageDimension();

  // Prepare paste filter and constant image source
  typename PasteFilterType::Pointer PasteFilter = PasteFilterType::New();
  typename ExtractFilterType::Pointer ExtractFilter = ExtractFilterType::New();

  // Create a stack of empty projection images
  typename EmptyProjectionStackSourceType::Pointer EmptyProjectionStackSource = EmptyProjectionStackSourceType::New();
  EmptyProjectionStackSource->SetOrigin(this->GetInputProjectionStack()->GetOrigin());
  EmptyProjectionStackSource->SetSpacing(this->GetInputProjectionStack()->GetSpacing());
  EmptyProjectionStackSource->SetDirection(this->GetInputProjectionStack()->GetDirection());
  typename ProjectionStackType::SizeType ProjectionStackSize;
  ProjectionStackSize = this->GetInputProjectionStack()->GetLargestPossibleRegion().GetSize();
  ProjectionStackSize[Dimension-1] = m_NbSelectedProjs;
  EmptyProjectionStackSource->SetSize(ProjectionStackSize);
  EmptyProjectionStackSource->SetConstant( 0. );
  EmptyProjectionStackSource->Update();

  // Set the extract filter
  ExtractFilter->SetInput(this->GetInput(0));
  typename ExtractFilterType::InputImageRegionType projRegion;
  projRegion = this->GetInput(0)->GetLargestPossibleRegion();
  projRegion.SetSize(Dimension-1, 1);
  ExtractFilter->SetExtractionRegion(projRegion);

  // Set the Paste filter
  PasteFilter->SetSourceImage(ExtractFilter->GetOutput());
  PasteFilter->SetDestinationImage(EmptyProjectionStackSource->GetOutput());

  // Count the projections actually used in constructing the output
  int counter=0;

  for(unsigned int i=0; i < m_SelectedProjections.size(); i++)
    {
    if (m_SelectedProjections[i])
      {
      // After the first update, we need to use the output as input.
      if(counter>0)
        {
        typename ProjectionStackType::Pointer pimg = PasteFilter->GetOutput();
        pimg->DisconnectPipeline();
        PasteFilter->SetDestinationImage( pimg );
        }

      // Set the Extract Filter
      projRegion.SetIndex(Dimension - 1, i);
      ExtractFilter->SetExtractionRegion(projRegion);
      ExtractFilter->UpdateLargestPossibleRegion();

      // Set the Paste filter
      PasteFilter->SetSourceRegion(ExtractFilter->GetOutput()->GetLargestPossibleRegion());
      typename ProjectionStackType::IndexType DestinationIndex;
      DestinationIndex.Fill(0);
      DestinationIndex[Dimension-1]=counter;
      PasteFilter->SetDestinationIndex(DestinationIndex);

      // Update the filters
      PasteFilter->UpdateLargestPossibleRegion();
      PasteFilter->Update();

      counter++;
      }
    }

  this->GraftOutput( PasteFilter->GetOutput() );
}

}// end namespace


#endif
