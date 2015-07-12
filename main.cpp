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
  bad_format,
  invalid_command,
  plex_error
};

struct AppException {
  HardFailures failure;
  int line;
  AppException(HardFailures failure, int line) : failure(failure), line(line) {}
};

void HardfailMsgBox(HardFailures id, int line) {
  const char* err = nullptr;
  switch (id) {
    case HardFailures::none: err = "none"; break;
    case HardFailures::bad_config: err = "bad config"; break;
    case HardFailures::com_error: err = "com error"; break;
    case HardFailures::no_capture_device: err = "no capture device"; break;
    case HardFailures::bad_format: err = "bad format"; break;
    case HardFailures::invalid_command: err = "invalid command"; break;
    case HardFailures::plex_error: err = "plex error"; break;
    default: err = "(??)"; break;
  }

  auto err_text = plx::StringPrintf("Exception [%s]\nLine: %d", err, line);
  auto full_err = plx::UTF16FromUTF8(plx::RangeFromString(err_text), true);
  ::MessageBox(NULL, full_err.c_str(), L"CamCenter", MB_OK | MB_ICONEXCLAMATION);
}

struct Settings {
  std::string folder;
  int64_t seconds_per_file;
  int64_t average_bitrate;
  int64_t keep_file_count;
  int64_t clean_interval_minutes;
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
  settings.seconds_per_file = config["seconds_per_file"].get_int64();
  settings.average_bitrate = config["average_bitrate"].get_int64();
  settings.keep_file_count = config["keep_file_count"].get_int64();
  settings.clean_interval_minutes = config["clean_interval_minutes"].get_int64();
  return settings;
}

const D2D1_SIZE_F zero_offset = {0};

class DCoWindow : public plx::Window <DCoWindow> {
  // width and height are in logical pixels.
  const int width_;
  const int height_;

  std::function<void()> timer_callback_;

  plx::ComPtr<ID3D11Device> d3d_device_;
  plx::ComPtr<ID2D1Factory2> d2d_factory_;
  plx::ComPtr<ID2D1Device> d2d_device_;
  plx::ComPtr<IDCompositionDesktopDevice> dco_device_;
  plx::ComPtr<IDCompositionTarget> dco_target_;
  plx::ComPtr<IDCompositionVisual2> root_visual_;
  plx::ComPtr<IDCompositionSurface> root_surface_;
  
  enum BrushesMain {
    brush_close,
    brush_drag,
    brush_text,
    brush_last
  };

  plx::D2D1BrushManager brushes_;
  plx::ComPtr<ID2D1Geometry> geom_close_;
  plx::ComPtr<ID2D1Geometry> geom_move_;

public:
  DCoWindow(int width, int height)
      : width_(width), height_(height),
        brushes_(brush_last) {
    create_window(WS_EX_NOREDIRECTIONBITMAP,
                  WS_POPUP | WS_VISIBLE,
                  L"camcenter @ 2015",
                  nullptr, nullptr,
                  10, 10,
                  width_, height_,
                  nullptr,
                  nullptr);
    // do the d2d factory dance.
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

    auto widget_pos = D2D1::Point2F(18.0f, 18.0f);
    geom_close_ = plx::CreateD2D1Geometry(d2d_factory_,
        D2D1::Ellipse(D2D1::Point2F(width_ - 18.0f , 18.0f), 8.0f, 8.0f));

    geom_move_ = plx::CreateD2D1Geometry(d2d_factory_,
        D2D1::RoundedRect(D2D1::RectF(22.0f, 0, width_ - 25.0f, 16), 3.0f, 3.0f));

    {
      plx::ScopedD2D1DeviceContext dc(root_surface_, zero_offset, dpi(), nullptr);
      brushes_.set_solid(dc(), brush_close, 0xBD4B5B, 1.0f);
      brushes_.set_solid(dc(), brush_drag, 0x1E5D81, 0.4f);
      brushes_.set_solid(dc(), brush_text, 0x00AE4A, 1.0f);
    }

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
    dc->FillGeometry(geom_move_.Get(), brushes_.solid(brush_drag));
    dc->DrawGeometry(geom_close_.Get(), brushes_.solid(brush_close), 4.0f);
  }

