/*
This file is part of CanFestival, a library implementing CanOpen Stack. 

Copyright (C): Edouard TISSERANT and Francis DUPIN

See COPYING file for copyrights details.

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#ifndef __canfestival_datatypes__
#define __canfestival_datatypes__

#include <stdint.h>
#include <stddef.h>   /* NULL (used throughout, incl. CANOPEN_NODE_DATA_INITIALIZER) */

/* Generated stack sizing (SDO_MAX_LENGTH_TRANSFER, SDO_MAX_SIMULTANEOUS_TRANSFERS,
 * NMT_MAX_NODE_ID, EMCY_MAX_ERRORS, the REPEAT_*_TIMES macros, ...). Needed by the
 * API sub-headers and by the CO_Data struct / initializer below. */
#include "canfestival_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Primitive integer/real types and the CO_Data forward declaration. These, plus
 * the object-dictionary types defined further below (Message, subindex,
 * indextable, ODCallback_t, ...), are the foundation the CanFestival API
 * sub-headers depend on; those sub-headers are therefore included only once all
 * of it is in scope, just before the CO_Data structure definition. */

/* Integers */
#define INTEGER8  int8_t
#define INTEGER16 int16_t
#define INTEGER24 int32_t
#define INTEGER32 int32_t
#define INTEGER40 int64_t
#define INTEGER48 int64_t
#define INTEGER56 int64_t
#define INTEGER64 int64_t

/* Unsigned integers */
#define UNS8   uint8_t
#define UNS16  uint16_t
#define UNS24  uint32_t
#define UNS32  uint32_t
#define UNS40  uint64_t
#define UNS48  uint64_t
#define UNS56  uint64_t
#define UNS64  uint64_t

/* Reals */
#define REAL32 float
#define REAL64 double

typedef void* CAN_HANDLE;
typedef void* CAN_PORT;

typedef struct struct_CO_Data CO_Data;


/*
 * def.h 
 */

/** definitions used for object dictionary access. ie SDO Abort codes . (See DS 301 v.4.02 p.48)
 */
#define OD_SUCCESSFUL 	             0x00000000
#define OD_READ_NOT_ALLOWED          0x06010001
#define OD_WRITE_NOT_ALLOWED         0x06010002
#define OD_NO_SUCH_OBJECT            0x06020000
#define OD_NOT_MAPPABLE              0x06040041
#define OD_LENGTH_DATA_INVALID       0x06070010
#define OD_NO_SUCH_SUBINDEX 	     0x06090011
#define OD_VALUE_RANGE_EXCEEDED      0x06090030 /* Value range test result */
#define OD_VALUE_TOO_LOW             0x06090031 /* Value range test result */
#define OD_VALUE_TOO_HIGH            0x06090032 /* Value range test result */
/* Others SDO abort codes 
 */
#define SDOABT_TOGGLE_NOT_ALTERNED   0x05030000
#define SDOABT_TIMED_OUT             0x05040000
#define SDOABT_OUT_OF_MEMORY         0x05040005 /* Size data exceed SDO_MAX_LENGTH_TRANSFER */
#define SDOABT_GENERAL_ERROR         0x08000000 /* Error size of SDO message */
#define SDOABT_LOCAL_CTRL_ERROR      0x08000021 

/******************** CONSTANTS ****************/

/** Constantes which permit to define if a PDO frame
   is a request one or a data one
*/
/* Should not be modified */
#define REQUEST 1
#define NOT_A_REQUEST 0

/* Misc constants */
/* -------------- */
/* Should not be modified */
#define Rx 0
#define Tx 1
#ifndef TRUE
#define TRUE  1
#endif
#ifndef FALSE
#define FALSE 0
#endif
    
/** Status of the SDO transmission
 */
#define SDO_RESET                0x0      /* Transmission not started. Init state. */
#define SDO_FINISHED             0x1      /* data are available */                          
#define	SDO_ABORTED_RCV          0x80     /* Received an abort message. Data not available */
#define	SDO_ABORTED_INTERNAL     0x85     /* Aborted but not because of an abort message. */
#define	SDO_DOWNLOAD_IN_PROGRESS 0x2 
#define	SDO_UPLOAD_IN_PROGRESS   0x3   
#define	SDO_BLOCK_DOWNLOAD_IN_PROGRESS 0x4 
#define	SDO_BLOCK_UPLOAD_IN_PROGRESS   0x5

