// ExtendedControl.cpp: implementation of the CExtendedControl class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "DevInc.h"
#include "ActionObj.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

ITypeInfoPtr CExtendedControl::m_pTypeInfo = NULL;
ITypeInfoPtr CExtendedControl::m_pButtonTypeInfo = NULL;

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CExtendedControl::CExtendedControl() :
	m_nRefCount(0),
	m_pUnknown(NULL)
{
}

CExtendedControl::~CExtendedControl()
{
	if (m_pInnerDispatch != NULL)
	{
		AddRef();  // Undo the artificial Release
		m_pInnerDispatch.Release();
	}

	if (m_pUnknown != NULL)
	{
		m_pUnknown->Release();
		m_pUnknown = NULL;
	}	
}

HRESULT CExtendedControl::Init(REFCLSID clsidControl,
	CCtrlItem* pItem)
{
	USES_CONVERSION;
	HRESULT hResult;
	ULONG nOldRefCount;

	ASSERT(m_pItem != NULL);
	ASSERT(m_pUnknown == NULL);

	m_nRefCount++;  // Protect ourselves while we create the aggregate

	//注意此调用的参数
	hResult = CoCreateInstance(clsidControl, (IUnknown*)this,
		CLSCTX_INPROC_SERVER|CLSCTX_INPROC_HANDLER, IID_IUnknown,
		(void**)&m_pUnknown);
	if (FAILED(hResult))
	{
		return hResult;
	}

	nOldRefCount = m_nRefCount;  // Remember our refcount before we QI the
	// aggregate
	//查询内部的自动化接口，实际以调用此类中的QUERYINTERFACE
	m_pInnerDispatch = m_pUnknown;
	if (m_pInnerDispatch == NULL)
	{
		return E_NOINTERFACE;
	}
	if (m_nRefCount == nOldRefCount)
	{
		// The control didn't delegate its AddRef to us.  This probably means
		// that the control doesn't support aggregation, but didn't return
		// CLASS_E_NOAGGREGATION when we passed a non-null punkOuter to
		// CoCreateInstance().
		TRACE( "Control Bug: Aggregated control didn't delegate AddRef to extended control.  Trying without aggregation...\n" );
		m_pInnerDispatch.Release();
		m_nRefCount--;

		return CLASS_E_NOAGGREGATION;
	}
	Release();  // Artificially release because we QI'd the aggregate.

	
	if (m_pTypeInfo == NULL)
	{
		theApp.m_pHMITypeLib->GetTypeInfoOfGuid(__uuidof(ICxExtendedControl), &m_pTypeInfo);

		if (FAILED(hResult))
			return hResult;
	}
	if (m_pButtonTypeInfo == NULL)
	{
		theApp.m_pHMITypeLib->GetTypeInfoOfGuid(__uuidof(ICxExtendedButtonControl), &m_pButtonTypeInfo);

		if (FAILED(hResult))
			return hResult;
	}

	m_nRefCount--;  // Undo the extra refcount we added to protect ourself

	m_pItem = pItem;

	return S_OK;
}

//此函数为静态函数， 用于创建聚合的实例
HRESULT CExtendedControl::CreateInstance(REFCLSID clsidControl,
	CCtrlItem* pItem, IUnknown* pOuterUnknown, REFIID iid,
	void** ppInterface)
{
	CExtendedControl* pObject;
	HRESULT hResult;

	(void)pOuterUnknown;

	ASSERT(pOuterUnknown == NULL);
	ASSERT(ppInterface != NULL);
	*ppInterface = NULL;

	pObject = new CExtendedControl;
	if (pObject == NULL) 
	{
		return E_OUTOFMEMORY;
	}

	hResult = pObject->Init(clsidControl, pItem);
	if (FAILED(hResult))
	{
		delete pObject;
		return hResult;
	}

	hResult = pObject->QueryInterface(iid, ppInterface);
	if (FAILED(hResult))
	{
		delete pObject;
		return hResult;
	}

	return S_OK;
}

STDMETHODIMP_( ULONG ) CExtendedControl::AddRef()
{
	m_nRefCount++;

	return m_nRefCount;
}

STDMETHODIMP_( ULONG ) CExtendedControl::Release()
{
	m_nRefCount--;
	if (m_nRefCount == 0)
	{
		 m_nRefCount++;
		 delete this;
		 return 0;
	}

	return m_nRefCount;
}

