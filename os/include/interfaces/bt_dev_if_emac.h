#ifndef _BT_DEV_IF_EMAC_H_
#define _BT_DEV_IF_EMAC_H_

typedef enum _BT_NET_IF_EVENT {
	BT_NET_IF_RX_READY,
	BT_NET_IF_ADD_IF,
	BT_NET_IF_REMOVE_IF,
} BT_NET_IF_EVENT;

typedef enum _BT_MAC_EVENT {
	BT_MAC_RECEIVED,
	BT_MAC_TRANSMITTED,
} BT_MAC_EVENT;

typedef struct _BT_NET_IF BT_NET_IF;

typedef void (*BT_NET_IF_EVENTRECEIVER)(BT_NET_IF *pIF, BT_NET_IF_EVENT eEvent, BT_BOOL bInterruptContext);

typedef struct _BT_DEV_IF_EMAC {
	BT_u32 		ulCapabilities;			///< Primary Capability flags.
#define	BT_NET_IF_CAPABILITIES_ETHERNET				0x00000001
#define	BT_NET_IF_CAPABILITIES_100MBPS				0x00000002
#define	BT_NET_IF_CAPABILITIES_1000MBPS				0x00000004
#define	BT_NET_IF_CAPABILITIES_MDIX					0x00000008

	BT_ERROR 	(*pfnEventSubscribe)	(BT_HANDLE hIF, BT_NET_IF_EVENTRECEIVER pfnReceiver, BT_NET_IF *pIF);
	BT_ERROR	(*pfnInitialise)		(BT_HANDLE hIF);

	BT_ERROR	(*pfnGetMACAddr)		(BT_HANDLE hIF, BT_u8 *pAddr, BT_u32 ulLength);
	BT_ERROR	(*pfnSetMACAddr)		(BT_HANDLE hIF, const BT_u8 *pAddr, BT_u32 ulLength);
	BT_u32 		(*pfnGetMTU)			(BT_HANDLE hIF, BT_ERROR *pError);

	BT_u32		(*pfnDataReady)			(BT_HANDLE hIF, BT_ERROR *pError);
	BT_ERROR	(*pfnRead)				(BT_HANDLE hIF, BT_u32 ulSize, BT_u32 pos, void *pBuffer);
	BT_ERROR	(*pfnDropFrame)			(BT_HANDLE hIF, BT_u32 ulSize);
	BT_BOOL		(*pfnTxFifoReady)		(BT_HANDLE hIF, BT_ERROR *pError);
	BT_ERROR	(*pfnWrite)				(BT_HANDLE hIF, BT_u32 ulSize, void *pBuffer);
	BT_ERROR	(*pfnSendFrame)			(BT_HANDLE hIF);
	BT_ERROR	(*pfnSendEvent)			(BT_HANDLE hIF, BT_u32 ulEvent);

	void 		(*adjust_link)			(BT_HANDLE hIF, struct bt_phy_device *phy);
	void 		(*adjust_state)			(BT_HANDLE hIF, struct bt_phy_device *phy);

} BT_DEV_IF_EMAC;

typedef struct _BT_DEV_IF_MII {
	BT_u16 		(*pfnRead)	(BT_HANDLE hMII, BT_u32 phy_id, BT_u32 regnum, BT_ERROR *pError);
	BT_ERROR 	(*pfnWrite)	(BT_HANDLE hMII, BT_u32 phy_id, BT_u32 regnum, BT_u16 val);
	BT_ERROR	(*pfnReset)	(BT_HANDLE hMII);
} BT_DEV_IF_MII;

typedef struct BT_DEV_IF_PHY {
	BT_ERROR 	(*pfnConfigInit)		(struct bt_phy_device *phy);
	BT_ERROR 	(*pfnConfigAneg)		(struct bt_phy_device *phy);
	BT_ERROR	(*pfnReadStatus)		(struct bt_phy_device *phy);
//	BT_ERROR	(*pfnAckInterrupt)		(struct bt_phy_device *phy);
//	BT_ERROR	(*pfnConfigInterrupt)	(struct bt_phy_device *phy);
//	BT_ERROR 	(*pfnDidInterrupt)		(struct bt_phy_device *phy);
} BT_DEV_IF_PHY;

#endif
