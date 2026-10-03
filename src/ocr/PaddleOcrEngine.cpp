#include "PaddleOcrEngine.h"

#include "ModelStore.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QLibrary>
#include <QMutex>
#include <QTextStream>
#include <QThread>

#include <onnxruntime_c_api.h>

#include <algorithm>
#include <cmath>
#include <map>
#include <vector>

namespace {

const QString kModelDir = QStringLiteral("paddle");
const QString kDetFile = QStringLiteral("PP-OCRv6_small_det.onnx");
const QString kRecFile = QStringLiteral("PP-OCRv6_small_rec.onnx");
const QString kDictFile = QStringLiteral("PP-OCRv6_small_rec.yml");

// DB post-processing defaults of PaddleOCR.
constexpr float kBinaryThreshold = 0.3f;
constexpr float kBoxThreshold = 0.6f;
constexpr float kUnclipRatio = 1.5f;
constexpr int kMaxDetSide = 2048;
constexpr int kRecHeight = 48;

QString modelPath(const QString &file)
{
    return QDir(ModelStore::directory(kModelDir)).filePath(file);
}

// --- ONNX Runtime -------------------------------------------------------------

// The library, API table and environment are process-wide; sessions are cached
// by model path because loading a model takes far longer than running it.
// OrtSession::Run is thread-safe, so a cached session may serve any thread.
class OrtRuntime
{
public:
    static OrtRuntime *instance(QString *error)
    {
        static QMutex mutex;
        static OrtRuntime *runtime = nullptr;
        static QString loadError;
        QMutexLocker lock(&mutex);
        if (!runtime && loadError.isEmpty()) {
            auto *candidate = new OrtRuntime;
            if (candidate->load(&loadError))
                runtime = candidate;
            else
                delete candidate;
        }
        if (!runtime && error)
            *error = loadError;
        return runtime;
    }

    const OrtApi *api = nullptr;

    OrtSession *session(const QString &path, QString *error)
    {
        QMutexLocker lock(&m_mutex);
        auto it = m_sessions.find(path);
        if (it != m_sessions.end())
            return it->second;

        OrtSessionOptions *options = nullptr;
        OrtSession *session = nullptr;
        OrtStatus *status = api->CreateSessionOptions(&options);
        if (!status)
            status = api->SetIntraOpNumThreads(options, std::max(1, QThread::idealThreadCount() / 2));
        if (!status)
            status = api->SetSessionGraphOptimizationLevel(options, ORT_ENABLE_ALL);
        if (!status) {
#ifdef _WIN32
            status = api->CreateSession(m_env, reinterpret_cast<const wchar_t *>(QDir::toNativeSeparators(path).utf16()),
                                        options, &session);
#else
            status = api->CreateSession(m_env, QFile::encodeName(path).constData(), options, &session);
#endif
        }
        if (options)
            api->ReleaseSessionOptions(options);
        if (status) {
            if (error)
                *error = QCoreApplication::translate("Ocr", "Could not load the model %1: %2")
                             .arg(QDir::toNativeSeparators(path), QString::fromUtf8(api->GetErrorMessage(status)));
            api->ReleaseStatus(status);
            return nullptr;
        }
        m_sessions.emplace(path, session);
        return session;
    }

private:
    bool load(QString *error)
    {
        // Prefer the copy shipped next to Glimpse: Windows has an older
        // onnxruntime.dll in System32 for Windows ML.
        const QString appDir = QCoreApplication::applicationDirPath();
#ifdef _WIN32
        m_library.setFileName(QDir(appDir).filePath(QStringLiteral("onnxruntime.dll")));
#else
        m_library.setFileName(QDir(appDir).filePath(QStringLiteral("libonnxruntime")));
        if (!m_library.load())
            m_library.setFileName(QStringLiteral("onnxruntime"));
#endif
        if (!m_library.isLoaded() && !m_library.load()) {
            *error = QCoreApplication::translate("Ocr", "ONNX Runtime could not be loaded (%1). PaddleOCR is unavailable.").arg(m_library.errorString());
            return false;
        }
        using GetApiBase = const OrtApiBase *(ORT_API_CALL *)();
        const auto getApiBase = reinterpret_cast<GetApiBase>(m_library.resolve("OrtGetApiBase"));
        const OrtApiBase *base = getApiBase ? getApiBase() : nullptr;
        api = base ? base->GetApi(ORT_API_VERSION) : nullptr;
        if (!api) {
            *error = QCoreApplication::translate("Ocr", "The installed ONNX Runtime is too old (needs API version %1).").arg(ORT_API_VERSION);
            return false;
        }
        if (OrtStatus *status = api->CreateEnv(ORT_LOGGING_LEVEL_WARNING, "glimpse", &m_env)) {
            *error = QString::fromUtf8(api->GetErrorMessage(status));
            api->ReleaseStatus(status);
            return false;
        }
        return true;
    }

