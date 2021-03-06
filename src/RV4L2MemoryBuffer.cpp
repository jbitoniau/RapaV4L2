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
#include "RV4L2MemoryBuffer.h"

#include <stddef.h>		// For NULL
#include <memory.h>

namespace RV4L2
{

MemoryBuffer::MemoryBuffer()
	: mBytes(NULL),
	  mSizeInBytes(0)
{
}

MemoryBuffer::MemoryBuffer( unsigned int sizeInBytes )
	: mBytes(NULL),
	  mSizeInBytes(sizeInBytes)
{
	mBytes = new unsigned char[mSizeInBytes];
	fill(0);
}

MemoryBuffer::MemoryBuffer( const MemoryBuffer& other )
	: mBytes(NULL),
	  mSizeInBytes( other.getSizeInBytes() )
{
	mBytes = new unsigned char[mSizeInBytes];
	memcpy( mBytes, other.getBytes(), other.getSizeInBytes() );
}

MemoryBuffer::~MemoryBuffer()
{
	delete[] mBytes;
	mBytes = NULL;
	mSizeInBytes = 0;
}

void MemoryBuffer::fill( char value )
{
	memset( mBytes, value, getSizeInBytes() );
}

bool MemoryBuffer::copyFrom( const MemoryBuffer& other )
{
	return copyFrom( other.getBytes(), other.getSizeInBytes() );
}

bool MemoryBuffer::copyFrom( const unsigned char* otherBytes, unsigned int numOtherBytes )
{
	if ( numOtherBytes!=getSizeInBytes() )
		return false;
	memcpy( mBytes, otherBytes, getSizeInBytes() );
	return true;
}

}