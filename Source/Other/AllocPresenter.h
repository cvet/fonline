//------------------------------------------------------------------------------
// File: vcdplyer.h
//
// Desc: DirectShow sample code - header file for FOTextureRenderer class
//
// Copyright (c) 1994 - 2001, Microsoft Corporation.  All rights reserved.
//------------------------------------------------------------------------------

#include <dx8/DShow.h>

/* -------------------------------------------------------------------------
** FOTextureRenderer - an Mpeg movie playback class.
** -------------------------------------------------------------------------
*/
enum EMpegMovieMode { MOVIE_NOTOPENED = 0x00,
                      MOVIE_OPENED = 0x01,
                      MOVIE_PLAYING = 0x02,
                      MOVIE_STOPPED = 0x03,
                      MOVIE_PAUSED = 0x04 };

class FOTextureRenderer :
    public IVMRSurfaceAllocator,
    public IVMRImagePresenter,
    public IVMRSurfaceAllocatorNotify
{

public:
    // Our state variable - records whether we are opened, playing etc.
    EMpegMovieMode  m_Mode;
    HANDLE          m_MediaEvent;
    HWND            m_hwndApp;
    BOOL            m_bFullScreen;
    BOOL            m_bFullScreenPoss;
    int             m_iDuration;
    GUID            m_TimeFormat;

    LPDIRECTDRAWSURFACE7    m_pddsPrimary;
    LPDIRECTDRAWSURFACE7    m_pddsPriText;
    LPDIRECTDRAWSURFACE7    m_pddsRenderT;

    IFilterGraph*               m_Fg;
    IGraphBuilder*              m_Gb;
    IMediaControl*              m_Mc;
    IMediaSeeking*              m_Ms;
    IMediaEvent*                m_Me;

    IVMRSurfaceAllocator*       m_lpDefSA;
    IVMRImagePresenter*         m_lpDefIP;
    IVMRWindowlessControl*      m_lpDefWC;
    IVMRSurfaceAllocatorNotify* m_lpDefSAN;

    HRESULT CreateDefaultAllocatorPresenter();
    HRESULT AddVideoMixingRendererToFG();
    HRESULT OnSetDDrawDevice(LPDIRECTDRAW7 pDD, HMONITOR hMon);

     FOTextureRenderer(HWND hwndApplication);
    ~FOTextureRenderer();

    STDMETHODIMP NonDelegatingQueryInterface(REFIID, void**);

    // IVMRSurfaceAllocator
    STDMETHODIMP AllocateSurface(DWORD_PTR dwUserID,
                                VMRALLOCATIONINFO* lpAllocInfo,
                                 DWORD* lpdwActualBackBuffers,
                                 LPDIRECTDRAWSURFACE7* lplpSurface);
    STDMETHODIMP FreeSurface(DWORD_PTR dwUserID);
    STDMETHODIMP PrepareSurface(DWORD_PTR dwUserID,
                                LPDIRECTDRAWSURFACE7 lplpSurface,
                                DWORD dwSurfaceFlags);
    STDMETHODIMP AdviseNotify(IVMRSurfaceAllocatorNotify* lpIVMRSurfAllocNotify);

    // IVMRSurfaceAllocatorNotify
    STDMETHODIMP AdviseSurfaceAllocator(DWORD_PTR dwUserID,
                                        IVMRSurfaceAllocator* lpIVRMSurfaceAllocator);
    STDMETHODIMP SetDDrawDevice(LPDIRECTDRAW7 lpDDrawDevice,HMONITOR hMonitor);
    STDMETHODIMP ChangeDDrawDevice(LPDIRECTDRAW7 lpDDrawDevice,HMONITOR hMonitor);
    STDMETHODIMP RestoreDDrawSurfaces();
    STDMETHODIMP NotifyEvent(LONG EventCode, LONG_PTR lp1, LONG_PTR lp2);
    STDMETHODIMP SetBorderColor(COLORREF clr);

    // IVMRImagePresenter
    STDMETHODIMP StartPresenting(DWORD_PTR dwUserID);
    STDMETHODIMP StopPresenting(DWORD_PTR dwUserID);
    STDMETHODIMP PresentImage(DWORD_PTR dwUserID,
                              VMRPRESENTATIONINFO* lpPresInfo);

     void            DisplayModeChanged() {
        if (m_lpDefWC) {
            m_lpDefWC->DisplayModeChanged();
        }
    }

};
