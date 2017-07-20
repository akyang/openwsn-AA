#ifndef __LLATENCY_H
#define __LLATENCY_H

/**
\addtogroup AppUdp
\{
\addtogroup uinject
\{
*/

#include "opentimers.h"
#include "openudp.h"

//=========================== define ==========================================

#define LLATENCY_PERIOD_MS 300

//=========================== typedef =========================================

//=========================== variables =======================================

typedef struct {
   opentimers_id_t     timerId;  ///< periodic timer which triggers transmission
   uint16_t             counter;  ///< incrementing counter which is written into the packet
   uint16_t              period;  ///< uinject packet sending period>
   udp_resource_desc_t     desc;  ///< resource descriptor for this module, used to register at UDP stack
} llatency_vars_t;

//=========================== prototypes ======================================

void llatency_init(void);
void llatency_sendDone(OpenQueueEntry_t* msg, owerror_t error);
void llatency_receive(OpenQueueEntry_t* msg);
void llatency_get_values(uint32_t* values);
/**
\}
\}
*/

#endif

