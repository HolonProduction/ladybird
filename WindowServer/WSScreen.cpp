#include "WSScreen.h"
#include "WSMessageLoop.h"
#include "WSMessage.h"
#include "WSWindowManager.h"
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

static WSScreen* s_the;

WSScreen& WSScreen::the()
{
    ASSERT(s_the);
    return *s_the;
}

WSScreen::WSScreen(unsigned width, unsigned height)
    : m_width(width)
    , m_height(height)
{
    ASSERT(!s_the);
    s_the = this;
    m_cursor_location = rect().center();
    m_framebuffer_fd = open("/dev/bxvga", O_RDWR);
    ASSERT(m_framebuffer_fd >= 0);

    set_resolution(width, height);
}

WSScreen::~WSScreen()
{
}

void WSScreen::set_resolution(int width, int height)
{
    struct BXVGAResolution {
        int width;
        int height;
    };
    BXVGAResolution resolution { (int)width, (int)height};
    int rc = ioctl(m_framebuffer_fd, 1985, (int)&resolution);
    ASSERT(rc == 0);

    if (m_framebuffer) {
        size_t previous_size_in_bytes = m_width * m_height * sizeof(RGBA32) * 2;
        int rc = munmap(m_framebuffer, previous_size_in_bytes);
        ASSERT(rc == 0);
    }

    size_t framebuffer_size_in_bytes = width * height * sizeof(RGBA32) * 2;
    m_framebuffer = (RGBA32*)mmap(nullptr, framebuffer_size_in_bytes, PROT_READ | PROT_WRITE, MAP_SHARED, m_framebuffer_fd, 0);
    ASSERT(m_framebuffer && m_framebuffer != (void*)-1);

    m_width = width;
    m_height = height;

    m_cursor_location.constrain(rect());
}

void WSScreen::on_receive_mouse_data(int dx, int dy, bool left_button, bool right_button, bool middle_button)
{
    auto prev_location = m_cursor_location;
    m_cursor_location.move_by(dx, dy);
    m_cursor_location.constrain(rect());
    unsigned buttons = 0;
    if (left_button)
        buttons |= (unsigned)MouseButton::Left;
    if (right_button)
        buttons |= (unsigned)MouseButton::Right;
    if (middle_button)
        buttons |= (unsigned)MouseButton::Middle;
    bool prev_left_button = m_left_mouse_button_pressed;
    bool prev_right_button = m_right_mouse_button_pressed;
    bool prev_middle_button = m_middle_mouse_button_pressed;
    m_left_mouse_button_pressed = left_button;
    m_right_mouse_button_pressed = right_button;
    m_middle_mouse_button_pressed = middle_button;
    if (prev_left_button != left_button) {
        auto message = make<WSMouseEvent>(left_button ? WSMessage::MouseDown : WSMessage::MouseUp, m_cursor_location, buttons, MouseButton::Left);
        WSMessageLoop::the().post_message(WSWindowManager::the(), move(message));
    }
    if (prev_right_button != right_button) {
        auto message = make<WSMouseEvent>(right_button ? WSMessage::MouseDown : WSMessage::MouseUp, m_cursor_location, buttons, MouseButton::Right);
        WSMessageLoop::the().post_message(WSWindowManager::the(), move(message));
    }
    if (prev_middle_button != middle_button) {
        auto message = make<WSMouseEvent>(middle_button ? WSMessage::MouseDown : WSMessage::MouseUp, m_cursor_location, buttons, MouseButton::Middle);
        WSMessageLoop::the().post_message(WSWindowManager::the(), move(message));
    }
    if (m_cursor_location != prev_location) {
        auto message = make<WSMouseEvent>(WSMessage::MouseMove, m_cursor_location, buttons);
        WSMessageLoop::the().post_message(WSWindowManager::the(), move(message));
    }
    // NOTE: Invalidate the cursor if it moved, or if the left button changed state (for the cursor color inversion.)
    if (m_cursor_location != prev_location || prev_left_button != left_button)
        WSWindowManager::the().invalidate_cursor();
}

void WSScreen::on_receive_keyboard_data(KeyEvent kernel_event)
{
    auto message = make<WSKeyEvent>(kernel_event.is_press() ? WSMessage::KeyDown : WSMessage::KeyUp, kernel_event.key, kernel_event.character, kernel_event.modifiers());
    WSMessageLoop::the().post_message(WSWindowManager::the(), move(message));
}

void WSScreen::set_y_offset(int offset)
{
    int rc = ioctl(m_framebuffer_fd, 1982, offset);
    ASSERT(rc == 0);
}
