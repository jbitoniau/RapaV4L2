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
#ifdef _MSC_VER
	#pragma warning( push )
	#pragma warning ( disable : 4127 )
	#pragma warning ( disable : 4231 )
	#pragma warning ( disable : 4251 )
	#pragma warning ( disable : 4800 )	
#endif
#include <QApplication>
#ifdef _MSC_VER
	#pragma warning( pop )
#endif

#include "RV4L2QDeviceWidget.h"
#include "RV4L2QDeviceUpdater.h"

int main(int argc, char *argv[])
{		
    QApplication app( argc, argv );
	
	std::string deviceName = "/dev/video0";
    if ( argc>=2 )
		deviceName = argv[1];

    RV4L2::Device* device = new RV4L2::Device( deviceName.c_str() );

    RV4L2::QDeviceUpdater* deviceUpdater = new RV4L2::QDeviceUpdater(device, 2, NULL);
    deviceUpdater->start();

	RV4L2::QDeviceWidget* mainWindow = new RV4L2::QDeviceWidget(NULL, device);
    mainWindow->setWindowTitle(deviceName.c_str());

	mainWindow->resize(320, 480);
	mainWindow->show();

    int ret = app.exec();

	delete mainWindow;
	mainWindow = NULL;

    delete deviceUpdater;
    deviceUpdater = NULL;

    delete device;
    device = NULL;

	return ret;
}
