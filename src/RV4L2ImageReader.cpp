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
#include "RV4L2ImageReader.h"

#include <stdio.h>
#include <cstring>
#include <assert.h>

namespace RV4L2
{

Image* ImageReader::readBinaryPGMImage( const char* filename )
{
	FILE* file = fopen( filename, "rb" );
	if ( !file )
		return NULL;
	
	int width = 0;
	int height = 0;
	int maxValue = 0;
	if ( fscanf( file, "P5%d %d %d", &width, &height, &maxValue)!= 3 )
	{
		// Couldn't read header
		fclose( file );
		return NULL;
	}

	if ( width<=0 || height<=0 )
	{
		// Invalid image size
		fclose( file );
		return NULL;
	}
	
	if ( maxValue>0xFF )
	{
		// Only support 1-byte pixel value
		fclose( file );
		return NULL;
	}

	while ( fgetc( file )!=0x0a )
	{
		if ( feof( file ) )
		{
			// Reached end of file before any image data could be found
			return NULL;
		}
	}

	// Create image
	ImageFormat imageFormat( width, height, ImageFormat::Grayscale8 );
	Image* image = new Image( imageFormat );
	
	// Read image data 
	int numBytes = image->getBuffer().getSizeInBytes();
	unsigned char* bytes = image->getBuffer().getBytes();
	if ( fread( bytes, numBytes, 1, file )!=1 )
	{
		// Failed to read the image data
		fclose( file );
		delete image;
		image = NULL;
		return NULL;
	}

	fclose( file );		
	return image;
}
}
