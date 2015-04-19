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
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <sstream>
#include <algorithm>

#include <getopt.h>             /* getopt_long() */
#include <fcntl.h>              /* low-level i/o */
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

#include <linux/videodev2.h>

#define CLEAR(x) memset(&(x), 0, sizeof(x))

/*
	Notes:
	- check for isValid() everywhere!
	- NumBuffer should be settable in ctor... Reduce to 2 to reduce latency?

	// PIXEL FORMAT 
	// http://linuxtv.org/downloads/v4l-dvb-apis/pixfmt.html
*/
namespace RV4L2
{

/*
	Device::InternalCaptureSettings
*/
Device::InternalCaptureSettings::InternalCaptureSettings()
	:	pixelFormatName(),
		pixelFormat(0),
		width(0),
		height(0),
		frameIntervalNumerator(0),
		frameIntervalDenominator(0)
{
}

float Device::InternalCaptureSettings::getFrameRateInHz() const 
{ 
    if ( frameIntervalNumerator==0 )
        return 0.f;
	return static_cast<float>(frameIntervalDenominator)/static_cast<float>(frameIntervalNumerator);
}

float Device::InternalCaptureSettings::getFrameIntervalInS() const 
{ 
    if ( frameIntervalDenominator==0 )
        return 0.f;
	return static_cast<float>(frameIntervalNumerator)/static_cast<float>(frameIntervalDenominator);
}

std::string Device::InternalCaptureSettings::toString() const
{
	std::stringstream stream;
	stream.precision(3);
	stream << std::fixed;
	stream << "pixelFormatName:'" << pixelFormatName << "' ";
	stream << "pixelFormat:" << pixelFormat << " ";
	stream << "width:" << width << "px ";
	stream << "height:" << height << "px ";
	stream << "frameInterval:" << frameIntervalNumerator<< "/" << frameIntervalDenominator << " (rate:" << getFrameRateInHz() << "hz ival:" << getFrameIntervalInS() << "s)";
	return stream.str();
}

/*
	Device
*/
Device::Device( const char* deviceName )
	:	mDeviceName(deviceName),
		mHandle(-1),
		mInternalCaptureSettingsList(),
		mCaptureSettingsList(),
		mCaptureSettingsToInternalCaptureSettingsIndices(),
		mIsCapturing(false),
		mCapturedImage(NULL),
		mBuffers(NULL),
		mNumBuffers(0),
        mListeners(),
        mFallbackFrameSizes()
{
    mFallbackFrameSizes.push_back( std::make_pair(320, 240) );
    mFallbackFrameSizes.push_back( std::make_pair(640, 480) );
    mFallbackFrameSizes.push_back( std::make_pair(720, 480) );
    mFallbackFrameSizes.push_back( std::make_pair(720, 576) );

    if ( !openDevice() )
		return;

	if ( !checkDeviceCapabilities() )
	{
		closeDevice();
		return;
	}

	initializeInternalCaptureSettingsList();
	initializeCaptureSettingsList();
}

Device::~Device()
{
	closeDevice();
}

bool Device::openDevice()
{
	assert( mHandle==-1 );

	struct stat st;
	if ( stat(mDeviceName.c_str(), &st)==-1 )
	{
		fprintf( stderr, "Cannot identify '%s'. %s (%d)\n", mDeviceName.c_str(), strerror(errno), errno );
		return false;
	}

	if ( !S_ISCHR(st.st_mode) ) 
	{
		fprintf( stderr, "%s is not a device\n", mDeviceName.c_str() );
		return false;	
	}

	mHandle = open( mDeviceName.c_str(), O_RDWR | O_NONBLOCK, 0 );
    if ( mHandle==-1 )
	{
		fprintf( stderr, "Cannot open %s. %s (%d)\n", mDeviceName.c_str(), strerror(errno), errno );
		return false;	
	}
	return true;
}

bool Device::checkDeviceCapabilities()
{
	assert( mHandle!=-1 );

	struct v4l2_capability cap;
    if ( xioctl(mHandle, VIDIOC_QUERYCAP, &cap)==-1 ) 
	{
		if ( EINVAL == errno )  
		{
			fprintf( stderr, "%s is not a V4L2 device\n", mDeviceName.c_str() );
			return false; 
		}
		else	
		{
			fprintf( stderr, "VIDIOC_QUERYCAP failed for device %s\n", mDeviceName.c_str() );
			return false; 
		}
	}

	if ( !(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE) ) 
	{
		fprintf( stderr, "%s is a V4L2 device but doesn't support capture\n", mDeviceName.c_str() );
		return false;	
	}

	if ( !(cap.capabilities & V4L2_CAP_STREAMING) ) 
	{
		fprintf( stderr, "%s is a V4L2 capture device but does not support streaming i/o\n", mDeviceName.c_str() );
		return false;	
	}

	return true;
}

void Device::closeDevice()
{
	if ( mHandle!=-1 )
	{
		close( mHandle );
		mHandle = -1;
	}
}

void Device::initializeInternalCaptureSettingsList()
{
	mInternalCaptureSettingsList.clear();

	// Enumerate all the image formats supported
	struct v4l2_fmtdesc fmtDesc;
	CLEAR(fmtDesc);
	fmtDesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	fmtDesc.index = 0;
	int retFmtDesc = 0;
	do 
	{
		retFmtDesc = xioctl( mHandle, VIDIOC_ENUM_FMT, &fmtDesc );
//printf("DEBUG: VIDIOC_ENUM_FMT:%d\n", retFmtDesc);
		if ( retFmtDesc==0 )
		{
			// For each image format, enumerate the frame sizes it supports
			v4l2_frmsizeenum frmSizeEnum;
			CLEAR(frmSizeEnum);
			frmSizeEnum.index = 0;
			frmSizeEnum.pixel_format = fmtDesc.pixelformat;
			int retFrmSize = 0;
			do
			{
				retFrmSize = xioctl( mHandle, VIDIOC_ENUM_FRAMESIZES, &frmSizeEnum );
//printf("DEBUG: VIDIOC_ENUM_FRAMESIZES:%d\n", retFmtDesc);
				if ( retFrmSize==0 )
				{
					if ( frmSizeEnum.type==V4L2_FRMSIZE_TYPE_DISCRETE )
					{
						// For each discrete frame size of an image format, enumerate the frame intervals it supports
						v4l2_frmivalenum  frmIvalEnum;
						CLEAR( frmIvalEnum );
						frmIvalEnum.index = 0;
						frmIvalEnum.pixel_format = fmtDesc.pixelformat;	
						frmIvalEnum.width = frmSizeEnum.discrete.width;
						frmIvalEnum.height = frmSizeEnum.discrete.height;
						int retFrmIval = 0;
						do
						{
							retFrmIval = xioctl( mHandle, VIDIOC_ENUM_FRAMEINTERVALS, &frmIvalEnum );
//printf("DEBUG: VIDIOC_ENUM_FRAMEINTERVALS:%d\n", retFrmIval);
							if ( retFrmIval==0 )
							{
								if ( frmIvalEnum.type==V4L2_FRMIVAL_TYPE_DISCRETE )
								{
									InternalCaptureSettings internalCaptureSettings;
									internalCaptureSettings.pixelFormatName = reinterpret_cast<char*>(fmtDesc.description);
									internalCaptureSettings.pixelFormat = fmtDesc.pixelformat;
									internalCaptureSettings.width = frmSizeEnum.discrete.width;
									internalCaptureSettings.height = frmSizeEnum.discrete.height;
									internalCaptureSettings.frameIntervalNumerator = frmIvalEnum.discrete.numerator;
									internalCaptureSettings.frameIntervalDenominator = frmIvalEnum.discrete.denominator;
									mInternalCaptureSettingsList.push_back( internalCaptureSettings );
								}
								else
								{
									fprintf( stderr, "%s supports a non-discrete frame-size (VIDIOC_ENUM_FRAMEINTERVALS) which is unsupported by the implementation\n", mDeviceName.c_str() );
								}
								frmIvalEnum.index++;
							}
						}
						while ( retFrmIval==0 );
					}
					else if ( frmSizeEnum.type==V4L2_FRMSIZE_TYPE_STEPWISE )
					{
                        //fprintf( stderr, ">>>>>>>V4L2_FRMSIZE_TYPE_STEPWISE\n");
                        //fprintf( stderr, ">>>>>>>  min_width=%d\n", frmSizeEnum.stepwise.min_width );
                        //fprintf( stderr, ">>>>>>>  max_width=%d\n", frmSizeEnum.stepwise.max_width );
                        //fprintf( stderr, ">>>>>>>  step_width=%d\n", frmSizeEnum.stepwise.step_width );
                        //fprintf( stderr, ">>>>>>>  min_height=%d\n", frmSizeEnum.stepwise.min_height );
                        //fprintf( stderr, ">>>>>>>  max_height=%d\n", frmSizeEnum.stepwise.max_height );
                        //fprintf( stderr, ">>>>>>>  step_height=%d\n", frmSizeEnum.stepwise.step_height );

						// Should check frame interval... but doesn't seem to work :(
						InternalCaptureSettings internalCaptureSettings;
						internalCaptureSettings.pixelFormatName = reinterpret_cast<char*>(fmtDesc.description);
						internalCaptureSettings.pixelFormat = fmtDesc.pixelformat;
						internalCaptureSettings.width = frmSizeEnum.stepwise.max_width;
						internalCaptureSettings.height = frmSizeEnum.stepwise.max_height;
						//internalCaptureSettings.width = frmSizeEnum.stepwise.min_width;
						//internalCaptureSettings.height = frmSizeEnum.stepwise.min_height;
                        internalCaptureSettings.frameIntervalNumerator = 0;
                        internalCaptureSettings.frameIntervalDenominator = 0;
						mInternalCaptureSettingsList.push_back( internalCaptureSettings );
					}
					else if ( frmSizeEnum.type==V4L2_FRMSIZE_TYPE_CONTINUOUS  )
					{
                        //fprintf( stderr, ">>>>>>>V4L2_FRMSIZE_TYPE_CONTINUOUS\n");
						// fprintf( stderr, ">>>>>>>  min_width=%d\n", frmSizeEnum.stepwise.min_width );
						// fprintf( stderr, ">>>>>>>  max_width=%d\n", frmSizeEnum.stepwise.max_width );
						// fprintf( stderr, ">>>>>>>  step_width=%d\n", frmSizeEnum.stepwise.step_width );
						// fprintf( stderr, ">>>>>>>  min_height=%d\n", frmSizeEnum.stepwise.min_height );
						// fprintf( stderr, ">>>>>>>  max_height=%d\n", frmSizeEnum.stepwise.max_height );
						// fprintf( stderr, ">>>>>>>  step_height=%d\n", frmSizeEnum.stepwise.step_height );
					}
					else 
					{
						fprintf( stderr, ">>>>>>>UNKNOWN\n");
					}

					frmSizeEnum.index++;
				}
				else
				{
                    //printf("FAILED to enumerate frame sizes, defaulting to something\n");
                    for ( std::size_t i=0; i<mFallbackFrameSizes.size(); ++i )
                    {
                        InternalCaptureSettings internalCaptureSettings;
                        internalCaptureSettings.pixelFormatName = reinterpret_cast<char*>(fmtDesc.description);
                        internalCaptureSettings.pixelFormat = fmtDesc.pixelformat;
                        internalCaptureSettings.width = mFallbackFrameSizes[i].first;
                        internalCaptureSettings.height = mFallbackFrameSizes[i].second;
                        internalCaptureSettings.frameIntervalNumerator = 0;
                        internalCaptureSettings.frameIntervalDenominator = 0;
                        mInternalCaptureSettingsList.push_back( internalCaptureSettings );
                    }
				}
			}
			while ( retFrmSize==0 );
			fmtDesc.index++;
		}			
	}
	while ( retFmtDesc==0 );

	// For debug
    printf("DEBUG: Internal capture settings list: %d\n", static_cast<int>(mInternalCaptureSettingsList.size()) );
	for ( std::size_t i=0; i<mInternalCaptureSettingsList.size(); ++i )
	{
		const InternalCaptureSettings& internalCaptureSettings = mInternalCaptureSettingsList[i];
		printf("DEBUG: %s\n", internalCaptureSettings.toString().c_str() );
	}
}

bool Device::getImageFormatEncoding( unsigned int v4l2PixelFormat, ImageFormat::Encoding& encoding )
{
	switch ( v4l2PixelFormat )
	{
		case V4L2_PIX_FMT_YUYV:	
			encoding=ImageFormat::YUYV;
			return true;
			
		case V4L2_PIX_FMT_SBGGR8:
		case V4L2_PIX_FMT_SGBRG8: 
		case V4L2_PIX_FMT_SGRBG8:
		case V4L2_PIX_FMT_SRGGB8:
			encoding=ImageFormat::YUYV;	//_32Bits;
			return true;
	}
	return false;	
}

void Device::initializeCaptureSettingsList()
{
	mCaptureSettingsList.clear();
	mCaptureSettingsToInternalCaptureSettingsIndices.clear();

	for ( std::size_t i=0; i<mInternalCaptureSettingsList.size(); ++i )
	{
		const InternalCaptureSettings& internalCaptureSettings = mInternalCaptureSettingsList[i];
		ImageFormat::Encoding encoding;
		unsigned int pixelFormat = internalCaptureSettings.pixelFormat;
		if ( getImageFormatEncoding( pixelFormat, encoding ) )
		{
			ImageFormat format( internalCaptureSettings.width, internalCaptureSettings.height, encoding );
			CaptureSettings captureSettings( format, internalCaptureSettings.getFrameRateInHz() );

			// Remember which InternalCaptureSettings corresponds to this new CaptureSettings
			mCaptureSettingsToInternalCaptureSettingsIndices.push_back( mCaptureSettingsList.size() );

			// Add this CaptureSettings to the list
			mCaptureSettingsList.push_back( captureSettings );
		}	
		else
		{
			fprintf( stderr, "%s supports pixel format '%s' which is unsupported by the implementation\n", mDeviceName.c_str(), internalCaptureSettings.pixelFormatName.c_str() );
		}
	}
	assert( mCaptureSettingsToInternalCaptureSettingsIndices.size()==mCaptureSettingsList.size() );
}

bool Device::getSupportedCaptureSettingsIndex( const CaptureSettings& captureSettings, std::size_t& index ) const
{
	index = 0;
	for ( std::size_t i=0; i<mCaptureSettingsList.size(); ++i )
	{
		if ( mCaptureSettingsList[i]==captureSettings )
		{
			index = i;
			return true;
		}
	}
	return false;
}
	
bool Device::startCapture( std::size_t captureSettingsIndex )
{
	if ( isCapturing() )
		return false;

	if ( captureSettingsIndex>=mCaptureSettingsList.size() )
		return false;

	// Retrieve CaptureSettings
	const CaptureSettings& captureSettings = mCaptureSettingsList[captureSettingsIndex];
	
	// Retrieve corresponding InternalCaptureSettings
	std::size_t internalCaptureSettingsIndex = mCaptureSettingsToInternalCaptureSettingsIndices[captureSettingsIndex];
	const InternalCaptureSettings& internalCaptureSettings = mInternalCaptureSettingsList[internalCaptureSettingsIndex];

	// Prepare a V4L2 format struct for the capture (we should probably read it from the hardware 
	// and modify it rather than creating a cleared one)
	struct v4l2_format fmt;
	CLEAR(fmt);
	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	fmt.fmt.pix.width = internalCaptureSettings.width;
    fmt.fmt.pix.height = internalCaptureSettings.height;
    fmt.fmt.pix.pixelformat = internalCaptureSettings.pixelFormat;
    //fmt.fmt.pix.field = ???
	if ( xioctl( mHandle, VIDIOC_S_FMT, &fmt )==-1 )		
	{
		fprintf( stderr, "VIDIOC_S_FMT failed for device %s\n", mDeviceName.c_str() );
		return false;	
	}

	// Check that we've got what we wanted
	if ( xioctl( mHandle, VIDIOC_G_FMT, &fmt )==-1 )		
	{
		fprintf( stderr, "VIDIOC_G_FMT failed for device %s\n", mDeviceName.c_str() );
		return false;	
	}
	if ( fmt.fmt.pix.width != internalCaptureSettings.width ||
		 fmt.fmt.pix.height != internalCaptureSettings.height ||
		 fmt.fmt.pix.pixelformat != internalCaptureSettings.pixelFormat )
	{
		fprintf( stderr, "Failed to set the capture format for device %s\n", mDeviceName.c_str() );
		return false;	
	}
	
	// Check that the hardware will provide us with images of the expected size
	if ( fmt.fmt.pix.sizeimage!=captureSettings.getImageFormat().getDataSizeInBytes() )
	{
		fprintf( stderr, "The image size that %s provides for the capture (%d) is not the expected one (%d)\n", mDeviceName.c_str(), fmt.fmt.pix.sizeimage, captureSettings.getImageFormat().getDataSizeInBytes());
		return false;
	}

	// Specify the frame interval
    if ( internalCaptureSettings.frameIntervalNumerator!=0 &&
         internalCaptureSettings.frameIntervalDenominator!=0 )
	{
		struct v4l2_streamparm parm;
		CLEAR(parm);
		parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		parm.parm.capture.timeperframe.numerator = internalCaptureSettings.frameIntervalNumerator;
		parm.parm.capture.timeperframe.denominator = internalCaptureSettings.frameIntervalDenominator;
		if ( xioctl( mHandle, VIDIOC_S_PARM, &parm )==-1 )		
		{
			fprintf( stderr, "VIDIOC_S_PARM failed for device %s\n", mDeviceName.c_str() );
			return false;	
		}
		
		// Check that we've got what we wanted
		if ( xioctl( mHandle, VIDIOC_G_PARM, &parm )==-1 )		
		{
			fprintf( stderr, "VIDIOC_G_PARM failed for device %s\n", mDeviceName.c_str() );
			return false;	
		}
		if ( parm.parm.capture.timeperframe.numerator != internalCaptureSettings.frameIntervalNumerator ||
			 parm.parm.capture.timeperframe.denominator != internalCaptureSettings.frameIntervalDenominator )
		{
			fprintf( stderr, "Failed to set the frame interval for device %s\n", mDeviceName.c_str() );
			return false;
		}
	}
	else
	{
		fprintf( stderr, "VIDIOC_S_PARM not specified for device %s\n", mDeviceName.c_str() );
	}
		
	// Construct CapturedImage to receive image data
	assert( !mCapturedImage );
	mCapturedImage = new CapturedImage( captureSettings.getImageFormat() );
	
	// Request buffers for MMAP transfer
	struct v4l2_requestbuffers req;
	CLEAR(req);
    req.count = mNumBuffersToUse;
	req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
	if ( xioctl( mHandle, VIDIOC_REQBUFS, &req )==-1 ) 
	{
        if ( EINVAL==errno ) 
		{
			fprintf( stderr, "Failed to request MMAP buffers for device %s\n", mDeviceName.c_str() );
			return false; 
		}
		else
		{
			fprintf( stderr, "VIDIOC_REQBUFS failed for device %s\n", mDeviceName.c_str() );
			return false;
		}
	}
	
	
	// Allocate the list of buffers
	int numBuffers = req.count;
	if ( numBuffers<2 ) 
	{
		fprintf( stderr, "Insufficient buffer memory for device %s\n", mDeviceName.c_str() );
		return false;
	}
	
	struct buffer* buffers = (buffer*)calloc( numBuffers, sizeof(*buffers) );
	if ( !buffers )
	{
		fprintf( stderr, "Out of memory\n" );
		return false;
	}

	// Map memory buffer
	for ( int bufferIndex=0; bufferIndex<numBuffers; ++bufferIndex ) 
	{
		struct v4l2_buffer buf;
		CLEAR(buf);
		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = bufferIndex;

		if ( xioctl( mHandle, VIDIOC_QUERYBUF, &buf )==-1 )
		{
			fprintf( stderr, "VIDIOC_QUERYBUF failed for device %s\n", mDeviceName.c_str() );
			// Note: aquired resources should be properly released here!
			return false;	
		}
		buffers[bufferIndex].length = buf.length;
        buffers[bufferIndex].start = mmap( NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, mHandle, buf.m.offset );
		if ( buffers[bufferIndex].start==MAP_FAILED )
		{
			fprintf( stderr, "MMAP failed for device %s\n", mDeviceName.c_str() );
			// Note: aquired resources should be properly released here!
			return false;	
		}
	}

	// Initiate memory mapping with previous buffers
    for ( int bufferIndex=0; bufferIndex<numBuffers; ++bufferIndex ) 
	{
		struct v4l2_buffer buf;
		CLEAR(buf);
		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_MMAP;
		buf.index = bufferIndex;
		if ( xioctl( mHandle, VIDIOC_QBUF, &buf )==-1 )
		{
			fprintf( stderr, "VIDIOC_QBUF failed for device %s\n", mDeviceName.c_str() );
			// Note: aquired resources should be properly released here!
			return false;	
		}
	}
	
	// Start streaming/capture
    enum v4l2_buf_type type;
	type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if ( xioctl( mHandle, VIDIOC_STREAMON, &type)==-1 )
	{
		fprintf( stderr, "VIDIOC_STREAMON failed for device %s\n", mDeviceName.c_str() );
		// Note: aquired resources should be properly released here!
		return false;			
	}
	
	// Capture indicator
	mIsCapturing = true;

	// Update buffer members from local variables
	assert( mNumBuffers==0);
	assert( !mBuffers );
	mNumBuffers = numBuffers;
	mBuffers = buffers;
	
	// Notify
	for ( Listeners::const_iterator itr=mListeners.begin(); itr!=mListeners.end(); ++itr )
		(*itr)->onDeviceStarted( this );

	return true;
}

bool Device::stopCapture()
{
	if ( !isCapturing() )
		return true;

	// Stop streaming/capture
	enum v4l2_buf_type type;
	type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if ( xioctl( mHandle, VIDIOC_STREAMOFF, &type )==-1 )
	{
		fprintf( stderr, "VIDIOC_STREAMOFF failed for device %s\n", mDeviceName.c_str() );
		return false;
	}

	// Unmap buffers
	for ( int i=0; i<mNumBuffers; ++i )				
	{
		if ( munmap(mBuffers[i].start, mBuffers[i].length)==-1 )
		{
			fprintf( stderr, "MUNMAP failed for device %s\n", mDeviceName.c_str() );
			// Note: aquired resources should be properly released here!
			return false;
		}
	}

	// Free the list of buffers
	free( mBuffers );
	mBuffers = NULL;
	mNumBuffers = 0;

	// Free the CapturedImage object
	delete mCapturedImage;
	mCapturedImage = NULL;

	// Capture indicator
	mIsCapturing = false;
	
	// Notify
	for ( Listeners::const_iterator itr=mListeners.begin(); itr!=mListeners.end(); ++itr )
		(*itr)->onDeviceStopped( this );

	return true;
}

// If an image was read into the CapturedImage, notifies client code and return true.
// Returns false if either no image was ready or an error occured
bool Device::updateCapturedImage()
{
	assert( mCapturedImage );

	struct v4l2_buffer buf;
    unsigned int i;
    CLEAR(buf);
	buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
	if ( xioctl( mHandle, VIDIOC_DQBUF, &buf )==-1 ) 
	{
		if ( errno==EAGAIN )
			return false;		// No image is ready yet, this is a normal situation
		//else if ( errno!=EIO ) 
		//	return true;		// This might be an ignorable error (?)
		fprintf( stderr, "VIDIOC_DQBUF failed for device %s\n", mDeviceName.c_str() );
		return false; 
	}
	assert( buf.index<mNumBuffers );
	
	unsigned char* sourceBytes = static_cast<unsigned char*>(mBuffers[buf.index].start);		// SHOULD BE unsignd char* directly
	unsigned int numBytes = buf.bytesused;
	MemoryBuffer& destBuffer = mCapturedImage->getImage().getBuffer();
	bool ret = destBuffer.copyFrom( sourceBytes, numBytes );
	if ( !ret )
	{
		fprintf( stderr, "Failed to copy image data from MMAP buffer to captured image for device %s\n", mDeviceName.c_str() );		

		// Re-queue the buffer we've just dequeued
		if ( xioctl( mHandle, VIDIOC_QBUF, &buf )==-1 )
		{
			fprintf( stderr, "VIDIOC_QBUF failed for device %s\n", mDeviceName.c_str() );
			return false;	
		}	

		return false;
	}

	// Update sequence number
    //mCapturedImage->setSequenceNumber( buf.sequence );
    mCapturedImage->setSequenceNumber( mCapturedImage->getSequenceNumber()+1 );

	// Update timestamp 
	struct timeval timestampVal = buf.timestamp;
	unsigned int timestampInMs = timestampVal.tv_sec * 1000 + ( timestampVal.tv_usec / 1000 );
	float timestampInSec = static_cast<float>(timestampInMs) / 1000.f;
	mCapturedImage->setTimestampInSec( timestampInSec );
	
	// Notify
	for ( Listeners::const_iterator itr=mListeners.begin(); itr!=mListeners.end(); ++itr )
		(*itr)->onDeviceCapturedImage( this );

	// Re-queue the buffer we've just dequeued
	if ( xioctl( mHandle, VIDIOC_QBUF, &buf )==-1 )
	{
		fprintf( stderr, "VIDIOC_QBUF failed for device %s\n", mDeviceName.c_str() );
		return false;	
	}	

	return true;
}

void Device::update()
{
	if ( !isCapturing() )
		return;
	
	bool cont = false; 
	do 
	{
		cont = updateCapturedImage();
	}
	while ( cont );
}

int Device::xioctl(int fh, int request, void *arg)
{
	int r;
	do 
	{
		r = ioctl( fh, request, arg );
	} 
	while ( r==-1 && errno==EINTR );
	return r;
}

void Device::addListener( Listener* listener )
{
	assert(listener);
	mListeners.push_back(listener);
}

bool Device::removeListener( Listener* listener )
{
	Listeners::iterator itr = std::find( mListeners.begin(), mListeners.end(), listener );
	if ( itr==mListeners.end() )
		return false;
	mListeners.erase( itr );
	return true;
}

}
