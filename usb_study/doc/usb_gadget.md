# 重要結構

* usb_composite_dev
```c
struct usb_composite_dev {
	struct usb_gadget		*gadget;
	struct usb_request		*req;
	struct usb_request		*os_desc_req;

	struct usb_configuration	*config;

	/* OS String is a custom (yet popular) extension to the USB standard. */
	u8				qw_sign[OS_STRING_QW_SIGN_LEN];
	u8				b_vendor_code;
	struct usb_configuration	*os_desc_config;
	unsigned int			use_os_string:1;

	/* private: */
	/* internals */
	unsigned int			suspended:1;
	struct usb_device_descriptor	desc;
	struct list_head		configs;
	struct list_head		gstrings;
	struct usb_composite_driver	*driver;
	u8				next_string_id;
	char				*def_manufacturer;

	/* the gadget driver won't enable the data pullup
	 * while the deactivation count is nonzero.
	 */
	unsigned			deactivations;

	/* the composite driver won't complete the control transfer's
	 * data/status stages till delayed_status is zero.
	 */
	int				delayed_status;

	/* protects deactivations and delayed_status counts*/
	spinlock_t			lock;

	/* public: */
	unsigned int			setup_pending:1;
	unsigned int			os_desc_pending:1;
};
```

* usb_gadget
```c
struct usb_gadget {
	struct work_struct		work;
	struct usb_udc			*udc;
	/* readonly to gadget driver */
	const struct usb_gadget_ops	*ops;
	struct usb_ep			*ep0;
	struct list_head		ep_list;	/* of usb_ep */
	enum usb_device_speed		speed;
	enum usb_device_speed		max_speed;

	/* USB SuperSpeed Plus only */
	enum usb_ssp_rate		ssp_rate;
	enum usb_ssp_rate		max_ssp_rate;

	enum usb_device_state		state;
	const char			*name;
	struct device			dev;
	unsigned			isoch_delay;
	unsigned			out_epnum;
	unsigned			in_epnum;
	unsigned			mA;
	struct usb_otg_caps		*otg_caps;

	unsigned			sg_supported:1;
	unsigned			is_otg:1;
	unsigned			is_a_peripheral:1;
	unsigned			b_hnp_enable:1;
	unsigned			a_hnp_support:1;
	unsigned			a_alt_hnp_support:1;
	unsigned			hnp_polling_support:1;
	unsigned			host_request_flag:1;
	unsigned			quirk_ep_out_aligned_size:1;
	unsigned			quirk_altset_not_supp:1;
	unsigned			quirk_stall_not_supp:1;
	unsigned			quirk_zlp_not_supp:1;
	unsigned			quirk_avoids_skb_reserve:1;
	unsigned			is_selfpowered:1;
	unsigned			deactivated:1;
	unsigned			connected:1;
	unsigned			lpm_capable:1;
	unsigned			wakeup_capable:1;
	unsigned			wakeup_armed:1;
	int				irq;
	int				id_number;
};
```
* usb_ep
```c
struct usb_ep {
	void			*driver_data;

	const char		*name;
	const struct usb_ep_ops	*ops;
	struct list_head	ep_list;
	struct usb_ep_caps	caps;
	bool			claimed;
	bool			enabled;
	unsigned		maxpacket:16;
	unsigned		maxpacket_limit:16;
	unsigned		max_streams:16;
	unsigned		mult:2;
	unsigned		maxburst:5;
	u8			address;
	const struct usb_endpoint_descriptor	*desc;
	const struct usb_ss_ep_comp_descriptor	*comp_desc;
};
```