STDMETHODIMP CExtendedControl::QueryInterface( REFIID iid, void** ppInterface )
{
	HRESULT hResult;

	ASSERT(ppInterface != NULL);
	*ppInterface = NULL;

	if (iid == IID_IDispatch)
	{
		*ppInterface = (IDispatch*)this;
		AddRef();
	}
	else if (iid == __uuidof(ICxExtendedControl) )
	{
		//此类从ICxExtendedControl继承
		*ppInterface = (ICxExtendedControl*)this;
		AddRef();
	}
	else if (iid == __uuidof(ICxExtendedButtonControl) )
	{
		//此类从ICxExtendedControl继承
		*ppInterface = (ICxExtendedButtonControl*)this;
		AddRef();
	}
	else
	{
		ASSERT(m_pUnknown != NULL);
		hResult = m_pUnknown->QueryInterface(iid, ppInterface);
		if (FAILED(hResult))
		{
			return hResult;
		}
	 }

	return S_OK;
}

STDMETHODIMP CExtendedControl::GetIDsOfNames(REFIID iid, LPOLESTR* ppszNames,
	UINT nNames, LCID lcid, DISPID* pDispIDs)
{
	HRESULT hResult;

	if (m_pItem->ActsLikeButton())
		hResult = m_pButtonTypeInfo->GetIDsOfNames(ppszNames, nNames, pDispIDs);
	else
		hResult = m_pTypeInfo->GetIDsOfNames(ppszNames, nNames, pDispIDs);

	if (FAILED(hResult))
	{
		hResult = m_pInnerDispatch->GetIDsOfNames(iid, ppszNames, nNames, lcid,
			pDispIDs);
	}

	return hResult;
}

STDMETHODIMP CExtendedControl::GetTypeInfo(UINT iTypeInfo, LCID lcid,
	ITypeInfo** ppTypeInfo)
{
	UINT nInfoCount;
	m_pInnerDispatch->GetTypeInfoCount(&nInfoCount);

	if (iTypeInfo < nInfoCount)
	{
		return m_pInnerDispatch->GetTypeInfo(iTypeInfo, lcid, ppTypeInfo);
	}
	else if (iTypeInfo == nInfoCount)
	{
		if (m_pItem->ActsLikeButton())
			*ppTypeInfo = m_pButtonTypeInfo;
		else
			*ppTypeInfo = m_pTypeInfo;
		(*ppTypeInfo)->AddRef();
		return S_OK;
	}
	
	return DISP_E_BADINDEX;
}

STDMETHODIMP CExtendedControl::GetTypeInfoCount(UINT* pnInfoCount)
{
	HRESULT hr = m_pInnerDispatch->GetTypeInfoCount(pnInfoCount);
	(*pnInfoCount)++;
	return hr;
}

HRESULT CExtendedControl::InternalInvoke(DISPID dispidMember, REFIID iid,
	LCID lcid, WORD wFlags, DISPPARAMS* pdpParams, VARIANT* pvarResult,
	EXCEPINFO* pExceptionInfo, UINT* piArgError)
{
	(void)iid;
	(void)lcid;

	ITypeInfo* pTypeInfo;
	if (m_pItem->ActsLikeButton())
		pTypeInfo = m_pButtonTypeInfo;
	else
		pTypeInfo = m_pTypeInfo;
	return pTypeInfo->Invoke(this, dispidMember, wFlags, pdpParams,
		pvarResult, pExceptionInfo, piArgError);
}

STDMETHODIMP CExtendedControl::Invoke(DISPID dispidMember, REFIID iid,
	LCID lcid, WORD wFlags, DISPPARAMS* pdpParams, VARIANT* pvarResult,
	EXCEPINFO* pExceptionInfo, UINT* piArgError)
{
	HRESULT hResult;

	if (pdpParams == NULL)
	{
		return E_INVALIDARG;
	}

	hResult = DISP_E_MEMBERNOTFOUND;
	if (iid == IID_NULL)
	{
		hResult = InternalInvoke(dispidMember, iid, lcid, wFlags, pdpParams,
			pvarResult, pExceptionInfo, piArgError);
	}
	if (hResult == DISP_E_MEMBERNOTFOUND)
	{
		hResult = m_pInnerDispatch->Invoke(dispidMember, iid, lcid, wFlags,
			pdpParams, pvarResult, pExceptionInfo, piArgError);
	}

	return hResult;
}

