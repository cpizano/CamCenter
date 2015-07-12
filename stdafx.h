// This is the plex precompiled header, not the same as the VC precompiled header.

#pragma once
#define NOMINMAX

#include <SDKDDKVer.h>





#include <objbase.h>
#include <shellscalingapi.h>
#include <mfidl.h>
#include <mfapi.h>
#include <shlobj.h>
#include <dshow.h>
#include <wmcodecdsp.h>
#include <uuids.h>
#include <d2d1_2.h>
#include <d3d11_2.h>
#include <dcomp.h>
#include <mferror.h>
#include <wrl.h>
#include <dvdmedia.h>
#include <string.h>
#include <array>
#include <functional>
#include <initializer_list>
#include <cctype>
#include <iterator>
#include <list>
#include <memory>
#include <map>
#include <algorithm>
#include <utility>
#include <limits>
#include <type_traits>
#include <string>
#include <thread>
#include <vector>
#include <stdint.h>
#include <stdarg.h>

const int plex_vista_support = 1;
#include <windows.h>








///////////////////////////////////////////////////////////////////////////////
// plx::Exception
// line_ : The line of code, usually __LINE__.
// message_ : Whatever useful text.
//
namespace plx {
class Exception {
  int line_;
  const char* message_;

protected:
  void PostCtor() {
    if (::IsDebuggerPresent()) {
      //__debugbreak();
    }
  }

public:
  Exception(int line, const char* message) : line_(line), message_(message) {}
  virtual ~Exception() {}
  const char* Message() const { return message_; }
  int Line() const { return line_; }
};


///////////////////////////////////////////////////////////////////////////////
// plx::ComException (thrown when COM fails)
//
class ComException : public plx::Exception {
  HRESULT hr_;

public:
  ComException(int line, HRESULT hr)
      : Exception(line, "COM failure"), hr_(hr) {
    PostCtor();
  }

  HRESULT hresult() const {
    return hr_;
  }

};


///////////////////////////////////////////////////////////////////////////////
// plx::ComObject : helps implement COM objects.
//
template <typename... T> using ComObject =
    Microsoft::WRL::RuntimeClass <
        Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>, T...>;

template <typename T, typename... Args>
Microsoft::WRL::ComPtr <T> MakeComObj(Args&&... args) {
  return Microsoft::WRL::Make<T>(std::forward<Args>(args)...);
}


///////////////////////////////////////////////////////////////////////////////
// plx::ComPtr : smart COM pointer.
//
template <typename T> using ComPtr = Microsoft::WRL::ComPtr <T>;


///////////////////////////////////////////////////////////////////////////////
// plx::CreateDCoDevice2 : DirectComposition Desktop Device.
//

plx::ComPtr<IDCompositionDesktopDevice> CreateDCoDevice2(
    plx::ComPtr<ID2D1Device> device2D) ;


///////////////////////////////////////////////////////////////////////////////
// plx::DPI
//
//  96 DPI = 1.00 scaling
// 120 DPI = 1.25 scaling
// 144 DPI = 1.50 scaling
// 192 DPI = 2.00 scaling
class DPI {
  float scale_x_;
  float scale_y_;
  unsigned int dpi_x_;
  unsigned int dpi_y_;

public:
  DPI() : scale_x_(1.0f), scale_y_(1.0f), dpi_x_(96), dpi_y_(96) {
  }

  void set_from_monitor(HMONITOR monitor) {
    unsigned int dpi_x, dpi_y;
    auto hr = ::GetDpiForMonitor(monitor, MDT_EFFECTIVE_DPI, &dpi_x, &dpi_y);
    if (hr != S_OK)
      throw plx::ComException(__LINE__, hr);
    set_dpi(dpi_x, dpi_y);
  }

  void set_from_screen(int x, int y) {
    POINT point = {x, y};
    auto monitor = ::MonitorFromPoint(point, MONITOR_DEFAULTTONEAREST);
    set_from_monitor(monitor);
  }

  void set_dpi(unsigned int dpi_x, unsigned int dpi_y) {
    dpi_x_ = dpi_x;
    dpi_y_ = dpi_y;
    scale_x_ = dpi_x_ / 96.0f;
    scale_y_ = dpi_y_ / 96.0f;
  }

  bool isomorphic_scale() const { return (scale_x_ == scale_y_); }

  float get_scale_x() const { return scale_x_; }

  float get_scale_y() const { return scale_y_; }

  unsigned int get_dpi_x() const { return dpi_x_; }

  unsigned int get_dpi_y() const { return dpi_y_; }

  template <typename T>
  float to_logical_x(const T physical_pix) const {
    return physical_pix / scale_x_;
  }

  template <typename T>
  float to_logical_y(const T physical_pix) const {
    return physical_pix / scale_y_;
  }

  template <typename T>
  float to_physical_x(const T logical_pix) const {
    return logical_pix * scale_x_;
  }

  template <typename T>
  float to_physical_y(const T logical_pix) const {
    return logical_pix * scale_y_;
  }

};


///////////////////////////////////////////////////////////////////////////////
// plx::CreateDCoSurface : Makes a DirectComposition compatible surface.
//

plx::ComPtr<IDCompositionSurface> CreateDCoSurface(
    plx::ComPtr<IDCompositionDesktopDevice> device, unsigned int w, unsigned int h) ;


///////////////////////////////////////////////////////////////////////////////
// plx::CreateDCoVisual : Makes a DirectComposition Visual.
//

plx::ComPtr<IDCompositionVisual2> CreateDCoVisual(plx::ComPtr<IDCompositionDesktopDevice> device) ;


///////////////////////////////////////////////////////////////////////////////
// plx::CreateDCoWindowTarget : Makes a DirectComposition target for a window.
//

plx::ComPtr<IDCompositionTarget> CreateDCoWindowTarget(
    plx::ComPtr<IDCompositionDesktopDevice> device, HWND window) ;



///////////////////////////////////////////////////////////////////////////////
// plx::CreateD2D1FactoryST : Direct2D fatory.
// plx::CreateDeviceD2D1 : Direct2D device.
//

plx::ComPtr<ID2D1Factory2> CreateD2D1FactoryST(D2D1_DEBUG_LEVEL debug_level) ;

plx::ComPtr<ID2D1Device> CreateDeviceD2D1(plx::ComPtr<ID3D11Device> device3D,
                                          plx::ComPtr<ID2D1Factory2> factoryD2D1) ;


///////////////////////////////////////////////////////////////////////////////
// plx::CreateDeviceD3D11 : Direct3D device fatory.
//

plx::ComPtr<ID3D11Device> CreateDeviceD3D11(int extra_flags) ;


///////////////////////////////////////////////////////////////////////////////
// plx::CreateDCoDeviceCtx : Makes a DirectComposition DC for drawing.
//

plx::ComPtr<ID2D1DeviceContext> CreateDCoDeviceCtx(
    plx::ComPtr<IDCompositionSurface> surface,
    const plx::DPI& dpi, const D2D1_SIZE_F& extra_offset) ;


///////////////////////////////////////////////////////////////////////////////
// plx::RangeException (thrown by ItRange and others)
//
class RangeException : public plx::Exception {
  void* ptr_;
public:
  RangeException(int line, void* ptr)
      : Exception(line, "Invalid Range"), ptr_(ptr) {
    PostCtor();
  }

