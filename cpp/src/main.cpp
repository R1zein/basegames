// Точка входа: захват экрана -> распознавание -> решатель -> оверлей.
//
// Расчёт живёт в отдельном потоке: на большой глубине один ход занимает
// заметное время, и делать это в потоке окна значило бы подвешивать отрисовку.
// Готовый результат кладётся под мьютекс, а окну шлётся уведомление.
//
// Ключи запуска:
//   --auto          самому нажимать стрелки
//   --depth N       фиксированная глубина поиска вместо адаптивной
//   --interval MS   пауза между кадрами

#include <atomic>
#include <chrono>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

#include <windows.h>
#include <shellapi.h>

#include <algorithm>
using std::max;
using std::min;

#include <objidl.h>
#include <gdiplus.h>

#include "bitboard.h"
#include "board.h"
#include "capture.h"
#include "input.h"
#include "overlay.h"
#include "solver.h"

namespace {

constexpr UINT WM_HINT_READY = WM_APP + 1;
constexpr int kHotkeyQuit = 1;

// Сколько кадров подряд доска может читаться как мусор, прежде чем мы решим,
// что она уехала, и запустим полный поиск заново.
constexpr int kMissesBeforeRescan = 8;

struct Options {
    bool autoplay = false;
    int depth = 0;
    int intervalMs = 0;  // 0 — выбрать по режиму
};

struct Hint {
    std::optional<app::Rect> board;
    std::optional<bb::Move> move;
    std::wstring status = L"поиск доски...";

    bool operator==(const Hint& other) const {
        const bool sameBoard = board.has_value() == other.board.has_value() &&
                               (!board || (board->x == other.board->x && board->y == other.board->y &&
                                           board->width == other.board->width));
        return sameBoard && move == other.move && status == other.status;
    }
};

std::mutex g_mutex;
Hint g_hint;
std::atomic<bool> g_running{true};

const wchar_t* moveName(bb::Move move) {
    switch (move) {
        case bb::Move::Left:  return L"влево";
        case bb::Move::Right: return L"вправо";
        case bb::Move::Up:    return L"вверх";
        case bb::Move::Down:  return L"вниз";
    }
    return L"?";
}

// Позиция принимается только после того, как повторится в двух кадрах подряд:
// кадр, пойманный посреди анимации сдвига плиток, даёт бессмыслицу.
class StableBoard {
public:
    std::optional<bb::Board> update(bb::Board board) {
        if (board == last_) {
            ++repeats_;
        } else {
            last_ = board;
            repeats_ = 1;
        }
        return repeats_ >= 2 ? std::optional<bb::Board>(board) : std::nullopt;
    }

    void reset() {
        last_ = 0;
        repeats_ = 0;
    }

private:
    bb::Board last_ = 0;
    int repeats_ = 0;
};

Options parseOptions() {
    Options options;
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv == nullptr) {
        return options;
    }
    for (int i = 1; i < argc; ++i) {
        const std::wstring arg(argv[i]);
        if (arg == L"--auto") {
            options.autoplay = true;
        } else if (arg == L"--depth" && i + 1 < argc) {
            options.depth = _wtoi(argv[++i]);
        } else if (arg == L"--interval" && i + 1 < argc) {
            options.intervalMs = _wtoi(argv[++i]);
        }
    }
    LocalFree(argv);
    if (options.intervalMs <= 0) {
        // В автоигре темп задаём мы сами, поэтому опрашиваем часто; в режиме
        // подсказки чаще четырёх раз в секунду смысла нет.
        options.intervalMs = options.autoplay ? 12 : 250;
    }
    return options;
}

}  // namespace

namespace {

void publish(HWND window, Hint hint) {
    {
        const std::lock_guard<std::mutex> lock(g_mutex);
        if (g_hint == hint) {
            return;  // при 12 мс на кадр незачем дёргать окно на каждом снимке
        }
        g_hint = std::move(hint);
    }
    PostMessageW(window, WM_HINT_READY, 0, 0);
}

class Session {
public:
    Session(HWND window, const Options& options) : window_(window), options_(options) {}

    void run() {
        while (g_running.load(std::memory_order_relaxed)) {
            const auto started = std::chrono::steady_clock::now();
            step();
            std::this_thread::sleep_until(started + std::chrono::milliseconds(options_.intervalMs));
        }
    }

private:
    // Пока доска не найдена, снимаем весь экран; как только найдена — только
    // её прямоугольник. На четырёх мониторах это разница в сотню раз.
    bool acquireFrame(app::Rect& local) {
        if (known_.has_value()) {
            if (!capture_.grabRegion(known_->x, known_->y, known_->width, known_->height, frame_)) {
                return false;
            }
            local = app::Rect{0, 0, known_->width, known_->height};
            return true;
        }

        if (!capture_.grab(frame_)) {
            return false;
        }
        const auto detection = app::findBoard(frame_);
        if (!detection.has_value()) {
            return false;
        }
        local = detection->rect;
        themeIndex_ = detection->theme;
        known_ = app::Rect{frame_.originX + local.x, frame_.originY + local.y, local.width, local.height};
        return true;
    }

