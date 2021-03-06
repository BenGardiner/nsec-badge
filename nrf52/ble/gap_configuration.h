//  Copyright (c) 2018
//  Nicolas Aubry <nicolas.aubry@hotmail.com>
//
//  License: MIT (see LICENSE for details)


#include <ble_gap.h>
#include "vendor_service.h"


void set_default_gap_parameters(const char* device_name, ble_gap_adv_params_t* advertising_parameters);

void set_default_advertised_service(VendorService*);
