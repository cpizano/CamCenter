// Thuis file is the main driver for CamCenter.

#include "stdafx.h"

#include <mfreadwrite.h>
#include "resource.h"

// this pragma cannot be done by plex because there is MS header ordering issue.
#pragma comment(lib, "mfreadwrite.lib")

enum class HardFailures {
  none,
  bad_config,
  com_error,
  no_capture_device,
  bad_format
};

struct AppException {
  HardFailures failure;
  int line;
  AppException(HardFailures failure, int line) : failure(failure), line(line) {}
};

void HardfailMsgBox(HardFailures id, const wchar_t* info) {
  // $$ implement.
  __debugbreak();
}

struct Settings {
  std::string folder;
  std::string file_prefix;
  int64_t avg_file_size;
};

plx::File OpenConfigFile() {
  auto appdata_path = plx::GetAppDataPath(false);
  auto path = appdata_path.append(L"vortex\\camcenter\\config.json");
  plx::FileParams fparams = plx::FileParams::Read_SharedRead();
  return plx::File::Create(path, fparams, plx::FileSecurity());
}

Settings LoadSettings() {
  auto config = plx::JsonFromFile(OpenConfigFile());
  if (config.type() != plx::JsonType::OBJECT)
    throw plx::IOException(__LINE__, L"<unexpected json>");

  Settings settings;
  settings.folder = config["folder"].get_string();
  settings.file_prefix = config["file_prefix"].get_string();
  settings.avg_file_size = config["avg_file_size"].get_int64();
  return settings;
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

plx::ComPtr<IMFAttributes> MakeMFAttributes(uint32_t count) {
  plx::ComPtr<IMFAttributes> attribs;
  auto hr = ::MFCreateAttributes(attribs.GetAddressOf(), count);
  if (hr != S_OK)
    throw plx::ComException(__LINE__, hr);
  return attribs;
}

void CopyMFAttribute(const GUID& key, IMFAttributes* src, IMFAttributes* dest) {
  PROPVARIANT var;
  ::PropVariantInit(&var);
  auto hr = src->GetItem(key, &var);
  if (hr != S_OK)
    throw plx::ComException(__LINE__, hr);
  dest->SetItem(key, var);
  ::PropVariantClear(&var);
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
  plx::ComPtr<IMFSinkWriter> writer_;
  LONGLONG base_time_;
  LONGLONG frame_count_;
  std::wstring file_;

public:
  CaptureHandler(plx::ComPtr<IMFMediaSource> source,
                 const Settings& settings) 
      : base_time_(0ULL), frame_count_(0ULL) {
    auto attributes = MakeMFAttributes(2);
    attributes->SetUnknown(MF_SOURCE_READER_ASYNC_CALLBACK, this);
    auto hr = ::MFCreateSourceReaderFromMediaSource(
        source.Get(), attributes.Get(), reader_.GetAddressOf());
    if (hr != S_OK)
      throw plx::ComException(__LINE__, hr);

    // Loop until we find the right format, then select it.
    DWORD media_type_ix = 0;
    plx::ComPtr<IMFMediaType> mtype;
    bool done = false;
    while (!done) {
      hr = reader_->GetNativeMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM,
                                       media_type_ix++,
                                       mtype.ReleaseAndGetAddressOf());
      if (hr == MF_E_NO_MORE_TYPES)
        throw AppException(HardFailures::bad_format, __LINE__);

      if (hr != S_OK)
        throw plx::ComException(__LINE__, hr);
      GUID subtype = {0};
      hr = mtype->GetGUID(MF_MT_SUBTYPE, &subtype);
      if (hr != S_OK)
        throw plx::ComException(__LINE__, hr);

      if ((subtype != MFVideoFormat_YUY2) && (subtype !=  MFVideoFormat_NV12))
        continue;

      AM_MEDIA_TYPE* amr = nullptr;
      hr = mtype->GetRepresentation(AM_MEDIA_TYPE_REPRESENTATION, reinterpret_cast<void**>(&amr));
      if (hr != S_OK)
        throw plx::ComException(__LINE__, hr);

      if (amr->formattype == FORMAT_VideoInfo2) {
        auto vih = reinterpret_cast<VIDEOINFOHEADER2*>(amr->pbFormat);
        if ((vih->bmiHeader.biWidth > 600) && (vih->bmiHeader.biHeight > 400)) {
          if (vih->bmiHeader.biBitCount > 8) {
            // This is an aceptable combination.
            hr = reader_->SetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM,
                                              nullptr,
                                              mtype.Get());
            if (hr != S_OK)
              throw plx::ComException(__LINE__, hr);
            done = true;
          }
        }
        
      } else {
        // $$$ decode other types like FORMAT_VideoInfo.
      }

      mtype->FreeRepresentation(AM_MEDIA_TYPE_REPRESENTATION, amr);
    }


    // Register the color converter DSP for this process. This will enable the sink writer
    // to find the color converter when the sink writer attempts to match the media types.
    hr = ::MFTRegisterLocalByCLSID(__uuidof(CColorConvertDMO),
                                   MFT_CATEGORY_VIDEO_PROCESSOR, L"",
                                   MFT_ENUM_FLAG_SYNCMFT,
                                   0, nullptr, 0, nullptr);
    if (hr != S_OK)
      throw plx::ComException(__LINE__, hr);

    auto prefix = settings.folder + '\\' + settings.file_prefix;
    file_ = plx::UTF16FromUTF8(plx::RangeFromString(prefix), true);
  }

  void start() {
    if (writer_)
      return;

    auto hr = MFCreateSinkWriterFromURL(
        gen_filename(), nullptr, nullptr, writer_.GetAddressOf());
    if (hr != S_OK)
      throw plx::ComException(__LINE__, hr);

    plx::ComPtr<IMFMediaType> reader_mtype;
    hr = reader_->GetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM,
                                      reader_mtype.GetAddressOf());
    if (hr != S_OK)
      throw plx::ComException(__LINE__, hr);

    plx::ComPtr<IMFMediaType> writer_mtype;
    hr = MFCreateMediaType(writer_mtype.GetAddressOf());
    if (hr != S_OK)
      throw plx::ComException(__LINE__, hr);

    writer_mtype->SetGUID( MF_MT_MAJOR_TYPE, MFMediaType_Video);
    writer_mtype->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264);
    writer_mtype->SetUINT32(MF_MT_AVG_BITRATE, 240 * 1000);
    CopyMFAttribute(MF_MT_FRAME_SIZE, reader_mtype.Get(), writer_mtype.Get());
    CopyMFAttribute(MF_MT_FRAME_RATE, reader_mtype.Get(), writer_mtype.Get());
    CopyMFAttribute(MF_MT_PIXEL_ASPECT_RATIO, reader_mtype.Get(), writer_mtype.Get());
    CopyMFAttribute(MF_MT_INTERLACE_MODE, reader_mtype.Get(), writer_mtype.Get());

    DWORD stream_index;
    hr = writer_->AddStream(writer_mtype.Get(), &stream_index);
    if (hr != S_OK)
      throw plx::ComException(__LINE__, hr);

    hr = writer_->SetInputMediaType(stream_index, reader_mtype.Get(), nullptr);
    if (hr != S_OK)
      throw plx::ComException(__LINE__, hr);

    base_time_ = 0ULL;
    frame_count_ = 0ULL;

    hr = writer_->BeginWriting();
    if (hr != S_OK)
      throw plx::ComException(__LINE__, hr);

    hr = reader_->ReadSample(MF_SOURCE_READER_FIRST_VIDEO_STREAM,
                             0, nullptr, nullptr, nullptr, nullptr);
    if (hr != S_OK)
      throw plx::ComException(__LINE__, hr);
  }

  void stop() {
    auto lock = rw_lock_.write_lock();

    if (!writer_)
      return;

    writer_->Finalize();
    writer_.Reset();
    reader_.Reset();
  }

