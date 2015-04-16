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
#pragma once

#include <string>
#include "RV4L2CaptureSettings.h"
#include "RV4L2CapturedImage.h"

namespace RV4L2
{

class Device
{
public:
    Device( const char* deviceName );
	virtual ~Device();
	
	const std::string&			getDeviceName() const					{ return mDeviceName; }	
	bool						isValid() const							{ return mHandle!=-1; }

	const CaptureSettingsList&	getSupportedCaptureSettingsList() const	{ return mCaptureSettingsList; }
	bool						getSupportedCaptureSettingsIndex( const CaptureSettings& captureSettings, std::size_t& index ) const;

	bool						startCapture( std::size_t captureSettingsIndex ); 
	bool						isCapturing() const { return mIsCapturing; }
	bool						stopCapture(); 
	const CapturedImage*		getCapturedImage() const				{ return mCapturedImage; }
	void						update();

	class Listener
	{
	public:
		virtual ~Listener() {}
		virtual void onDeviceStarted( Device* /*device*/ ) {}
		virtual void onDeviceCapturedImage( Device* /*device*/ ) {}
		virtual void onDeviceStopped( Device* /*device*/ ) {}
	};

	void						addListener( Listener* listener );
	bool						removeListener( Listener* listener );

protected:
	static int					xioctl( int fh, int request, void *arg );
	bool						openDevice();
	bool						checkDeviceCapabilities();
	void						closeDevice();	
	void						initializeInternalCaptureSettingsList();
	static bool					getImageFormatEncoding( unsigned int v4l2PixelFormat, ImageFormat::Encoding& encoding );
	void						initializeCaptureSettingsList();
	bool						updateCapturedImage();

private:
	struct InternalCaptureSettings
	{
		InternalCaptureSettings();
		std::string		pixelFormatName;
		unsigned int	pixelFormat;
		unsigned int	width;
		unsigned int	height;
		unsigned int	frameIntervalNumerator;
		unsigned int	frameIntervalDenominator;
		
		float			getFrameRateInHz() const; 
		float			getFrameIntervalInS() const; 
		std::string		toString() const;
	};

	std::string								mDeviceName;
	int 									mHandle;

	std::vector<InternalCaptureSettings>	mInternalCaptureSettingsList;
	CaptureSettingsList						mCaptureSettingsList;
	std::vector<std::size_t>				mCaptureSettingsToInternalCaptureSettingsIndices;
	
	bool									mIsCapturing;
	CapturedImage*							mCapturedImage;

	struct buffer 
	{
        void   *start;		// unsigned char?
        size_t  length;
	};
	struct buffer*							mBuffers;
	unsigned int							mNumBuffers;
    static const unsigned int				mNumBuffersToUse = 3;

	typedef	std::vector<Listener*> Listeners; 
    Listeners								mListeners;

    std::vector<std::pair< unsigned int, unsigned int> > mFallbackFrameSizes;
};

}
