#ifndef _LIGHTSWITCH_H_
#define _LIGHTSWITCH_H_

void light_io_init(void);
#if 0
void led_status_report_task(void* arm);
#endif
void hap_register_device_handler(char *acc_id);

#endif