/** getReadResultNetworkDict may return any of above status value or this one
 */
#define SDO_PROVIDED_BUFFER_TOO_SMALL   0x8A

/* Status of the node during the SDO transfer : */
#define SDO_SERVER  0x1
#define SDO_CLIENT  0x2
#define SDO_UNKNOWN 0x3             

/* SDOrx ccs: client command specifier */
#define DOWNLOAD_SEGMENT_REQUEST     0
#define INITIATE_DOWNLOAD_REQUEST    1
#define INITIATE_UPLOAD_REQUEST      2
#define UPLOAD_SEGMENT_REQUEST       3
#define ABORT_TRANSFER_REQUEST       4
#define BLOCK_UPLOAD_REQUEST         5
#define BLOCK_DOWNLOAD_REQUEST       6

/* SDOtx scs: server command specifier */
#define UPLOAD_SEGMENT_RESPONSE      0
#define DOWNLOAD_SEGMENT_RESPONSE    1
#define INITIATE_DOWNLOAD_RESPONSE   3
#define INITIATE_UPLOAD_RESPONSE     2
#define ABORT_TRANSFER_REQUEST       4
#define BLOCK_DOWNLOAD_RESPONSE    	 5
#define BLOCK_UPLOAD_RESPONSE        6

/* SDO block upload client subcommand */
#define SDO_BCS_INITIATE_UPLOAD_REQUEST 0
#define SDO_BCS_END_UPLOAD_REQUEST      1
#define SDO_BCS_UPLOAD_RESPONSE         2
#define SDO_BCS_START_UPLOAD            3

/* SDO block upload server subcommand */
#define SDO_BSS_INITIATE_UPLOAD_RESPONSE 0
#define SDO_BSS_END_UPLOAD_RESPONSE      1

/* SDO block download client subcommand */
#define SDO_BCS_INITIATE_DOWNLOAD_REQUEST 0
#define SDO_BCS_END_DOWNLOAD_REQUEST      1

/* SDO block download server subcommand */
#define SDO_BSS_INITIATE_DOWNLOAD_RESPONSE 0
#define SDO_BSS_END_DOWNLOAD_RESPONSE      1
#define SDO_BSS_DOWNLOAD_RESPONSE          2

/*  Function Codes 
   ---------------
  defined in the canopen DS301 
*/
#define NMT	   0x0
#define SYNC       0x1
#define TIME_STAMP 0x2
#define PDO1tx     0x3
#define PDO1rx     0x4
#define PDO2tx     0x5
#define PDO2rx     0x6
#define PDO3tx     0x7
#define PDO3rx     0x8
#define PDO4tx     0x9
#define PDO4rx     0xA
#define SDOtx      0xB
#define SDOrx      0xC
#define NODE_GUARD 0xE
#define LSS 	   0xF

/* NMT Command Specifier, sent by master to change a slave state */
/* ------------------------------------------------------------- */
/* Should not be modified */
#define NMT_Start_Node              0x01
#define NMT_Stop_Node               0x02
#define NMT_Enter_PreOperational    0x80
#define NMT_Reset_Node              0x81
#define NMT_Reset_Comunication      0x82

/** Status of the LSS transmission
 */
#define LSS_RESET                0x0      /* Transmission not started. Init state. */
#define LSS_FINISHED             0x1      /* data are available */                          
#define	LSS_ABORTED_INTERNAL     0x2     /* Aborted but not because of an abort message. */
#define	LSS_TRANS_IN_PROGRESS 	 0x3    

/* constantes used in the different state machines */
/* ----------------------------------------------- */
/* Must not be modified */
#define state1  0x01
#define state2  0x02
#define state3  0x03
#define state4  0x04
#define state5  0x05
#define state6  0x06
#define state7  0x07
#define state8  0x08
#define state9  0x09
#define state10 0x0A
#define state11 0x0B

/*
 * can.h
 */

/** 
 * @brief The CAN message structure 
 * @ingroup can
 */
typedef struct {
  UNS16 cob_id;	/**< message's ID */
  UNS8 rtr;		/**< remote transmission request. (0 if not rtr message, 1 if rtr message) */
  UNS8 len;		/**< message's length (0 to 8) */
  UNS8 data[8]; /**< message's datas */
} Message;

