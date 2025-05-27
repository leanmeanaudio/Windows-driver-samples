// C:\Dev\Windows Drivers Examples\audio\Lama\SysvadLoopback_Dev\Common\unknown.h

#if !defined(_LAMA_SYSVAD_UNKNOWN_H_) && !defined(_UNKNOWN_H_)
#define _LAMA_SYSVAD_UNKNOWN_H_
#define _UNKNOWN_H_

// Prevent standard WDK unknown.h from being included
#define _UNKNOWN_H

#include <ntdef.h>      // For NTSTATUS, PVOID, ULONG, TRUE, FALSE, NULL
#include <wdm.h>        // For InterlockedIncrement, InterlockedDecrement, GUID, REFCLSID, REFIID. Also PUNKNOWN.
#include <stdunk.h>     // For STDMETHOD, STDMETHOD_, IUnknown. Also REFIID.

//=============================================================================
// Macros
//=============================================================================

// from stdunk.h (these are local definitions for CUnknown class)
#define DECLARE_STD_UNKNOWN() \
    STDMETHOD_(ULONG,AddRef)(); \
    STDMETHOD_(ULONG,Release)(); \
    STDMETHOD(QueryInterface)(REFIID,PVOID *);

#define DEFINE_STD_CONSTRUCTOR(Class) \
    Class(PUNKNOWN pUnknownOuter) : CUnknown(pUnknownOuter) {}

#define IMPLEMENT_STD_UNKNOWN(Class) \
STDMETHODIMP_(ULONG) Class::AddRef() \
{ \
    InterlockedIncrement(reinterpret_cast<PLONG>(&m_RefCount)); \
    return m_RefCount; \
} \
STDMETHODIMP_(ULONG) Class::Release() \
{ \
    ULONG ref = InterlockedDecrement(reinterpret_cast<PLONG>(&m_RefCount)); \
    if (0 == ref) \
    { \
        delete this; \
    } \
    return ref; \
} \
STDMETHODIMP Class::QueryInterface(REFIID riid, PVOID *ppv) \
{ \
    return CUnknown::QueryInterface(riid, ppv); \
}

//=============================================================================
// Classes
//=============================================================================

class CUnknown
{
protected:
    LONG m_RefCount;
    PUNKNOWN m_pUnknownOuter;

public:
    CUnknown(PUNKNOWN pUnknownOuter = NULL)
        : m_RefCount(0)
        , m_pUnknownOuter(pUnknownOuter)
    {}

    virtual ~CUnknown() {}

    // Non-delegating QueryInterface
    STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, PVOID *ppvObject)
    {
        if (!ppvObject)
        {
            return STATUS_INVALID_PARAMETER;
        }
        *ppvObject = NULL;

        if (IsEqualGUID(riid, __uuidof(IUnknown)))
        {
            *ppvObject = PVOID(PUNKNOWN(this));
            AddRef(); // AddRef through the CUnknown interface.
            return STATUS_SUCCESS;
        }
        return STATUS_NO_INTERFACE;
    }

    // Non-delegating AddRef
    STDMETHODIMP_(ULONG) NonDelegatingAddRef(void)
    {
        InterlockedIncrement(&m_RefCount);
        return m_RefCount;
    }

    // Non-delegating Release
    STDMETHODIMP_(ULONG) NonDelegatingRelease(void)
    {
        ULONG ref = InterlockedDecrement(&m_RefCount);
        if (ref == 0)
        {
            delete this;
        }
        return ref;
    }
    
    // Delegating QueryInterface
    STDMETHODIMP QueryInterface(REFIID riid, PVOID *ppvObject)
    {
        if (m_pUnknownOuter)
            return m_pUnknownOuter->QueryInterface(riid, ppvObject);
        else
            return NonDelegatingQueryInterface(riid, ppvObject);
    }

    // Delegating AddRef
    STDMETHODIMP_(ULONG) AddRef(void)
    {
        if (m_pUnknownOuter)
            return m_pUnknownOuter->AddRef();
        else
            return NonDelegatingAddRef();
    }

    // Delegating Release
    STDMETHODIMP_(ULONG) Release(void)
    {
        if (m_pUnknownOuter)
            return m_pUnknownOuter->Release();
        else
            return NonDelegatingRelease();
    }
};

//=============================================================================
// Function Prototypes
//=============================================================================
// Example: NTSTATUS NewUnknown(PUNKNOWN *ppUnknown, PUNKNOWN pUnknownOuter);

#endif // _LAMA_SYSVAD_UNKNOWN_H_
