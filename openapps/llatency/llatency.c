#include "opendefs.h"
#include "llatency.h"
#include "openqueue.h"
#include "openserial.h"
#include "packetfunctions.h"
#include "scheduler.h"
#include "IEEE802154E.h"
#include "idmanager.h"
#include "sctimer.h"
#include "debugpins.h"

//=========================== variables =======================================

llatency_vars_t llatency_vars;

static const uint8_t llatency_payload[]    = "llatency";
static const uint8_t llatency_dst_addr[]   = {
   0xbb, 0xbb, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01
}; 

//=========================== prototypes ======================================

void llatency_timer_cb(void);
void llatency_task_cb(void);

//=========================== public ==========================================

void llatency_init() {
   
    // clear local variables
    memset(&llatency_vars,0,sizeof(llatency_vars_t));

    // register at UDP stack
    llatency_vars.desc.port              = WKP_UDP_INJECT;
    llatency_vars.desc.callbackReceive   = &llatency_receive;
    llatency_vars.desc.callbackSendDone  = &llatency_sendDone;
    openudp_register(&llatency_vars.desc);

    llatency_vars.period = LLATENCY_PERIOD_MS;
    // start periodic timer
    llatency_vars.timerId = opentimers_create();
    opentimers_scheduleIn(
        llatency_vars.timerId,
        LLATENCY_PERIOD_MS,
        TIME_MS,
        TIMER_PERIODIC,
        llatency_timer_cb
    );
    debugpins_init();
}

void llatency_sendDone(OpenQueueEntry_t* msg, owerror_t error) {
   openqueue_freePacketBuffer(msg);
}

void llatency_receive(OpenQueueEntry_t* pkt) {
   
   openqueue_freePacketBuffer(pkt);
   
   openserial_printError(
      COMPONENT_LLATENCY,
      ERR_RCVD_ECHO_REPLY,
      (errorparameter_t)0,
      (errorparameter_t)0
   );
}

void llatency_get_values(uint32_t* values) {
  values[0] = opentimers_getValue();
  values[1] = ieee154e_getStartOfSlotReference();
}

//=========================== private =========================================

/**
\note timer fired, but we don't want to execute task in ISR mode instead, push
   task to scheduler with CoAP priority, and let scheduler take care of it.
*/
void llatency_timer_cb(void){
   
   scheduler_push_task(llatency_task_cb,TASKPRIO_COAP);
}

void llatency_task_cb() {
   OpenQueueEntry_t*    pkt;
   uint8_t              asnArray[5];
   uint32_t             values[2];
   
   // don't run if not synch
   if (ieee154e_isSynch() == FALSE) return;
   
   // don't run on dagroot
   if (idmanager_getIsDAGroot()) {
      opentimers_destroy(llatency_vars.timerId);
      return;
   }
   
   // if you get here, send a packet
   debugpins_exp_set();
   llatency_get_values(values);
   debugpins_exp_clr();
   // get a free packet buffer
   pkt = openqueue_getFreePacketBuffer(COMPONENT_LLATENCY);
   if (pkt==NULL) {
      openserial_printError(
         COMPONENT_LLATENCY,
         ERR_NO_FREE_PACKET_BUFFER,
         (errorparameter_t)0,
         (errorparameter_t)0
      );
      return;
   }
   
   pkt->owner                         = COMPONENT_LLATENCY;
   pkt->creator                       = COMPONENT_LLATENCY;
   pkt->l4_protocol                   = IANA_UDP;
   pkt->l4_destination_port           = WKP_UDP_INJECT;
   pkt->l4_sourcePortORicmpv6Type     = WKP_UDP_INJECT;
   pkt->l3_destinationAdd.type        = ADDR_128B;
   memcpy(&pkt->l3_destinationAdd.addr_128b[0],llatency_dst_addr,16);
   
   // add payload
   packetfunctions_reserveHeaderSize(pkt,sizeof(llatency_payload)-1);
   memcpy(&pkt->payload[0],llatency_payload,sizeof(llatency_payload)-1);
   
   packetfunctions_reserveHeaderSize(pkt,sizeof(uint16_t));
   pkt->payload[1] = (uint8_t)((llatency_vars.counter & 0xff00)>>8);
   pkt->payload[0] = (uint8_t)(llatency_vars.counter & 0x00ff);
   llatency_vars.counter++;
   
   packetfunctions_reserveHeaderSize(pkt,sizeof(asn_t));

   ieee154e_getAsn(asnArray);
   pkt->payload[0] = asnArray[0];
   pkt->payload[1] = asnArray[1];
   pkt->payload[2] = asnArray[2];
   pkt->payload[3] = asnArray[3];
   pkt->payload[4] = asnArray[4];


   packetfunctions_reserveHeaderSize(pkt,sizeof(uint32_t));
   pkt->payload[1] = (uint8_t)((values[0] & 0xff000000)>>24);
   pkt->payload[0] = (uint8_t)((values[0] & 0x00ff0000)>>16);
   pkt->payload[3] = (uint8_t)((values[0] & 0x0000ff00)>>8);
   pkt->payload[2] = (uint8_t)(values[0] & 0x000000ff);


   packetfunctions_reserveHeaderSize(pkt,sizeof(uint32_t));
   pkt->payload[1] = (uint8_t)((values[1] & 0xff000000)>>24);
   pkt->payload[0] = (uint8_t)((values[1] & 0x00ff0000)>>16);
   pkt->payload[3] = (uint8_t)((values[1] & 0x0000ff00)>>8);
   pkt->payload[2] = (uint8_t)(values[1] & 0x000000ff);

   
   if ((openudp_send(pkt))==E_FAIL) {
      openqueue_freePacketBuffer(pkt);
   }
}