    void forgetBoard() {
        known_.reset();
        misses_ = 0;
        stable_.reset();
        acted_ = false;
    }

    void step() {
        app::Rect local;
        if (!acquireFrame(local)) {
            forgetBoard();
            publish(window_, Hint{std::nullopt, std::nullopt, L"доска не найдена"});
            return;
        }

        const bb::Board position = app::readBoard(frame_, local, themeIndex_);
        if (!app::looksLikeGame(position)) {
            if (++misses_ >= kMissesBeforeRescan) {
                forgetBoard();
            }
            publish(window_, Hint{known_, std::nullopt, L"доска пуста"});
            return;
        }
        misses_ = 0;

        const std::optional<bb::Board> settled = stable_.update(position);
        if (!settled.has_value()) {
            publish(window_, Hint{known_, std::nullopt, L"ход в процессе..."});
            return;
        }

        const std::optional<bb::Move> move = solver_.bestMove(*settled, options_.depth);
        if (!move.has_value()) {
            publish(window_, Hint{known_, std::nullopt, L"ходов нет"});
            return;
        }

        if (!options_.autoplay) {
            publish(window_, Hint{known_, move, std::wstring(L"ход: ") + moveName(*move)});
            return;
        }
        playMove(*settled, *move);
    }

    void playMove(bb::Board settled, bb::Move move) {
        if (acted_ && settled == lastActed_) {
            // Позиция не изменилась после нашего нажатия. Обычно это ещё не
            // доигранная анимация, но если так и не сдвинулось — пробуем снова.
            if (++repeatsOnSame_ < 40) {
                return;
            }
            repeatsOnSame_ = 0;
            acted_ = false;
            return;
        }

        const int centerX = known_->x + known_->width / 2;
        const int centerY = known_->y + known_->height / 2;
        if (!app::boardWindowIsActive(centerX, centerY)) {
            publish(window_, Hint{known_, move, L"окно игры не в фокусе"});
            return;
        }

        app::sendMove(move);
        lastActed_ = settled;
        acted_ = true;
        repeatsOnSame_ = 0;
        ++moves_;
        publish(window_, Hint{known_, move, L"автоигра, ходов: " + std::to_wstring(moves_)});
    }

    HWND window_;
    Options options_;
    app::ScreenCapture capture_;
    app::Frame frame_;
    StableBoard stable_;
    bb::Solver solver_;
    std::optional<app::Rect> known_;
    int themeIndex_ = 0;
    int misses_ = 0;
    bb::Board lastActed_ = 0;
    bool acted_ = false;
    int repeatsOnSame_ = 0;
    long long moves_ = 0;
};

}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    // Без этого Windows отдавала бы виртуализованные координаты при масштабе
    // экрана, отличном от 100%, и рамка съезжала бы относительно доски.
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    const Options options = parseOptions();

    Gdiplus::GdiplusStartupInput startupInput;
    ULONG_PTR gdiplusToken = 0;
    Gdiplus::GdiplusStartup(&gdiplusToken, &startupInput, nullptr);

    bb::init();

    app::Overlay overlay;
    if (!overlay.create(instance)) {
        MessageBoxW(nullptr, L"Не удалось создать окно оверлея", L"smt", MB_ICONERROR);
        return 1;
    }
    overlay.render(std::nullopt, std::nullopt,
                   options.autoplay ? L"автоигра: поиск доски..." : L"поиск доски...");
    RegisterHotKey(overlay.handle(), kHotkeyQuit, MOD_CONTROL | MOD_ALT, 'Q');

    std::thread worker([&overlay, &options] {
        Session session(overlay.handle(), options);
        session.run();
    });

    MSG message;
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        if (message.message == WM_HOTKEY && message.wParam == kHotkeyQuit) {
            PostQuitMessage(0);
            continue;
        }
        if (message.message == WM_HINT_READY) {
            Hint hint;
            {
                const std::lock_guard<std::mutex> lock(g_mutex);
                hint = g_hint;
            }
            overlay.render(hint.board, hint.move, hint.status);
            continue;
        }
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    g_running.store(false, std::memory_order_relaxed);
    worker.join();

    UnregisterHotKey(overlay.handle(), kHotkeyQuit);
    overlay.destroy();
    Gdiplus::GdiplusShutdown(gdiplusToken);
    return 0;
}
