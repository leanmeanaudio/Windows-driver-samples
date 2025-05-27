#pragma once
// Compatibility header for Windows Audio Driver WDK issues
// Include this before any other headers

// Disable warnings
#pragma warning(disable: 4005)  // Macro redefinition
#pragma warning(disable: 4100)  // Unreferenced formal parameter
#pragma warning(disable: 4127)  // Conditional expression is constant
#pragma warning(disable: 4201)  // Nameless struct/union
#pragma warning(disable: 4214)  // Bit field types other than int
#pragma warning(disable: 4701)  // Potentially uninitialized variable
#pragma warning(disable: 4703)  // Potentially uninitialized pointer

// C++ compatibility - to avoid 'unknown override specifier' errors
#ifdef __cplusplus
    // MS C++ 11+ compiler treats anonymous struct/union field names as override specifiers
    #define STRUCT_FIELD(name) name
#else
    #define STRUCT_FIELD(name) name
#endif

// Common field names in anonymous structs/unions
#define Set STRUCT_FIELD(Set)
#define Id STRUCT_FIELD(Id)
#define Flags STRUCT_FIELD(Flags)
#define PriorityClass STRUCT_FIELD(PriorityClass)
#define PrioritySubClass STRUCT_FIELD(PrioritySubClass)
#define Alignment STRUCT_FIELD(Alignment)
#define NodeId STRUCT_FIELD(NodeId)
#define Reserved STRUCT_FIELD(Reserved)
#define Size STRUCT_FIELD(Size)
#define Count STRUCT_FIELD(Count)
#define AccessFlags STRUCT_FIELD(AccessFlags)
#define DescriptionSize STRUCT_FIELD(DescriptionSize)
#define MembersListCount STRUCT_FIELD(MembersListCount)
#define MembersFlags STRUCT_FIELD(MembersFlags)
#define MembersSize STRUCT_FIELD(MembersSize)
#define MembersCount STRUCT_FIELD(MembersCount)
#define SignedMinimum STRUCT_FIELD(SignedMinimum)
#define SignedMaximum STRUCT_FIELD(SignedMaximum)
#define UnsignedMinimum STRUCT_FIELD(UnsignedMinimum)
#define UnsignedMaximum STRUCT_FIELD(UnsignedMaximum)
#define Granularity STRUCT_FIELD(Granularity)
#define Relation STRUCT_FIELD(Relation)
#define Type STRUCT_FIELD(Type)
#define MembersHeader STRUCT_FIELD(MembersHeader)
#define x STRUCT_FIELD(x)
#define y STRUCT_FIELD(y)
#define z STRUCT_FIELD(z)
#define dvX STRUCT_FIELD(dvX)
#define dvY STRUCT_FIELD(dvY)
#define dvZ STRUCT_FIELD(dvZ)
#define DistanceFactor STRUCT_FIELD(DistanceFactor)
#define RolloffFactor STRUCT_FIELD(RolloffFactor)
#define DopplerFactor STRUCT_FIELD(DopplerFactor)
#define MinDistance STRUCT_FIELD(MinDistance)