  void* pointer() const {
    return ptr_;
  }
};




///////////////////////////////////////////////////////////////////////////////
// plx::ItRange
// s_ : first element
// e_ : one past the last element
//
template <typename It>
class ItRange {
  It s_;
  It e_;

public:
  typedef typename std::iterator_traits<
      typename std::remove_reference<It>::type
  >::reference RefT;

  typedef typename std::iterator_traits<
      typename std::remove_reference<It>::type
  >::value_type ValueT;

  typedef typename std::remove_const<It>::type NoConstIt;

  ItRange() : s_(), e_() {
  }

  template <typename U>
  ItRange(const ItRange<U>& other) : s_(other.start()), e_(other.end()) {
  }

  ItRange(It start, It end) : s_(start), e_(end) {
  }

  ItRange(It start, size_t size) : s_(start), e_(start + size) {
  }

  bool empty() const {
    return (s_ == e_);
  }

  size_t size() const {
    return (e_ - s_);
  }

  It start() const {
    return s_;
  }

  It begin() const {
    return s_;
  }

  It end() const {
    return e_;
  }

  bool valid() const {
    return (e_ >= s_);
  }

  RefT front() const {
    if (s_ >= e_)
      throw plx::RangeException(__LINE__, nullptr);
    return s_[0];
  }

  RefT back() const {
    return e_[-1];
  }

  RefT operator[](size_t i) const {
    return s_[i];
  }

  bool equals(const ItRange<It>& o) const {
    if (o.size() != size())
      return false;
    return (memcmp(s_, o.s_, size()) == 0);
  }

  size_t starts_with(const ItRange<It>& o) const {
    if (o.size() > size())
      return 0;
    return (memcmp(s_, o.s_, o.size()) == 0) ? o.size() : 0;
  }

  bool contains(const uint8_t* ptr) const {
    return ((ptr >= reinterpret_cast<uint8_t*>(s_)) &&
            (ptr < reinterpret_cast<uint8_t*>(e_)));
  }

  bool contains(ValueT x, size_t* pos) const {
    auto c = s_;
    while (c != e_) {
      if (*c == x) {
        *pos = c - s_;
        return true;
      }
      ++c;
    }
    return false;
  }

  template <size_t count>
  size_t CopyToArray(ValueT (&arr)[count]) const {
    auto copied = std::min(size(), count);
    auto last = copied + s_;
    std::copy(s_, last, arr);
    return copied;
  }

  template <size_t count>
  size_t CopyToArray(std::array<ValueT, count>& arr) const {
    auto copied = std::min(size(), count);
    auto last = copied + s_;
    std::copy(s_, last, arr.begin());
    return copied;
  }

  intptr_t advance(size_t count) {
    auto ns = s_ + count;
    if (ns > e_)
      return (e_ - ns);
    s_ = ns;
    return size();
  }

  void clear() {
    s_ = It();
    e_ = It();
  }

  void reset_start(It new_start) {
    auto sz = size();
    s_ = new_start;
    e_ = s_ + sz;
  }

  void extend(size_t count) {
    e_ += count;
  }

  ItRange<const uint8_t*> const_bytes() const {
    auto s = reinterpret_cast<const uint8_t*>(s_);
    auto e = reinterpret_cast<const uint8_t*>(e_);
    return ItRange<const uint8_t*>(s, e);
  }

  ItRange<uint8_t*> bytes() const {
    auto s = reinterpret_cast<uint8_t*>(s_);
    auto e = reinterpret_cast<uint8_t*>(e_);
    return ItRange<uint8_t*>(s, e);
  }