STDMETHODIMP CExtendedControl::get_Name(BSTR* pbstrName)
{
	ASSERT(pbstrName != NULL);

	CCtrlObj* pObj = m_pItem->m_pCtrlObj;
	ASSERT(pObj != NULL);

	*pbstrName = pObj->GetDisplayName().AllocSysString();

	return S_OK;
}

STDMETHODIMP CExtendedControl::put_Name(BSTR bstrName)
{
	CCtrlObj* pObj = m_pItem->m_pCtrlObj;
	ASSERT(pObj != NULL);
	
	pObj->PutDisplayName(CString(bstrName));

	return S_OK;
}

STDMETHODIMP CExtendedControl::get_HorizontalPosition(float* px)
{
	if (px == NULL)
	{
		return E_POINTER;
	}

	CCtrlObj* pObj = m_pItem->m_pCtrlObj;
	ASSERT(pObj != NULL);

	*px = pObj->GetPositionRect().left;

	return S_OK;
}

STDMETHODIMP CExtendedControl::put_HorizontalPosition(float x)
{
   	CCtrlObj* pObj = m_pItem->m_pCtrlObj;
	ASSERT(pObj != NULL);

	CRectF rect = pObj->GetPositionRect();
	rect.OffsetRect(x - rect.left, 0);
	pObj->MoveTo(rect, FALSE);

	return S_OK;
}

STDMETHODIMP CExtendedControl::get_VerticalPosition(float* py)
{
	if (py == NULL)
	{
		return E_POINTER;
	}

	CCtrlObj* pObj = m_pItem->m_pCtrlObj;
	ASSERT(pObj != NULL);

	*py = pObj->GetPositionRect().top;

	return S_OK;
 }

STDMETHODIMP CExtendedControl::put_VerticalPosition(float y)
{
	CCtrlObj* pObj = m_pItem->m_pCtrlObj;
	ASSERT(pObj != NULL);

	CRectF rect = pObj->GetPositionRect();
	rect.OffsetRect(0, y - rect.top);
	pObj->MoveTo(rect, FALSE);

	return S_OK;
}

STDMETHODIMP CExtendedControl::get_Height(float* px)
{
	if (px == NULL)
	{
		return E_POINTER;
	}

	CCtrlObj* pObj = m_pItem->m_pCtrlObj;
	ASSERT(pObj != NULL);

	*px = pObj->GetPositionRect().Height();

	return S_OK;
}

STDMETHODIMP CExtendedControl::put_Height(float y)
{
	CCtrlObj* pObj = m_pItem->m_pCtrlObj;
	ASSERT(pObj != NULL);

	CRectF rect = pObj->GetPositionRect();
	rect.bottom = rect.top + y;
	pObj->MoveTo(rect, FALSE);

	return S_OK;
}

STDMETHODIMP CExtendedControl::get_Width(float* py)
{
	if (py == NULL)
	{
		return E_POINTER;
	}

	CCtrlObj* pObj = m_pItem->m_pCtrlObj;
	ASSERT(pObj != NULL);

	*py = pObj->GetPositionRect().Width();

	return S_OK;
}

STDMETHODIMP CExtendedControl::put_Width(float x)
{
	CCtrlObj* pObj = m_pItem->m_pCtrlObj;
	ASSERT(pObj != NULL);

	CRectF rect = pObj->GetPositionRect();
	rect.right = rect.left + x;
	pObj->MoveTo(rect, FALSE);

	return S_OK;
}


STDMETHODIMP CExtendedControl::get_Visible(VARIANT_BOOL *pVal)
{
	CCtrlObj* pObj = m_pItem->m_pCtrlObj;
	ASSERT(pObj != NULL);

	*pVal = pObj->m_bVisible ? VARIANT_TRUE : VARIANT_FALSE;
	return S_OK;
}

STDMETHODIMP CExtendedControl::put_Visible(VARIANT_BOOL newVal)
{
	CCtrlObj* pObj = m_pItem->m_pCtrlObj;
	ASSERT(pObj != NULL);

	pObj->m_bVisible = (newVal != VARIANT_FALSE);
//	m_pItem->DoVerb(OLEIVERB_HIDE, pDoc->GetLayoutView());
	pObj->Invalidate();

	return S_OK;
}

STDMETHODIMP CExtendedControl::get_Layer(int *pVal)
{
	CCtrlObj* pObj = m_pItem->m_pCtrlObj;
	ASSERT(pObj != NULL);

	*pVal = pObj->m_nLayer + 1;
	return S_OK;
}

