#include "hotplugtest.h"

USB3Driver::USB3Driver(uint16_t _vid, uint16_t _pid, bool hotplugen)
{
	isSupportHotplug = false;
	running = true;
	monitor_running = false;
	isOpened = false;
	hotplug_init_flag = false;
	devNum = -1;
	endpoint_bulk_in = 0x00;
	endpoint_bulk_out = 0x80;

	ctx = NULL;
	devs = NULL;
	devHandle = NULL;
	usbDev.first = "";
	usbDev.second = NULL;

	this->vid = _vid;
	this->pid = _pid;
	func_attach_flag = false;
	func_detach_flag = true;
	isHotPlugEN = hotplugen;	
	pthread_mutex_init(&mutex_find, NULL);
	pthread_mutex_init(&mutex_open, NULL);
	//pthread_mutex_init(&mutex_devHandle, NULL);
}

USB3Driver::~USB3Driver()
{
	printf("USB3_Driver destruct\n");
	destory();
}

int USB3Driver::get_devnum()
{
	return devNum;
}

string USB3Driver::get_device_location(libusb_device * usbDev)
{
	stringstream ss;
	uint8_t busnum, portnums[7];
	busnum = libusb_get_bus_number(usbDev);
	int portnums_len = libusb_get_port_numbers(usbDev, portnums, 7);
	//devaddr = libusb_get_device_address(usbDev);
	ss << "[";
	ss << unsigned(busnum);
	for(int i = 0; i < portnums_len; i++)
	{
		if(i == 0)
		{
			ss << "-";
		}
		ss << unsigned(portnums[i]);
		if(i < portnums_len - 1)
		{
			ss << ".";
		}
		
	}
	ss << "]";
	return ss.str();
}


bool USB3Driver::is_open()
{
	return isOpened;
}

string USB3Driver::get_devname()
{
	return devName;
}

void USB3Driver::clear_eventsflag(bool isAttach)
{
	if (isAttach)
	{
		func_attach_flag = false;
	}
	else
	{
		func_detach_flag = false;
	}
}

bool USB3Driver::is_hotplug()
{
	return isSupportHotplug;
}


int USB3Driver::hotplug_callback_dettach(struct libusb_context* ctx, struct libusb_device* dev, libusb_hotplug_event event, void* user_data)
{
	USB3Driver* class_ptr = (USB3Driver*)user_data;
	switch (event)
	{
	case LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT:
		class_ptr->device_dettach_process(class_ptr);
		break;
	default:
		printf("Unhandled usb event.\n");
		break;
	}
	return 0;
}

int USB3Driver::hotplug_callback_attach(struct libusb_context* ctx, struct libusb_device* dev, libusb_hotplug_event event, void* user_data)
{
	USB3Driver* class_ptr = (USB3Driver*)user_data;
	switch (event)
	{
	case LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED:
		class_ptr->device_attach_process(class_ptr);
		break;
	default:
		printf("Unhandled usb event.\n");
		break;
	}
	return 0;
}


/// <summary>
/// Init hotpulg for USB Device
/// </summary>
/// <returns>0:0, fail < 0 </returns>
int USB3Driver::hotplug_init()
{
	int status = 0;
	printf("Init HotPlug...\n");
	if (!hotplug_init_flag)
	{
		if (isSupportHotplug)
		{
			status = libusb_hotplug_register_callback(ctx,
				LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT,
				0,
				vid,
				pid,
				LIBUSB_HOTPLUG_MATCH_ANY,
				hotplug_callback_dettach,
				this,
				&hpHANDLE);
			status = libusb_hotplug_register_callback(ctx,
				LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED,
				0,
				vid,
				pid,
				LIBUSB_HOTPLUG_MATCH_ANY,
				hotplug_callback_attach,
				this,
				&hpHANDLE);
			if (LIBUSB_SUCCESS != status)
			{
				printf("Register hotplug callback function failed.\n");
				libusb_exit(ctx);
			}
			else
			{
				printf("Register hotplug callback function success.\n");
			}
		}
		start_monitor();
		hotplug_init_flag = true;
	}
	printf("HotPlug initialized.\n");
	return status;
}