  ItRange<It> slice(size_t start, size_t count = 0) const {
    return ItRange<It>(s_ + start,
                       count ? (s_ + start + count) : e_ );
  }

};

template <typename U, size_t count>
ItRange<U*> RangeFromLitStr(U (&str)[count]) {
  return ItRange<U*>(str, str + count - 1);
}

template <typename U, size_t count>
ItRange<U*> RangeFromArray(U (&str)[count]) {
  return ItRange<U*>(str, str + count);
}

template <typename U>
ItRange<U*> RangeUntilValue(U* start, U value) {
  auto stop = start;
  while (*stop != value) {
    ++stop;
  }
  return ItRange<U*>(start, stop);
}

template <typename U>
ItRange<U*> RangeFromVector(std::vector<U>& vec, size_t len = 0) {
  auto s = &vec[0];
  return ItRange<U*>(s, len ? s + len : s + vec.size());
}

template <typename U>
ItRange<const U*> RangeFromVector(const std::vector<U>& vec, size_t len = 0) {
  auto s = &vec[0];
  return ItRange<const U*>(s, len ? s + len : s + vec.size());
}

ItRange<uint8_t*> RangeFromBytes(void* start, size_t count) ;

ItRange<const uint8_t*> RangeFromBytes(const void* start, size_t count) ;

ItRange<const uint8_t*> RangeFromString(const std::string& str) ;

ItRange<uint8_t*> RangeFromString(std::string& str) ;

ItRange<const uint16_t*> RangeFromString(const std::wstring& str) ;

ItRange<uint16_t*> RangeFromString(std::wstring& str) ;

template <typename U>
std::string StringFromRange(const ItRange<U>& r) {
  return std::string(r.start(), r.end());
}

template <typename U>
std::wstring WideStringFromRange(const ItRange<U>& r) {
  return std::wstring(r.start(), r.end());
}

template <typename U>
std::unique_ptr<U[]> HeapRange(ItRange<U*>&r) {
  std::unique_ptr<U[]> ptr(new U[r.size()]);
  r.reset_start(ptr.get());
  return ptr;
}


///////////////////////////////////////////////////////////////////////////////
// plx::IOException
// error_code_ : The win32 error code of the last operation.
// name_ : The file or pipe in question.
//
class IOException : public plx::Exception {
  DWORD error_code_;
  const std::wstring name_;

public:
  IOException(int line, const wchar_t* name)
      : Exception(line, "IO problem"),
        error_code_(::GetLastError()),
        name_(name) {
    PostCtor();
  }
  DWORD ErrorCode() const { return error_code_; }
  const wchar_t* Name() const { return name_.c_str(); }
};


///////////////////////////////////////////////////////////////////////////////
// plx::InvalidParamException
// parameter_ : the position of the offending parameter, zero if unknown.
//
class InvalidParamException : public plx::Exception {
  int parameter_;

public:
  InvalidParamException(int line, int parameter)
      : Exception(line, "Invalid parameter"), parameter_(parameter) {
    PostCtor();
  }
  int Parameter() const { return parameter_; }
};


///////////////////////////////////////////////////////////////////////////////
// plx::FilePath
// path_ : The actual path (ucs16 probably).
//
class FilePath {
private:
  std::wstring path_;
  friend class File;

public:
  explicit FilePath(const wchar_t* path)
    : path_(path) {
  }

  explicit FilePath(const std::wstring& path)
    : path_(path) {
  }

  FilePath parent() const {
    auto pos = path_.find_last_of(L'\\');
    if (pos == std::string::npos)
      return FilePath();
    return FilePath(path_.substr(0, pos));
  }

  std::wstring leaf() const {
    auto pos = path_.find_last_of(L'\\');
    if (pos == std::string::npos) {
      return is_drive() ? std::wstring() : path_;
    }
    return path_.substr(pos + 1);
  }

  FilePath append(const std::wstring& name) const {
    if (name.empty())
      throw plx::IOException(__LINE__, path_.c_str());

    std::wstring full(path_);
    if (!path_.empty())
      full.append(1, L'\\');
    full.append(name);
    return FilePath(full);
  }

  bool is_drive() const {
    return (path_.size() != 2) ? false : drive_impl();
  }

  bool has_drive() const {
    return (path_.size() < 2) ? false : drive_impl();
  }

  const wchar_t* raw() const {
    return path_.c_str();
  }

private:
  FilePath() {}

  bool drive_impl() const {
    if (path_[1] != L':')
      return false;
    auto dl = path_[0];
    if ((dl >= L'A') && (dl <= L'Z'))
      return true;
    if ((dl >= L'a') && (dl <= L'z'))
      return true;
    return false;
  }
};


///////////////////////////////////////////////////////////////////////////////
// plx::FileSecurity
//

///////////////////////////////////////////////////////////////////////////////
//
class FileSecurity {
private:
  SECURITY_ATTRIBUTES* sattr_;
  friend class File;
public:
  FileSecurity()
    : sattr_(nullptr) {
  }
};


///////////////////////////////////////////////////////////////////////////////
// plx::Range  (alias for ItRange<T*>)
//
template <typename T>
using Range = plx::ItRange<T*>;


///////////////////////////////////////////////////////////////////////////////
// plx::GetAppDataPath
//
plx::FilePath GetAppDataPath(bool roaming) ;



///////////////////////////////////////////////////////////////////////////////
// HexASCII (converts a byte into a two-char readable representation.
//
static const char HexASCIITable[] =
    { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };

char* HexASCII(uint8_t byte, char* out) ;


///////////////////////////////////////////////////////////////////////////////
std::string HexASCIIStr(const plx::Range<const uint8_t>& r, char separator) ;


///////////////////////////////////////////////////////////////////////////////
// plx::FileParams
// access_  : operations allowed to the file.
// sharing_ : operations others can do to the file.
// disposition_ : if the file is reset or opened as is.
// flags_ : hints or special modes go here.
//
class FileParams {
private:
  DWORD access_;
  DWORD sharing_;
  DWORD disposition_;
  DWORD attributes_;  // For existing files these are generally ignored.
  DWORD flags_;       // For existing files these are generally combined.
  DWORD sqos_;
  friend class File;

public:
  static const DWORD kShareNone = 0;
  static const DWORD kShareAll = FILE_SHARE_DELETE |
                                 FILE_SHARE_READ |
                                 FILE_SHARE_WRITE;
  FileParams()
    : access_(0),
      sharing_(kShareAll),
      disposition_(OPEN_EXISTING),
      attributes_(FILE_ATTRIBUTE_NORMAL),
      flags_(0),
      sqos_(0) {
  }

  FileParams(DWORD access,
             DWORD sharing,
             DWORD disposition,
             DWORD attributes,
             DWORD flags,
             DWORD sqos)
    : access_(access),
      sharing_(sharing),
      disposition_(disposition),
      attributes_(attributes),
      flags_(flags),
      sqos_(sqos) {
    // Don't allow generic access masks.
    if (access & 0xF0000000)
      throw plx::InvalidParamException(__LINE__, 1);
  }

  bool can_modify() const {
    return (access_ & (FILE_WRITE_DATA |
                       FILE_WRITE_ATTRIBUTES |
                       FILE_WRITE_EA |
                       FILE_APPEND_DATA)) != 0;
  }

  bool exclusive() const {
    return (sharing_ == 0);
  }

  static FileParams Append_SharedRead() {
    return FileParams(FILE_APPEND_DATA, FILE_SHARE_READ,
                      OPEN_ALWAYS,
                      FILE_ATTRIBUTE_NORMAL, 0, 0);
  }

  static FileParams Read_SharedRead() {
    return FileParams(FILE_GENERIC_READ, FILE_SHARE_READ,
                      OPEN_EXISTING,
                      FILE_ATTRIBUTE_NORMAL, 0, 0);
  }

