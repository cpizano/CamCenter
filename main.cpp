// Thuis file is the main driver for CamCenter.

#include "stdafx.h"

#include <mfreadwrite.h>
#include "resource.h"

#pragma comment(lib, "mfreadwrite.lib")

enum class HardFailures {
  none,
  bad_config,
  com_error,
  no_capture_device
};

void HardfailMsgBox(HardFailures id, const wchar_t* info) {
  // $$ implement.
  __debugbreak();
}

struct Settings {
  std::string camera;
  std::wstring folder;
};

plx::File OpenConfigFile() {
  auto appdata_path = plx::GetAppDataPath(false);
  auto path = appdata_path.append(L"vortex\\texto\\config.json");
  plx::FileParams fparams = plx::FileParams::Read_SharedRead();
  return plx::File::Create(path, fparams, plx::FileSecurity());
}

Settings LoadSettings() {
  auto config = plx::JsonFromFile(OpenConfigFile());
  if (config.type() != plx::JsonType::OBJECT)
    throw plx::IOException(__LINE__, L"<unexpected json>");
  // $$ read & set something here.
  return Settings();
}

const D2D1_SIZE_F zero_offset = {0};

class DCoWindow : public plx::Window <DCoWindow> {
  // width and height are in logical pixels.
  const int width_;
  const int height_;

  plx::ComPtr<ID3D11Device> d3d_device_;
  plx::ComPtr<ID2D1Factory2> d2d_factory_;
  plx::ComPtr<ID2D1Device> d2d_device_;
  plx::ComPtr<IDCompositionDesktopDevice> dco_device_;
  plx::ComPtr<IDCompositionTarget> dco_target_;

  plx::ComPtr<IDCompositionVisual2> root_visual_;
  plx::ComPtr<IDCompositionSurface> root_surface_;


public:
  DCoWindow(int width, int height)
      : width_(width), height_(height) {

    create_window(WS_EX_NOREDIRECTIONBITMAP,
                  WS_POPUP | WS_VISIBLE,
                  L"camcenter @ 2015",
                  nullptr, nullptr,
                  10, 10,
                  width_, height_,
                  nullptr,
                  nullptr);

    d3d_device_ = plx::CreateDeviceD3D11(0);
    d2d_factory_ = plx::CreateD2D1FactoryST(D2D1_DEBUG_LEVEL_NONE);

    d2d_device_ = plx::CreateDeviceD2D1(d3d_device_, d2d_factory_);
    dco_device_ = plx::CreateDCoDevice2(d2d_device_);
    // create the composition target and the root visual.
    dco_target_ = plx::CreateDCoWindowTarget(dco_device_, window());
    root_visual_ = plx::CreateDCoVisual(dco_device_);
    // bind direct composition to our window.
    auto hr = dco_target_->SetRoot(root_visual_.Get());
    if (hr != S_OK)
      throw plx::ComException(__LINE__, hr);
    // allocate the gpu surface and bind it to the root visual.
    root_surface_ = plx::CreateDCoSurface(
        dco_device_,
        static_cast<unsigned int>(dpi().to_physical_x(width_)),
        static_cast<unsigned int>(dpi().to_physical_x(height_)));
    hr = root_visual_->SetContent(root_surface_.Get());
    if (hr != S_OK)
      throw plx::ComException(__LINE__, hr);

    update_screen();
  }

  void update_screen() {
    if (root_surface_) {
      D2D1::ColorF bk_color(0x000000, 0.9f);
      plx::ScopedD2D1DeviceContext dc(root_surface_, zero_offset, dpi(), &bk_color);
      draw_frame(dc());
    }

    dco_device_->Commit();
  }

  void draw_frame(ID2D1DeviceContext* dc) {

  }

  LRESULT message_handler(const UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
      case WM_DESTROY: {
        ::PostQuitMessage(0);
        return 0;
      }
      case WM_PAINT: {
        // just recovery here when using direct composition.
        break;
      }

      case WM_DPICHANGED: {
        return dpi_changed_handler(lparam);
      }
    }

