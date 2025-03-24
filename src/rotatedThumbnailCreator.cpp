#include <string>
#include <exception>
#include <memory>
#include <vector>
#include <iostream>
#include <cmath>
#include <cstring>

#include <FreeImage.h>

#include <cuda_runtime.h>
#include <npp.h>

#include <Exceptions.h>
#include <ImagesCPU.h>
#include <ImagesNPP.h>

#include <helper_cuda.h>

// The code doesn't follow exactly the Google C++ style guide but instead a similar style guide.

constexpr int defaultDstSizeBoth = 200;
constexpr const char* defaultpreExtSuffix = "_thumb";

void printUsage()
{
  std::cout << "purpose: converts image files into thumbnails where the image has been rotated\n";
  std::cout << "usage: <executable> <flags> <file names>, where\n";
  std::cout << "<file names>: zero or more image files\n";
  std::cout << "--size: the dimension in pixels (for both width and height) of the resulting (square) thumbnail\n";
  std::cout << "--suffix: this suffix is added to the original file name (right before the extension, if any) to get the thumbnail file name\n";
  exit(0);
}

// TODO: This could be made more sophisticated, e.g. use a professional argument parsing library.
std::tuple<std::vector<std::string>, std::string, int> parseArgLine(int argc, char *argv[])
{
  std::vector<std::string> inputFiles;
  int dstSizeBoth = defaultDstSizeBoth;
  std::string preExtSuffix = defaultpreExtSuffix;

  // slight overapproximation
  inputFiles.reserve(argc);

  for (size_t i = 1; i < argc; ++i)
  {
    std::string arg(argv[i]);
    if (arg.find("--") != std::string::npos)
    {
      std::string flag = arg.substr(2);
      if (flag == "suffix")
      {
        ++i;
        if (i >= argc)
        {
          std::cerr << "ignoring flag '" << arg << "' without actual argument" << std::endl;
          continue;
        }
        // TODO: Could protect against illegal/bad inputs.
        preExtSuffix = argv[i];
      }
      else if (flag == "size")
      {
        ++i;
        if (i >= argc)
        {
          std::cerr << "ignoring flag '" << arg << "' without actual argument" << std::endl;
          continue;
        }
        // TODO: Could disallow "garbage" at the end and check against <= 0 or "too big" values.
        dstSizeBoth = std::stoi(argv[i]);
      }
      else if (flag == "help")
      {
        printUsage();
      }
      else
      {
        std::cerr << "ignoring unsupported flag '" << arg << "'" << std::endl;
      }
    }
    else
    {
      inputFiles.push_back(std::move(arg));
    }
  }

  return {std::move(inputFiles), std::move(preExtSuffix), dstSizeBoth};
}

// Load an image using FreeImage and convert into an NPP helper object.
// The image is also converted into a standardized format to easy later processing.
std::pair<FREE_IMAGE_FORMAT, std::unique_ptr<npp::ImageCPU_8u_C3>> loadImage(const std::string& inputFile)
{
  FREE_IMAGE_FORMAT format = FreeImage_GetFileType(inputFile.c_str());
  if (format == FIF_UNKNOWN)
  {
    // If the image signature cannot be determined (some file formats don't have any), try using the file extension.
    format = FreeImage_GetFIFFromFilename(inputFile.c_str());
  }
  NPP_ASSERT(format != FIF_UNKNOWN);
  NPP_ASSERT(FreeImage_FIFSupportsReading(format));

  FIBITMAP* const origBitmap = FreeImage_Load(format, inputFile.c_str());
  NPP_ASSERT_NOT_NULL(origBitmap);

  FIBITMAP* const convBitmap = FreeImage_ConvertTo24Bits(origBitmap);
  NPP_ASSERT_NOT_NULL(convBitmap);

  // needs to be a smart pointer due to buggy code in the library
  auto image = std::make_unique<npp::ImageCPU_8u_C3>(FreeImage_GetWidth(convBitmap), FreeImage_GetHeight(convBitmap));

  // Copy the FreeImage data into the ImageCPU object.
  unsigned int srcPitch = FreeImage_GetPitch(convBitmap);
  const Npp8u* srcLine = FreeImage_GetBits(convBitmap) + srcPitch * (FreeImage_GetHeight(convBitmap) - 1);
  unsigned int dstPitch = image->pitch();
  Npp8u *dstLine = image->data();
  for (size_t line = 0; line < image->height(); ++line)
  {
    memcpy(dstLine, srcLine, image->width() * 3 * sizeof(Npp8u));
    srcLine -= srcPitch;
    dstLine += dstPitch;
  }

  FreeImage_Unload(origBitmap);
  FreeImage_Unload(convBitmap);

  return {format, std::move(image)};
}

