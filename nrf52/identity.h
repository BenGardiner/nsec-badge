//  Copyright (c) 2017
//  Benjamin Vanheuverzwijn <bvanheu@gmail.com>
//  Marc-Etienne M. Leveille <marc.etienne.ml@gmail.com>
//
//  License: MIT (see LICENSE for details)

#ifndef identity_h
#define identity_h

#include <string.h>

#define NSEC_IDENTITY_AVATAR_WIDTH 32
#define NSEC_IDENTITY_AVATAR_HEIGHT 32

void init_identity_service(void);
void nsec_identity_draw(void);
void nsec_identity_get_unlock_key(char * data, size_t length);
void nsec_identity_update_nearby(void);

#endif /* identity_h */
