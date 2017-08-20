#include "kvlMatlabRunner.h" 
#include "kvlMatlabObjectArray.h"
#include "kvlCroppedImageReader.h"
#include "itkCastImageFilter.h"


namespace kvl
{


class ReadCroppedImage : public MatlabRunner
{
public:
  /** Smart pointer typedef support. */
  typedef ReadCroppedImage         Self;
  typedef itk::Object              Superclass;
  typedef itk::SmartPointer<Self>  Pointer;
  typedef itk::SmartPointer<const Self>  ConstPointer;

  /** Method for creation through the object factory. */
  itkNewMacro( Self );

  /** Run-time type information (and related methods). */
  itkTypeMacro( ReadCroppedImage, itk::Object );

  virtual void Run( int nlhs, mxArray* plhs[],
                    int nrhs, const mxArray* prhs[] )
    {
    //std::cout << "I am " << this->GetNameOfClass() 
    //          << " and I'm running! " << std::endl;
              
    // [ image, transform, croppedRegion ] = kvlReadCroppedImage( imageFileName, boundingFileName )
  
    // Make sure input arguments are correct
    if ( ( nrhs != 2 ) || 
         !mxIsChar( prhs[ 0 ] ) || 
         !mxIsChar( prhs[ 1 ] ) || 
         ( nlhs != 2 ) )
      {
      mexErrMsgTxt( "Incorrect arguments" );
      }

    // Retrieve input arguments
    const std::string imageFileName = mxArrayToString( prhs[0] );
    const std::string boundingFileName = mxArrayToString( prhs[1] );

    // Read the image
    kvl::CroppedImageReader::Pointer  reader = kvl::CroppedImageReader::New();
    //reader->SetExtraFraction( 0.1 );
    reader->SetExtraFraction( 0.0 );
    reader->Read( imageFileName.c_str(), boundingFileName.c_str() );
    

    // Convert the image to float
    typedef itk::Image< float, 3 >  ImageType;
    typedef itk::CastImageFilter< kvl::CroppedImageReader::ImageType, ImageType >  CasterType;
    CasterType::Pointer  caster = CasterType::New();
    caster->SetInput( reader->GetImage() );
    caster->Update();

    
    // Store the image and transform in persistent memory
    const int imageHandle = kvl::MatlabObjectArray::GetInstance()->AddObject( caster->GetOutput() );
    const int transformHandle = kvl::MatlabObjectArray::GetInstance()->AddObject( reader->GetTransform() );
    
    // Return the handles to Matlab
    mwSize  dims[ 1 ];
    dims[ 0 ] = 1;
    plhs[ 0 ] = mxCreateNumericArray( 1, dims, mxINT64_CLASS, mxREAL );
    *( static_cast< int* >( mxGetData( plhs[ 0 ] ) ) ) = imageHandle;
    plhs[ 1 ] = mxCreateNumericArray( 1, dims, mxINT64_CLASS, mxREAL );
    *( static_cast< int* >( mxGetData( plhs[ 1 ] ) ) ) = transformHandle;
    

    }
  
protected:
  ReadCroppedImage() {};
  virtual ~ReadCroppedImage() {};


  ReadCroppedImage(const Self&); //purposely not implemented
  void operator=(const Self&); //purposely not implemented

private:

};

} // end namespace kvl