  static FileParams ReadWrite_SharedRead(DWORD disposition) {
    return FileParams(FILE_GENERIC_READ | FILE_GENERIC_WRITE, FILE_SHARE_READ,
                      disposition,
                      FILE_ATTRIBUTE_NORMAL, 0, 0);
  }

  static FileParams Directory_ShareAll() {
    return FileParams(FILE_GENERIC_READ, kShareAll,
                     OPEN_EXISTING,
                     0, FILE_FLAG_BACKUP_SEMANTICS, 0);
  }
};


///////////////////////////////////////////////////////////////////////////////
// plx::File
//
class File {
  HANDLE handle_;
  unsigned int  status_;
  friend class FilesInfo;

private:
  File(HANDLE handle,
       unsigned int status)
    : handle_(handle),
      status_(status) {
  }

  File() = delete;
  File(const File&) = delete;
  File& operator=(const File&) = delete;

public:
  enum Status {
    none              = 0,
    brandnew          = 1,   //
    existing          = 2,
    delete_on_close   = 4,
    directory         = 8,   // $$$ just use backup semantics?
    exclusive         = 16,
    readonly          = 32,
    information       = 64,
  };

  File(File&& file)
    : handle_(INVALID_HANDLE_VALUE),
      status_(none) {
    std::swap(file.handle_, handle_);
    std::swap(file.status_, status_);
  }

  static File Create(const plx::FilePath& path,
                     const plx::FileParams& params,
                     const plx::FileSecurity& security) {
    HANDLE file = ::CreateFileW(path.path_.c_str(),
                                params.access_,
                                params.sharing_,
                                security.sattr_,
                                params.disposition_,
                                params.attributes_ | params.flags_ | params.sqos_,
                                0);
    DWORD gle = ::GetLastError();
    unsigned int status = none;

    if (file != INVALID_HANDLE_VALUE) {

      switch (params.disposition_) {
        case OPEN_EXISTING:
        case TRUNCATE_EXISTING:
          status |= existing; break;
        case CREATE_NEW:
          status |= (gle == ERROR_FILE_EXISTS) ? existing : brandnew; break;
        case CREATE_ALWAYS:
        case OPEN_ALWAYS:
          status |= (gle == ERROR_ALREADY_EXISTS) ? existing : brandnew; break;
        default:
          __debugbreak();
      };

      if (params.flags_ == FILE_FLAG_BACKUP_SEMANTICS) status |= directory;
      if (params.flags_ & FILE_FLAG_DELETE_ON_CLOSE) status |= delete_on_close;
      if (params.sharing_ == 0) status |= exclusive;
      if (params.access_ == 0) status |= information;
      if (params.access_ == GENERIC_READ) status |= readonly;
    }

    return File(file, status);
  }

  ~File() {
    if (handle_ != INVALID_HANDLE_VALUE) {
      if (!::CloseHandle(handle_)) {
        throw IOException(__LINE__, nullptr);
      }
    }
  }

  // $$ this ignores the volume id, so technically incorrect.
  long long get_unique_id() {
    BY_HANDLE_FILE_INFORMATION bhfi;
    if (!::GetFileInformationByHandle(handle_, &bhfi))
      throw IOException(__LINE__, nullptr);
    LARGE_INTEGER li = { bhfi.nFileIndexLow, bhfi.nFileIndexHigh };
    return li.QuadPart;
  }

  unsigned int status() const {
    return status_;
  }

  bool is_valid() const {
    return (handle_ != INVALID_HANDLE_VALUE);
  }

  long long size_in_bytes() const {
    LARGE_INTEGER li = {0};
    ::GetFileSizeEx(handle_, &li);
    return li.QuadPart;
  }

  size_t read(plx::Range<uint8_t>& mem, unsigned int from = -1) {
    return read(mem.start(), mem.size(), from);
  }

  size_t read(uint8_t* buf, size_t len, int from) {
    OVERLAPPED ov = {0};
    ov.Offset = from;
    DWORD read = 0;
    if (!::ReadFile(handle_, buf, static_cast<DWORD>(len),
                    &read, (from < 0) ? nullptr : &ov))
      return 0;
    return read;
  }

  size_t write(const plx::Range<const uint8_t>& mem, int from = -1) {
    return write(mem.start(), mem.size(), from);
  }

  size_t write(const uint8_t* buf, size_t len, int from) {
    OVERLAPPED ov = {0};
    ov.Offset = from;
    DWORD written = 0;
    if (!::WriteFile(handle_, buf, static_cast<DWORD>(len),
                     &written, (from < 0) ? nullptr : &ov))
      return 0;
    return written;
  }
};


///////////////////////////////////////////////////////////////////////////////
// plx::CodecException (thrown by some decoders)
// bytes_ : The 16 bytes or less that caused the issue.
//
class CodecException : public plx::Exception {
  uint8_t bytes_[16];
  size_t count_;

public:
  CodecException(int line, const plx::Range<const unsigned char>* br)
      : Exception(line, "Codec exception"), count_(0) {
    if (br)
      count_ = br->CopyToArray(bytes_);
    PostCtor();
  }

  std::string bytes() const {
    return plx::HexASCIIStr(plx::Range<const uint8_t>(bytes_, count_), ',');
  }
};


///////////////////////////////////////////////////////////////////////////////
// plx::JsonException
//
class JsonException : public plx::Exception {

public:
  JsonException(int line)
      : Exception(line, "Json exception") {
    PostCtor();
  }
};


///////////////////////////////////////////////////////////////////////////////
// plx::JsonType
//
enum class JsonType {
  NULLT,
  BOOL,
  INT64,
  DOUBLE,
  ARRAY,
  OBJECT,
  STRING,
};


///////////////////////////////////////////////////////////////////////////////
// plx::JsonValue
// type_ : the actual type from the Data union.
// u_ : the storage for all the possible values.
template <typename T> using AligedStore =
    std::aligned_storage<sizeof(T), __alignof(T)>;

class JsonValue {
  //typedef std::unordered_map<std::string, JsonValue> ObjectImpl;
  typedef std::map<std::string, JsonValue> ObjectImpl;
  typedef std::vector<JsonValue> ArrayImpl;
  typedef std::string StringImpl;