    QLibrary m_library;
    OrtEnv *m_env = nullptr;
    QMutex m_mutex;
    std::map<QString, OrtSession *> m_sessions;
};

// Runs a single-input, single-output float model. Returns the output tensor
// data and shape, or an empty vector with `error` set.
std::vector<float> run(OrtRuntime *ort, OrtSession *session, std::vector<float> &input,
                       const std::vector<int64_t> &shape, std::vector<int64_t> *outShape, QString *error)
{
    const OrtApi *api = ort->api;
    OrtAllocator *allocator = nullptr;
    OrtMemoryInfo *memory = nullptr;
    OrtValue *inputValue = nullptr;
    OrtValue *outputValue = nullptr;
    char *inputName = nullptr;
    char *outputName = nullptr;
    std::vector<float> output;

    OrtStatus *status = api->GetAllocatorWithDefaultOptions(&allocator);
    if (!status)
        status = api->SessionGetInputName(session, 0, allocator, &inputName);
    if (!status)
        status = api->SessionGetOutputName(session, 0, allocator, &outputName);
    if (!status)
        status = api->CreateCpuMemoryInfo(OrtArenaAllocator, OrtMemTypeDefault, &memory);
    if (!status)
        status = api->CreateTensorWithDataAsOrtValue(memory, input.data(), input.size() * sizeof(float), shape.data(),
                                                     shape.size(), ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT, &inputValue);
    if (!status) {
        const char *inputs[] = {inputName};
        const char *outputs[] = {outputName};
        status = api->Run(session, nullptr, inputs, &inputValue, 1, outputs, 1, &outputValue);
    }
    if (!status) {
        OrtTensorTypeAndShapeInfo *info = nullptr;
        float *data = nullptr;
        size_t count = 0, dims = 0;
        status = api->GetTensorTypeAndShape(outputValue, &info);
        if (!status)
            status = api->GetTensorShapeElementCount(info, &count);
        if (!status)
            status = api->GetDimensionsCount(info, &dims);
        if (!status) {
            outShape->assign(dims, 0);
            status = api->GetDimensions(info, outShape->data(), dims);
        }
        if (!status)
            status = api->GetTensorMutableData(outputValue, reinterpret_cast<void **>(&data));
        if (!status)
            output.assign(data, data + count);
        if (info)
            api->ReleaseTensorTypeAndShapeInfo(info);
    }

    if (status) {
        *error = QCoreApplication::translate("Ocr", "PaddleOCR failed: %1").arg(QString::fromUtf8(api->GetErrorMessage(status)));
        api->ReleaseStatus(status);
        output.clear();
    }
    if (outputValue)
        api->ReleaseValue(outputValue);
    if (inputValue)
        api->ReleaseValue(inputValue);
    if (memory)
        api->ReleaseMemoryInfo(memory);
    if (allocator) {
        if (inputName)
            allocator->Free(allocator, inputName);
        if (outputName)
            allocator->Free(allocator, outputName);
    }
    return output;
}

// --- dictionary -------------------------------------------------------------

// The recognizer's characters are the "character_dict" list in the model's
// inference.yml, one YAML scalar per line ("  - x", or quoted "  - 'x'").
QStringList loadDictionary(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    QTextStream in(&file);
    in.setEncoding(QStringConverter::Utf8);
    QStringList characters;
    bool inDict = false;
    while (!in.atEnd()) {
        const QString line = in.readLine();
        if (!inDict) {
            inDict = line.trimmed() == QLatin1String("character_dict:");
            continue;
        }
        if (!line.startsWith(QLatin1String("  - ")))
            break;
        QString value = line.mid(4);
        if (value.size() >= 2 && value.startsWith(QLatin1Char('\'')) && value.endsWith(QLatin1Char('\'')))
            value = value.mid(1, value.size() - 2).replace(QLatin1String("''"), QLatin1String("'"));
        else if (value.size() >= 2 && value.startsWith(QLatin1Char('"')) && value.endsWith(QLatin1Char('"')))
            value = value.mid(1, value.size() - 2).replace(QLatin1String("\\\""), QLatin1String("\""))
                        .replace(QLatin1String("\\\\"), QLatin1String("\\"));
        characters << value;
    }
    return characters;
}

// --- image helpers ----------------------------------------------------------

// Packs an RGB image into planar BGR floats (PaddleOCR models are trained on
// BGR), applying per-channel (x / 255 - mean) / std.
std::vector<float> toTensor(const QImage &rgb, const float mean[3], const float stddev[3], int paddedWidth = 0)
{
    const int width = paddedWidth > 0 ? paddedWidth : rgb.width();
    const int height = rgb.height();
    const size_t plane = size_t(width) * height;
    std::vector<float> tensor(plane * 3, 0.0f);
    for (int y = 0; y < height; ++y) {
        const uchar *row = rgb.constScanLine(y);
        for (int x = 0; x < rgb.width(); ++x) {
            const uchar *px = row + x * 3; // R, G, B
            const size_t i = size_t(y) * width + x;
            tensor[i] = (px[2] / 255.0f - mean[0]) / stddev[0];
            tensor[plane + i] = (px[1] / 255.0f - mean[1]) / stddev[1];
            tensor[2 * plane + i] = (px[0] / 255.0f - mean[2]) / stddev[2];
        }
    }
    return tensor;
}

int roundTo32(double value)
{
    return std::max(32, int(std::lround(value / 32.0)) * 32);
}

struct DetectedBox {
    QRect rect; // in source image pixels
};

} // namespace