STDMETHODIMP CExtendedControl::put_Layer(int lLayer)
{
	CCtrlObj* pObj = m_pItem->m_pCtrlObj;
	ASSERT(pObj != NULL);

	if (lLayer > 30 || lLayer < 1)
		return E_INVALIDARG;

	pObj->m_nLayer = lLayer - 1;
	pObj->Invalidate();

	return S_OK;
}

STDMETHODIMP CExtendedControl::get_Locked(VARIANT_BOOL *pVal)
{
	CCtrlObj* pObj = m_pItem->m_pCtrlObj;
	ASSERT(pObj != NULL);
	
	*pVal = pObj->m_bLocked ? VARIANT_TRUE : VARIANT_FALSE;
	return S_OK;
}

STDMETHODIMP CExtendedControl::put_Locked(VARIANT_BOOL newVal)
{
	CCtrlObj* pObj = m_pItem->m_pCtrlObj;
	ASSERT(pObj != NULL);
	
	pObj->m_bLocked = (newVal != VARIANT_FALSE);
	//	m_pItem->DoVerb(OLEIVERB_HIDE, pDoc->GetLayoutView());
	pObj->Invalidate();
	
	return S_OK;
}

STDMETHODIMP CExtendedControl::get_Privilege(int *pVal)
{
	CCtrlObj* pObj = m_pItem->m_pCtrlObj;
	ASSERT(pObj != NULL);

	*pVal = pObj->m_nPrivilege;
	return S_OK;
}

STDMETHODIMP CExtendedControl::put_Privilege(int nPrivilege)
{
	CCtrlObj* pObj = m_pItem->m_pCtrlObj;
	ASSERT(pObj != NULL);

	if (nPrivilege < 0 || nPrivilege > 1000)
		return E_INVALIDARG;

	pObj->m_nPrivilege = nPrivilege;

	return S_OK;
}

STDMETHODIMP CExtendedControl::get_ToolTipText(BSTR *pVal)
{
	CCtrlObj* pObj = m_pItem->m_pCtrlObj;
	ASSERT(pObj != NULL);
	
	*pVal = pObj->m_strToolTip.AllocSysString();
	return S_OK;
}

STDMETHODIMP CExtendedControl::put_ToolTipText(BSTR newVal)
{
	CCtrlObj* pObj = m_pItem->m_pCtrlObj;
	ASSERT(pObj != NULL);
	
	pObj->m_strToolTip = newVal;
	return S_OK;
}

STDMETHODIMP CExtendedControl::get_Description(BSTR *pVal)
{
	CCtrlObj* pObj = m_pItem->m_pCtrlObj;
	ASSERT(pObj != NULL);
	
	*pVal = pObj->m_strDescription.AllocSysString();
	return S_OK;
}

STDMETHODIMP CExtendedControl::put_Description(BSTR newVal)
{
	CCtrlObj* pObj = m_pItem->m_pCtrlObj;
	ASSERT(pObj != NULL);
	
	pObj->m_strDescription = newVal;
	return S_OK;
}

STDMETHODIMP CExtendedControl::get_TabIndex(int *pVal)
{
	CCtrlObj* pObj = m_pItem->m_pCtrlObj;
	ASSERT(pObj != NULL);

	*pVal = pObj->m_nTabIndex;
	return S_OK;
}

STDMETHODIMP CExtendedControl::put_TabIndex(int newVal)
{
	CCtrlObj* pObj = m_pItem->m_pCtrlObj;
	ASSERT(pObj != NULL);

	pObj->SetTabIndex(newVal);

	return S_OK;
}

STDMETHODIMP CExtendedControl::get_TabStop(VARIANT_BOOL *pVal)
{
	CCtrlObj* pObj = m_pItem->m_pCtrlObj;
	ASSERT(pObj != NULL);
	
	*pVal = pObj->m_bTabStop ? VARIANT_TRUE : VARIANT_FALSE;

	return S_OK;
}

STDMETHODIMP CExtendedControl::put_TabStop(VARIANT_BOOL newVal)
{
	CCtrlObj* pObj = m_pItem->m_pCtrlObj;
	ASSERT(pObj != NULL);
	
	if (!m_pItem->IsLabelControl())
		pObj->m_bTabStop = (newVal != VARIANT_FALSE);
	
	return S_OK;
}