  plx::JsonType type_;
  union Data {
    bool bolv;
    double dblv;
    int64_t intv;
    AligedStore<StringImpl>::type str;
    AligedStore<ArrayImpl>::type arr;
    AligedStore<ObjectImpl>::type obj;
  } u_;

 public:
  typedef ObjectImpl::const_iterator KeyValueIterator;

  JsonValue() : type_(JsonType::NULLT) {
  }

  JsonValue(const plx::JsonType& type) : type_(type) {
    if (type_ == JsonType::ARRAY)
      new (&u_.arr) ArrayImpl;
    else if (type_ == JsonType::OBJECT)
      new (&u_.obj) ObjectImpl();
    else
      throw plx::InvalidParamException(__LINE__, 1);
  }

  JsonValue(const JsonValue& other) : type_(JsonType::NULLT) {
    *this = other;
  }

  JsonValue(JsonValue&& other) : type_(JsonType::NULLT) {
    *this = std::move(other);
  }

  ~JsonValue() {
    Destroy();
  }

  JsonValue(nullptr_t) : type_(JsonType::NULLT) {
  }

  JsonValue(bool b) : type_(JsonType::BOOL) {
    u_.bolv = b;
  }

  JsonValue(int v) : type_(JsonType::INT64) {
    u_.intv = v;
  }

  JsonValue(int64_t v) : type_(JsonType::INT64) {
    u_.intv = v;
  }

  JsonValue(double v) : type_(JsonType::DOUBLE) {
    u_.dblv = v;
  }

  JsonValue(const std::string& s) : type_(JsonType::STRING) {
    new (&u_.str) std::string(s);
  }

  JsonValue(const char* s) : type_(JsonType::STRING) {
    new (&u_.str) StringImpl(s);
  }

  JsonValue(std::initializer_list<JsonValue> il) : type_(JsonType::ARRAY) {
    new (&u_.arr) ArrayImpl(il.begin(), il.end());
  }

  template<class It>
  JsonValue(It first, It last) : type_(JsonType::ARRAY) {
    new (&u_.arr) ArrayImpl(first, last);
  }

  JsonValue& operator=(const JsonValue& other) {
    if (this != &other) {
      Destroy();

      if (other.type_ == JsonType::BOOL)
        u_.bolv = other.u_.bolv;
      else if (other.type_ == JsonType::INT64)
        u_.intv = other.u_.intv;
      else if (other.type_ == JsonType::DOUBLE)
        u_.dblv = other.u_.dblv;
      else if (other.type_ == JsonType::ARRAY)
        new (&u_.arr) ArrayImpl(*other.GetArray());
      else if (other.type_ == JsonType::OBJECT)
        new (&u_.obj) ObjectImpl(*other.GetObject());
      else if (other.type_ == JsonType::STRING)
        new (&u_.str) StringImpl(*other.GetString());

      type_ = other.type_;
    }
    return *this;
  }

  JsonValue& operator=(JsonValue&& other) {
    if (this != &other) {
      Destroy();

      if (other.type_ == JsonType::BOOL)
        u_.bolv = other.u_.bolv;
      else if (other.type_ == JsonType::INT64)
        u_.intv = other.u_.intv;
      else if (other.type_ == JsonType::DOUBLE)
        u_.dblv = other.u_.dblv;
      else if (other.type_ == JsonType::ARRAY)
        new (&u_.arr) ArrayImpl(std::move(*other.GetArray()));
      else if (other.type_ == JsonType::OBJECT)
        new (&u_.obj) ObjectImpl(std::move(*other.GetObject()));
      else if (other.type_ == JsonType::STRING)
        new (&u_.str) StringImpl(std::move(*other.GetString()));

      type_ = other.type_;
    }
    return *this;
  }

  JsonValue& operator[](const std::string& s) {
    return (*GetObject())[s];
  }

  JsonValue& operator[](size_t ix) {
    return (*GetArray())[ix];
  }

  plx::JsonType type() const {
    return type_;
  }

  bool get_bool() const {
    return u_.bolv;
  }

  int64_t get_int64() const {
    return u_.intv;
  }

  double get_double() const {
    return u_.dblv;
  }

  std::string get_string() const {
    return *GetString();
  }

  bool has_key(const std::string& k) const {
    return (GetObject()->find(k) != end(*GetObject()));
  }

  std::pair<KeyValueIterator, KeyValueIterator>  get_iterator() const {
    return std::make_pair(GetObject()->begin(), GetObject()->end());
  }

  void push_back(JsonValue&& value) {
    GetArray()->push_back(value);
  }

  size_t size() const {
   if (type_ == JsonType::ARRAY)
      return GetArray()->size();
    else if (type_ == JsonType::OBJECT)
      return GetObject()->size();
    else return 0;
  }

 private:

  ObjectImpl* GetObject() {
    if (type_ != JsonType::OBJECT)
      throw plx::JsonException(__LINE__);
    void* addr = &u_.obj;
    return reinterpret_cast<ObjectImpl*>(addr);
  }

  const ObjectImpl* GetObject() const {
    if (type_ != JsonType::OBJECT)
      throw plx::JsonException(__LINE__);
    const void* addr = &u_.obj;
    return reinterpret_cast<const ObjectImpl*>(addr);
  }

  ArrayImpl* GetArray() {
    if (type_ != JsonType::ARRAY)
      throw plx::JsonException(__LINE__);
    void* addr = &u_.arr;
    return reinterpret_cast<ArrayImpl*>(addr);
  }

  const ArrayImpl* GetArray() const {
    if (type_ != JsonType::ARRAY)
      throw plx::JsonException(__LINE__);
    const void* addr = &u_.arr;
    return reinterpret_cast<const ArrayImpl*>(addr);
  }

  std::string* GetString() {
    if (type_ != JsonType::STRING)
      throw plx::JsonException(__LINE__);
    void* addr = &u_.str;
    return reinterpret_cast<StringImpl*>(addr);
  }

  const std::string* GetString() const {
    if (type_ != JsonType::STRING)
      throw plx::JsonException(__LINE__);
    const void* addr = &u_.str;
    return reinterpret_cast<const StringImpl*>(addr);
  }

