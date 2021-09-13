#pragma once
#include <map> 
#include <string>
#include <vector>
#include <sstream>
#include "stdint.h"
#include "stdio.h"
#include "libusb-1.0/libusb.h"
#include "pthread.h"
#include "string.h"
#include "unistd.h"

using namespace std;

#ifndef ARRAYSIZE
#define ARRAYSIZE(A)  sizeof(A)/sizeof(A[0])
#endif

#define UCHAR unsigned char
#define PCHAR char*
#define PUCHAR unsigned char*

enum class usb_event
{
	usb_device_attach,
	usb_device_detach,
};

typedef struct {
	uint16_t vid;
	uint16_t pid;
	int type;
	const char* designation;
}known_device;

#define FX_TYPE_AN21		0	/* Original AnchorChips parts */
#define FX_TYPE_FX1			1	/* Updated Cypress versions */
#define FX_TYPE_FX2			2	/* USB 2.0 versions */
#define FX_TYPE_FX2LP		3	/* Updated FX2 */
#define FX_TYPE_FX3			4	/* USB 3.0 versions */
#define UNKNOWN_TYPE		-1  /*unknown usb device*/

#define KNOWN_DEVICES { \
	{ 0x5744, 0x0001, FX_TYPE_FX3, "Digital Receiver RevA"},\
	{ 0x5744, 0x0002, FX_TYPE_FX3, "Digital Receiver RevB"},\
	{ 0x5744, 0x0102, FX_TYPE_FX3, "Signal Generator RevB"},\
	{ 0x5744, 0x0003, FX_TYPE_FX3, "Digital Receiver RevC"},\
}

typedef struct
{
	int speed;
	const char* desc; //description
}SpeedInfo;

#define SPEED_LEVELS { \
	{0,"Unknown"},\
	{1,"Low Speed (1.5Mbps)"},\
	{2,"Full Speed (12Mbps)"},\
	{3,"Hight Speed (480Mbps)"},\
	{4,"Super Speed (5Gbps)"},\
	{5, "Super Speed Plus (10Gbps)"},\
}

typedef struct EndPoint
{
	uint16_t endpoint;
	const char* direction;
	const char* type;
}EndPoint;

#define BULK_TRANSFER_SIZE (1024)
#define CONTROL_TRANSFER_SIZE (512)
#define MEM_GAP_SIZE (2)

class USB3Driver
{
	typedef struct pthread_args_t
	{
		USB3Driver* class_ptr;
		//add others args at here
	}pthread_args_t;

private:
	pthread_mutex_t mutex_find;
	pthread_mutex_t mutex_open;
	
	libusb_context* ctx;
	std::pair<string, libusb_device*> usbDev;
	libusb_device **devs;
	libusb_device_handle* devHandle;
	bool isOpened;
	bool isSupportHotplug;
	bool isHotPlugEN;
	int devNum;
	string devName;
	int libusb_status;
	map<string, libusb_device*> dev_lists;
	libusb_hotplug_callback_handle hpHANDLE;
	uint8_t endpoint_bulk_in;
	uint8_t endpoint_bulk_out;
	uint16_t vid;
	uint16_t pid;
	uint16_t packet_size;

	bool func_attach_flag;
	bool func_detach_flag;
	pthread_t monitor_pthread;
	pthread_attr_t monitor_pthread_attr;
	sched_param monitor_pthread_param;

	bool hotplug_init_flag;
	
	void device_attach_process(USB3Driver* class_ptr);
	void device_dettach_process(USB3Driver* class_ptr);
	bool running;
	bool monitor_running;
	void monitor_device();
	static void* monitor_device_wrapper(void* args);
	int find(uint16_t _vid, uint16_t _pid, bool is_quiet = false);
	int find();
	int find_quiet();
	struct libusb_transfer* bulkread_transfer, * bulkwrite_transfer, * ctl_transfer;
	void start_events_thread();
	pthread_t events_pthread;
	static void* events_wrapper(void* args);
	void events_process();
public:
	uint16_t get_packet_size();
	int device_monitor_register_callback(void(*func)(usb_event event, void*), void* cb_user_data, const char* str);
	USB3Driver(uint16_t _vid, uint16_t _pid, bool hotplugen = false);
	~USB3Driver();
	int init();
	int open(uint16_t _vid, uint16_t _pid);
	int open();
	int close();
	void start_monitor();
	void stop_monitor();
	void destory();
	int get_devnum();
	string get_device_location(libusb_device * usbDev);
	bool is_open();
	string get_devname();
	void clear_eventsflag(bool isAttach);
	bool is_hotplug();
	int hotplug_init();
	int close_transfer();
	static int hotplug_callback_dettach(libusb_context* ctx, libusb_device* dev, libusb_hotplug_event event, void* user_data);
	static int hotplug_callback_attach(libusb_context* ctx, libusb_device* dev, libusb_hotplug_event event, void* user_data);
};