#ifndef _LIGHTSWITCH_H_
#define _LIGHTSWITCH_H_

void light_io_init(void);
void temperature_humidity_monitoring_task(void* arm);
void hap_register_device_handler(char *acc_id);

#endif