void USB3Driver::device_attach_process(USB3Driver* class_ptr)
{
	if (class_ptr->func_detach_flag || !class_ptr->isOpened)
	{
		printf("Device monitor: device attached.\n");
		class_ptr->find();
		class_ptr->open();
		class_ptr->func_detach_flag = false;
		class_ptr->func_attach_flag = true;
	}
}

void USB3Driver::device_dettach_process(USB3Driver* class_ptr)
{
	if (class_ptr->func_attach_flag)
	{
		class_ptr->isOpened = false;	
		if(class_ptr->devHandle)
		{
			libusb_close(class_ptr->devHandle);
			printf("Close Device.\n");
			class_ptr->devHandle = NULL;
		}
		// if(NULL != usbDev)
		// {
		// 	libusb_unref_device(usbDev);
		// 	usbDev = NULL;
		// }
		printf("Device monitor: device detached.\n");
		class_ptr->func_attach_flag = false;
		class_ptr->func_detach_flag = true;
	}
}

void USB3Driver::monitor_device()
{
	while (monitor_running)
	{
		if (find_quiet() == 0)
		{
			device_attach_process(this);
		}
		else
		{
			device_dettach_process(this);
		}
		usleep(300000);
	}
}

void* USB3Driver::monitor_device_wrapper(void* ptr)
{
	try 
	{
		pthread_detach(pthread_self());
		USB3Driver* pargs = (USB3Driver*)(ptr);
		pargs->monitor_device();
		pthread_exit(NULL);
	}
#ifdef __PtW32CatchAll
	catch (__ptw32_exception_exit)
	{
		printf("Device Monitor thread exited.\n");
	}
#endif
	catch(exception& ex)
	{
		printf("monitor_device_wrapper exception: %s\n", ex.what());
	}
	return NULL;
}

int USB3Driver::init()
{
	int status = 0;
	status = libusb_init(&ctx);
	start_events_thread();
	printf("Start Initialize USB Driver...\n");
	if (status != 0)
	{
		printf("libusb init failed!\n");
		return status;
	}

	if (0 == find_quiet())
	{
		device_attach_process(this);
	}
	else
	{
		printf("Device [0x%04X:0x%04X] Not Found.\n", this->vid, this->pid);
		//device_dettach_process(this);
	}

	if (isHotPlugEN)
	{
		if (libusb_has_capability(LIBUSB_CAP_HAS_HOTPLUG) == 0)
		{
			isSupportHotplug = false;
			printf("Hotplug capabilites are not supported on this platform.\n");
		}
		else
		{
			isSupportHotplug = true;
			printf("Hotplug capabilites are supported on this platform.\n");
		}
		hotplug_init();
	}
	printf("USB Driver Initialized.\n");
	return status;
}

