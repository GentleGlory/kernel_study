* Tested in RK3576.
* Before you runing this program, you need to stop the usbdevice.service and then install the g_zero.ko.
	```bash
	sudo systemctl stop usbdevice.service
	sudo systemctl disable usbdevice.service
	sudo insmod g_zero.ko
	```