#define Message_Initializer {0,0,0,{0,0,0,0,0,0,0,0}}

typedef UNS8 (*canSend_t)(Message *);

/*
 * data.h
 */

/************************* CONSTANTES **********************************/
/** this are static defined datatypes taken fCODE the canopen standard. They
 *  are located at index 0x0001 to 0x001B. As described in the standard, they
 *  are in the object dictionary for definition purpose only. a device does not
 *  to support all of this datatypes.
 */
#define boolean         0x01
#define int8            0x02
#define int16           0x03
#define int32           0x04
#define uint8           0x05
#define uint16          0x06
#define uint32          0x07
#define real32          0x08
#define visible_string  0x09
#define octet_string    0x0A
#define unicode_string  0x0B
#define time_of_day     0x0C
#define time_difference 0x0D

#define domain          0x0F
#define int24           0x10
#define real64          0x11
#define int40           0x12
#define int48           0x13
#define int56           0x14
#define int64           0x15
#define uint24          0x16

#define uint40          0x18
#define uint48          0x19
#define uint56          0x1A
#define uint64          0x1B

#define pdo_communication_parameter 0x20
#define pdo_mapping                 0x21
#define sdo_parameter               0x22
#define identity                    0x23

/* CanFestival is using 0x24 to 0xFF to define some types containing a 
 value range (See how it works in objdict.c)
 */


/** Each entry of the object dictionary can be READONLY (RO), READ/WRITE (RW),
 *  WRITE-ONLY (WO)
 */
#define RW     0x00  
#define WO     0x01
#define RO     0x02

#define TO_BE_SAVE  0x04
#define DCF_TO_SEND 0x08

/************************ STRUCTURES ****************************/
/** This are some structs which are neccessary for creating the entries
 *  of the object dictionary.
 */
typedef struct td_subindex
{
    UNS8                    bAccessType;
    UNS8                    bDataType; /* Defines of what datatype the entry is */
    UNS32                   size;      /* The size (in Byte) of the variable */
    void*                   pObject;   /* This is the pointer of the Variable */
} subindex;

/** Struct for creating entries in the communictaion profile
 */
typedef struct td_indextable
{
    subindex*   pSubindex;   /* Pointer to the subindex */
    UNS8   bSubCount;   /* the count of valid entries for this subindex
                         * This count here defines how many memory has been
                         * allocated. this memory does not have to be used.
                         */
    UNS16   index;
} indextable;

typedef struct s_quick_index{
	UNS16 SDO_SVR;
	UNS16 SDO_CLT;
	UNS16 PDO_RCV;
	UNS16 PDO_RCV_MAP;
	UNS16 PDO_TRS;
	UNS16 PDO_TRS_MAP;
}quick_index;


/*typedef struct struct_CO_Data CO_Data; */
typedef UNS32 (*ODCallback_t)(CO_Data* d, const indextable *, UNS8 bSubindex);
typedef const indextable * (*scanIndexOD_t)(UNS16 wIndex, UNS32 * errorCode, ODCallback_t **Callback);

/************************** MACROS *********************************/

/* CANopen usefull helpers */
#define GET_NODE_ID(m)         (UNS16_LE(m.cob_id) & 0x7f)
#define GET_FUNCTION_CODE(m)   (UNS16_LE(m.cob_id) >> 7)


/**
 * @brief The CAN board configuration
 * @ingroup can
 */
struct struct_s_BOARD;

typedef struct struct_s_BOARD s_BOARD;

struct struct_s_BOARD {
  char * busname;  /**< The bus name on which the CAN board is connected */
  char * baudrate; /**< The board baudrate */
};

/* CanFestival API sub-headers. Included here, after the foundational types
 * above, because their typedefs use the UNS and INTEGER integer types, CO_Data,
 * indextable and ODCallback_t. They in turn supply the member types of CO_Data
 * defined just below (e_nodeState, s_transfer, s_PDO_status, the *_t callback
 * types, s_errors, ...). nmtSlave.h / nmtMaster.h are intentionally NOT here:
 * they only declare NMT functions and are pulled in via canfestival.h. */