  void Destroy() {
    if (type_ == JsonType::ARRAY)
      GetArray()->~ArrayImpl();
    else if (type_ == JsonType::OBJECT)
      GetObject()->~ObjectImpl();
    else if (type_ == JsonType::STRING)
      GetString()->~StringImpl();
  }

};


///////////////////////////////////////////////////////////////////////////////
// plx::OverflowKind
//
enum class OverflowKind {
  None,
  Positive,
  Negative,
};


///////////////////////////////////////////////////////////////////////////////
// plx::LinkedBuffers
//
class LinkedBuffers {
  struct Item {
    size_t size;
    std::unique_ptr<uint8_t[]> data;
    Item(size_t size)
        : size(size), data(new uint8_t[size]) {
    }

    Item(const Item& other)
        : size(other.size), data(new uint8_t[size]) {
      memcpy(data.get(), other.data.get(), size);
    }

    Item(Item&& other) = delete;
  };

  typedef std::list<Item> BList;

  BList buffers_;
  BList::iterator loop_it_;

public:
  LinkedBuffers() {
  }

  LinkedBuffers(const LinkedBuffers& other)
      : buffers_(other.buffers_) {
  }

  LinkedBuffers(LinkedBuffers&& other) {
    buffers_.swap(other.buffers_);
    std::swap(loop_it_, other.loop_it_);
  }

  plx::Range<uint8_t> new_buffer(size_t size_bytes) {
    buffers_.emplace_back(size_bytes);
    auto start = &(buffers_.back().data)[0];
    return plx::Range<uint8_t>(start, start + size_bytes);
  }

  void remove_last_buffer() {
    buffers_.pop_back();
  }

  void first() {
    loop_it_ = begin(buffers_);
  }

  void next() {
    ++loop_it_;
  }

  bool done() {
    return (loop_it_ == end(buffers_));
  }

  plx::Range<uint8_t> get() {
    auto start = &(loop_it_->data)[0];
    return plx::Range<uint8_t>(start, start + loop_it_->size);
  }
};


///////////////////////////////////////////////////////////////////////////////
// plx::OverflowException (thrown by some numeric converters)
// kind_ : Type of overflow, positive or negative.
//
class OverflowException : public plx::Exception {
  plx::OverflowKind kind_;

public:
  OverflowException(int line, plx::OverflowKind kind)
      : Exception(line, "Overflow"), kind_(kind) {
    PostCtor();
  }
  plx::OverflowKind kind() const { return kind_; }
};


///////////////////////////////////////////////////////////////////////////////
// plx::NextInt  integer promotion.

short NextInt(char value) ;

int NextInt(short value) ;

long long NextInt(int value) ;

long long NextInt(long value) ;

long long NextInt(long long value) ;

short NextInt(unsigned char value) ;

int NextInt(unsigned short value) ;

long long NextInt(unsigned int value) ;

long long NextInt(unsigned long value) ;

long long NextInt(unsigned long long value) ;


///////////////////////////////////////////////////////////////////////////////
// plx::To  (integer to integer type safe cast)
//

template <bool src_signed, bool tgt_signed>
struct ToCastHelper;

template <>
struct ToCastHelper<false, false> {
  template <typename Tgt, typename Src>
  static inline Tgt cast(Src value) {
    if (sizeof(Tgt) >= sizeof(Src)) {
      return static_cast<Tgt>(value);
    } else {
      if (value > std::numeric_limits<Tgt>::max())
        throw plx::OverflowException(__LINE__, OverflowKind::Positive);
      if (value < std::numeric_limits<Tgt>::min())
        throw plx::OverflowException(__LINE__, OverflowKind::Negative);
      return static_cast<Tgt>(value);
    }
  }
};

template <>
struct ToCastHelper<true, true> {
  template <typename Tgt, typename Src>
  static inline Tgt cast(Src value) {
    if (sizeof(Tgt) >= sizeof(Src)) {
      return static_cast<Tgt>(value);
    } else {
      if (value > std::numeric_limits<Tgt>::max())
        throw plx::OverflowException(__LINE__, OverflowKind::Positive);
      if (value < std::numeric_limits<Tgt>::min())
        throw plx::OverflowException(__LINE__, OverflowKind::Negative);
      return static_cast<Tgt>(value);
    }
  }
};

template <>
struct ToCastHelper<false, true> {
  template <typename Tgt, typename Src>
  static inline Tgt cast(Src value) {
    if (plx::NextInt(value) > std::numeric_limits<Tgt>::max())
      throw plx::OverflowException(__LINE__, OverflowKind::Positive);
    if (plx::NextInt(value) < std::numeric_limits<Tgt>::min())
      throw plx::OverflowException(__LINE__, OverflowKind::Negative);
    return static_cast<Tgt>(value);
  }
};

template <>
struct ToCastHelper<true, false> {
  template <typename Tgt, typename Src>
  static inline Tgt cast(Src value) {
    if (value < Src(0))
      throw plx::OverflowException(__LINE__, OverflowKind::Negative);
    if (unsigned(value) > std::numeric_limits<Tgt>::max())
      throw plx::OverflowException(__LINE__, OverflowKind::Positive);
    return static_cast<Tgt>(value);
  }
};

template <typename Tgt, typename Src>
typename std::enable_if<
    std::numeric_limits<Tgt>::is_integer &&
    std::numeric_limits<Src>::is_integer,
    Tgt>::type
To(const Src & value) {
  return ToCastHelper<std::numeric_limits<Src>::is_signed,
                      std::numeric_limits<Tgt>::is_signed>::cast<Tgt>(value);
}


///////////////////////////////////////////////////////////////////////////////
// SkipWhitespace (advances a range as long isspace() is false.
//
template <typename T>
typename std::enable_if<
    sizeof(T) == 1,
    plx::Range<T>>::type
SkipWhitespace(const plx::Range<T>& r) {
  auto wr = r;
  while (!wr.empty()) {
    if (!std::isspace(wr.front()))
      break;
    wr.advance(1);
  }
  return wr;
}


///////////////////////////////////////////////////////////////////////////////
// plx::DecodeString (decodes a json-style encoded string)
//
std::string DecodeString(plx::Range<const char>& range) ;


///////////////////////////////////////////////////////////////////////////////
// plx::FilesInfo
//
#pragma comment(user, "plex.define=plex_vista_support")

class FilesInfo {
private:
  FILE_ID_BOTH_DIR_INFO* info_;
  plx::LinkedBuffers link_buffs_;
  mutable bool done_;

private:
  FilesInfo() : info_(nullptr), done_(false) {}
  FilesInfo(const FilesInfo&) = delete;

public:
  static FilesInfo FromDir(plx::File& file, long buffer_hint = 32) {
    if (file.status() != (plx::File::directory | plx::File::existing))
      throw plx::IOException(__LINE__, nullptr);

    FilesInfo finf;
    // |buffer_size| controls the tradeoff between speed and memory use.
    const size_t buffer_size = buffer_hint * 128;

    plx::Range<unsigned char> data;
    for (size_t count = 0; ;++count) {
      data = finf.link_buffs_.new_buffer(buffer_size);
      if (!::GetFileInformationByHandleEx(
          file.handle_,
          count == 0 ? FileIdBothDirectoryRestartInfo: FileIdBothDirectoryInfo,
          data.start(), plx::To<DWORD>(data.size()))) {
        if (::GetLastError() != ERROR_NO_MORE_FILES)
          throw plx::IOException(__LINE__, nullptr);
        break;
      }
    }
    // The last buffer contains garbage. remove it.
    finf.link_buffs_.remove_last_buffer();
    return std::move(finf);
  }

