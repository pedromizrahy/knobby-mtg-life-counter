#ifndef _PLAYGROUP_API_H
#define _PLAYGROUP_API_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* USB/Serial provisioning + connectivity smoke test.
   No secrets are compiled into firmware or printed back to Serial. */
void playgroup_process_serial(void);
bool playgroup_credentials_ready(void);

#ifdef __cplusplus
}
#endif

#endif // _PLAYGROUP_API_H