    return ::DefWindowProc(window(), message, wparam, lparam);
  }

  LRESULT dpi_changed_handler(LPARAM lparam) {
    // $$ test this.
    plx::RectL r(plx::SizeL(
          static_cast<long>(dpi().to_physical_x(width_)),
          static_cast<long>(dpi().to_physical_x(height_))));
    
    auto suggested = reinterpret_cast<const RECT*> (lparam);
    ::AdjustWindowRectEx(&r, 
        ::GetWindowLong(window(), GWL_STYLE),
        FALSE,
        ::GetWindowLong(window(), GWL_EXSTYLE));
    ::SetWindowPos(window(), nullptr, suggested->left, suggested->top,
                   r.width(), r.height(),
                   SWP_NOACTIVATE | SWP_NOZORDER);
    return 0L;
  }

};

////////////////////////////////////////////////////////////////////////////////////////////////

struct AppException {
  HardFailures failure;
  int line;
  AppException(HardFailures failure, int line) : failure(failure), line(line) {}
};

plx::ComPtr<IMFAttributes> MakeMFAttributes(uint32_t count) {
  plx::ComPtr<IMFAttributes> attribs;
  auto hr = ::MFCreateAttributes(attribs.GetAddressOf(), count);
  if (hr != S_OK)
    throw plx::ComException(__LINE__, hr);
  return attribs;
}

plx::ComPtr<IMFMediaSource> GetCaptureDevice() {
  auto attributes = MakeMFAttributes(1);
  attributes->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE,
                      MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);

  IMFActivate** sources;
  uint32_t count = 0;
  auto hr = ::MFEnumDeviceSources(attributes.Get(), &sources, &count);
  if (hr != S_OK)
    throw plx::ComException(__LINE__, hr);

  if (count == 0)
    throw AppException(HardFailures::no_capture_device, __LINE__);

  plx::ComPtr<IMFMediaSource> device;
  hr = sources[0]->ActivateObject(__uuidof(device),
                                  reinterpret_cast<void **>(device.GetAddressOf()));
  if (hr != S_OK)
    throw plx::ComException(__LINE__, hr);

  for (uint32_t ix = 0; ix != count; ++ix) {
    sources[ix]->Release();
  }
  
  ::CoTaskMemFree(sources);
  return device;
}

class MediaFoundationInit {
public:
  MediaFoundationInit() {
    auto hr = ::MFStartup(MF_VERSION, MFSTARTUP_LITE);
    if (hr != S_OK)
      throw plx::ComException(__LINE__, hr);
  }

  ~MediaFoundationInit() {
    ::MFShutdown();
  }
};

class CaptureHandler : public plx::ComObject <IMFSourceReaderCallback> {
  plx::ReaderWriterLock rw_lock_;
  plx::ComPtr<IMFSourceReader> reader_;

public:
  CaptureHandler(plx::ComPtr<IMFMediaSource> source) {
    auto attributes = MakeMFAttributes(2);
    attributes->SetUnknown(MF_SOURCE_READER_ASYNC_CALLBACK, this);
    auto hr = ::MFCreateSourceReaderFromMediaSource(
        source.Get(), attributes.Get(), reader_.GetAddressOf());
    if (hr != S_OK)
      throw plx::ComException(__LINE__, hr);
  }

private:
  HRESULT __stdcall OnReadSample(HRESULT status,
                                 DWORD stream_index,
                                 DWORD stream_flags,
                                 LONGLONG timestamp,
                                 IMFSample *pSample) override {
    auto lock = rw_lock_.write_lock();
    return S_OK;
  };

  HRESULT __stdcall OnEvent(DWORD, IMFMediaEvent *) override {
    return S_OK;
  }

  HRESULT __stdcall OnFlush(DWORD) {
    return S_OK;
  }
};


int __stdcall wWinMain(HINSTANCE instance, HINSTANCE,
                       wchar_t* cmdline, int cmd_show) {

  // Despite this, MediaFoundation IMFSourceReaderCallback is multithreaded.
  ::CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

  try {
    auto settings = LoadSettings();
    DCoWindow window(300, 200);

    MediaFoundationInit mf_init;
    CaptureHandler capture_handler(GetCaptureDevice());

    MSG msg = {0};
    while (::GetMessage(&msg, NULL, 0, 0)) {
      ::TranslateMessage(&msg);
      ::DispatchMessage(&msg);
    }

    return (int) msg.wParam;

  } catch (plx::IOException& ex) {
    HardfailMsgBox(HardFailures::bad_config, ex.Name());
    return 1;
  } catch (plx::ComException& ex) {
    auto l = ex.Line();
    HardfailMsgBox(HardFailures::com_error, L"COM");
    return 2;
  } catch (AppException& ex) {
    HardfailMsgBox(ex.failure, L"App");
    return 3;
  }
}
