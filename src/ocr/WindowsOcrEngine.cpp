#include "WindowsOcrEngine.h"

#include <QCoreApplication>
#include <QThread>

#include <windows.h>
#include <asyncinfo.h>
#include <inspectable.h>
#include <roapi.h>
#include <robuffer.h>
#include <windows.storage.streams.h>
#include <winstring.h>

#include <cstring>

// --- WinRT ABI declarations -------------------------------------------------
// IIDs and method order come from the Windows metadata (Windows.Media.Ocr,
// Windows.Graphics.Imaging, Windows.Globalization). Each interface lists its
// methods in vtable order after IInspectable; only the prefix we call is needed.
namespace {

constexpr GUID kIID_IOcrEngineStatics = {0x5bffa85a, 0x3384, 0x3540, {0x99, 0x40, 0x69, 0x91, 0x20, 0xd4, 0x28, 0xa8}};
constexpr GUID kIID_ISoftwareBitmapStatics = {0xdf0385db, 0x672f, 0x4a9d, {0x80, 0x6e, 0xc2, 0x44, 0x2f, 0x34, 0x3e, 0x86}};
constexpr GUID kIID_ILanguageFactory = {0x9b0252ac, 0x0c27, 0x44f8, {0xb7, 0x92, 0x97, 0x93, 0xfb, 0x66, 0xc6, 0x3e}};
constexpr GUID kIID_IBufferByteAccess = {0x905a0fef, 0xbc53, 0x11df, {0x8c, 0x49, 0x00, 0x1e, 0x4f, 0xc6, 0x86, 0xda}};
constexpr GUID kIID_IAsyncInfo = {0x00000036, 0x0000, 0x0000, {0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}};

constexpr int kBitmapPixelFormatBgra8 = 87;

struct WinRect {
    float X, Y, Width, Height;
};

// IVectorView<T> for reference types: GetAt, get_Size, IndexOf, GetMany.
struct IVectorViewRaw : IInspectable {
    virtual HRESULT STDMETHODCALLTYPE GetAt(UINT32 index, IInspectable **item) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_Size(UINT32 *size) = 0;
};

// IAsyncOperation<T>: put_Completed, get_Completed, GetResults.
struct IAsyncOperationRaw : IInspectable {
    virtual HRESULT STDMETHODCALLTYPE put_Completed(IUnknown *handler) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_Completed(IUnknown **handler) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetResults(IInspectable **result) = 0;
};

struct IOcrWord : IInspectable {
    virtual HRESULT STDMETHODCALLTYPE get_BoundingRect(WinRect *rect) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_Text(HSTRING *text) = 0;
};

struct IOcrLine : IInspectable {
    virtual HRESULT STDMETHODCALLTYPE get_Words(IVectorViewRaw **words) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_Text(HSTRING *text) = 0;
};

struct IOcrResult : IInspectable {
    virtual HRESULT STDMETHODCALLTYPE get_Lines(IVectorViewRaw **lines) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_TextAngle(IInspectable **angle) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_Text(HSTRING *text) = 0;
};

struct IOcrEngine : IInspectable {
    virtual HRESULT STDMETHODCALLTYPE RecognizeAsync(IInspectable *bitmap, IAsyncOperationRaw **operation) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_RecognizerLanguage(IInspectable **language) = 0;
};

struct IOcrEngineStatics : IInspectable {
    virtual HRESULT STDMETHODCALLTYPE get_MaxImageDimension(UINT32 *value) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_AvailableRecognizerLanguages(IVectorViewRaw **languages) = 0;
    virtual HRESULT STDMETHODCALLTYPE IsLanguageSupported(IInspectable *language, boolean *supported) = 0;
    virtual HRESULT STDMETHODCALLTYPE TryCreateFromLanguage(IInspectable *language, IOcrEngine **engine) = 0;
    virtual HRESULT STDMETHODCALLTYPE TryCreateFromUserProfileLanguages(IOcrEngine **engine) = 0;
};

struct ISoftwareBitmapStatics : IInspectable {
    virtual HRESULT STDMETHODCALLTYPE Copy(IInspectable *source, IInspectable **result) = 0;
    virtual HRESULT STDMETHODCALLTYPE Convert(IInspectable *source, int format, IInspectable **result) = 0;
    virtual HRESULT STDMETHODCALLTYPE ConvertWithAlpha(IInspectable *source, int format, int alpha,
                                                       IInspectable **result) = 0;
    virtual HRESULT STDMETHODCALLTYPE CreateCopyFromBuffer(ABI::Windows::Storage::Streams::IBuffer *source, int format,
                                                           INT32 width, INT32 height, IInspectable **result) = 0;
};

struct ILanguageFactory : IInspectable {
    virtual HRESULT STDMETHODCALLTYPE CreateLanguage(HSTRING tag, IInspectable **language) = 0;
};

// Minimal owning COM pointer.
template <typename T>
class Ref
{
public:
    Ref() = default;
    Ref(const Ref &) = delete;
    Ref &operator=(const Ref &) = delete;
    ~Ref() { reset(); }