  FilesInfo(FilesInfo&& other)
      : link_buffs_(std::move(other.link_buffs_)) {
    std::swap(info_, other.info_);
    std::swap(done_, other.done_);
  }

  void first() {
    done_ = false;
    link_buffs_.first();
    info_ = reinterpret_cast<FILE_ID_BOTH_DIR_INFO*>(link_buffs_.get().start());
  }

  void next() {
    if (!info_->NextEntryOffset) {
      // last entry of this buffer. Move to next buffer.
      link_buffs_.next();
      if (link_buffs_.done()) {
        done_ = true;
      } else {
        info_ = reinterpret_cast<FILE_ID_BOTH_DIR_INFO*>(link_buffs_.get().start());
      }
    } else {
      info_ =reinterpret_cast<FILE_ID_BOTH_DIR_INFO*>(
          ULONG_PTR(info_) + info_->NextEntryOffset);
    }
  }

  bool done() const {
    return done_;
  }

  const plx::ItRange<wchar_t*> file_name() const {
    return plx::ItRange<wchar_t*>(
      info_->FileName,
      info_->FileName+ (info_->FileNameLength / sizeof(wchar_t)));
  }

  long long creation_ns1600() const {
    return info_->CreationTime.QuadPart;
  }

  bool is_directory() const {
    return info_->FileAttributes & FILE_ATTRIBUTE_DIRECTORY? true : false;
  }
};




///////////////////////////////////////////////////////////////////////////////
// ReaderWriterLock and its scoped unlockers.
//
class ScopedWriteLock {
  SRWLOCK* lock_;
  explicit ScopedWriteLock(SRWLOCK* lock) : lock_(lock) {}
  friend class ReaderWriterLock;

public:
  ScopedWriteLock() = delete;
  ScopedWriteLock(const ScopedWriteLock&) = delete;
  // void* operator new(std::size_t) =delete

  ~ScopedWriteLock() {
    ::ReleaseSRWLockExclusive(lock_);
  }
};

class ScopedReadLock {
  SRWLOCK* lock_;
  explicit ScopedReadLock(SRWLOCK* lock) : lock_(lock) {}
  friend class ReaderWriterLock;

public:
  ScopedReadLock() = delete;
  ScopedReadLock(const ScopedReadLock&) = delete;
  // void* operator new(std::size_t) =delete

  ~ScopedReadLock() {
    ::ReleaseSRWLockShared(lock_);
  }
};

// The SRW lock is implemented using a single pointer-sized atomic variable
// which can take on a number of different states, depending on the values
// of the low bits. The number of state transitions is pretty high, here are
// some common ones:
// - initial lock state: (0, ControlBits:0) -- An SRW lock starts with all
//   bits set to 0.
// - shared state: (ShareCount: n, ControlBits: 1) -- When there is no conflicting
//   exclusive acquire and the lock is held shared, the share count is stored
//   directly in the lock variable.
// - exclusive state: (ShareCount: 0, ControlBits: 1) -- When there is no
//   conflicting shared acquire or exclusive acquire, the lock has a low bit set
//   and nothing else.
// - contended case 1 : (WaitPtr:ptr, ControlBits: 3) -- When there is a conflict,
//   the threads that are waiting for the lock form a queue using data allocated
//   on the waiting threads' stacks. The lock variable stores a pointer to the
//   tail of the queue instead of a share count.

class ReaderWriterLock {
  char cache_line_pad_[64 - sizeof(SRWLOCK)];
  SRWLOCK lock_;

public:
  ReaderWriterLock() {
    ::InitializeSRWLock(&lock_);
  }

  ReaderWriterLock(ReaderWriterLock&) = delete;
  ReaderWriterLock& operator=(const ReaderWriterLock&) = delete;

  // Acquiring an exclusive lock when you don't know the initial state is a
  // single write to the lock word, to set the low bit (LOCK BTS instruction)
  // If you succeeded you can proceed into the locked region with no further
  // operations.

  ScopedWriteLock write_lock() {
    ::AcquireSRWLockExclusive(&lock_);
    return ScopedWriteLock(&lock_);
  }

  // Trying to acquire a shared lock is a more involved operation: You need to
  // first read the initial value of the lock variable to determine the old
  // share count, increment the share count you read, and then write the updated
  // value back conditionally with the LOCK CMPXCHG instruction.

  ScopedReadLock read_lock() {
    ::AcquireSRWLockShared(&lock_);
    return ScopedReadLock(&lock_);
  }
};


///////////////////////////////////////////////////////////////////////////////
// plx::SizeL : windows compatible SIZE wrapper.
//

struct SizeL : public ::SIZE {
  SizeL() : SIZE() {}
  SizeL(long width, long height) : SIZE({width, height}) {}
  bool empty() const { return (cx == 0L) && (cy == 0L); }
};


///////////////////////////////////////////////////////////////////////////////
// plx::ScopedD2D1DeviceContext : Direct2D DC for drawing.
//
class ScopedD2D1DeviceContext {
  plx::ComPtr<ID2D1DeviceContext> dc_;
  plx::ComPtr<IDCompositionSurface> ics_;