#include "applicfg.h"
#include "objacces.h"
#include "sdo.h"
#include "pdo.h"
#include "states.h"
#include "lifegrd.h"
#include "sync.h"
#include "emcy.h"
#ifdef CO_ENABLE_LSS
#include "lss.h"
#endif

/**
 * @ingroup od
 * @brief This structure contains all necessary informations to define a CANOpen node
 */

struct struct_CO_Data {
	/* Object dictionary */
	UNS8 *bDeviceNodeId;
	const indextable *objdict;
	s_PDO_status *PDO_status;
	TIMER_HANDLE *RxPDO_EventTimers;
	void (*RxPDO_EventTimers_Handler)(CO_Data*, UNS32);
	const quick_index *firstIndex;
	const quick_index *lastIndex;
	const UNS16 *ObjdictSize;
	const UNS8 *iam_a_slave;
	valueRangeTest_t valueRangeTest;
	
	/* SDO */
	s_transfer transfers[SDO_MAX_SIMULTANEOUS_TRANSFERS];
	/* s_sdo_parameter *sdo_parameters; */

	/* State machine */
	e_nodeState nodeState;
	s_state_communication CurrentCommunicationState;
	initialisation_t initialisation;
	preOperational_t preOperational;
	operational_t operational;
	stopped_t stopped;
     void (*NMT_Slave_Node_Reset_Callback)(CO_Data*);
     void (*NMT_Slave_Communications_Reset_Callback)(CO_Data*);
     
	/* NMT-heartbeat */
	UNS8 *ConsumerHeartbeatCount;
	UNS32 *ConsumerHeartbeatEntries;
	TIMER_HANDLE *ConsumerHeartBeatTimers;
	UNS16 *ProducerHeartBeatTime;
	TIMER_HANDLE ProducerHeartBeatTimer;
	heartbeatError_t heartbeatError;
	e_nodeState NMTable[NMT_MAX_NODE_ID]; 

	/* NMT-nodeguarding */
	TIMER_HANDLE GuardTimeTimer;
	TIMER_HANDLE LifeTimeTimer;
	nodeguardError_t nodeguardError;
	UNS16 *GuardTime;
	UNS8 *LifeTimeFactor;
	UNS8 nodeGuardStatus[NMT_MAX_NODE_ID];

	/* SYNC */
	TIMER_HANDLE syncTimer;
	UNS32 *COB_ID_Sync;
	UNS32 *Sync_Cycle_Period;
	/*UNS32 *Sync_window_length;;*/
	post_sync_t post_sync;
	post_TPDO_t post_TPDO;
	post_SlaveBootup_t post_SlaveBootup;
    post_SlaveStateChange_t post_SlaveStateChange;
	
	/* General */
	UNS8 toggle;
	CAN_PORT canHandle;	
	scanIndexOD_t scanIndexOD;
	storeODSubIndex_t storeODSubIndex; 
	
	/* DCF concise */
    const indextable* dcf_odentry;
	UNS8* dcf_cursor;
	UNS32 dcf_entries_count;
	UNS8 dcf_status;
    UNS32 dcf_size;
    UNS8* dcf_data;
	
	/* EMCY */
	e_errorState error_state;
	UNS8 error_history_size;
	UNS8* error_number;
	UNS32* error_first_element;
	UNS8* error_register;
    UNS32* error_cobid;
	s_errors error_data[EMCY_MAX_ERRORS];
	post_emcy_t post_emcy;
	
#ifdef CO_ENABLE_LSS
	/* LSS */
	lss_transfer_t lss_transfer;
	lss_StoreConfiguration_t lss_StoreConfiguration;
#endif	
};

#define NMTable_Initializer Unknown_state,
#define nodeGuardStatus_Initializer 0x00,

#ifdef SDO_DYNAMIC_BUFFER_ALLOCATION
#define s_transfer_Initializer {\
		0,          /* CliServ{REPEAT_NMT_MAX_NODE_ID_TIMES(NMTable_Initializer)},Nbr */\
		0,          /* wohami */\
		SDO_RESET,  /* state */\
		0,          /* toggle */\
		0,          /* abortCode */\
		0,          /* index */\
		0,          /* subIndex */\
		0,          /* count */\
		0,          /* offset */\
		{0},        /* data (static use, so that all the table is initialize at 0)*/\
    NULL,       /* dynamicData */ \
    0,          /* dynamicDataSize */ \
		0,          /* peerCRCsupport */\
		0,          /* blksize */\
		0,          /* ackseq */\
		0,          /* objsize */\
		0,          /* lastblockoffset */\
		0,          /* seqno */\
		0,          /* endfield */\
		RXSTEP_INIT,/* rxstep */\
		{0},        /* tmpData */\
		0,          /* dataType */\
		-1,         /* timer */\
		NULL        /* Callback */\
	  },