  void set_timer_callback(int milisecs, std::function<void()> callback) {
    timer_callback_ = callback;
    ::SetTimer(window(), 169, milisecs, nullptr);
  }

  void reset_timer() {
    if (timer_callback_)
      ::KillTimer(window(), 169);
  }

  LRESULT message_handler(const UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
      case WM_DESTROY: {
        reset_timer();
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
      case WM_LBUTTONDOWN: {
        return left_mouse_button_handler(true, MAKEPOINTS(lparam));
      }
      case WM_LBUTTONUP: {
        return left_mouse_button_handler(false, MAKEPOINTS(lparam));
      }
      case WM_TIMER: {
        timer_callback_();
        return 0;
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

  LRESULT left_mouse_button_handler(bool down, POINTS pts) {
    BOOL hit = 0;
    // check the close button.
    geom_close_->FillContainsPoint(
        D2D1::Point2F(pts.x, pts.y), D2D1::Matrix3x2F::Identity(), &hit);
    if (hit != 0) {
      if (!down)
        ::PostQuitMessage(0);
      return 0L;
    }

    if (!down)
      return 0L;

    // Not in the close. check hit for move window widget.
    geom_move_->FillContainsPoint(
        D2D1::Point2F(pts.x, pts.y), D2D1::Matrix3x2F::Identity(), &hit);
    if (hit != 0) {
      ::SendMessageW(window(), WM_SYSCOMMAND, SC_MOVE|0x0002, 0);
    } else {
      // probably on the main text.
    }
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

class VideoCaptureH264 : public plx::ComObject <IMFSourceReaderCallback> {
  plx::ReaderWriterLock rw_lock_;
  plx::ComPtr<IMFSourceReader> reader_;
  plx::ComPtr<IMFSinkWriter> writer_;
  uint32_t avg_bitrate_;
  LONGLONG base_time_;
  LONGLONG frame_count_;

public:
  VideoCaptureH264(plx::ComPtr<IMFMediaSource> source, uint32_t bitrate) 
      : avg_bitrate_(bitrate),
        base_time_(0ULL),
        frame_count_(0ULL) {
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
  }

  void start(const wchar_t* filename) {
    if (writer_)
      throw AppException(HardFailures::invalid_command, __LINE__);

    auto hr = MFCreateSinkWriterFromURL(
        filename, nullptr, nullptr, writer_.GetAddressOf());
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
    writer_mtype->SetUINT32(MF_MT_AVG_BITRATE, avg_bitrate_);
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

  HRESULT __stdcall OnEvent(DWORD, IMFMediaEvent*) override {
    return S_OK;
  }

  HRESULT __stdcall OnFlush(DWORD) {
    return S_OK;
  }
};

plx::File OpenDirectory(plx::FilePath& path) {
  auto dir_par = plx::FileParams::Directory_ShareAll();
  return plx::File::Create(path, dir_par, plx::FileSecurity());
}

bool EnumAndClean(plx::FilesInfo& files, const plx::FilePath& dirname, int64_t keep_count) {
  std::map<long long, plx::Range<wchar_t>> file_map;
  for (files.first(); !files.done(); files.next()) {
    if (files.is_directory())
      continue;
    if (files.file_name().back() == '4') {
      file_map[files.creation_ns1600()] = files.file_name();
    }
  }
  // Figure out how many to delete.
  auto delete_count = plx::To<int64_t>(file_map.size()) - keep_count;
  if (delete_count <= 1)
    return true;

  for (auto& it : file_map) {
    std::wstring name(it.second.start(), it.second.end());
    auto ffn = dirname.append(name);
    if (!::DeleteFile(ffn.raw())) {
      auto gle = ::GetLastError();
      continue;
    }
    // success, adjust the count.
    --delete_count;
    if (!delete_count)
      break;
  }

  return true;
}

void _stdcall DoNothingAPC(ULONG_PTR dwParam) {}

class CaptureManager {
  const Settings settings_;
  uint64_t capture_start_ms_;
  uint32_t capture_count_;
  plx::ComPtr<VideoCaptureH264> capture_;
  std::unique_ptr<std::thread> cleaner_thread_;

public:
  CaptureManager(const Settings& settings) 
      : settings_(settings),
        capture_start_ms_(0ULL),
        capture_count_(0UL) {
    auto bitrate = plx::To<uint32_t>(settings.average_bitrate);
    // validate config.
    if (settings_.seconds_per_file < 10)
      throw AppException(HardFailures::bad_config, __LINE__);
    if (settings_.average_bitrate < 50000)
      throw AppException(HardFailures::bad_config, __LINE__);
    // Open camera and configure capture device.
    capture_ = plx::MakeComObj<VideoCaptureH264>(GetCaptureDevice(), bitrate);
    // configure cleaner thread.
    cleaner_thread_ = std::make_unique<std::thread>(
        &CaptureManager::cleaner_threadproc, settings);
  }

  CaptureManager(const CaptureManager&) = delete;

  ~CaptureManager() {
    if (cleaner_thread_) {
      ::QueueUserAPC(&DoNothingAPC, cleaner_thread_->native_handle(), 0);
      cleaner_thread_->join();
    }
  }

  void start() {
    // configure encoder and start capturing.
    auto file = gen_filename();
    capture_->start(file.c_str());
    capture_start_ms_ = ::GetTickCount64();
    ++capture_count_;
  }

  void stop() {
    if (capture_start_ms_) {
      // stop only destroys the encoder.
      capture_->stop();
      capture_start_ms_ = 0ULL;
    }
  }

  void on_timer() {
    if (!capture_start_ms_)
      return;
    int64_t elapsed_s = (::GetTickCount64() - capture_start_ms_) / 1000ULL;
    if (elapsed_s > settings_.seconds_per_file) {
      // Time to start a new file.
      stop();
      ::Sleep(100);
      start();
    }
  }

private:

  std::wstring gen_filename() {
    SYSTEMTIME st = {0};
    ::GetLocalTime(&st);
    st.wYear -= 2000;

    auto filename = settings_.folder;
    if (st.wHour < 13)
      filename += plx::StringPrintf(
          "\\Y%02d-%02d-%02d-am-%02dh%02dm%02ds.mp4",
          st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    else {
      filename += plx::StringPrintf(
          "\\Y%02d-%02d-%02d-pm-%02dh%02dm%02ds.mp4",
          st.wYear, st.wMonth, st.wDay, st.wHour - 12, st.wMinute, st.wSecond);
    }
    return plx::UTF16FromUTF8(plx::RangeFromString(filename), true);
  }

  static void cleaner_threadproc(const Settings settings) {
    while (true) {
      // alertable sleep.
      auto le = ::SleepEx(10 * 60 * 1000, TRUE);
      if (le != 0)
        break;
      // timed out. Proceed to clean.
      plx::FilePath path(std::wstring(settings.folder.begin(), settings.folder.end()));
      auto dir = OpenDirectory(path);
      if (dir.status() != (plx::File::directory | plx::File::existing))
        continue;
      EnumAndClean(plx::FilesInfo::FromDir(dir), path, settings.keep_file_count);
    }
  }

};

int __stdcall wWinMain(HINSTANCE instance, HINSTANCE,
                       wchar_t* cmdline, int cmd_show) {
  ::CoInitializeEx(NULL, COINIT_MULTITHREADED | COINIT_DISABLE_OLE1DDE);

  try {
    auto settings = LoadSettings();
    DCoWindow window(300, 200);

    MediaFoundationInit mf_init;
    CaptureManager capture_manager(LoadSettings());

    capture_manager.start();
    window.set_timer_callback(
        1000, std::bind(&CaptureManager::on_timer, &capture_manager));

    MSG msg = {0};
    while (::GetMessage(&msg, NULL, 0, 0)) {
      ::TranslateMessage(&msg);
      ::DispatchMessage(&msg);
    }

    window.reset_timer();
    capture_manager.stop();
    // Exit program.
    return (int) msg.wParam;

  } catch (plx::ComException& ex) {
    HardfailMsgBox(HardFailures::com_error, ex.Line());
    return 1;
  } catch (plx::Exception& ex) {
    HardfailMsgBox(HardFailures::plex_error, ex.Line());
    return 2;
  } catch (AppException& ex) {
    HardfailMsgBox(ex.failure, ex.line);
    return 3;
  }
}
