
class FOImagePresenter : IVMRImagePresenter
{
	HRESULT PresentImage(DWORD_PTR  w, VMRPRESENTATIONINFO*  p)
	{
		//
		// Call the app specific function to render the scene
		//
		return S_OK;
	}

	HRESULT StartPresenting(DWORD_PTR  w)
	{
		return S_OK;
	}

	HRESULT StopPresenting(DWORD_PTR  w)
	{
		return S_OK;
	}
};

class FOSurfaceAllocator : IVMRSurfaceAllocator
{

};

HRESULT InitWindowlessVMR(
						  HWND hwndApp,                  // Window to hold the video. 
						  IGraphBuilder* pGraph,         // Pointer to the Filter Graph Manager. 
						  IVMRWindowlessControl** ppWc   // Receives a pointer to the VMR.
						  ) 
{
	if (!pGraph || !ppWc)
	{
		return E_POINTER;
	}
	IBaseFilter* pVmr = NULL;
	IVMRWindowlessControl* pWc = NULL;
	// Create the VMR.
	HRESULT hr = CoCreateInstance(CLSID_VideoMixingRenderer, NULL,
		CLSCTX_INPROC, IID_IBaseFilter, (void**)&pVmr);
	if (FAILED(hr))
	{
		return hr;
	}

	// Add the VMR to the filter graph.
	hr = pGraph->AddFilter(pVmr, L"Video Mixing Renderer");
	if (FAILED(hr))
	{
		pVmr->Release();
		return hr;
	}
	// Set the rendering mode. 
	IVMRFilterConfig* pConfig; 
	hr = pVmr->QueryInterface(IID_IVMRFilterConfig, (void**)&pConfig); 
	if (SUCCEEDED(hr)) 
	{ 
		hr = pConfig->SetRenderingMode(VMRMode_Renderless); 
		pConfig->Release(); 
	}
	if (SUCCEEDED(hr))
	{
		// Set the window. 
		hr = pVmr->QueryInterface(IID_IVMRWindowlessControl, (void**)&pWc);
		if( SUCCEEDED(hr)) 
		{ 
			hr = pWc->SetVideoClippingWindow(hwndApp); 
			if (SUCCEEDED(hr))
			{
				*ppWc = pWc; // Return this as an AddRef'd pointer. 
			}
			else
			{
				// An error occurred, so release the interface.
				pWc->Release();
			}
		}
	}
	pVmr->Release(); 
	return hr; 
}