#else
#define s_transfer_Initializer {\
		0,          /* nodeId */\
		0,          /* wohami */\
		SDO_RESET,  /* state */\
		0,          /* toggle */\
		0,          /* abortCode */\
		0,          /* index */\
		0,          /* subIndex */\
		0,          /* count */\
		0,          /* offset */\
		{0},        /* data (static use, so that all the table is initialize at 0)*/\
		0,          /* peerCRCsupport */\
		0,          /* blksize */\
		0,          /* ackseq */\
		0,          /* objsize */\
		0,          /* lastblockoffset */\
		0,          /* seqno */\
		0,          /* endfield */\
		RXSTEP_INIT,/* rxstep */\
		{0},        /* tmpData */\
		0,          /*  */\
		-1,         /*  */\
		NULL        /*  */\
	  },
#endif //SDO_DYNAMIC_BUFFER_ALLOCATION

#define ERROR_DATA_INITIALIZER \
	{\
	0, /* errCode */\
	0, /* errRegMask */\
	0 /* active */\
	},
	
#ifdef CO_ENABLE_LSS

#ifdef CO_ENABLE_LSS_FS	
#define lss_fs_Initializer \
		,0,						/* IDNumber */\
  		128, 					/* BitChecked */\
  		0,						/* LSSSub */\
  		0, 						/* LSSNext */\
  		0, 						/* LSSPos */\
  		LSS_FS_RESET,			/* FastScan_SM */\
  		-1,						/* timerFS */\
  		{{0,0,0,0},{0,0,0,0}}   /* lss_fs_transfer */
#else
#define lss_fs_Initializer
#endif		

#define lss_Initializer {\
		LSS_RESET,  			/* state */\
		0,						/* command */\
		LSS_WAITING_MODE, 		/* mode */\
		0,						/* dat1 */\
		0,						/* dat2 */\
		0,          			/* NodeID */\
		0,          			/* addr_sel_match */\
		0,          			/* addr_ident_match */\
		"none",         		/* BaudRate */\
		0,          			/* SwitchDelay */\
		SDELAY_OFF,   			/* SwitchDelayState */\
		NULL,					/* canHandle_t */\
		-1,						/* TimerMSG */\
		-1,          			/* TimerSDELAY */\
		NULL,        			/* Callback */\
		0						/* LSSanswer */\
		lss_fs_Initializer		/*FastScan service initialization */\
	  },\
	  NULL 	/* _lss_StoreConfiguration*/
#else
#define lss_Initializer
#endif


