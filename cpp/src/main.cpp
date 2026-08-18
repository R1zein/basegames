// Точка входа: захват экрана -> распознавание -> решатель -> оверлей.
//
// Расчёт живёт в отдельном потоке: на глубине 6 один ход занимает заметное
// время, и делать это в потоке окна значило бы подвешивать отрисовку.
// Готовый результат кладётся под мьютекс, а окну шлётся уведомление.

#include <atomic>
#include <chrono>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>

#include "bitboard.h"
#include "board.h"
#include "capture.h"
#include "overlay.h"
#include "solver.h"

namespace {

constexpr UINT WM_HINT_READY = WM_APP + 1;
constexpr int kHotkeyQuit = 1;
constexpr auto kInterval = std::chrono::milliseconds(250);

struct Hint {
    std::optional<app::Rect> board;
    std::optional<bb::Move> move;
    std::wstring status = L"поиск доски...";
};

std::mutex g_mutex;
Hint g_hint;
std::atomic<bool> g_running{true};

void publish(HWND window, Hint hint) {
    {
        const std::lock_guard<std::mutex> lock(g_mutex);
        g_hint = std::move(hint);
    }
    PostMessageW(window, WM_HINT_READY, 0, 0);
}

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

void workerLoop(HWND window) {
    app::ScreenCapture capture;
    app::Frame frame;
    StableBoard stable;
    bb::Solver solver;

    while (g_running.load(std::memory_order_relaxed)) {
        const auto started = std::chrono::steady_clock::now();

        if (!capture.grab(frame)) {
            publish(window, Hint{std::nullopt, std::nullopt, L"не удалось снять экран"});
        } else {
            const std::optional<app::Rect> rect = app::findBoard(frame);
            if (!rect.has_value()) {
                stable.reset();
                publish(window, Hint{std::nullopt, std::nullopt, L"доска не найдена"});
            } else {
                const bb::Board position = app::readBoard(frame, *rect);
                if (!app::looksLikeGame(position)) {
                    stable.reset();
                    publish(window, Hint{rect, std::nullopt, L"доска пуста"});
                } else if (const std::optional<bb::Board> settled = stable.update(position); !settled) {
                    publish(window, Hint{rect, std::nullopt, L"ход в процессе..."});
                } else {
                    const std::optional<bb::Move> move = solver.bestMove(*settled);
                    const std::wstring status =
                        move ? std::wstring(L"ход: ") + moveName(*move) : std::wstring(L"ходов нет");
                    publish(window, Hint{rect, move, status});
                }
            }
        }

        std::this_thread::sleep_until(started + kInterval);
    }
}

}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    // Без этого Windows отдавала бы виртуализованные координаты при масштабе
    // экрана, отличном от 100%, и рамка съезжала бы относительно доски.
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    Gdiplus::GdiplusStartupInput startupInput;
    ULONG_PTR gdiplusToken = 0;
    Gdiplus::GdiplusStartup(&gdiplusToken, &startupInput, nullptr);

    bb::init();

    app::Overlay overlay;
    if (!overlay.create(instance)) {
        MessageBoxW(nullptr, L"Не удалось создать окно оверлея", L"smt", MB_ICONERROR);
        return 1;
    }
    overlay.render(std::nullopt, std::nullopt, L"поиск доски...");
    RegisterHotKey(overlay.handle(), kHotkeyQuit, MOD_CONTROL | MOD_ALT, 'Q');

    std::thread worker(workerLoop, overlay.handle());

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