    void reset()
    {
        if (m_p)
            m_p->Release();
        m_p = nullptr;
    }
    T *get() const { return m_p; }
    T *operator->() const { return m_p; }
    explicit operator bool() const { return m_p; }
    T **put()
    {
        reset();
        return &m_p;
    }
    void **putVoid() { return reinterpret_cast<void **>(put()); }
    IInspectable **putInspectable() { return reinterpret_cast<IInspectable **>(put()); }

private:
    T *m_p = nullptr;
};

// Owns an HSTRING created from a literal or QString.
class HString
{
public:
    explicit HString(const QString &text)
    {
        WindowsCreateString(reinterpret_cast<const wchar_t *>(text.utf16()), UINT32(text.size()), &m_h);
    }
    ~HString() { WindowsDeleteString(m_h); }
    HSTRING get() const { return m_h; }

private:
    HSTRING m_h = nullptr;
};

QString takeString(HSTRING h)
{
    UINT32 length = 0;
    const wchar_t *raw = WindowsGetStringRawBuffer(h, &length);
    const QString text = QString::fromWCharArray(raw, int(length));
    WindowsDeleteString(h);
    return text;
}

template <typename T>
HRESULT activationFactory(const wchar_t *className, const GUID &iid, Ref<T> &out)
{
    const HString name(QString::fromWCharArray(className));
    return RoGetActivationFactory(name.get(), iid, out.putVoid());
}

// Windows OCR language tags for our codes, most specific first.
QStringList languageTags(const QString &code)
{
    if (code == QLatin1String("zh_CN"))
        return {QStringLiteral("zh-Hans-CN"), QStringLiteral("zh-Hans"), QStringLiteral("zh-CN")};
    if (code == QLatin1String("ja"))
        return {QStringLiteral("ja-JP"), QStringLiteral("ja")};
    return {QStringLiteral("en-US"), QStringLiteral("en")};
}

} // namespace

struct WindowsOcrEngine::Private {
    bool uninitialize = false;
    Ref<IOcrEngine> engine;
    Ref<ISoftwareBitmapStatics> bitmaps;
    UINT32 maxDimension = 0;
};

WindowsOcrEngine::~WindowsOcrEngine()
{
    if (!d)
        return;
    const bool uninitialize = d->uninitialize;
    d.reset(); // release WinRT objects before leaving the apartment
    if (uninitialize)
        RoUninitialize();
}

std::unique_ptr<OcrEngine> WindowsOcrEngine::create(const QStringList &languages, QString *error)
{
    std::unique_ptr<WindowsOcrEngine> self(new WindowsOcrEngine);
    self->d = std::make_unique<Private>();
    const auto fail = [error](const QString &message) {
        if (error)
            *error = message;
        return nullptr;
    };

    // Worker threads have no apartment yet; S_FALSE / RPC_E_CHANGED_MODE mean one exists.
    const HRESULT init = RoInitialize(RO_INIT_MULTITHREADED);
    self->d->uninitialize = SUCCEEDED(init);

    Ref<IOcrEngineStatics> statics;
    Ref<ILanguageFactory> languageFactory;
    if (FAILED(activationFactory(L"Windows.Media.Ocr.OcrEngine", kIID_IOcrEngineStatics, statics))
        || FAILED(activationFactory(L"Windows.Globalization.Language", kIID_ILanguageFactory, languageFactory))
        || FAILED(activationFactory(L"Windows.Graphics.Imaging.SoftwareBitmap", kIID_ISoftwareBitmapStatics,
                                    self->d->bitmaps)))
        return fail(QCoreApplication::translate("Ocr", "Windows OCR is not available on this system."));

    statics->get_MaxImageDimension(&self->d->maxDimension);

    // One recognizer language per engine: take the first selected language
    // that has an installed OCR component.
    for (const QString &code : languages) {
        for (const QString &tag : languageTags(code)) {
            const HString hTag(tag);
            Ref<IInspectable> language;
            boolean supported = false;
            if (SUCCEEDED(languageFactory->CreateLanguage(hTag.get(), language.put()))
                && SUCCEEDED(statics->IsLanguageSupported(language.get(), &supported)) && supported
                && SUCCEEDED(statics->TryCreateFromLanguage(language.get(), self->d->engine.put()))
                && self->d->engine)
                return self;
        }
    }

    return fail(QCoreApplication::translate("Ocr", "None of the selected languages has Windows OCR installed. Add the language (with its "
                   "optical character recognition feature) in Windows Settings > Time & language > Language."));
}

