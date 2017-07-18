#include "opendefs.h"
#include "openbridge.h"
#include "openserial.h"
#include "packetfunctions.h"
#include "iphc.h"
#include "idmanager.h"
#include "openqueue.h"
#include "debugpins.h"
#include "opentimers.h"
#include "IEEE802154E.h"
#include "llatency.h"

//=========================== variables =======================================

//=========================== prototypes ======================================
//=========================== public ==========================================

void openbridge_init() {
  debugpins_init();
}

void openbridge_triggerData() {
   uint8_t           input_buffer[136];//worst case: 8B of next hop + 128B of data
   OpenQueueEntry_t* pkt;
   uint8_t           numDataBytes;
  
   numDataBytes = openserial_getInputBufferFilllevel();
  
   //poipoi xv
   //this is a temporal workaround as we are never supposed to get chunks of data
   //longer than input buffer size.. I assume that HDLC will solve that.
   // MAC header is 13B + 8 next hop so we cannot accept packets that are longer than 118B
   if (numDataBytes>(136 - 10/*21*/) || numDataBytes<8){
   //to prevent too short or too long serial frames to kill the stack  
       openserial_printError(COMPONENT_OPENBRIDGE,ERR_INPUTBUFFER_LENGTH,
                   (errorparameter_t)numDataBytes,
                   (errorparameter_t)0);
       return;
   }
  
   //copying the buffer once we know it is not too big
   openserial_getInputBuffer(&(input_buffer[0]),numDataBytes);
  
   if (idmanager_getIsDAGroot()==TRUE && numDataBytes>0) {
      pkt = openqueue_getFreePacketBuffer(COMPONENT_OPENBRIDGE);
      if (pkt==NULL) {
         openserial_printError(COMPONENT_OPENBRIDGE,ERR_NO_FREE_PACKET_BUFFER,
                               (errorparameter_t)0,
                               (errorparameter_t)0);
         return;
      }
      //admin
      pkt->creator  = COMPONENT_OPENBRIDGE;
      pkt->owner    = COMPONENT_OPENBRIDGE;
      //debugpins_exp_toggle();
      //l2
      pkt->l2_nextORpreviousHop.type = ADDR_64B;
      memcpy(&(pkt->l2_nextORpreviousHop.addr_64b[0]),&(input_buffer[0]),8);
      //payload
      packetfunctions_reserveHeaderSize(pkt,numDataBytes-8);
      memcpy(pkt->payload,&(input_buffer[8]),numDataBytes-8);
      
      //this is to catch the too short packet. remove it after fw-103 is solved.
      if (numDataBytes<16){
              openserial_printError(COMPONENT_OPENBRIDGE,ERR_INVALIDSERIALFRAME,
                            (errorparameter_t)0,
                            (errorparameter_t)0);
      }        
      //send
      if ((iphc_sendFromBridge(pkt))==E_FAIL) {
         openqueue_freePacketBuffer(pkt);
      }
   }
}

void openbridge_sendDone(OpenQueueEntry_t* msg, owerror_t error) {
   //debugpins_exp_toggle();
   msg->owner = COMPONENT_OPENBRIDGE;
   if (msg->creator!=COMPONENT_OPENBRIDGE) {
      openserial_printError(COMPONENT_OPENBRIDGE,ERR_UNEXPECTED_SENDDONE,
                            (errorparameter_t)0,
                            (errorparameter_t)0);
   }
   openqueue_freePacketBuffer(msg);
}

/**
\brief Receive a frame at the openbridge, which sends it out over serial.
*/
void openbridge_receive(OpenQueueEntry_t* msg) {
   
   // prepend previous hop
   packetfunctions_reserveHeaderSize(msg,LENGTH_ADDR64b);
   memcpy(msg->payload,msg->l2_nextORpreviousHop.addr_64b,LENGTH_ADDR64b);
   
   // prepend next hop (me)
   packetfunctions_reserveHeaderSize(msg,LENGTH_ADDR64b);
   memcpy(msg->payload,idmanager_getMyID(ADDR_64B)->addr_64b,LENGTH_ADDR64b);

   uint32_t values[2];
   debugpins_exp_set();
   //llatency_get_values(values);
   values[0] = opentimers_getValue();
   values[0] = ieee154e_getStartOfSlotReference();
   debugpins_exp_clr();

   packetfunctions_reserveHeaderSize(msg,sizeof(uint32_t));
   msg->payload[1] = (uint8_t)((values[0] & 0xff000000)>>24);
   msg->payload[0] = (uint8_t)((values[0] & 0x00ff0000)>>16);
   msg->payload[3] = (uint8_t)((values[0] & 0x0000ff00)>>8);
   msg->payload[2] = (uint8_t)(values[0] & 0x000000ff);


   packetfunctions_reserveHeaderSize(msg,sizeof(uint32_t));
   msg->payload[1] = (uint8_t)((values[1] & 0xff000000)>>24);
   msg->payload[0] = (uint8_t)((values[1] & 0x00ff0000)>>16);
   msg->payload[3] = (uint8_t)((values[1] & 0x0000ff00)>>8);
   msg->payload[2] = (uint8_t)(values[1] & 0x000000ff);

   // send packet over serial (will be memcopied into serial buffer)
   openserial_printData((uint8_t*)(msg->payload),msg->length);
   
   // free packet
   openqueue_freePacketBuffer(msg);
}

//=========================== private =========================================
