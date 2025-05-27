#pragma once

// This header fixes C++ compilation issues with ks.h
// It must be included BEFORE any other Windows headers

// Ensure this is only applied when compiling as C++
#ifdef __cplusplus

// Disable warnings about anonymous structs
#pragma warning(disable: 4201)  // nameless struct/union
#pragma warning(disable: 4214)  // bit field types other than int
#pragma warning(disable: 4115)  // named type definition in parentheses

// This trick prevents C++ from treating field names in anonymous unions/structs
// as override specifiers by making macros that evaluate to themselves

// KS.H anonymous struct/union field names
#define PriorityClass PriorityClass
#define PrioritySubClass PrioritySubClass
#define Set Set
#define Id Id
#define Flags Flags
#define Alignment Alignment
#define NodeId NodeId
#define Reserved Reserved
#define Size Size
#define Count Count
#define AccessFlags AccessFlags
#define DescriptionSize DescriptionSize
#define MembersListCount MembersListCount
#define MembersFlags MembersFlags
#define MembersSize MembersSize
#define MembersCount MembersCount
#define SignedMinimum SignedMinimum
#define SignedMaximum SignedMaximum
#define UnsignedMinimum UnsignedMinimum
#define UnsignedMaximum UnsignedMaximum
#define Granularity Granularity
#define Relation Relation
#define Type Type
#define MembersHeader MembersHeader
#define SteppingDelta SteppingDelta
#define NotificationType NotificationType
#define Event Event
#define Semaphore Semaphore
#define Adjustment Adjustment
#define ObjectHandle ObjectHandle
#define MarkTime MarkTime
#define TimeBase TimeBase
#define Interval Interval
#define Manufacturer Manufacturer
#define Product Product
#define Component Component
#define Name Name
#define Version Version
#define Revision Revision
#define Current Current
#define Stop Stop
#define Earliest Earliest
#define Latest Latest
#define SourceFormat SourceFormat
#define TargetFormat TargetFormat
#define Time Time
#define FromNode FromNode
#define FromNodePin FromNodePin
#define ToNode ToNode
#define ToNodePin ToNodePin
#define CategoriesCount CategoriesCount
#define GUID GUID
#define TopologyNodesCount TopologyNodesCount
#define TopologyConnectionsCount TopologyConnectionsCount
#define KSTOPOLOGY_CONNECTION KSTOPOLOGY_CONNECTION
#define CreateFlags CreateFlags
#define Node Node
#define PinId PinId

// For SAL annotations, we should NOT redefine them but instead handle annotations properly
// by including the SAL headers in the right order and with the right settings

#endif // __cplusplus