/* A macro to initialize the data in client app.*/
/* CO_Data structure */
#define CANOPEN_NODE_DATA_INITIALIZER(NODE_PREFIX) {\
	/* Object dictionary*/\
	& NODE_PREFIX ## _bDeviceNodeId,     /* bDeviceNodeId */\
	NODE_PREFIX ## _objdict,             /* objdict  */\
	NODE_PREFIX ## _PDO_status,          /* PDO_status */\
	NULL,                                /* RxPDO_EventTimers */\
	NULL,                                /* RxPDO_EventTimers_Handler (NULL -> _RxPDO_EventTimers_Handler at call site) */\
	& NODE_PREFIX ## _firstIndex,        /* firstIndex */\
	& NODE_PREFIX ## _lastIndex,         /* lastIndex */\
	& NODE_PREFIX ## _ObjdictSize,       /* ObjdictSize */\
	& NODE_PREFIX ## _iam_a_slave,       /* iam_a_slave */\
	NODE_PREFIX ## _valueRangeTest,      /* valueRangeTest */\
	\
	/* SDO, structure s_transfer */\
	{\
          REPEAT_SDO_MAX_SIMULTANEOUS_TRANSFERS_TIMES(s_transfer_Initializer)\
	},\
	\
	/* State machine*/\
	Unknown_state,      /* nodeState */\
	/* structure s_state_communication */\
	{\
		0,          /* csBoot_Up */\
		0,          /* csSDO */\
		0,          /* csEmergency */\
		0,          /* csSYNC */\
		0,          /* csHeartbeat */\
		0,           /* csPDO */\
		0           /* csLSS */\
	},\
	NULL,                /* initialisation (NULL -> _initialisation at call site) */\
	NULL,                /* preOperational (NULL -> _preOperational at call site) */\
	NULL,                /* operational    (NULL -> _operational at call site)    */\
	NULL,                /* stopped        (NULL -> _stopped at call site)        */\
	NULL,                /* NMT node reset callback */\
	NULL,                /* NMT communications reset callback */\
	\
	/* NMT-heartbeat */\
	& NODE_PREFIX ## _highestSubIndex_obj1016, /* ConsumerHeartbeatCount */\
	NODE_PREFIX ## _obj1016,                   /* ConsumerHeartbeatEntries */\
	NODE_PREFIX ## _heartBeatTimers,           /* ConsumerHeartBeatTimers  */\
	& NODE_PREFIX ## _obj1017,                 /* ProducerHeartBeatTime */\
	TIMER_NONE,                                /* ProducerHeartBeatTimer */\
	NULL,                      /* heartbeatError (NULL -> _heartbeatError at call site) */\
	\
	{REPEAT_NMT_MAX_NODE_ID_TIMES(NMTable_Initializer)},\
                                                   /* is  well initialized at "Unknown_state". Is it ok ? (FD)*/\
	\
	/* NMT-nodeguarding */\
	TIMER_NONE,                                /* GuardTimeTimer */\
	TIMER_NONE,                                /* LifeTimeTimer */\
	NULL,                      /* nodeguardError (NULL -> _nodeguardError at call site) */\
	& NODE_PREFIX ## _obj100C,                 /* GuardTime */\
	& NODE_PREFIX ## _obj100D,                 /* LifeTimeFactor */\
	{REPEAT_NMT_MAX_NODE_ID_TIMES(nodeGuardStatus_Initializer)},\
	\
	/* SYNC */\
	TIMER_NONE,                                /* syncTimer */\
	& NODE_PREFIX ## _obj1005,                 /* COB_ID_Sync */\
	& NODE_PREFIX ## _obj1006,                 /* Sync_Cycle_Period */\
	/*& NODE_PREFIX ## _obj1007, */            /* Sync_window_length */\
	NULL,                       /* post_sync (NULL -> _post_sync at call site) */\
	NULL,                       /* post_TPDO (NULL -> _post_TPDO at call site) */\
	NULL,			/* post_SlaveBootup (NULL -> _post_SlaveBootup at call site) */\
	NULL,			/* post_SlaveStateChange (NULL -> _post_SlaveStateChange at call site) */\
	\
	/* General */\
	0,                                         /* toggle */\
	NULL,                   /* canSend */\
	NODE_PREFIX ## _scanIndexOD,                /* scanIndexOD */\
	NULL,                            /* storeODSubIndex (NULL -> _storeODSubIndex at call site) */\
    /* DCF concise */\
    NULL,       /*dcf_odentry*/\
	NULL,		/*dcf_cursor*/\
	1,		/*dcf_entries_count*/\
	0,		/* dcf_status*/\
    0,      /* dcf_size */\
    NULL,   /* dcf_data */\
	\
	/* EMCY */\
	Error_free,                      /* error_state */\
	sizeof(NODE_PREFIX ## _obj1003) / sizeof(NODE_PREFIX ## _obj1003[0]),      /* error_history_size */\
	& NODE_PREFIX ## _highestSubIndex_obj1003,    /* error_number */\
	& NODE_PREFIX ## _obj1003[0],    /* error_first_element */\
	& NODE_PREFIX ## _obj1001,       /* error_register */\
    & NODE_PREFIX ## _obj1014,       /* error_cobid */\
	/* error_data: structure s_errors */\
	{\
	REPEAT_EMCY_MAX_ERRORS_TIMES(ERROR_DATA_INITIALIZER)\
	},\
	NULL,                    /* post_emcy (NULL -> _post_emcy at call site) */\
	/* LSS */\
	lss_Initializer\
}

#ifdef __cplusplus
};
#endif

#endif /* __canfestival_datatypes__ */