OcrResult WindowsOcrEngine::recognize(const QImage &image)
{
    OcrResult result;
    if (image.isNull())
        return result;

    // Small screen text recognizes better enlarged; HiDPI captures are large enough.
    int scale = image.devicePixelRatio() >= 1.75 ? 1 : 2;
    const UINT32 limit = d->maxDimension ? d->maxDimension : 10000;
    while (scale > 1 && UINT32(std::max(image.width(), image.height()) * scale) > limit)
        --scale;
    QImage input = image.convertToFormat(QImage::Format_ARGB32_Premultiplied); // BGRA in memory
    if (scale > 1)
        input = input.scaled(input.size() * scale, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    if (UINT32(std::max(input.width(), input.height())) > limit) {
        input = input.scaled(int(limit), int(limit), Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    const qreal toImage = qreal(image.width()) / input.width();

    // Pixels -> Windows.Storage.Streams.Buffer -> SoftwareBitmap.
    const UINT32 bytes = UINT32(input.width()) * input.height() * 4;
    Ref<ABI::Windows::Storage::Streams::IBufferFactory> bufferFactory;
    Ref<ABI::Windows::Storage::Streams::IBuffer> buffer;
    Ref<Windows::Storage::Streams::IBufferByteAccess> byteAccess;
    BYTE *data = nullptr;
    if (FAILED(activationFactory(L"Windows.Storage.Streams.Buffer",
                                 __uuidof(ABI::Windows::Storage::Streams::IBufferFactory), bufferFactory))
        || FAILED(bufferFactory->Create(bytes, buffer.put()))
        || FAILED(buffer->QueryInterface(kIID_IBufferByteAccess, byteAccess.putVoid()))
        || FAILED(byteAccess->Buffer(&data))) {
        result.error = QCoreApplication::translate("Ocr", "Windows OCR could not prepare the image.");
        return result;
    }
    for (int y = 0; y < input.height(); ++y)
        std::memcpy(data + qsizetype(y) * input.width() * 4, input.constScanLine(y), size_t(input.width()) * 4);
    buffer->put_Length(bytes);

    Ref<IInspectable> bitmap;
    if (FAILED(d->bitmaps->CreateCopyFromBuffer(buffer.get(), kBitmapPixelFormatBgra8, input.width(), input.height(),
                                                bitmap.put()))) {
        result.error = QCoreApplication::translate("Ocr", "Windows OCR could not prepare the image.");
        return result;
    }

    // We are on a worker thread already, so wait for the async operation by polling.
    Ref<IAsyncOperationRaw> operation;
    Ref<IAsyncInfo> info;
    if (FAILED(d->engine->RecognizeAsync(bitmap.get(), operation.put()))
        || FAILED(operation->QueryInterface(kIID_IAsyncInfo, info.putVoid()))) {
        result.error = QCoreApplication::translate("Ocr", "Windows OCR failed to recognize the image.");
        return result;
    }
    AsyncStatus status = AsyncStatus::Started;
    while (SUCCEEDED(info->get_Status(&status)) && status == AsyncStatus::Started)
        QThread::msleep(10);

    Ref<IOcrResult> ocr;
    if (status != AsyncStatus::Completed || FAILED(operation->GetResults(ocr.putInspectable())) || !ocr) {
        result.error = QCoreApplication::translate("Ocr", "Windows OCR failed to recognize the image.");
        return result;
    }

    Ref<IVectorViewRaw> lines;
    UINT32 lineCount = 0;
    if (FAILED(ocr->get_Lines(lines.put())) || FAILED(lines->get_Size(&lineCount)))
        return result;
    for (UINT32 i = 0; i < lineCount; ++i) {
        Ref<IOcrLine> line;
        HSTRING text = nullptr;
        if (FAILED(lines->GetAt(i, line.putInspectable())) || FAILED(line->get_Text(&text)))
            continue;
        OcrLine out;
        out.text = Ocr::tidyCjkSpacing(takeString(text));

        // The line's box is the union of its words' boxes.
        Ref<IVectorViewRaw> words;
        UINT32 wordCount = 0;
        if (SUCCEEDED(line->get_Words(words.put())) && SUCCEEDED(words->get_Size(&wordCount))) {
            QRectF box;
            for (UINT32 w = 0; w < wordCount; ++w) {
                Ref<IOcrWord> word;
                WinRect rect{};
                if (SUCCEEDED(words->GetAt(w, word.putInspectable())) && SUCCEEDED(word->get_BoundingRect(&rect)))
                    box |= QRectF(rect.X, rect.Y, rect.Width, rect.Height);
            }
            out.box = QRectF(box.topLeft() * toImage, box.size() * toImage).toAlignedRect();
        }
        result.lines << out;
    }
    return result;
}
