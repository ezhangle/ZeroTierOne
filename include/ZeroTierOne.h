/*
 * ZeroTier One - Network Virtualization Everywhere
 * Copyright (C) 2011-2015  ZeroTier, Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * --
 *
 * ZeroTier may be used and distributed under the terms of the GPLv3, which
 * are available at: http://www.gnu.org/licenses/gpl-3.0.html
 *
 * If you would like to embed ZeroTier into a commercial application or
 * redistribute it in a modified binary form, please contact ZeroTier Networks
 * LLC. Start here: http://www.zerotier.com/
 */

/*
 * This defines the external C API for ZeroTier's core network virtualization
 * engine.
 */

#ifndef ZT_ZEROTIERONE_H
#define ZT_ZEROTIERONE_H

#include <stdint.h>

// For the struct sockaddr_storage structure
#if defined(_WIN32) || defined(_WIN64)
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <Windows.h>
#else /* not Windows */
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#endif /* Windows or not */

#ifdef __cplusplus
extern "C" {
#endif

/****************************************************************************/
/* Core constants                                                           */
/****************************************************************************/

/**
 * Default UDP port for devices running a ZeroTier endpoint
 */
#define ZT_DEFAULT_PORT 9993

/**
 * Maximum MTU for ZeroTier virtual networks
 *
 * This is pretty much an unchangeable global constant. To make it change
 * across nodes would require logic to send ICMP packet too big messages,
 * which would complicate things. 1500 has been good enough on most LANs
 * for ages, so a larger MTU should be fine for the forseeable future. This
 * typically results in two UDP packets per single large frame. Experimental
 * results seem to show that this is good. Larger MTUs resulting in more
 * fragments seemed too brittle on slow/crummy links for no benefit.
 *
 * If this does change, also change it in tap.h in the tuntaposx code under
 * mac-tap.
 *
 * Overhead for a normal frame split into two packets:
 *
 * 1414 = 1444 (typical UDP MTU) - 28 (packet header) - 2 (ethertype)
 * 1428 = 1444 (typical UDP MTU) - 16 (fragment header)
 * SUM: 2842
 *
 * We use 2800, which leaves some room for other payload in other types of
 * messages such as multicast propagation or future support for bridging.
 */
#define ZT_MAX_MTU 2800

/**
 * Maximum length of network short name
 */
#define ZT_MAX_NETWORK_SHORT_NAME_LENGTH 255

/**
 * Maximum number of statically assigned IP addresses per network endpoint using ZT address management (not DHCP)
 */
#define ZT_MAX_ZT_ASSIGNED_ADDRESSES 16

/**
 * Maximum number of multicast group subscriptions per network
 */
#define ZT_MAX_NETWORK_MULTICAST_SUBSCRIPTIONS 4096

/**
 * Maximum number of direct network paths to a given peer
 */
#define ZT_MAX_PEER_NETWORK_PATHS 4

/**
 * Feature flag: ZeroTier One was built to be thread-safe -- concurrent processXXX() calls are okay
 */
#define ZT_FEATURE_FLAG_THREAD_SAFE 0x00000001

/**
 * Feature flag: FIPS compliant build (not available yet, but reserved for future use if we ever do this)
 */
#define ZT_FEATURE_FLAG_FIPS 0x00000002

/**
 * Maximum number of hops in a ZeroTier circuit test
 *
 * This is more or less the max that can be fit in a given packet (with
 * fragmentation) and only one address per hop.
 */
#define ZT_CIRCUIT_TEST_MAX_HOPS 512

/**
 * Maximum number of addresses per hop in a circuit test
 */
#define ZT_CIRCUIT_TEST_MAX_HOP_BREADTH 256

/**
 * Maximum number of cluster members (and max member ID plus one)
 */
#define ZT_CLUSTER_MAX_MEMBERS 128

/**
 * Maximum number of physical ZeroTier addresses a cluster member can report
 */
#define ZT_CLUSTER_MAX_ZT_PHYSICAL_ADDRESSES 16

/**
 * Maximum allowed cluster message length in bytes
 */
#define ZT_CLUSTER_MAX_MESSAGE_LENGTH (1500 - 48)

/**
 * A null/empty sockaddr (all zero) to signify an unspecified socket address
 */
extern const struct sockaddr_storage ZT_SOCKADDR_NULL;

/****************************************************************************/
/* Structures and other types                                               */
/****************************************************************************/

/**
 * Function return code: OK (0) or error results
 *
 * Use ZT_ResultCode_isFatal() to check for a fatal error. If a fatal error
 * occurs, the node should be considered to not be working correctly. These
 * indicate serious problems like an inaccessible data store or a compile
 * problem.
 */
enum ZT_ResultCode
{
	/**
	 * Operation completed normally
	 */
	ZT_RESULT_OK = 0,

	// Fatal errors (>0, <1000)

	/**
	 * Ran out of memory
	 */
	ZT_RESULT_FATAL_ERROR_OUT_OF_MEMORY = 1,

	/**
	 * Data store is not writable or has failed
	 */
	ZT_RESULT_FATAL_ERROR_DATA_STORE_FAILED = 2,

	/**
	 * Internal error (e.g. unexpected exception indicating bug or build problem)
	 */
	ZT_RESULT_FATAL_ERROR_INTERNAL = 3,

	// Non-fatal errors (>1000)

	/**
	 * Network ID not valid
	 */
	ZT_RESULT_ERROR_NETWORK_NOT_FOUND = 1000,

	/**
	 * The requested operation is not supported on this version or build
	 */
	ZT_RESULT_ERROR_UNSUPPORTED_OPERATION = 1001,

	/**
	 * The requestion operation was given a bad parameter or was called in an invalid state
	 */
	ZT_RESULT_ERROR_BAD_PARAMETER = 1002
};

/**
 * @param x Result code
 * @return True if result code indicates a fatal error
 */
#define ZT_ResultCode_isFatal(x) ((((int)(x)) > 0)&&(((int)(x)) < 1000))

/**
 * Status codes sent to status update callback when things happen
 */
enum ZT_Event
{
	/**
	 * Node has been initialized
	 *
	 * This is the first event generated, and is always sent. It may occur
	 * before Node's constructor returns.
	 *
	 * Meta-data: none
	 */
	ZT_EVENT_UP = 0,

	/**
	 * Node is offline -- network does not seem to be reachable by any available strategy
	 *
	 * Meta-data: none
	 */
	ZT_EVENT_OFFLINE = 1,

	/**
	 * Node is online -- at least one upstream node appears reachable
	 *
	 * Meta-data: none
	 */
	ZT_EVENT_ONLINE = 2,

	/**
	 * Node is shutting down
	 *
	 * This is generated within Node's destructor when it is being shut down.
	 * It's done for convenience, since cleaning up other state in the event
	 * handler may appear more idiomatic.
	 *
	 * Meta-data: none
	 */
	ZT_EVENT_DOWN = 3,

	/**
	 * Your identity has collided with another node's ZeroTier address
	 *
	 * This happens if two different public keys both hash (via the algorithm
	 * in Identity::generate()) to the same 40-bit ZeroTier address.
	 *
	 * This is something you should "never" see, where "never" is defined as
	 * once per 2^39 new node initializations / identity creations. If you do
	 * see it, you're going to see it very soon after a node is first
	 * initialized.
	 *
	 * This is reported as an event rather than a return code since it's
	 * detected asynchronously via error messages from authoritative nodes.
	 *
	 * If this occurs, you must shut down and delete the node, delete the
	 * identity.secret record/file from the data store, and restart to generate
	 * a new identity. If you don't do this, you will not be able to communicate
	 * with other nodes.
	 *
	 * We'd automate this process, but we don't think silently deleting
	 * private keys or changing our address without telling the calling code
	 * is good form. It violates the principle of least surprise.
	 *
	 * You can technically get away with not handling this, but we recommend
	 * doing so in a mature reliable application. Besides, handling this
	 * condition is a good way to make sure it never arises. It's like how
	 * umbrellas prevent rain and smoke detectors prevent fires. They do, right?
	 *
	 * Meta-data: none
	 */
	ZT_EVENT_FATAL_ERROR_IDENTITY_COLLISION = 4,

	/**
	 * Trace (debugging) message
	 *
	 * These events are only generated if this is a TRACE-enabled build.
	 *
	 * Meta-data: C string, TRACE message
	 */
	ZT_EVENT_TRACE = 5
};

/**
 * Current node status
 */
typedef struct
{
	/**
	 * 40-bit ZeroTier address of this node
	 */
	uint64_t address;

	/**
	 * Current world ID
	 */
	uint64_t worldId;

	/**
	 * Current world revision/timestamp
	 */
	uint64_t worldTimestamp;

	/**
	 * Public identity in string-serialized form (safe to send to others)
	 *
	 * This pointer will remain valid as long as the node exists.
	 */
	const char *publicIdentity;

	/**
	 * Full identity including secret key in string-serialized form
	 *
	 * This pointer will remain valid as long as the node exists.
	 */
	const char *secretIdentity;

	/**
	 * True if some kind of connectivity appears available
	 */
	int online;
} ZT_NodeStatus;

/**
 * Virtual network status codes
 */
enum ZT_VirtualNetworkStatus
{
	/**
	 * Waiting for network configuration (also means revision == 0)
	 */
	ZT_NETWORK_STATUS_REQUESTING_CONFIGURATION = 0,

	/**
	 * Configuration received and we are authorized
	 */
	ZT_NETWORK_STATUS_OK = 1,

	/**
	 * Netconf master told us 'nope'
	 */
	ZT_NETWORK_STATUS_ACCESS_DENIED = 2,

	/**
	 * Netconf master exists, but this virtual network does not
	 */
	ZT_NETWORK_STATUS_NOT_FOUND = 3,

	/**
	 * Initialization of network failed or other internal error
	 */
	ZT_NETWORK_STATUS_PORT_ERROR = 4,

	/**
	 * ZeroTier One version too old
	 */
	ZT_NETWORK_STATUS_CLIENT_TOO_OLD = 5
};

/**
 * Virtual network type codes
 */
enum ZT_VirtualNetworkType
{
	/**
	 * Private networks are authorized via certificates of membership
	 */
	ZT_NETWORK_TYPE_PRIVATE = 0,

	/**
	 * Public networks have no access control -- they'll always be AUTHORIZED
	 */
	ZT_NETWORK_TYPE_PUBLIC = 1
};

/**
 * An Ethernet multicast group
 */
typedef struct
{
	/**
	 * MAC address (least significant 48 bits)
	 */
	uint64_t mac;

	/**
	 * Additional distinguishing information (usually zero)
	 */
	unsigned long adi;
} ZT_MulticastGroup;

/**
 * Virtual network configuration update type
 */
enum ZT_VirtualNetworkConfigOperation
{
	/**
	 * Network is coming up (either for the first time or after service restart)
	 */
	ZT_VIRTUAL_NETWORK_CONFIG_OPERATION_UP = 1,

	/**
	 * Network configuration has been updated
	 */
	ZT_VIRTUAL_NETWORK_CONFIG_OPERATION_CONFIG_UPDATE = 2,

	/**
	 * Network is going down (not permanently)
	 */
	ZT_VIRTUAL_NETWORK_CONFIG_OPERATION_DOWN = 3,

	/**
	 * Network is going down permanently (leave/delete)
	 */
	ZT_VIRTUAL_NETWORK_CONFIG_OPERATION_DESTROY = 4
};

/**
 * Local interface trust levels
 */
enum ZT_LocalInterfaceAddressTrust {
	ZT_LOCAL_INTERFACE_ADDRESS_TRUST_NORMAL = 0,
	ZT_LOCAL_INTERFACE_ADDRESS_TRUST_PRIVACY = 10,
	ZT_LOCAL_INTERFACE_ADDRESS_TRUST_ULTIMATE = 20
};

/**
 * What trust hierarchy role does this peer have?
 */
enum ZT_PeerRole {
	ZT_PEER_ROLE_LEAF = 0,     // ordinary node
	ZT_PEER_ROLE_RELAY = 1,    // relay node
	ZT_PEER_ROLE_ROOT = 2      // root server
};

/**
 * Vendor ID
 */
enum ZT_Vendor {
	ZT_VENDOR_UNSPECIFIED = 0,
	ZT_VENDOR_ZEROTIER = 1
};

/**
 * Platform type
 */
enum ZT_Platform {
	ZT_PLATFORM_UNSPECIFIED = 0,
	ZT_PLATFORM_LINUX = 1,
	ZT_PLATFORM_WINDOWS = 2,
	ZT_PLATFORM_MACOS = 3,
	ZT_PLATFORM_ANDROID = 4,
	ZT_PLATFORM_IOS = 5,
	ZT_PLATFORM_SOLARIS_SMARTOS = 6,
	ZT_PLATFORM_FREEBSD = 7,
	ZT_PLATFORM_NETBSD = 8,
	ZT_PLATFORM_OPENBSD = 9,
	ZT_PLATFORM_RISCOS = 10,
	ZT_PLATFORM_VXWORKS = 11,
	ZT_PLATFORM_FREERTOS = 12,
	ZT_PLATFORM_SYSBIOS = 13,
	ZT_PLATFORM_HURD = 14
};

/**
 * Architecture type
 */
enum ZT_Architecture {
	ZT_ARCHITECTURE_UNSPECIFIED = 0,
	ZT_ARCHITECTURE_X86 = 1,
	ZT_ARCHITECTURE_X64 = 2,
	ZT_ARCHITECTURE_ARM32 = 3,
	ZT_ARCHITECTURE_ARM64 = 4,
	ZT_ARCHITECTURE_MIPS32 = 5,
	ZT_ARCHITECTURE_MIPS64 = 6,
	ZT_ARCHITECTURE_POWER32 = 7,
	ZT_ARCHITECTURE_POWER64 = 8
};

/**
 * Virtual network configuration
 */
typedef struct
{
	/**
	 * 64-bit ZeroTier network ID
	 */
	uint64_t nwid;

	/**
	 * Ethernet MAC (48 bits) that should be assigned to port
	 */
	uint64_t mac;

	/**
	 * Network name (from network configuration master)
	 */
	char name[ZT_MAX_NETWORK_SHORT_NAME_LENGTH + 1];

	/**
	 * Network configuration request status
	 */
	enum ZT_VirtualNetworkStatus status;

	/**
	 * Network type
	 */
	enum ZT_VirtualNetworkType type;

	/**
	 * Maximum interface MTU
	 */
	unsigned int mtu;

	/**
	 * If nonzero, the network this port belongs to indicates DHCP availability
	 *
	 * This is a suggestion. The underlying implementation is free to ignore it
	 * for security or other reasons. This is simply a netconf parameter that
	 * means 'DHCP is available on this network.'
	 */
	int dhcp;

	/**
	 * If nonzero, this port is allowed to bridge to other networks
	 *
	 * This is informational. If this is false (0), bridged packets will simply
	 * be dropped and bridging won't work.
	 */
	int bridge;

	/**
	 * If nonzero, this network supports and allows broadcast (ff:ff:ff:ff:ff:ff) traffic
	 */
	int broadcastEnabled;

	/**
	 * If the network is in PORT_ERROR state, this is the error most recently returned by the port config callback
	 */
	int portError;

	/**
	 * Is this network enabled? If not, all frames to/from are dropped.
	 */
	int enabled;

	/**
	 * Network config revision as reported by netconf master
	 *
	 * If this is zero, it means we're still waiting for our netconf.
	 */
	unsigned long netconfRevision;

	/**
	 * Number of multicast group subscriptions
	 */
	unsigned int multicastSubscriptionCount;

	/**
	 * Multicast group subscriptions
	 */
	ZT_MulticastGroup multicastSubscriptions[ZT_MAX_NETWORK_MULTICAST_SUBSCRIPTIONS];

	/**
	 * Number of assigned addresses
	 */
	unsigned int assignedAddressCount;

	/**
	 * ZeroTier-assigned addresses (in sockaddr_storage structures)
	 *
	 * For IP, the port number of the sockaddr_XX structure contains the number
	 * of bits in the address netmask. Only the IP address and port are used.
	 * Other fields like interface number can be ignored.
	 *
	 * This is only used for ZeroTier-managed address assignments sent by the
	 * virtual network's configuration master.
	 */
	struct sockaddr_storage assignedAddresses[ZT_MAX_ZT_ASSIGNED_ADDRESSES];
} ZT_VirtualNetworkConfig;

/**
 * A list of networks
 */
typedef struct
{
	ZT_VirtualNetworkConfig *networks;
	unsigned long networkCount;
} ZT_VirtualNetworkList;

/**
 * Physical network path to a peer
 */
typedef struct
{
	/**
	 * Address of endpoint
	 */
	struct sockaddr_storage address;

	/**
	 * Time of last send in milliseconds or 0 for never
	 */
	uint64_t lastSend;

	/**
	 * Time of last receive in milliseconds or 0 for never
	 */
	uint64_t lastReceive;

	/**
	 * Is path active?
	 */
	int active;

	/**
	 * Is path preferred?
	 */
	int preferred;
} ZT_PeerPhysicalPath;

/**
 * Peer status result buffer
 */
typedef struct
{
	/**
	 * ZeroTier address (40 bits)
	 */
	uint64_t address;

	/**
	 * Time we last received a unicast frame from this peer
	 */
	uint64_t lastUnicastFrame;

	/**
	 * Time we last received a multicast rame from this peer
	 */
	uint64_t lastMulticastFrame;

	/**
	 * Remote major version or -1 if not known
	 */
	int versionMajor;

	/**
	 * Remote minor version or -1 if not known
	 */
	int versionMinor;

	/**
	 * Remote revision or -1 if not known
	 */
	int versionRev;

	/**
	 * Last measured latency in milliseconds or zero if unknown
	 */
	unsigned int latency;

	/**
	 * What trust hierarchy role does this device have?
	 */
	enum ZT_PeerRole role;

	/**
	 * Number of paths (size of paths[])
	 */
	unsigned int pathCount;

	/**
	 * Known network paths to peer
	 */
	ZT_PeerPhysicalPath paths[ZT_MAX_PEER_NETWORK_PATHS];
} ZT_Peer;

/**
 * List of peers
 */
typedef struct
{
	ZT_Peer *peers;
	unsigned long peerCount;
} ZT_PeerList;

/**
 * ZeroTier circuit test configuration and path
 */
typedef struct {
	/**
	 * Test ID -- an arbitrary 64-bit identifier
	 */
	uint64_t testId;

	/**
	 * Timestamp -- sent with test and echoed back by each reporter
	 */
	uint64_t timestamp;

	/**
	 * Originator credential: network ID
	 *
	 * If this is nonzero, a network ID will be set for this test and
	 * the originator must be its primary network controller. This is
	 * currently the only authorization method available, so it must
	 * be set to run a test.
	 */
	uint64_t credentialNetworkId;

	/**
	 * Hops in circuit test (a.k.a. FIFO for graph traversal)
	 */
	struct {
		/**
		 * Hop flags (currently unused, must be zero)
		 */
		unsigned int flags;

		/**
		 * Number of addresses in this hop (max: ZT_CIRCUIT_TEST_MAX_HOP_BREADTH)
		 */
		unsigned int breadth;

		/**
		 * 40-bit ZeroTier addresses (most significant 24 bits ignored)
		 */
		uint64_t addresses[ZT_CIRCUIT_TEST_MAX_HOP_BREADTH];
	} hops[ZT_CIRCUIT_TEST_MAX_HOPS];

	/**
	 * Number of hops (max: ZT_CIRCUIT_TEST_MAX_HOPS)
	 */
	unsigned int hopCount;

	/**
	 * If non-zero, circuit test will report back at every hop
	 */
	int reportAtEveryHop;

	/**
	 * An arbitrary user-settable pointer
	 */
	void *ptr;

	/**
	 * Pointer for internal use -- initialize to zero and do not modify
	 */
	void *_internalPtr;
} ZT_CircuitTest;

/**
 * Circuit test result report
 */
typedef struct {
	/**
	 * Sender of report (current hop)
	 */
	uint64_t current;

	/**
	 * Previous hop
	 */
	uint64_t upstream;

	/**
	 * 64-bit test ID
	 */
	uint64_t testId;

	/**
	 * Timestamp from original test (echoed back at each hop)
	 */
	uint64_t timestamp;

	/**
	 * Timestamp on remote device
	 */
	uint64_t remoteTimestamp;

	/**
	 * 64-bit packet ID of packet received by the reporting device
	 */
	uint64_t sourcePacketId;

	/**
	 * Flags (currently unused, will be zero)
	 */
	uint64_t flags;

	/**
	 * ZeroTier protocol-level hop count of packet received by reporting device (>0 indicates relayed)
	 */
	unsigned int sourcePacketHopCount;

	/**
	 * Error code (currently unused, will be zero)
	 */
	unsigned int errorCode;

	/**
	 * Remote device vendor ID
	 */
	enum ZT_Vendor vendor;

	/**
	 * Remote device protocol compliance version
	 */
	unsigned int protocolVersion;

	/**
	 * Software major version
	 */
	unsigned int majorVersion;

	/**
	 * Software minor version
	 */
	unsigned int minorVersion;

	/**
	 * Software revision
	 */
	unsigned int revision;

	/**
	 * Platform / OS
	 */
	enum ZT_Platform platform;

	/**
	 * System architecture
	 */
	enum ZT_Architecture architecture;

	/**
	 * Local device address on which packet was received by reporting device
	 *
	 * This may have ss_family equal to zero (null address) if unspecified.
	 */
	struct sockaddr_storage receivedOnLocalAddress;

	/**
	 * Remote address from which reporter received the test packet
	 *
	 * This may have ss_family set to zero (null address) if unspecified.
	 */
	struct sockaddr_storage receivedFromRemoteAddress;

	/**
	 * Next hops to which packets are being or will be sent by the reporter
	 *
	 * In addition to reporting back, the reporter may send the test on if
	 * there are more recipients in the FIFO. If it does this, it can report
	 * back the address(es) that make up the next hop and the physical address
	 * for each if it has one. The physical address being null/unspecified
	 * typically indicates that no direct path exists and the next packet
	 * will be relayed.
	 */
	struct {
		/**
		 * 40-bit ZeroTier address
		 */
		uint64_t address;

		/**
		 * Physical address or null address (ss_family == 0) if unspecified or unknown
		 */
		struct sockaddr_storage physicalAddress;
	} nextHops[ZT_CIRCUIT_TEST_MAX_HOP_BREADTH];

	/**
	 * Number of next hops reported in nextHops[]
	 */
	unsigned int nextHopCount;
} ZT_CircuitTestReport;

/**
 * A cluster member's status
 */
typedef struct {
	/**
	 * This cluster member's ID (from 0 to 1-ZT_CLUSTER_MAX_MEMBERS)
	 */
	unsigned int id;

	/**
	 * Number of milliseconds since last 'alive' heartbeat message received via cluster backplane address
	 */
	unsigned int msSinceLastHeartbeat;

	/**
	 * Non-zero if cluster member is alive
	 */
	int alive;

	/**
	 * X, Y, and Z coordinates of this member (if specified, otherwise zero)
	 *
	 * What these mean depends on the location scheme being used for
	 * location-aware clustering. At present this is GeoIP and these
	 * will be the X, Y, and Z coordinates of the location on a spherical
	 * approximation of Earth where Earth's core is the origin (in km).
	 * They don't have to be perfect and need only be comparable with others
	 * to find shortest path via the standard vector distance formula.
	 */
	int x,y,z;

	/**
	 * Cluster member's last reported load
	 */
	uint64_t load;

	/**
	 * Number of peers this cluster member "has"
	 */
	uint64_t peers;

	/**
	 * Physical ZeroTier endpoints for this member (where peers are sent when directed here)
	 */
	struct sockaddr_storage zeroTierPhysicalEndpoints[ZT_CLUSTER_MAX_ZT_PHYSICAL_ADDRESSES];

	/**
	 * Number of physical ZeroTier endpoints this member is announcing
	 */
	unsigned int numZeroTierPhysicalEndpoints;
} ZT_ClusterMemberStatus;

/**
 * ZeroTier cluster status
 */
typedef struct {
	/**
	 * My cluster member ID (a record for 'self' is included in member[])
	 */
	unsigned int myId;

	/**
	 * Number of cluster members
	 */
	unsigned int clusterSize;

	/**
	 * Cluster member statuses
	 */
	ZT_ClusterMemberStatus members[ZT_CLUSTER_MAX_MEMBERS];
} ZT_ClusterStatus;

/**
 * An instance of a ZeroTier One node (opaque)
 */
typedef void ZT_Node;

/****************************************************************************/
/* Callbacks used by Node API                                               */
/****************************************************************************/

/**
 * Callback called to update virtual network port configuration
 *
 * This can be called at any time to update the configuration of a virtual
 * network port. The parameter after the network ID specifies whether this
 * port is being brought up, updated, brought down, or permanently deleted.
 *
 * This in turn should be used by the underlying implementation to create
 * and configure tap devices at the OS (or virtual network stack) layer.
 *
 * The supplied config pointer is not guaranteed to remain valid, so make
 * a copy if you want one.
 *
 * This should not call multicastSubscribe() or other network-modifying
 * methods, as this could cause a deadlock in multithreaded or interrupt
 * driven environments.
 *
 * This must return 0 on success. It can return any OS-dependent error code
 * on failure, and this results in the network being placed into the
 * PORT_ERROR state.
 */
typedef int (*ZT_VirtualNetworkConfigFunction)(
	ZT_Node *,
	void *,
	uint64_t,
	enum ZT_VirtualNetworkConfigOperation,
	const ZT_VirtualNetworkConfig *);

/**
 * Function to send a frame out to a virtual network port
 *
 * Parameters: (1) node, (2) user ptr, (3) network ID, (4) source MAC,
 * (5) destination MAC, (6) ethertype, (7) VLAN ID, (8) frame data,
 * (9) frame length.
 */
typedef void (*ZT_VirtualNetworkFrameFunction)(
	ZT_Node *,
	void *,
	uint64_t,
	uint64_t,
	uint64_t,
	unsigned int,
	unsigned int,
	const void *,
	unsigned int);

/**
 * Callback for events
 *
 * Events are generated when the node's status changes in a significant way
 * and on certain non-fatal errors and events of interest. The final void
 * parameter points to event meta-data. The type of event meta-data (and
 * whether it is present at all) is event type dependent. See the comments
 * in the definition of ZT_Event.
 */
typedef void (*ZT_EventCallback)(
	ZT_Node *,
	void *,
	enum ZT_Event,
	const void *);

/**
 * Function to get an object from the data store
 *
 * Parameters: (1) object name, (2) buffer to fill, (3) size of buffer, (4)
 * index in object to start reading, (5) result parameter that must be set
 * to the actual size of the object if it exists.
 *
 * Object names can contain forward slash (/) path separators. They will
 * never contain .. or backslash (\), so this is safe to map as a Unix-style
 * path if the underlying storage permits. For security reasons we recommend
 * returning errors if .. or \ are used.
 *
 * The function must return the actual number of bytes read. If the object
 * doesn't exist, it should return -1. -2 should be returned on other errors
 * such as errors accessing underlying storage.
 *
 * If the read doesn't fit in the buffer, the max number of bytes should be
 * read. The caller may call the function multiple times to read the whole
 * object.
 */
typedef long (*ZT_DataStoreGetFunction)(
	ZT_Node *,
	void *,
	const char *,
	void *,
	unsigned long,
	unsigned long,
	unsigned long *);

/**
 * Function to store an object in the data store
 *
 * Parameters: (1) node, (2) user ptr, (3) object name, (4) object data,
 * (5) object size, (6) secure? (bool).
 *
 * If secure is true, the file should be set readable and writable only
 * to the user running ZeroTier One. What this means is platform-specific.
 *
 * Name semantics are the same as the get function. This must return zero on
 * success. You can return any OS-specific error code on failure, as these
 * may be visible in logs or error messages and might aid in debugging.
 *
 * If the data pointer is null, this must be interpreted as a delete
 * operation.
 */
typedef int (*ZT_DataStorePutFunction)(
	ZT_Node *,
	void *,
	const char *,
	const void *,
	unsigned long,
	int);

/**
 * Function to send a ZeroTier packet out over the wire
 *
 * Parameters:
 *  (1) Node
 *  (2) User pointer
 *  (3) Local interface address
 *  (4) Remote address
 *  (5) Packet data
 *  (6) Packet length
 *
 * If there is only one local interface it is safe to ignore the local
 * interface address. Otherwise if running with multiple interfaces, the
 * correct local interface should be chosen by address unless NULL. If
 * the ss_family field is zero (NULL address), a random or preferred
 * default interface should be used.
 *
 * The function must return zero on success and may return any error code
 * on failure. Note that success does not (of course) guarantee packet
 * delivery. It only means that the packet appears to have been sent.
 */
typedef int (*ZT_WirePacketSendFunction)(
	ZT_Node *,                      /* Node */
	void *,                          /* User ptr */
	const struct sockaddr_storage *, /* Local address */
	const struct sockaddr_storage *, /* Remote address */
	const void *,                    /* Packet data */
	unsigned int);                   /* Packet length */

/****************************************************************************/
/* C Node API                                                               */
/****************************************************************************/

/**
 * Create a new ZeroTier One node
 *
 * Note that this can take a few seconds the first time it's called, as it
 * will generate an identity.
 *
 * @param node Result: pointer is set to new node instance on success
 * @param uptr User pointer to pass to functions/callbacks
 * @param now Current clock in milliseconds
 * @param dataStoreGetFunction Function called to get objects from persistent storage
 * @param dataStorePutFunction Function called to put objects in persistent storage
 * @param virtualNetworkConfigFunction Function to be called when virtual LANs are created, deleted, or their config parameters change
 * @param eventCallback Function to receive status updates and non-fatal error notices
 * @return OK (0) or error code if a fatal error condition has occurred
 */
enum ZT_ResultCode ZT_Node_new(
	ZT_Node **node,
	void *uptr,
	uint64_t now,
	ZT_DataStoreGetFunction dataStoreGetFunction,
	ZT_DataStorePutFunction dataStorePutFunction,
	ZT_WirePacketSendFunction wirePacketSendFunction,
	ZT_VirtualNetworkFrameFunction virtualNetworkFrameFunction,
	ZT_VirtualNetworkConfigFunction virtualNetworkConfigFunction,
	ZT_EventCallback eventCallback);

/**
 * Delete a node and free all resources it consumes
 *
 * If you are using multiple threads, all other threads must be shut down
 * first. This can crash if processXXX() methods are in progress.
 *
 * @param node Node to delete
 */
void ZT_Node_delete(ZT_Node *node);

/**
 * Process a packet received from the physical wire
 *
 * @param node Node instance
 * @param now Current clock in milliseconds
 * @param localAddress Local address, or point to ZT_SOCKADDR_NULL if unspecified
 * @param remoteAddress Origin of packet
 * @param packetData Packet data
 * @param packetLength Packet length
 * @param nextBackgroundTaskDeadline Value/result: set to deadline for next call to processBackgroundTasks()
 * @return OK (0) or error code if a fatal error condition has occurred
 */
enum ZT_ResultCode ZT_Node_processWirePacket(
	ZT_Node *node,
	uint64_t now,
	const struct sockaddr_storage *localAddress,
	const struct sockaddr_storage *remoteAddress,
	const void *packetData,
	unsigned int packetLength,
	volatile uint64_t *nextBackgroundTaskDeadline);

/**
 * Process a frame from a virtual network port (tap)
 *
 * @param node Node instance
 * @param now Current clock in milliseconds
 * @param nwid ZeroTier 64-bit virtual network ID
 * @param sourceMac Source MAC address (least significant 48 bits)
 * @param destMac Destination MAC address (least significant 48 bits)
 * @param etherType 16-bit Ethernet frame type
 * @param vlanId 10-bit VLAN ID or 0 if none
 * @param frameData Frame payload data
 * @param frameLength Frame payload length
 * @param nextBackgroundTaskDeadline Value/result: set to deadline for next call to processBackgroundTasks()
 * @return OK (0) or error code if a fatal error condition has occurred
 */
enum ZT_ResultCode ZT_Node_processVirtualNetworkFrame(
	ZT_Node *node,
	uint64_t now,
	uint64_t nwid,
	uint64_t sourceMac,
	uint64_t destMac,
	unsigned int etherType,
	unsigned int vlanId,
	const void *frameData,
	unsigned int frameLength,
	volatile uint64_t *nextBackgroundTaskDeadline);

/**
 * Perform periodic background operations
 *
 * @param node Node instance
 * @param now Current clock in milliseconds
 * @param nextBackgroundTaskDeadline Value/result: set to deadline for next call to processBackgroundTasks()
 * @return OK (0) or error code if a fatal error condition has occurred
 */
enum ZT_ResultCode ZT_Node_processBackgroundTasks(ZT_Node *node,uint64_t now,volatile uint64_t *nextBackgroundTaskDeadline);

/**
 * Join a network
 *
 * This may generate calls to the port config callback before it returns,
 * or these may be deffered if a netconf is not available yet.
 *
 * If we are already a member of the network, nothing is done and OK is
 * returned.
 *
 * @param node Node instance
 * @param nwid 64-bit ZeroTier network ID
 * @return OK (0) or error code if a fatal error condition has occurred
 */
enum ZT_ResultCode ZT_Node_join(ZT_Node *node,uint64_t nwid);

/**
 * Leave a network
 *
 * If a port has been configured for this network this will generate a call
 * to the port config callback with a NULL second parameter to indicate that
 * the port is now deleted.
 *
 * @param node Node instance
 * @param nwid 64-bit network ID
 * @return OK (0) or error code if a fatal error condition has occurred
 */
enum ZT_ResultCode ZT_Node_leave(ZT_Node *node,uint64_t nwid);

/**
 * Subscribe to an Ethernet multicast group
 *
 * ADI stands for additional distinguishing information. This defaults to zero
 * and is rarely used. Right now its only use is to enable IPv4 ARP to scale,
 * and this must be done.
 *
 * For IPv4 ARP, the implementation must subscribe to 0xffffffffffff (the
 * broadcast address) but with an ADI equal to each IPv4 address in host
 * byte order. This converts ARP from a non-scalable broadcast protocol to
 * a scalable multicast protocol with perfect address specificity.
 *
 * If this is not done, ARP will not work reliably.
 *
 * Multiple calls to subscribe to the same multicast address will have no
 * effect. It is perfectly safe to do this.
 *
 * This does not generate an update call to networkConfigCallback().
 *
 * @param node Node instance
 * @param nwid 64-bit network ID
 * @param multicastGroup Ethernet multicast or broadcast MAC (least significant 48 bits)
 * @param multicastAdi Multicast ADI (least significant 32 bits only, use 0 if not needed)
 * @return OK (0) or error code if a fatal error condition has occurred
 */
enum ZT_ResultCode ZT_Node_multicastSubscribe(ZT_Node *node,uint64_t nwid,uint64_t multicastGroup,unsigned long multicastAdi);

/**
 * Unsubscribe from an Ethernet multicast group (or all groups)
 *
 * If multicastGroup is zero (0), this will unsubscribe from all groups. If
 * you are not subscribed to a group this has no effect.
 *
 * This does not generate an update call to networkConfigCallback().
 *
 * @param node Node instance
 * @param nwid 64-bit network ID
 * @param multicastGroup Ethernet multicast or broadcast MAC (least significant 48 bits)
 * @param multicastAdi Multicast ADI (least significant 32 bits only, use 0 if not needed)
 * @return OK (0) or error code if a fatal error condition has occurred
 */
enum ZT_ResultCode ZT_Node_multicastUnsubscribe(ZT_Node *node,uint64_t nwid,uint64_t multicastGroup,unsigned long multicastAdi);

/**
 * Get this node's 40-bit ZeroTier address
 *
 * @param node Node instance
 * @return ZeroTier address (least significant 40 bits of 64-bit int)
 */
uint64_t ZT_Node_address(ZT_Node *node);

/**
 * Get the status of this node
 *
 * @param node Node instance
 * @param status Buffer to fill with current node status
 */
void ZT_Node_status(ZT_Node *node,ZT_NodeStatus *status);

/**
 * Get a list of known peer nodes
 *
 * The pointer returned here must be freed with freeQueryResult()
 * when you are done with it.
 *
 * @param node Node instance
 * @return List of known peers or NULL on failure
 */
ZT_PeerList *ZT_Node_peers(ZT_Node *node);

/**
 * Get the status of a virtual network
 *
 * The pointer returned here must be freed with freeQueryResult()
 * when you are done with it.
 *
 * @param node Node instance
 * @param nwid 64-bit network ID
 * @return Network configuration or NULL if we are not a member of this network
 */
ZT_VirtualNetworkConfig *ZT_Node_networkConfig(ZT_Node *node,uint64_t nwid);

/**
 * Enumerate and get status of all networks
 *
 * @param node Node instance
 * @return List of networks or NULL on failure
 */
ZT_VirtualNetworkList *ZT_Node_networks(ZT_Node *node);

/**
 * Free a query result buffer
 *
 * Use this to free the return values of listNetworks(), listPeers(), etc.
 *
 * @param node Node instance
 * @param qr Query result buffer
 */
void ZT_Node_freeQueryResult(ZT_Node *node,void *qr);

/**
 * Add a local interface address
 *
 * Local interface addresses may be added if you want remote peers
 * with whom you have a trust relatinship (e.g. common network membership)
 * to receive information about these endpoints as potential endpoints for
 * direct communication.
 *
 * Take care that these are never ZeroTier interface addresses, otherwise
 * strange things might happen or they simply won't work.
 *
 * Addresses can also be added here if they are the result of a UPnP or
 * NAT-PMP port mapping or other discovery or mapping means.
 *
 * This returns a boolean indicating whether or not the address was
 * accepted. ZeroTier will only communicate over certain address types
 * and (for IP) address classes. Thus it's safe to just dump your OS's
 * entire remote IP list (excluding ZeroTier interface IPs) into here
 * and let ZeroTier determine which addresses it will use. It will
 * reject bad, empty, and unusable addresses.
 *
 * @param addr Local interface address
 * @param metric Local interface metric
 * @param trust How much do you trust the local network under this interface?
 * @return Boolean: non-zero if address was accepted and added
 */
int ZT_Node_addLocalInterfaceAddress(ZT_Node *node,const struct sockaddr_storage *addr,int metric, enum ZT_LocalInterfaceAddressTrust trust);

/**
 * Clear local interface addresses
 */
void ZT_Node_clearLocalInterfaceAddresses(ZT_Node *node);

/**
 * Set a network configuration master instance for this node
 *
 * Normal nodes should not need to use this. This is for nodes with
 * special compiled-in support for acting as network configuration
 * masters / controllers.
 *
 * The supplied instance must be a C++ object that inherits from the
 * NetworkConfigMaster base class in node/. No type checking is performed,
 * so a pointer to anything else will result in a crash.
 *
 * @param node ZertTier One node
 * @param networkConfigMasterInstance Instance of NetworkConfigMaster C++ class or NULL to disable
 * @return OK (0) or error code if a fatal error condition has occurred
 */
void ZT_Node_setNetconfMaster(ZT_Node *node,void *networkConfigMasterInstance);

/**
 * Initiate a VL1 circuit test
 *
 * This sends an initial VERB_CIRCUIT_TEST and reports results back to the
 * supplied callback until circuitTestEnd() is called. The supplied
 * ZT_CircuitTest structure should be initially zeroed and then filled
 * in with settings and hops.
 *
 * It is the caller's responsibility to call circuitTestEnd() and then
 * to dispose of the test structure. Otherwise this node will listen
 * for results forever.
 *
 * @param node Node instance
 * @param test Test configuration
 * @param reportCallback Function to call each time a report is received
 * @return OK or error if, for example, test is too big for a packet or support isn't compiled in
 */
enum ZT_ResultCode ZT_Node_circuitTestBegin(ZT_Node *node,ZT_CircuitTest *test,void (*reportCallback)(ZT_Node *, ZT_CircuitTest *,const ZT_CircuitTestReport *));

/**
 * Stop listening for results to a given circuit test
 *
 * This does not free the 'test' structure. The caller may do that
 * after calling this method to unregister it.
 *
 * Any reports that are received for a given test ID after it is
 * terminated are ignored.
 *
 * @param node Node instance
 * @param test Test configuration to unregister
 */
void ZT_Node_circuitTestEnd(ZT_Node *node,ZT_CircuitTest *test);

/**
 * Initialize cluster operation
 *
 * This initializes the internal structures and state for cluster operation.
 * It takes two function pointers. The first is to a function that can be
 * used to send data to cluster peers (mechanism is not defined by Node),
 * and the second is to a function that can be used to get the location of
 * a physical address in X,Y,Z coordinate space (e.g. as cartesian coordinates
 * projected from the center of the Earth).
 *
 * Send function takes an arbitrary pointer followed by the cluster member ID
 * to send data to, a pointer to the data, and the length of the data. The
 * maximum message length is ZT_CLUSTER_MAX_MESSAGE_LENGTH (65535). Messages
 * must be delivered whole and may be dropped or transposed, though high
 * failure rates are undesirable and can cause problems. Validity checking or
 * CRC is also not required since the Node validates the authenticity of
 * cluster messages using cryptogrphic methods and will silently drop invalid
 * messages.
 *
 * Address to location function is optional and if NULL geo-handoff is not
 * enabled (in this case x, y, and z in clusterInit are also unused). It
 * takes an arbitrary pointer followed by a physical address and three result
 * parameters for x, y, and z. It returns zero on failure or nonzero if these
 * three coordinates have been set. Coordinate space is arbitrary and can be
 * e.g. coordinates on Earth relative to Earth's center. These can be obtained
 * from latitutde and longitude with versions of the Haversine formula.
 *
 * See: http://stackoverflow.com/questions/1185408/converting-from-longitude-latitude-to-cartesian-coordinates
 *
 * Neither the send nor the address to location function should block. If the
 * address to location function does not have a location for an address, it
 * should return zero and then look up the address for future use since it
 * will be called again in (typically) 1-3 minutes.
 *
 * Note that both functions can be called from any thread from which the
 * various Node functions are called, and so must be thread safe if multiple
 * threads are being used.
 *
 * @param node Node instance
 * @param myId My cluster member ID (less than or equal to ZT_CLUSTER_MAX_MEMBERS)
 * @param zeroTierPhysicalEndpoints Preferred physical address(es) for ZeroTier clients to contact this cluster member (for peer redirect)
 * @param numZeroTierPhysicalEndpoints Number of physical endpoints in zeroTierPhysicalEndpoints[] (max allowed: 255)
 * @param x My cluster member's X location
 * @param y My cluster member's Y location
 * @param z My cluster member's Z location
 * @param sendFunction Function to be called to send data to other cluster members
 * @param sendFunctionArg First argument to sendFunction()
 * @param addressToLocationFunction Function to be called to get the location of a physical address or NULL to disable geo-handoff
 * @param addressToLocationFunctionArg First argument to addressToLocationFunction()
 * @return OK or UNSUPPORTED_OPERATION if this Node was not built with cluster support
 */
enum ZT_ResultCode ZT_Node_clusterInit(
	ZT_Node *node,
	unsigned int myId,
	const struct sockaddr_storage *zeroTierPhysicalEndpoints,
	unsigned int numZeroTierPhysicalEndpoints,
	int x,
	int y,
	int z,
	void (*sendFunction)(void *,unsigned int,const void *,unsigned int),
	void *sendFunctionArg,
	int (*addressToLocationFunction)(void *,const struct sockaddr_storage *,int *,int *,int *),
	void *addressToLocationFunctionArg);

/**
 * Add a member to this cluster
 *
 * Calling this without having called clusterInit() will do nothing.
 *
 * @param node Node instance
 * @param memberId Member ID (must be less than or equal to ZT_CLUSTER_MAX_MEMBERS)
 * @return OK or error if clustering is disabled, ID invalid, etc.
 */
enum ZT_ResultCode ZT_Node_clusterAddMember(ZT_Node *node,unsigned int memberId);

/**
 * Remove a member from this cluster
 *
 * Calling this without having called clusterInit() will do nothing.
 *
 * @param node Node instance
 * @param memberId Member ID to remove (nothing happens if not present)
 */
void ZT_Node_clusterRemoveMember(ZT_Node *node,unsigned int memberId);

/**
 * Handle an incoming cluster state message
 *
 * The message itself contains cluster member IDs, and invalid or badly
 * addressed messages will be silently discarded.
 *
 * Calling this without having called clusterInit() will do nothing.
 *
 * @param node Node instance
 * @param msg Cluster message
 * @param len Length of cluster message
 */
void ZT_Node_clusterHandleIncomingMessage(ZT_Node *node,const void *msg,unsigned int len);

/**
 * Get the current status of the cluster from this node's point of view
 *
 * Calling this without clusterInit() or without cluster support will just
 * zero out the structure and show a cluster size of zero.
 *
 * @param node Node instance
 * @param cs Cluster status structure to fill with data
 */
void ZT_Node_clusterStatus(ZT_Node *node,ZT_ClusterStatus *cs);

/**
 * Get ZeroTier One version
 *
 * @param major Result: major version
 * @param minor Result: minor version
 * @param revision Result: revision
 * @param featureFlags: Result: feature flag bitmap
 */
void ZT_version(int *major,int *minor,int *revision,unsigned long *featureFlags);

#ifdef __cplusplus
}
#endif

#endif
