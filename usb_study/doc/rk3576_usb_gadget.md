# usb gadget 初始化流程

* dwc3_probe 在 dwc3_core_init_mode 中建立好 drd_work . 等到 dwc3_set_mode 被呼叫的時候 就會去呼叫 queue_work 然後觸發 __dwc3_set_mode. __dwc3_set_mode 會根據要設定模式去初始化 usb controller. 如果是要初始化為 device 就會再呼叫 dwc3_gadget_init.
```c
kernel-6.1/drivers/usb/dwc3/core.c

dwc3_probe
	dwc3_core_init_mode
		INIT_WORK(&dwc->drd_work, __dwc3_set_mode);


//dwc3_set_mode 會設定 dwc->desired_dr_role . 
//然後再呼叫 queue_work 觸發 __dwc3_set_mode.
dwc3_set_mode(struct dwc3 *dwc, u32 mode)
	dwc->desired_dr_role = mode;
	queue_work(system_freezable_wq, &dwc->drd_work);


__dwc3_set_mode
	dwc3_otg_update
		dwc3_gadget_init
```
* dwc3_gadget_init
分配 dma buffer, 以及 struct gadget . 呼叫 usb_initialize_gadget 初始化 struct gadget. 呼叫 dwc3_gadget_init_endpoints 加入 end points. 最後呼叫 usb_add_gadget 將 gadget 加進去 udc driver list 中

```c
dwc3_gadget_init
	dwc3_gadget_get_irq
	usb_initialize_gadget
	dwc3_gadget_init_endpoints
	usb_add_gadget

```
* RK3576 的 ep num 為 16. 從 DWC3_GHWPARAMS3 (0xC14C) 讀出來.
```c
#define DWC3_NUM_EPS(p)		(((p)->hwparams3 &		\
			(DWC3_NUM_EPS_MASK)) >> 12)

static void dwc3_core_num_eps(struct dwc3 *dwc)
{
	struct dwc3_hwparams	*parms = &dwc->hwparams;

	dwc->num_eps = DWC3_NUM_EPS(parms);
}

```
* endpoint 的初始化

```c
dwc3_gadget_init_endpoints
	dwc3_gadget_init_endpoint

//如果 endpoint num 為 0. control endpoint
dwc3_gadget_init_control_endpoint
//out
dwc3_gadget_init_out_endpoint
//in
dwc3_gadget_init_in_endpoint
```
* Control endpoint: 將 dwc3_gadget_ep0_desc 設定到 controll end point上. 然後呼叫 dwc3_gadget_init_control_endpoint

```c

static struct usb_endpoint_descriptor dwc3_gadget_ep0_desc = {
	.bLength	= USB_DT_ENDPOINT_SIZE, // 7
	.bDescriptorType = USB_DT_ENDPOINT,
	.bmAttributes	= USB_ENDPOINT_XFER_CONTROL, // Control endpoint
};

if (!(dep->number > 1)) {
	dep->endpoint.desc = &dwc3_gadget_ep0_desc;
	dep->endpoint.comp_desc = NULL;
}

if (num == 0)
		ret = dwc3_gadget_init_control_endpoint(dep);

static const struct usb_ep_ops dwc3_gadget_ep0_ops = {
	.enable		= dwc3_gadget_ep0_enable,
	.disable	= dwc3_gadget_ep0_disable,
	.alloc_request	= dwc3_gadget_ep_alloc_request,
	.free_request	= dwc3_gadget_ep_free_request,
	.queue		= dwc3_gadget_ep0_queue,
	.dequeue	= dwc3_gadget_ep_dequeue,
	.set_halt	= dwc3_gadget_ep0_set_halt,
	.set_wedge	= dwc3_gadget_ep_set_wedge,
};

static int dwc3_gadget_init_control_endpoint(struct dwc3_ep *dep)
{
	struct dwc3 *dwc = dep->dwc;

	usb_ep_set_maxpacket_limit(&dep->endpoint, 512);
	dep->endpoint.maxburst = 1;
	dep->endpoint.ops = &dwc3_gadget_ep0_ops;
	if (!dep->direction)
		dwc->gadget->ep0 = &dep->endpoint;

	dep->endpoint.caps.type_control = true;

	return 0;
}

```
* in endpoint: 將 dwc3_gadget_ep_ops 設定給 in endpoint.

```c
dwc3_gadget_init_endpoint
	if (num == 0)
		ret = dwc3_gadget_init_control_endpoint(dep);
	else if (direction)
		ret = dwc3_gadget_init_in_endpoint(dep);
	else
		ret = dwc3_gadget_init_out_endpoint(dep);

static const struct usb_ep_ops dwc3_gadget_ep_ops = {
	.enable		= dwc3_gadget_ep_enable,
	.disable	= dwc3_gadget_ep_disable,
	.alloc_request	= dwc3_gadget_ep_alloc_request,
	.free_request	= dwc3_gadget_ep_free_request,
	.queue		= dwc3_gadget_ep_queue,
	.dequeue	= dwc3_gadget_ep_dequeue,
	.set_halt	= dwc3_gadget_ep_set_halt,
	.set_wedge	= dwc3_gadget_ep_set_wedge,
};


dwc3_gadget_init_in_endpoint
	...
	usb_ep_set_maxpacket_limit(&dep->endpoint, size);

	dep->endpoint.max_streams = 16;
	dep->endpoint.ops = &dwc3_gadget_ep_ops;
	list_add_tail(&dep->endpoint.ep_list,
			&dwc->gadget->ep_list);
	dep->endpoint.caps.type_iso = true;
	dep->endpoint.caps.type_bulk = true;
	dep->endpoint.caps.type_int = true;
	return dwc3_alloc_trb_pool(dep);
	...

```
* out endpoint: 將 dwc3_gadget_ep_ops 設定給 out endpoint.

```c
dwc3_gadget_init_endpoint
	if (num == 0)
		ret = dwc3_gadget_init_control_endpoint(dep);
	else if (direction)
		ret = dwc3_gadget_init_in_endpoint(dep);
	else
		ret = dwc3_gadget_init_out_endpoint(dep);

static const struct usb_ep_ops dwc3_gadget_ep_ops = {
	.enable		= dwc3_gadget_ep_enable,
	.disable	= dwc3_gadget_ep_disable,
	.alloc_request	= dwc3_gadget_ep_alloc_request,
	.free_request	= dwc3_gadget_ep_free_request,
	.queue		= dwc3_gadget_ep_queue,
	.dequeue	= dwc3_gadget_ep_dequeue,
	.set_halt	= dwc3_gadget_ep_set_halt,
	.set_wedge	= dwc3_gadget_ep_set_wedge,
};

dwc3_gadget_init_out_endpoint
	usb_ep_set_maxpacket_limit(&dep->endpoint, size);
	dep->endpoint.max_streams = 16;
	dep->endpoint.ops = &dwc3_gadget_ep_ops;
	list_add_tail(&dep->endpoint.ep_list,
			&dwc->gadget->ep_list);
	dep->endpoint.caps.type_iso = true;
	dep->endpoint.caps.type_bulk = true;
	dep->endpoint.caps.type_int = true;

	return dwc3_alloc_trb_pool(dep);
```