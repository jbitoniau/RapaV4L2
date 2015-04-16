/*
   The MIT License (MIT) (http://opensource.org/licenses/MIT)
   
   Copyright (c) 2015 Jacques Menuet
   
   Permission is hereby granted, free of charge, to any person obtaining a copy
   of this software and associated documentation files (the "Software"), to deal
   in the Software without restriction, including without limitation the rights
   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
   copies of the Software, and to permit persons to whom the Software is
   furnished to do so, subject to the following conditions:
   
   The above copyright notice and this permission notice shall be included in all
   copies or substantial portions of the Software.
   
   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
   SOFTWARE.
*/
#include "RV4L2Device.h"

#include <stdio.h>
#include <assert.h>
#include <sstream>
#include <string>
#include <unistd.h>
#include <stdlib.h>

#include "RV4L2ImageConverter.h"
#include "RV4L2ImageWriter.h"

class MyListener : public RV4L2::Device::Listener
{
public:
	MyListener()
		: mImageFormat(),
		  mImageIndex(0),
		  mImages()
	{
	}

	virtual ~MyListener()
	{
		clearImages();
	}

	void clearImages()
	{
		for ( std::size_t i=0; i<mImages.size(); ++i )
			delete mImages[i];		
		mImages.clear();
		mImageIndex = 0;
	}

	void prepareImages( std::size_t numImagesToSave, RV4L2::ImageFormat format )
	{
		clearImages();
		mImageFormat = format;
		mImages.resize( numImagesToSave );
		for ( std::size_t i=0; i<mImages.size(); ++i )
			mImages[i] = new RV4L2::Image( format );
	}

	virtual void onDeviceStarted( RV4L2::Device* device )
	{
		printf("%s - capture started\n", device->getDeviceName().c_str() );
		mImageIndex = 0;
	}
		
	virtual void onDeviceCapturedImage( RV4L2::Device* device ) 
	{
		const RV4L2::CapturedImage* capturedImage = device->getCapturedImage();
		assert( capturedImage );
		printf("%s - image captured #%d at %f sec\n", device->getDeviceName().c_str(), capturedImage->getSequenceNumber(), capturedImage->getTimestampInSec() );

		/*unsigned int n = capturedImage->getImage().getBuffer().getSizeInBytes();
		const unsigned char* p = capturedImage->getImage().getBuffer().getBytes();
		bool containData = false;
		for ( int i=0; i<n; ++i )
			if ( p[i]!=0 )
				containData = true;
		if ( containData )
			printf("youpi!\n");
		else
			printf("prout!\n");
		*/
		
		if ( mImageIndex<mImages.size() )
		{
			bool ret = mImages[mImageIndex]->getBuffer().copyFrom( capturedImage->getImage().getBuffer() );
			assert( ret );
			mImageIndex++;		
		}
	}

	virtual void onDeviceStopped( RV4L2::Device* device ) 
	{
		printf("%s - capture stopped\n", device->getDeviceName().c_str() );
	}

	bool saveImages( int testNum, int devNum )
	{
		printf("Saving %d images to format:%s\n", mImages.size(), mImageFormat.toString().c_str() ) ;
		RV4L2::ImageConverter	converter( RV4L2::ImageFormat(mImageFormat.getWidth(), mImageFormat.getHeight(), RV4L2::ImageFormat::RGB24) );
		for ( std::size_t i=0; i<mImages.size(); ++i )
		{
			std::stringstream stream;
			stream << devNum << "_" << i << ".ppm";
			bool ret = converter.update( *mImages[i] );
			assert( ret );
			ret = RV4L2::ImageWriter::writeBinaryPPMImage( stream.str().c_str(), converter.getImage() );
			assert( ret );
			printf(".");
			fflush(stdout);
		}
		printf("\n");
	}

private:
	RV4L2::ImageFormat mImageFormat;
	std::size_t mImageIndex;
	std::vector<RV4L2::Image*> mImages;
};

int main( int argc, char** argv )
{
    // Get device name
    std::string deviceName = "/dev/video0";
    if ( argc>1 )
        deviceName = argv[1];

    std::size_t captureSettingsIndex = 0;
    if ( argc>2 )
        captureSettingsIndex = atoi(argv[2]);

    std::size_t numImagesToCaptures = 1;
	
	printf("Creating device\n");
	RV4L2::Device* device = new RV4L2::Device( deviceName.c_str() );
	if ( !device->isValid() )
	{
		printf("Failed to create device\n");
		return -1;
	}
	
	printf("Supported capture settings for devices '%s':\n", device->getDeviceName().c_str());
	const RV4L2::CaptureSettingsList& captureSettingsList = device->getSupportedCaptureSettingsList();
	for ( std::size_t i=0; i<captureSettingsList.size(); ++i )
		printf("\t%d - %s\n", i, captureSettingsList[i].toString().c_str() );
	
    printf("Starting capture #%d...\n", captureSettingsIndex);
	device->startCapture( captureSettingsIndex );
	const RV4L2::CaptureSettings& captureSettings = captureSettingsList[captureSettingsIndex];
	
	printf("Warming up...\n");
	for ( int k=0; k<10; ++k ) 
	{
		device->update();
		usleep(15 * 1000);
	}
	
	MyListener* listener = new MyListener();
	listener->prepareImages( numImagesToCaptures, captureSettings.getImageFormat() );
	device->addListener( listener );
	
	printf("Running a bit...\n");
	for ( int k=0; k<40; ++k ) 
	{
		device->update();
		usleep(15 * 1000);
	}
	
	printf("Stopping capture...\n");
	device->stopCapture();

	printf("Saving images...\n");
	listener->saveImages( 0, 0 );	
/*	
	std::vector<MyListener*> listeners;	
	for ( std::size_t i=0; i<devices.size(); ++i )
	{
		MyListener* listener = new MyListener();
		devices[i]->addListener( listener );
		listeners.push_back ( listener );
	}


	printf("Preparing image storage...\n");
	unsigned int numImagesToCaptures = 1;
	for ( std::size_t i=0; i<devices.size(); ++i )
	{
		const RV4L2::CaptureSettingsList& captureSettingsList = devices[i]->getSupportedCaptureSettingsList();
		if ( captureSettingsIndex<captureSettingsList.size() )
		{
			const RV4L2::CaptureSettings& captureSettings = captureSettingsList[captureSettingsIndex];
			listeners[i]->prepareImages( numImagesToCaptures, captureSettings.getImageFormat() );
		}
	}
	
	
	printf("Saving images...\n");
	for ( std::size_t i=0; i<devices.size(); ++i )
		listeners[i]->saveImages( 0, i );			// testnum?
	
	

	for ( std::size_t i=0; i<devices.size(); ++i )
		delete devices[i];
	devices.clear();

	for ( std::size_t i=0; i<listeners.size(); ++i )
		delete listeners[i];
	listeners.clear();*/

	return 0;
}
