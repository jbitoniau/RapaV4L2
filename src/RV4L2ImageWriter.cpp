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
#include "RV4L2ImageWriter.h"

#include <stdio.h>
#include <cstring>
#include <assert.h>

namespace RV4L2
{

bool ImageWriter::writeBinaryPGMImage( const char* filename, const Image& image )
{
	if ( image.getFormat().getEncoding()!=ImageFormat::Grayscale8 )
		return false;

	FILE* file = fopen( filename, "wb" ); 
	if ( !file )
		return false;

	unsigned int width = image.getFormat().getWidth();
	unsigned int height = image.getFormat().getHeight();
	fprintf( file, "P5\n%d %d\n255\n", width, height );

	const unsigned char* bytes = image.getBuffer().getBytes();
	unsigned int sizeInBytes = image.getBuffer().getSizeInBytes();
	if ( fwrite( bytes, sizeInBytes, 1, file )!=1 )
	{
		fclose(file);
		return false;
	}
	fclose(file);
  
	return true;
}

bool ImageWriter::writeBinaryPPMImage( const char* filename, const Image& image )
{
	if ( image.getFormat().getEncoding()!=ImageFormat::RGB24 )
		return false;

	FILE* file = fopen( filename, "wb" ); 
	if ( !file )
		return false;

	unsigned int width = image.getFormat().getWidth();
	unsigned int height = image.getFormat().getHeight();
	fprintf( file, "P6\n%d %d\n255\n", width, height );

	const unsigned char* bytes = image.getBuffer().getBytes();
	unsigned int sizeInBytes = image.getBuffer().getSizeInBytes();
	if ( fwrite( bytes, sizeInBytes, 1, file )!=1 )
	{
		fclose(file);
		return false;
	}
	fclose(file);
  
	return true;
}

}