private:
  HRESULT __stdcall OnReadSample(HRESULT status,
                                 DWORD stream_index,
                                 DWORD stream_flags,
                                 LONGLONG timestamp,
                                 IMFSample *sample) override {
    if (FAILED(status))
      return status;

    HRESULT hr;
    auto lock = rw_lock_.write_lock();

    if (!writer_)
      return S_OK;

    if (sample) {
      ++frame_count_;

      if (!base_time_)
        base_time_ = timestamp;
      
      auto norm_timestamp = timestamp - base_time_;
      sample->SetSampleTime(norm_timestamp);
      hr = writer_->WriteSample(0, sample);
      if (hr != S_OK)
        throw plx::ComException(__LINE__, hr);
    }

    // request the next sample.
    hr = reader_->ReadSample(MF_SOURCE_READER_FIRST_VIDEO_STREAM,
                             0, nullptr, nullptr, nullptr, nullptr);

    return hr;
  };

  HRESULT __stdcall OnEvent(DWORD, IMFMediaEvent *) override {
    return S_OK;
  }

  HRESULT __stdcall OnFlush(DWORD) {
    return S_OK;
  }


  const wchar_t* gen_filename() {
    file_ += L"01.mp4";
    return file_.c_str();
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
    CaptureHandler capture_handler(GetCaptureDevice(), settings);

    capture_handler.start();

    MSG msg = {0};
    while (::GetMessage(&msg, NULL, 0, 0)) {
      ::TranslateMessage(&msg);
      ::DispatchMessage(&msg);
    }

    capture_handler.stop();

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