  ScopedD2D1DeviceContext(const ScopedD2D1DeviceContext&) = delete;
  ScopedD2D1DeviceContext operator=(const ScopedD2D1DeviceContext&) = delete;
  void* operator new(size_t) = delete;

public:
  ScopedD2D1DeviceContext(plx::ComPtr<IDCompositionSurface> ics,
                          const D2D1_SIZE_F& offset,
                          const plx::DPI& dpi,
                          const D2D1_COLOR_F* clear_color) {
    dc_ = plx::CreateDCoDeviceCtx(ics, dpi, offset);
    ics_ =ics;
    if (clear_color)
      dc_->Clear(*clear_color);
  }

  ~ScopedD2D1DeviceContext() {
    ics_->EndDraw();
  }

  ID2D1DeviceContext* operator()() { return dc_.Get(); }
};


///////////////////////////////////////////////////////////////////////////////
// plx::RectL : windows compatible RECT wrapper.
//

struct RectL : public ::RECT {
  RectL() : RECT() {}
  RectL(long x0, long y0, long x1, long y1) : RECT({x0, y0, x1, y1}) {}
  RectL(const plx::SizeL& size): RECT({ 0L, 0L, size.cx, size.cy }) {}

  plx::SizeL size() { return SizeL(right - left, bottom - top); }
  long width() const { return right - left; }
  long height() const { return bottom - top; }
};


///////////////////////////////////////////////////////////////////////////////
// plx::ParseJsonValue (converts a JSON string into a JsonValue)
//
plx::JsonValue ParseJsonValue(plx::Range<const char>& range);

plx::JsonValue ParseJsonValue(plx::Range<const char>& range) ;


///////////////////////////////////////////////////////////////////////////////
// plx::StringPrintf  (c-style printf for std strings)
//

std::string StringPrintf(const char* fmt, ...) ;


///////////////////////////////////////////////////////////////////////////////
// plx::JsonFromFile.
plx::JsonValue JsonFromFile(plx::File& cfile) ;


///////////////////////////////////////////////////////////////////////////////
// plx::UTF16FromUTF8
std::wstring UTF16FromUTF8(const plx::Range<const uint8_t>& utf8, bool strict) ;


///////////////////////////////////////////////////////////////////////////////
// plx::User32Exception (thrown by user32 functions)
//
class User32Exception : public plx::Exception {
public:
  enum Kind {
    wclass,
    window,
    menu,
    device,
    cursor,
    icon,
    accelerator
  };

  User32Exception(int line, Kind type)
      : Exception(line, "user 32"), type_(type) {
    PostCtor();
  }

  Kind type() const {
    return type_;
  }

private:
  Kind type_;
};


///////////////////////////////////////////////////////////////////////////////
// plx::Window
//

template <typename Derived>
class Window {
  HWND window_;
  plx::DPI dpi_;

  Window(const Window&) = delete;
  Window& operator=(const Window&) = delete;

public:
  HWND window() { return window_; }
  const plx::DPI& dpi() const { return dpi_; }

protected:
  Window() : window_(nullptr) {}

  HWND create_window(DWORD ex_style, DWORD style,
                     LPCWSTR window_name,
                     HICON small_icon, HICON icon,
                     int x, int y, int width, int height,
                     HWND parent,
                     HMENU menu) {

    WNDCLASSEX wcex = { sizeof(wcex) };
    wcex.hCursor = ::LoadCursor(nullptr, IDC_ARROW);
    wcex.hInstance = reinterpret_cast<HINSTANCE>(&__ImageBase);
    wcex.lpszClassName = L"PlexWindowClass";
    wcex.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wcex.lpfnWndProc = WndProc;
    wcex.hIcon = icon;
    wcex.hIconSm = small_icon;

    auto atom = ::RegisterClassEx(&wcex);
    if (!atom)
      throw plx::User32Exception(__LINE__, plx::User32Exception::wclass);

    return ::CreateWindowExW(ex_style,
                             MAKEINTATOM(atom),
                             window_name,
                             style,
                             x, y, width, height,
                             parent,
                             menu,
                             wcex.hInstance,
                             this);
  }

  static Derived* this_from_window(HWND window) {
    return reinterpret_cast<Derived*>(GetWindowLongPtr(window, GWLP_USERDATA));
  }

  static LRESULT __stdcall WndProc(HWND window,
                                   const UINT message,
                                   WPARAM  wparam, LPARAM  lparam) {
    if (WM_NCCREATE == message) {
      auto cs = reinterpret_cast<CREATESTRUCT*>(lparam);
      auto obj = static_cast<Derived*>(cs->lpCreateParams);
      if (!obj)
        throw plx::User32Exception(__LINE__, plx::User32Exception::window);
      if (obj->window_)
        throw plx::User32Exception(__LINE__, plx::User32Exception::window);
      obj->window_ = window;
      ::SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(obj));
    } else {
      auto obj = this_from_window(window);
      if (obj) {
        if (message == WM_CREATE) {
          auto monitor = ::MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST);
          obj->dpi_.set_from_monitor(monitor);
          auto cs = reinterpret_cast<CREATESTRUCT*>(lparam);
          if ((cs->cx != CW_USEDEFAULT) && (cs->cy != CW_USEDEFAULT)) {
            RECT r = {
              0, 0,
              static_cast<long>(obj->dpi_.to_physical_x(cs->cx)),
              static_cast<long>(obj->dpi_.to_physical_x(cs->cy))
            };
            ::AdjustWindowRectEx(&r, cs->style, (cs->hMenu != NULL), cs->dwExStyle);
            ::SetWindowPos(window, nullptr, 0, 0,
                           r.right - r.left, r.bottom - r.top,
                           SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOZORDER);
          }

        } else if (message == WM_NCDESTROY) {
          ::SetWindowLongPtrW(window, GWLP_USERDATA, 0L);
          obj->window_ = nullptr;
        } else if (message == WM_DPICHANGED) {
          obj->dpi_.set_dpi(LOWORD(wparam), HIWORD(wparam));
        }
        return obj->message_handler(message, wparam, lparam);
      }
    }

    return ::DefWindowProc(window, message, wparam, lparam);
  }
};

}

extern "C" IMAGE_DOS_HEADER __ImageBase;