struct PaddleOcrEngine::Private {
    OrtRuntime *ort = nullptr;
    OrtSession *det = nullptr;
    OrtSession *rec = nullptr;
    QStringList dictionary;

    QList<DetectedBox> detect(const QImage &rgb, QString *error);
    QString read(const QImage &line, QString *error);
};

PaddleOcrEngine::~PaddleOcrEngine() = default;

QList<ModelFile> PaddleOcrEngine::requiredModels()
{
    const QString base = QStringLiteral("https://huggingface.co/PaddlePaddle/%1/resolve/main/%2");
    return {
        {kDetFile, QUrl(base.arg(QStringLiteral("PP-OCRv6_small_det_onnx"), QStringLiteral("inference.onnx"))),
         modelPath(kDetFile)},
        {kRecFile, QUrl(base.arg(QStringLiteral("PP-OCRv6_small_rec_onnx"), QStringLiteral("inference.onnx"))),
         modelPath(kRecFile)},
        {kDictFile, QUrl(base.arg(QStringLiteral("PP-OCRv6_small_rec_onnx"), QStringLiteral("inference.yml"))),
         modelPath(kDictFile)},
    };
}

std::unique_ptr<OcrEngine> PaddleOcrEngine::create(QString *error)
{
    std::unique_ptr<PaddleOcrEngine> self(new PaddleOcrEngine);
    self->d = std::make_unique<Private>();
    Private &p = *self->d;

    p.ort = OrtRuntime::instance(error);
    if (!p.ort)
        return nullptr;
    p.det = p.ort->session(modelPath(kDetFile), error);
    p.rec = p.det ? p.ort->session(modelPath(kRecFile), error) : nullptr;
    if (!p.rec)
        return nullptr;
    p.dictionary = loadDictionary(modelPath(kDictFile));
    if (p.dictionary.isEmpty()) {
        if (error)
            *error = QCoreApplication::translate("Ocr", "The PaddleOCR character dictionary is missing or invalid.");
        return nullptr;
    }
    return self;
}

