#include <libusb-1.0/libusb.h>
#include <stdio.h>
#include <signal.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#define ZERO_VID	0x0525
#define ZERO_PID	0xa4a0

#define CHECK_RESULT(func, err_str, err_flag)					\
	do {											\
		int __check_result = (func);				\
		if (__check_result != LIBUSB_SUCCESS) {		\
			fprintf(stderr, "%s: %s\n", err_str,	\
			        libusb_strerror(__check_result));\
			ret = __check_result;				\
			goto err_flag;					\
		}										\
		ret = __check_result; \
	} while (0)


static void print_usage(const char * app)
{
	printf("%s -l : List all configuration.\n", app);
	printf("%s -s config_num: Set the configuration with config_num.\n", app);
	printf("%s -wstr string: Write string.\n", app);
	printf("%s -rstr : Read string.\n", app);
	printf("%s -w <data1 data2 data3 ...>: Write Data.\n", app);
	printf("%s -r: Read data.\n", app);	
}

static int get_bulk_endponts(libusb_device *device, int *in_ep, int *out_ep, 
	int *max_in_packet_size, int *max_out_packet_size)
{	
	int ret = -1;
	struct libusb_config_descriptor *config;
	const struct libusb_interface *iface = NULL;
	const struct libusb_interface_descriptor *altsetting = NULL;
	const struct libusb_endpoint_descriptor *ep_desc = NULL;
	int found = 0;

	CHECK_RESULT(libusb_get_active_config_descriptor(device, &config), 
				"failed to get active config desc", error);

	iface = &config->interface[0];
	altsetting = &iface->altsetting[0];

	for (int ep_idx = 0; ep_idx < altsetting->bNumEndpoints; ep_idx++) {
		ep_desc = &altsetting->endpoint[ep_idx];
		if ((ep_desc->bmAttributes & LIBUSB_TRANSFER_TYPE_BULK) == LIBUSB_TRANSFER_TYPE_BULK) {
			if (ep_desc->bEndpointAddress & LIBUSB_ENDPOINT_IN) {
				*in_ep = ep_desc->bEndpointAddress;
				*max_in_packet_size = ep_desc->wMaxPacketSize;
				found ++;
			} else {
				*out_ep = ep_desc->bEndpointAddress;
				*max_out_packet_size = ep_desc->wMaxPacketSize;
				found ++;
			}
		}
	}
error:
	return ret == LIBUSB_SUCCESS ? 
		(found == 2 ? LIBUSB_SUCCESS : LIBUSB_ERROR_NOT_FOUND) : ret;
}

