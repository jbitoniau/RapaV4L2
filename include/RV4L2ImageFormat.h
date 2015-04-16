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

namespace RV4L2
{

class ImageFormat
{
public:
	enum Encoding
	{
		Grayscale8,
		RGB24,	
		YUYV,
		
		_32Bits,
				
		EncodingCount	
	};

	ImageFormat();
	ImageFormat( unsigned int width, unsigned int height, Encoding encoding );

	unsigned int			getWidth() const			{ return mWidth; }
	unsigned int			getHeight() const			{ return mHeight; }
	Encoding				getEncoding() const			{ return mEncoding; }
	const char*				getEncodingName() const		{ return getEncodingName( getEncoding() ); }
	static const char*		getEncodingName( Encoding encoding );
	
	unsigned int			getNumBitsPerPixel() const		{ return getNumBitsPerPixel( getEncoding() ); }
	static unsigned int		getNumBitsPerPixel( Encoding encoding );
	unsigned int			getNumBytesPerLine() const		{ return getNumBitsPerPixel()*getWidth()/8; }	// Note: rounded to the upper byte?
	unsigned int			getDataSizeInBytes() const;

	bool					operator==( const ImageFormat& other ) const;
	bool					operator!=( const ImageFormat& other ) const;

	std::string				toString() const;

private:
	
	static const char*		mEncodingNames[EncodingCount];
	static unsigned int		mEncodingBitsPerPixel[EncodingCount];
	
	unsigned int			mWidth;
	unsigned int			mHeight;
	Encoding				mEncoding;
};

}