QList<DetectedBox> PaddleOcrEngine::Private::detect(const QImage &rgb, QString *error)
{
    // Small screen text needs enlarging; huge captures are capped for speed.
    double scale = 1.0;
    const int longSide = std::max(rgb.width(), rgb.height());
    if (longSide * 2 <= kMaxDetSide && rgb.devicePixelRatio() < 1.75)
        scale = 2.0;
    else if (longSide > kMaxDetSide)
        scale = double(kMaxDetSide) / longSide;
    const int width = roundTo32(rgb.width() * scale);
    const int height = roundTo32(rgb.height() * scale);
    const QImage resized = rgb.scaled(width, height, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

    static const float mean[3] = {0.485f, 0.456f, 0.406f};
    static const float stddev[3] = {0.229f, 0.224f, 0.225f};
    std::vector<float> input = toTensor(resized, mean, stddev);
    std::vector<int64_t> outShape;
    const std::vector<float> prob = run(ort, det, input, {1, 3, height, width}, &outShape, error);
    if (prob.empty() || outShape.size() != 4)
        return {};
    const int mapH = int(outShape[2]);
    const int mapW = int(outShape[3]);

    // Connected components of the binarized probability map; screen text is
    // axis-aligned, so each component's bounding box is its line box.
    std::vector<int> label(size_t(mapW) * mapH, 0);
    std::vector<int> stack;
    QList<DetectedBox> boxes;
    const double toSourceX = double(rgb.width()) / mapW;
    const double toSourceY = double(rgb.height()) / mapH;
    int next = 0;
    for (int start = 0; start < mapW * mapH; ++start) {
        if (label[start] || prob[start] <= kBinaryThreshold)
            continue;
        ++next;
        int x0 = mapW, y0 = mapH, x1 = -1, y1 = -1;
        double sum = 0;
        int count = 0;
        stack.assign(1, start);
        label[start] = next;
        while (!stack.empty()) {
            const int i = stack.back();
            stack.pop_back();
            const int x = i % mapW, y = i / mapW;
            x0 = std::min(x0, x), x1 = std::max(x1, x), y0 = std::min(y0, y), y1 = std::max(y1, y);
            sum += prob[i];
            ++count;
            for (int dy = -1; dy <= 1; ++dy) {
                for (int dx = -1; dx <= 1; ++dx) {
                    const int nx = x + dx, ny = y + dy;
                    if (nx < 0 || ny < 0 || nx >= mapW || ny >= mapH)
                        continue;
                    const int n = ny * mapW + nx;
                    if (!label[n] && prob[n] > kBinaryThreshold) {
                        label[n] = next;
                        stack.push_back(n);
                    }
                }
            }
        }
        const int w = x1 - x0 + 1, h = y1 - y0 + 1;
        if (std::min(w, h) < 3 || sum / count < kBoxThreshold)
            continue;
        // DB predicts a shrunk text kernel; grow it back (PaddleOCR's unclip
        // offset for a rectangle: area * ratio / perimeter).
        const double grow = double(w) * h * kUnclipRatio / (2.0 * (w + h));
        const QRectF mapBox(x0 - grow, y0 - grow, w + 2 * grow, h + 2 * grow);
        const QRect box = QRectF(mapBox.x() * toSourceX, mapBox.y() * toSourceY, mapBox.width() * toSourceX,
                                 mapBox.height() * toSourceY)
                              .toAlignedRect()
                              .intersected(rgb.rect());
        if (!box.isEmpty())
            boxes << DetectedBox{box};
    }
    return boxes;
}

QString PaddleOcrEngine::Private::read(const QImage &line, QString *error)
{
    const int width = std::clamp(int(std::ceil(double(kRecHeight) * line.width() / line.height())), 16, 3200);
    const int padded = (width + 7) / 8 * 8;
    const QImage resized = line.scaled(width, kRecHeight, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

    static const float mean[3] = {0.5f, 0.5f, 0.5f};
    static const float stddev[3] = {0.5f, 0.5f, 0.5f};
    std::vector<float> input = toTensor(resized, mean, stddev, padded);
    std::vector<int64_t> outShape;
    const std::vector<float> probs = run(ort, rec, input, {1, 3, kRecHeight, padded}, &outShape, error);
    if (probs.empty() || outShape.size() != 3)
        return {};

    // CTC greedy decode: class 0 is the blank; 1..N map to the dictionary and
    // a trailing extra class (when present) is the space character.
    const int steps = int(outShape[1]);
    const int classes = int(outShape[2]);
    QString text;
    int previous = 0;
    for (int t = 0; t < steps; ++t) {
        const float *row = probs.data() + size_t(t) * classes;
        const int best = int(std::max_element(row, row + classes) - row);
        if (best != 0 && best != previous) {
            if (best - 1 < dictionary.size())
                text += dictionary.at(best - 1);
            else
                text += QLatin1Char(' ');
        }
        previous = best;
    }
    return text.trimmed();
}

OcrResult PaddleOcrEngine::recognize(const QImage &image)
{
    OcrResult result;
    if (image.isNull())
        return result;
    QImage rgb = image.convertToFormat(QImage::Format_RGB888);
    rgb.setDevicePixelRatio(image.devicePixelRatio());

    QList<DetectedBox> boxes = d->detect(rgb, &result.error);
    if (!result.error.isEmpty())
        return result;

    // Reading order: group boxes into rows by vertical overlap, then left to right.
    std::sort(boxes.begin(), boxes.end(),
              [](const DetectedBox &a, const DetectedBox &b) { return a.rect.center().y() < b.rect.center().y(); });
    QList<QList<DetectedBox>> rows;
    for (const DetectedBox &box : std::as_const(boxes)) {
        if (!rows.isEmpty()) {
            const QRect &last = rows.last().last().rect;
            const int overlap = std::min(last.bottom(), box.rect.bottom()) - std::max(last.top(), box.rect.top());
            if (overlap > std::min(last.height(), box.rect.height()) / 2) {
                rows.last() << box;
                continue;
            }
        }
        rows << QList<DetectedBox>{box};
    }

    for (QList<DetectedBox> &row : rows) {
        std::sort(row.begin(), row.end(),
                  [](const DetectedBox &a, const DetectedBox &b) { return a.rect.left() < b.rect.left(); });
        QStringList parts;
        QRect rowBox;
        for (const DetectedBox &box : std::as_const(row)) {
            const QString text = d->read(rgb.copy(box.rect), &result.error);
            if (!result.error.isEmpty())
                return result;
            if (!text.isEmpty())
                parts << text;
            rowBox |= box.rect;
        }
        if (!parts.isEmpty())
            result.lines << OcrLine{Ocr::tidyCjkSpacing(parts.join(QLatin1Char(' '))), rowBox};
    }
    return result;
}