STDMETHODIMP CExtendedControl::get_Default(VARIANT_BOOL *pVal)
{
	CCtrlObj* pObj = m_pItem->m_pCtrlObj;
	ASSERT(pObj != NULL);
	
	*pVal = pObj->m_bDefault ? VARIANT_TRUE : VARIANT_FALSE;

	return S_OK;
}

STDMETHODIMP CExtendedControl::put_Default(VARIANT_BOOL newVal)
{
	CCtrlObj* pObj = m_pItem->m_pCtrlObj;
	ASSERT(pObj != NULL);
	
	pObj->m_bDefault = (newVal != VARIANT_FALSE);
	
	return S_OK;
}

STDMETHODIMP CExtendedControl::get_Cancel(VARIANT_BOOL *pVal)
{
	CCtrlObj* pObj = m_pItem->m_pCtrlObj;
	ASSERT(pObj != NULL);
	
	*pVal = pObj->m_bCancel ? VARIANT_TRUE : VARIANT_FALSE;

	return S_OK;
}

STDMETHODIMP CExtendedControl::put_Cancel(VARIANT_BOOL newVal)
{
	CCtrlObj* pObj = m_pItem->m_pCtrlObj;
	ASSERT(pObj != NULL);
	
	pObj->m_bCancel = (newVal != VARIANT_FALSE);
	
	return S_OK;
}

STDMETHODIMP CExtendedControl::IsConnected(BSTR bstrPropertyName, VARIANT_BOOL* pbHasConnection)
{
	DISPID dispid;
	GetIDsOfNames(IID_NULL, &bstrPropertyName, 1, LOCALE_USER_DEFAULT, &dispid);

	CCtrlObj* pObj = m_pItem->m_pCtrlObj;
	ASSERT(pObj != NULL);

	pObj->HasDynamic(dispid, pbHasConnection);

	return S_OK;
}

STDMETHODIMP CExtendedControl::ConnectObject(BSTR bstrPropertyName, IUnknown* punkObject)
{
	DISPID dispid;
	GetIDsOfNames(IID_NULL, &bstrPropertyName, 1, LOCALE_USER_DEFAULT, &dispid);

	CCtrlObj* pObj = m_pItem->m_pCtrlObj;
	ASSERT(pObj != NULL);

	pObj->AddDynamic(dispid, punkObject);

	return S_OK;
}

STDMETHODIMP CExtendedControl::ConnectDirect(BSTR bstrPropertyName, BSTR bstrDataSource)
{
	DISPID dispid;
	GetIDsOfNames(IID_NULL, &bstrPropertyName, 1, LOCALE_USER_DEFAULT, &dispid);

	CCtrlObj* pObj = m_pItem->m_pCtrlObj;
	ASSERT(pObj != NULL);

//	CDynamicSet* pSet = m_pObject->GetDynamicProperty();
//	pSet->HasDynamic(dispid, pbHasConnection);

	return S_OK;
}

STDMETHODIMP CExtendedControl::GetConnectObject(BSTR bstrPropertyName, IUnknown** ppunkObject)
{
	DISPID dispid;
	GetIDsOfNames(IID_NULL, &bstrPropertyName, 1, LOCALE_USER_DEFAULT, &dispid);

	CCtrlObj* pObj = m_pItem->m_pCtrlObj;
	ASSERT(pObj != NULL);

	pObj->GetDynamic(dispid, ppunkObject);

	return S_OK;
}

STDMETHODIMP CExtendedControl::Disconnect(BSTR bstrPropertyName)
{
	DISPID dispid;
	GetIDsOfNames(IID_NULL, &bstrPropertyName, 1, LOCALE_USER_DEFAULT, &dispid);

	CCtrlObj* pObj = m_pItem->m_pCtrlObj;
	ASSERT(pObj != NULL);
	pObj->RemoveDynamic(dispid);

	return S_OK;
}

STDMETHODIMP CExtendedControl::GetConnectState(DISPID dispid, int* pState)
{
	CCtrlObj* pObj = m_pItem->m_pCtrlObj;
	ASSERT(pObj != NULL);

	*pState = 0;
	VARIANT_BOOL bHasConnection;
	pObj->HasDynamic(dispid, &bHasConnection);
	if (bHasConnection == VARIANT_TRUE)
		*pState |= 0x2;

	return S_OK;
}

STDMETHODIMP CExtendedControl::SetFocus()
{
	return S_OK;
}