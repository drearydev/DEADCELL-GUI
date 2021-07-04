#include "window_manager.h"

#include <deque>

#include "../gui_instance.h"
#include "../wrapped/drawing.h"
#include "../wrapped/input.h"
#include "../wrapped/platform.h"

namespace deadcell::gui {
    void window_manager::add_window(const window_ptr &win) {
        const bool is_first_window = windows_.empty();

        windows_.push_back(win);

        if (is_first_window) {
            set_active_window(win);
        }
    }

    void window_manager::remove_window(const window_ptr& win) {
        if (const auto it = std::ranges::find_if(windows_, [&win](const window_ptr &ptr) {
            return ptr.get() == win.get();
        }); it != windows_.end()) {
            windows_.erase(it);
        }
    }

    void window_manager::set_active_window(const window_ptr &win) {
        if (win == active_window_) {
            return;
        }

        active_window_ = win;
    }

    void window_manager::move_to_front(const window_ptr &win, const bool make_active) {
        if (std::ranges::find(windows_, win) == windows_.end()) {
            return;
        }

        remove_window(win);
        add_window(win);

        if (make_active) {
            set_active_window(win);
        }
    }

    std::shared_ptr<window> window_manager::get_window_under_cursor() {
        const auto &io = ImGui::GetIO();

        std::deque<window_ptr> windows;

        for (auto &win : windows_) {
            if (!win->is_visible()) {
                continue;
            }

            if (input::is_in_bounds(win->get_position(), win->get_size())) {
                if (win == active_window_) {
                    return win;
                }

                windows.push_back(win);
            }
        }

        if (windows.size() > 1) {
            return windows.back();
        }
        
        if (windows.size() == 1) {
            return windows.front();
        }

        return nullptr;
    }

    void window_manager::process_mouse() {
        const window_ptr hovered_window = get_window_under_cursor();
        static window_ptr target_window = nullptr; // NOLINT(clang-diagnostic-exit-time-destructors)

        static bool is_dragging = false, is_resizing = false;
        static bool titlebar_hovered = false, resize_hovered = false;

        if (hovered_window) {
            hovered_window->dispatch_event(base_event::hover);

            const auto bottom_right = hovered_window->get_position() + hovered_window->get_size();

            titlebar_hovered = input::is_in_bounds(hovered_window->get_position(), { hovered_window->get_size().x, hovered_window->get_titlebar_height() });
            resize_hovered = input::is_in_bounds(bottom_right - 10, bottom_right);
        }

        if ((resize_hovered && (hovered_window && hovered_window->is_resizable())) || is_resizing) {
            platform::set_cursor(platform::cursor_resize_nwse);
        }

        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            if (hovered_window) {
                hovered_window->dispatch_event(base_event::mouse_click);
            }

            if (hovered_window) {
                move_to_front(hovered_window, true);

                if (titlebar_hovered) {
                    is_dragging = true;
                    target_window = hovered_window;
                }
                else if (resize_hovered && hovered_window->is_resizable()) {
                    is_resizing = true;
                    target_window = hovered_window;
                }
            }
        }

        if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            if (hovered_window) {
                hovered_window->dispatch_event(base_event::mouse_down);
            }

            if (target_window) {
                if (is_dragging) {
                    target_window->dispatch_event(window_event::drag_start);
                }
                else if (is_resizing) {
                    target_window->dispatch_event(window_event::resize_start);
                }
            }
        }

        if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
            if (hovered_window) {
                hovered_window->dispatch_event(base_event::mouse_up);
            }

            if (target_window) {
                if (is_dragging) {
                    target_window->dispatch_event(window_event::drag_end);
                    is_dragging = false;
                }
                else if (is_resizing) {
                    platform::set_cursor(platform::cursor_arrow);

                    target_window->dispatch_event(window_event::resize_end);
                    is_resizing = false;
                }
            }
        }
    }

    void window_manager::render() {
        for (auto &win : windows_) {
            if (win->is_visible()) {
                // Draw children recursively
                const auto draw = [](const auto &self, const std::shared_ptr<object> &obj) -> void {
                    obj->render();

                    for (auto &child : obj->get_children()) {
                        child->render();
                        self(self, child);
                    }
                };

                draw(draw, win);
            }
        }
    }

    void window_manager::new_frame() {
        process_mouse();
        render();
    }

    void window_manager::end_frame() {

    }
}