// Shrinks (or rather resizes) the image to the requested (square) size and rotates it appropriately.
std::unique_ptr<npp::ImageCPU_8u_C3> convertImage(const npp::ImageCPU_8u_C3 &inputImage, const int dstSizeBoth)
{
  npp::ImageNPP_8u_C3 deviceInputImage(inputImage);
  const NppiSize srcSize = {(int)deviceInputImage.width(), (int)deviceInputImage.height()};
  const double ratio = static_cast<double>(srcSize.width) / static_cast<double>(srcSize.height);

  const int shrinkSizeHeight = sqrt(2.) * dstSizeBoth / (1. + ratio);
  const int shrinkSizeWidth = shrinkSizeHeight * ratio;
  const NppiSize shrinkSize = {shrinkSizeWidth, shrinkSizeHeight};
  npp::ImageNPP_8u_C3 deviceShrinkImage(shrinkSize.width, shrinkSize.height);

  NPP_CHECK_NPP(nppiResize_8u_C3R(
      deviceInputImage.data(), deviceInputImage.pitch(), srcSize, {0, 0, srcSize.width, srcSize.height},
      deviceShrinkImage.data(), deviceShrinkImage.pitch(), shrinkSize, {0, 0, shrinkSize.width, shrinkSize.height},
      NPPI_INTER_CUBIC));

  const NppiSize dstSize = {dstSizeBoth, dstSizeBoth};
  npp::ImageNPP_8u_C3 deviceOutputImage(dstSize.width, dstSize.height);

  NPP_CHECK_NPP(nppiRotate_8u_C3R(
      deviceShrinkImage.data(), shrinkSize, deviceShrinkImage.pitch(), {0, 0, shrinkSize.width, shrinkSize.height},
      deviceOutputImage.data(), deviceOutputImage.pitch(), {0, 0, dstSize.width, dstSize.height},
      45, 0, shrinkSize.width / sqrt(2.),
      NPPI_INTER_CUBIC));

  // needs to be a smart pointer due to buggy code in the library
  auto outputImage = std::make_unique<npp::ImageCPU_8u_C3>(deviceOutputImage.size());
  deviceOutputImage.copyTo(outputImage->data(), outputImage->pitch());

  nppiFree(deviceInputImage.data());
  nppiFree(deviceShrinkImage.data());
  nppiFree(deviceOutputImage.data());

  return outputImage;
}

// Determines the output file name.
// TODO: Could be made more flexible.
std::string getOutputFileName(const std::string& inputFile, const std::string& preExtSuffix)
{
  const std::string::size_type dotLoc = inputFile.rfind('.');
  return dotLoc == std::string::npos ?
    inputFile + preExtSuffix : inputFile.substr(0, dotLoc) + preExtSuffix + inputFile.substr(dotLoc);
}

// Uses FreeImage again to save the resulting (rotated) thumbnail.
void saveImage(const std::string& outputFile, const npp::ImageCPU_8u_C3& outputImage, const FREE_IMAGE_FORMAT format)
{
  FIBITMAP* const resultBitmap = FreeImage_Allocate(outputImage.width(), outputImage.height(), 24);
  NPP_ASSERT_NOT_NULL(resultBitmap);

  const unsigned int srcPitch = outputImage.pitch();
  const Npp8u *srcLine = outputImage.data();
  const unsigned int dstPitch = FreeImage_GetPitch(resultBitmap);
  Npp8u* dstLine = FreeImage_GetBits(resultBitmap) + dstPitch * (outputImage.height() - 1);
  for (size_t line = 0; line < outputImage.height(); ++line)
  {
    memcpy(dstLine, srcLine, outputImage.width() * 3 * sizeof(Npp8u));
    srcLine += srcPitch;
    dstLine -= dstPitch;
  }

  const bool success = FreeImage_Save(format, resultBitmap, outputFile.c_str(), 0) == TRUE;
  NPP_ASSERT_MSG(success, "Failed to save result image.");
}

int main(int argc, char *argv[])
{
  const auto [inputFiles, preExtSuffix, dstSizeBoth] = parseArgLine(argc, argv);

  std::cout << "using thumbnail size of " << dstSizeBoth << " and suffix " << preExtSuffix << std::endl;

  if (inputFiles.empty())
  {
    std::cout << "no files specified and hence none converted" << std::endl;
    return 0;
  }

  for (const std::string &inputFile : inputFiles)
  {
    std::cout << "processing file " << inputFile << "...";
    
    try
    {
      const auto [format, inputImage] = loadImage(inputFile);
      const std::unique_ptr<npp::ImageCPU_8u_C3> outputImage = convertImage(*inputImage, dstSizeBoth);
      const std::string outputFile = getOutputFileName(inputFile, preExtSuffix);
      saveImage(outputFile, *outputImage, format);

      std::cout << " done" << std::endl;
    }
    catch (const npp::Exception& exn)
    {
      std::cerr << "\nProgram error! The following NPP exception occurred for input file " << inputFile << ": \n";
      std::cerr << exn << std::endl;
    }
    catch (const std::exception& exn)
    {
      std::cerr << "\nProgram error! The following exception occurred for input file " << inputFile << ": \n";
      std::cerr << exn.what() << std::endl;
    }
    catch (...)
    {
      std::cerr << "\nProgram error! An unknow type of exception occurred for input file " << inputFile << std::endl;
    }
  }

  return 0;
}