int main(int argc, char *argv[])
{
	int ret = -1;
	libusb_device_handle *handle = NULL;
	libusb_device *device = NULL;
	struct libusb_device_descriptor desc;
	int in_ep = 0, out_ep = 0, max_in_packet_size = 0, max_out_packet_size = 0;

	if (argc <= 1) {
		print_usage(argv[0]);
		return -1;
	}
	
	ret = libusb_init(NULL);
	if (ret) {
		fprintf(stderr,"Failed to init libusb, error:%d\n",ret);
		return -1;
	}

	handle = libusb_open_device_with_vid_pid(NULL, ZERO_VID, ZERO_PID);

	if (handle == NULL) {
		fprintf(stderr,"Failed to open zero device\n");
		ret = -1;
		goto error;
	}

	device = libusb_get_device(handle);

	if (!strcmp(argv[1], "-l")) {
		// list configs
		CHECK_RESULT(libusb_get_device_descriptor(device, &desc),
					"failed to get device desc", error);
		
		for (int cfg = 0; cfg < desc.bNumConfigurations; cfg++) {
			struct libusb_config_descriptor *config;
			CHECK_RESULT(libusb_get_config_descriptor(device, cfg, &config), 
					"failed to get configg desc", error);

			printf("Config value:%u\n", config->bConfigurationValue);
		}

		ret = 0;
	} else if (!strcmp(argv[1], "-s")) {
		if (argc != 3) {
			print_usage(argv[0]);
			ret = -1;
			goto error;
		}

		int config_num = atoi(argv[2]);

		CHECK_RESULT(libusb_set_auto_detach_kernel_driver(handle, 1),
					"failed to set auto detach kernel driver", error);

		ret = libusb_detach_kernel_driver(handle, 0);
		if (ret != LIBUSB_SUCCESS && ret != LIBUSB_ERROR_NOT_FOUND) {
			fprintf(stderr, "failed to detach kernel driver: %s\n",
					libusb_strerror(ret));
			goto error;
		}

		CHECK_RESULT(libusb_set_configuration(handle, config_num),
					"failed to set config", error);
		
		ret = 0;
	} else if (!strcmp(argv[1], "-wstr") || 
		!strcmp(argv[1], "-rstr") ||
		!strcmp(argv[1], "-w") || 
		!strcmp(argv[1], "-r") ) {
		
		CHECK_RESULT(get_bulk_endponts(device, &in_ep, &out_ep, &max_in_packet_size, &max_out_packet_size),
					"failed to get bulk end points", error);
		
		CHECK_RESULT(libusb_set_auto_detach_kernel_driver(handle, 1),
					"failed to set auto detach kernel driver", error);

		ret = libusb_detach_kernel_driver(handle, 0);
		if (ret != LIBUSB_SUCCESS && ret != LIBUSB_ERROR_NOT_FOUND) {
			fprintf(stderr, "failed to detach kernel driver: %s\n",
				libusb_strerror(ret));
			goto error;
		}

		CHECK_RESULT(libusb_claim_interface(handle, 0),
					"failed to claim interface", error);

		if (!strcmp(argv[1], "-wstr")) {

			if (argc != 3) {
				print_usage(argv[0]);
				ret = -1;
				goto error;
			}

			int len = strlen(argv[2]) > max_out_packet_size ?
					max_out_packet_size : strlen(argv[2]);
			int act_len;
			CHECK_RESULT(libusb_bulk_transfer(handle, out_ep, argv[2], len, &act_len, 1000),
						"failed to write string", error);
			
			printf("string len:%d send\n", act_len);

		} else if (!strcmp(argv[1], "-rstr")) {
			char *buffer = (char *)malloc(max_in_packet_size + 1);
			memset(buffer, 0, max_in_packet_size + 1);

			int act_len;
			CHECK_RESULT(libusb_bulk_transfer(handle, in_ep, buffer, max_in_packet_size, &act_len, 1000),
						"failed to read string", error);

			printf("string len:%d read, %s\n", act_len, buffer);

			free(buffer);
		} else if (!strcmp(argv[1], "-w")) {
			if (argc < 3) {
				print_usage(argv[0]);
				ret = -1;
				goto error;
			}
			
			char *buffer = (char *)malloc(max_out_packet_size);
			
			for (int i = 2; i < argc && i < max_out_packet_size + 2; i++) {
				buffer[i-2] = strtoul(argv[i], NULL, 0);
			}
			int len = argc >= max_out_packet_size + 2 ? max_out_packet_size : argc - 2;
			int act_len;
			CHECK_RESULT(libusb_bulk_transfer(handle, out_ep, buffer, len, &act_len, 1000),
						"failed to write data", error);
			free(buffer);
		} else {
			char *buffer = (char *)malloc(max_in_packet_size);
			int act_len;
			CHECK_RESULT(libusb_bulk_transfer(handle, in_ep, buffer, max_in_packet_size, &act_len, 1000),
						"failed to read data", error);

			printf("Read data: \n");
			for (int i = 0; i < act_len; i++) {
				printf("%02x ", buffer[i]);
				if ((i+1) % 16 == 0)
					printf("\n");
			}
		}
	} else {
		print_usage(argv[0]);
		goto error;
	}

error:
	if (handle != NULL) {
		libusb_close(handle);
		handle = NULL;
	}
	
	libusb_exit(NULL);
	return ret;
}