int USB3Driver::find(uint16_t _vid, uint16_t _pid, bool is_quiet)
{
	int status=0;
	libusb_device *dev;
	libusb_device_descriptor desc;
	uint16_t pid, vid;
	known_device known_devices[] = KNOWN_DEVICES;
	map<uint32_t, known_device*> devices;
	int count = 0;
	this->vid = _vid;
	this->pid = _pid;
	pthread_mutex_lock(&mutex_find);
	for (size_t i = 0; i < ARRAYSIZE(known_devices); i++)
	{
		devices.insert(std::pair<uint32_t, known_device*>((known_devices[i].vid<<16) + known_devices[i].pid, known_devices +i));
	}
	if (NULL != devs)
	{
		libusb_free_device_list(devs, 0);
		devs = NULL;
	}
	count = libusb_get_device_list(ctx, &devs);
	if (count < 0)
	{
		printf("libusb_get_device_list() failed: %s\n", libusb_error_name(libusb_status));
		pthread_mutex_unlock(&mutex_find);
		return -1;
	}
	for(std::map<string, libusb_device*>::iterator it = dev_lists.begin(); it != dev_lists.end(); it++)
	{
		if(NULL != it->second)
		{
			if (it->first != usbDev.first)
			{
				libusb_unref_device(it->second);
			}
		}
	}
	dev_lists.clear();
	for (int i = 0; i < count; i++)
	{
		dev = devs[i];
		status = libusb_get_device_descriptor(dev, &desc);
		string devLoc = get_device_location(dev);
		if (0 == status)
		{
			vid = desc.idVendor;
			pid = desc.idProduct;
			if (!is_quiet)
			{
				printf(
					"Examining [%04X:%04X] (%s)\n",
					vid,
					pid,
					devLoc.c_str());
			}
			if ((vid == _vid) && (pid == _pid))
			{		
				uint32_t key = (vid << 16) + pid;
				if(devices.count(key) == 1)
				{
					if (!is_quiet)
					{
						printf(
							"Found device \"%s\" [0x%04X:0x%04X] (%s)\n",
							devices[key]->designation,
							_vid,
							_pid,
							devLoc.c_str());
					}
				}
				else
				{
					if (!is_quiet)
					{
						printf(
							"Found device \"%s\" [0x%04X:0x%04X] (%s)\n",
							"unknown",
							vid,
							pid,
							devLoc.c_str());
					}
				}		
				dev_lists[devLoc] = dev;
			}
		}
	}
	devNum = dev_lists.size();
	pthread_mutex_unlock(&mutex_find);
	if (devNum > 0)
	{
		usbDev.first = dev_lists.begin()->first;
		usbDev.second = dev_lists.begin()->second;
		return 0;
	}
	else
	{
		usbDev.first = "";
		usbDev.second = NULL;
		if (!is_quiet)
		{
			printf("Not Found device [0x%04X:0x%04X]\n", _vid, _pid);
		}
		return -1;
	}
}

int USB3Driver::find()
{
	return find(this->vid, this->pid);		
}

int USB3Driver::find_quiet()
{
	return find(this->vid, this->pid, true);
}

int USB3Driver::open(uint16_t _vid, uint16_t _pid)
{
	int status = 0;
	libusb_device **devs;
	if (isOpened == true)
	{
		printf("[open] Device already opened, return\n");
		return 0;
	}
	if (NULL == usbDev.second)
	{
		status |= find(_vid, _pid);
		if (0 != status)
		{
			printf("No device found, try search device again!\n");
			return -1;
		}
		printf("Find %d device\n", dev_lists.size());
	}
	libusb_device_descriptor desc;
	status = libusb_get_device_descriptor(usbDev.second, &desc);
	if (LIBUSB_SUCCESS != status)
	{
		status = -1;
		printf("Get Device Descriptor failed! Error code: %d\n", status);
		return status;
	};
	if(devHandle)
	{
		libusb_close(devHandle);
		devHandle = NULL;
	}
	status = libusb_open(usbDev.second, &devHandle);
	if (LIBUSB_SUCCESS != status)
	{
		printf(
			"Open Device failed! Device: [0x%04x:0x%04x]. Error code: %d\n",
			desc.idVendor,
			desc.idProduct,
			status
		);
		return status;
	};

	if (libusb_set_auto_detach_kernel_driver(devHandle, 1))
	{
		printf("detach kernel driver failed\n");
	}
	status = libusb_claim_interface(devHandle, 0);
	if (status != LIBUSB_SUCCESS)
	{
		printf(
			"Claim Device failed!  Device: [0x%04x:0x%04x]. Error code: %d\n",
			desc.idVendor,
			desc.idProduct,
			status
		);
		return status;
	}
	else
	{
		int speed = 0;
		int maxPackageSize = 0;
		char friendlyName[128] = { 0 };
		int length = 0;
		std::stringstream ss;
		SpeedInfo speedtype[] = SPEED_LEVELS;
		speed = libusb_get_device_speed(usbDev.second);

		vector<libusb_endpoint_descriptor> endpoint_descs;
		for (int i = 0; i < desc.bNumConfigurations; i++)
		{
			struct libusb_config_descriptor* libusb_config_desc;
			status = libusb_get_config_descriptor(usbDev.second, i, &libusb_config_desc);
			if (status != LIBUSB_SUCCESS)
			{
				printf("libusb: get config descriptor failed!\n");
				continue;
			}
			for (int i = 0; i < libusb_config_desc->interface->altsetting->bNumEndpoints; i++)
			{
				endpoint_descs.push_back(libusb_config_desc->interface->altsetting->endpoint[i]);
			}
			libusb_free_config_descriptor(libusb_config_desc);
		}

		maxPackageSize = libusb_get_max_packet_size(usbDev.second, endpoint_descs[0].bEndpointAddress);
		packet_size = maxPackageSize;
		length = libusb_get_string_descriptor_ascii(devHandle, desc.iProduct, (UCHAR*)friendlyName, 128);
		ss << friendlyName;
		ss << " ";
		//get device location
		ss << get_device_location(usbDev.second);
		devName = ss.str();
		EndPoint endpoints[2] = { { 0,"","" }, { 0,"","" } };
		for (unsigned int i = 0; i < endpoint_descs.size(); i++)
		{
			endpoints[i].endpoint = endpoint_descs[i].bEndpointAddress;
			switch (endpoints[i].endpoint & 0x80)
			{
			case 0x80:
				endpoints[i].direction = "IN";
				switch (endpoint_descs[i].bmAttributes)
				{
				case libusb_transfer_type::LIBUSB_TRANSFER_TYPE_CONTROL:
					endpoints[i].type = "Control";
					break;
				case libusb_transfer_type::LIBUSB_TRANSFER_TYPE_ISOCHRONOUS:
					endpoints[i].type = "Isochronous";
					break;
				case libusb_transfer_type::LIBUSB_TRANSFER_TYPE_BULK:
					endpoint_bulk_in = endpoints[i].endpoint;
					endpoints[i].type = "Bulk";
					break;
				case libusb_transfer_type::LIBUSB_TRANSFER_TYPE_INTERRUPT:
					endpoints[i].type = "Interrupt";
					break;
				case libusb_transfer_type::LIBUSB_TRANSFER_TYPE_BULK_STREAM:
					endpoints[i].type = "Bulk Stream";
					endpoint_bulk_in = endpoints[i].endpoint;
					break;
				default:
					break;
				}
				break;
			case 0x00:
				endpoints[i].direction = "OUT";
				switch (endpoint_descs[i].bmAttributes)
				{
				case libusb_transfer_type::LIBUSB_TRANSFER_TYPE_CONTROL:
					endpoints[i].type = "Control";
					break;
				case libusb_transfer_type::LIBUSB_TRANSFER_TYPE_ISOCHRONOUS:
					endpoints[i].type = "Isochronous";
					break;
				case libusb_transfer_type::LIBUSB_TRANSFER_TYPE_BULK:
					endpoint_bulk_out = endpoints[i].endpoint;
					endpoints[i].type = "Bulk";
					break;
				case libusb_transfer_type::LIBUSB_TRANSFER_TYPE_INTERRUPT:
					endpoints[i].type = "Interrupt";
					break;
				case libusb_transfer_type::LIBUSB_TRANSFER_TYPE_BULK_STREAM:
					endpoint_bulk_out = endpoints[i].endpoint;
					endpoints[i].type = "Bulk Stream";
					break;
				default:
					break;
				}
				break;
			}
		}
		char chars[1024];
		memset(chars, 0, 1024);
		length = sprintf(chars, "Open Device Success!\nDevice Name: %s\n\t\tVID: 0x%04x\n\t\tPID: 0x%04x\n\t\tSpeedLevel: %s\n\t\tMaxPackageSize: %d\n",
			devName.c_str(),
			desc.idVendor,
			desc.idProduct,
			speedtype[speed].desc,
			maxPackageSize);
		for (unsigned int i = 0; i < endpoint_descs.size(); i++)
		{
			length = length + sprintf(chars + length, "\t\tEndPoint: 0x%02x Type: %s %s\n",
				endpoints[i].endpoint,
				endpoints[i].type,
				endpoints[i].direction);
		}
		printf("%s\n", chars);
		// func_detach_flag = false;
		// func_attach_flag = true;
		this->usbDev = usbDev;
		isOpened = true;
		status = 0;
	};
	return status;
}

int USB3Driver::open()
{
	int status = 0;
	status = open(vid, pid);
	return status;
}

int USB3Driver::close()
{
	int status = 0;
	isOpened = false;
	
	if (devHandle)
	{
		libusb_close(devHandle);
		devHandle = NULL;
	}
	if(NULL != usbDev.second)
	{
		libusb_unref_device(usbDev.second);
		usbDev.second = NULL;
	}
	func_attach_flag = false;
	func_detach_flag = true;

	return status;
}

void  USB3Driver::start_monitor()
{
	if(isSupportHotplug)
	{
		return;
	}
	monitor_running = true;
	pthread_attr_init(&monitor_pthread_attr);
	pthread_attr_setschedpolicy(&monitor_pthread_attr, SCHED_OTHER);
	pthread_attr_setdetachstate(&monitor_pthread_attr, PTHREAD_CREATE_JOINABLE);
	pthread_attr_setinheritsched(&monitor_pthread_attr, PTHREAD_EXPLICIT_SCHED);
	// monitor_pthread_param.sched_priority = thread_priority_medium;
	// pthread_attr_setschedparam(&monitor_pthread_attr, &monitor_pthread_param);
	pthread_create(&monitor_pthread, &monitor_pthread_attr, monitor_device_wrapper, this);
}

void  USB3Driver::stop_monitor()
{
	monitor_running = false;
	pthread_join(monitor_pthread, NULL);
}

void USB3Driver::destory()
{
	if (isSupportHotplug)
	{
		libusb_hotplug_deregister_callback(ctx, hpHANDLE);
	}
	if (isOpened)
	{
		libusb_release_interface(devHandle, 0);
		libusb_close(devHandle);
	}
	if (devs != NULL)
	{
		libusb_free_device_list(devs, 1);
		devs = NULL;
	}
	
	monitor_running = false;
	running = false;
	pthread_join(events_pthread, NULL);
	if(NULL != ctx)
	{
		libusb_exit(ctx);
		ctx = NULL;
	}
}

uint16_t USB3Driver::get_packet_size()
{
	return packet_size;
}

void USB3Driver::start_events_thread()
{
	pthread_attr_t pthread_attr;
    sched_param pthread_param;
    pthread_param.sched_priority = sched_get_priority_max(SCHED_RR);

    //set scheduling policy
    pthread_attr_init(&pthread_attr);
    pthread_attr_setschedpolicy(&pthread_attr, SCHED_RR);
    pthread_attr_setdetachstate(&pthread_attr, PTHREAD_CREATE_DETACHED);
    pthread_attr_setinheritsched(&pthread_attr, PTHREAD_EXPLICIT_SCHED);
    pthread_attr_setschedparam(&pthread_attr, &pthread_param);
	pthread_attr_setstacksize(&pthread_attr, 10*1024*1024);
	pthread_create(&events_pthread, NULL, events_wrapper, this);
}

void* USB3Driver::events_wrapper(void* ptr)
{
	try
    {
		//pthread_detach(pthread_self());
		pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
		USB3Driver* pargs = (USB3Driver*) ptr;
		pargs->events_process();
		pthread_exit(NULL);
    }
#ifdef __PtW32CatchAll
    catch (__ptw32_exception_exit)
    {
        printf("Events Process Thread exited.\n");
    }
#endif
    catch (exception& ex)
    {
        printf("events_wrapper exception: %s.\n", ex.what());
    }
    return NULL;
}

void USB3Driver::events_process()
{
	int rec = 0;
	timeval tv;
	tv.tv_sec = 0;
	tv.tv_usec = 100000;
	while (running)
	{
		//rec=libusb_handle_events(ctx);
		rec = libusb_handle_events_timeout(ctx, &tv);
		if(rec != LIBUSB_SUCCESS)
		{
			printf("Libusb Handle events Failed! rec: %d\n", rec);
		}
	}
}

int main()
{
	USB3Driver driver(0x09fb, 0x6010, true);
	driver.init();
	while (1)
	{
		usleep(3000000);
	}
	return 0;
}