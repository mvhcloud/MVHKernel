#include <stdint.h>
#include "mvh/block.h"
#include "mvh/desktop.h"
#include "mvh/framebuffer.h"
#include "mvh/hal.h"
#include "mvh/input.h"
#include "mvh/mvhfs.h"
#include "mvh/rtc.h"
#include "mvh/version.h"

#define COLOR_WHITE 0xF8FAFCu
#define COLOR_MUTED 0x94A3B8u
#define COLOR_PANEL 0x172033u
#define COLOR_PANEL_2 0x202A40u
#define COLOR_ACCENT 0x7C5CFCu
#define COLOR_CYAN 0x28D7E5u
#define COLOR_GREEN 0x34D399u
#define COLOR_RED 0xFB7185u

static int32_t screen_width(void) { return (int32_t)framebuffer_status()->width; }
static int32_t screen_height(void) { return (int32_t)framebuffer_status()->height; }

static void panel(int32_t x, int32_t y, int32_t width, int32_t height, uint32_t color)
{
    framebuffer_fill_rect(x + 10, y, width - 20, height, color);
    framebuffer_fill_rect(x, y + 10, width, height - 20, color);
    framebuffer_fill_rect(x + 4, y + 4, width - 8, height - 8, color);
}

static void button(int32_t x, int32_t y, int32_t width, const char *label, uint32_t color)
{
    panel(x, y, width, 48, color);
    framebuffer_draw_text(x + 18, y + 17, label, 2u, COLOR_WHITE);
}

static void shell_background(const char *section)
{
    rtc_time_t time;
    int32_t width = screen_width();
    framebuffer_gradient(0x0B1021u, 0x18254Bu);
    framebuffer_fill_rect(0, 0, width, 54, 0x10182Au);
    panel(18, 10, 34, 34, COLOR_ACCENT);
    framebuffer_draw_text(28, 20, "M", 2u, COLOR_WHITE);
    framebuffer_draw_text(66, 19, "MVH DESKTOP", 2u, COLOR_WHITE);
    framebuffer_draw_text(width - 255, 19, section, 2u, COLOR_MUTED);
    hal_clock_read(&time);
    framebuffer_draw_text(width - 74, 19, "LIVE", 2u, COLOR_GREEN);
}

static void progress_bar(int32_t x, int32_t y, int32_t width, uint32_t percent)
{
    framebuffer_fill_rect(x, y, width, 12, 0x303B55u);
    framebuffer_fill_rect(x, y, (int32_t)((uint32_t)width * percent / 100u), 12, COLOR_CYAN);
}

static void render_setup(uint32_t page, const char *status, uint32_t progress)
{
    int32_t width = screen_width();
    int32_t height = screen_height();
    int32_t card_width = width > 900 ? 760 : width - 80;
    int32_t x = (width - card_width) / 2;
    int32_t y = (height - 470) / 2;
    shell_background("SETUP");
    panel(x, y, card_width, 470, COLOR_PANEL);
    framebuffer_draw_text(x + 48, y + 42, "MVH SYSTEM SETUP", 4u, COLOR_WHITE);
    framebuffer_draw_text(x + 50, y + 84, MVH_KERNEL_VERSION, 2u, COLOR_CYAN);
    if (page == 0u) {
        framebuffer_draw_text(x + 50, y + 145, "WELCOME", 3u, COLOR_WHITE);
        framebuffer_draw_text(x + 50, y + 190, "INSTALL A CLEAN MODERN MVH ENVIRONMENT", 2u, COLOR_MUTED);
        framebuffer_draw_text(x + 50, y + 225, "APIC  HPET  PCIE  PERSISTENT STORAGE", 2u, COLOR_MUTED);
        button(x + card_width - 235, y + 370, 185, "CONTINUE", COLOR_ACCENT);
        framebuffer_draw_text(x + 50, y + 387, "ENTER", 2u, COLOR_CYAN);
    } else if (page == 1u) {
        framebuffer_draw_text(x + 50, y + 145, "STORAGE", 3u, COLOR_WHITE);
        framebuffer_draw_text(x + 50, y + 190,
                              block_count() != 0u ? "ATA DISK 0 IS READY" : "NO WRITABLE DISK FOUND",
                              2u, block_count() != 0u ? COLOR_GREEN : COLOR_RED);
        framebuffer_draw_text(x + 50, y + 230, "SETUP CREATES MVHFS SYSTEM METADATA", 2u, COLOR_MUTED);
        if (block_count() != 0u) button(x + card_width - 235, y + 370, 185, "INSTALL", COLOR_ACCENT);
        framebuffer_draw_text(x + 50, y + 387, "ESC BACK", 2u, COLOR_CYAN);
    } else if (page == 2u) {
        framebuffer_draw_text(x + 50, y + 145, "INSTALLING", 3u, COLOR_WHITE);
        framebuffer_draw_text(x + 50, y + 205, status, 2u, COLOR_MUTED);
        progress_bar(x + 50, y + 260, card_width - 100, progress);
    } else {
        framebuffer_draw_text(x + 50, y + 145, "READY", 3u, COLOR_GREEN);
        framebuffer_draw_text(x + 50, y + 195, status, 2u, COLOR_MUTED);
        button(x + card_width - 265, y + 370, 215, "OPEN DESKTOP", COLOR_ACCENT);
    }
}

static int install_system(void)
{
    mvhfs_t fs;
    static const char installed[] = "MVH Betriebsystem 1.1.8-2";
    static const char edition[] = "Desktop Edition";
    if (block_count() == 0u) return -1;
    render_setup(2u, "FORMATTING PERSISTENT SYSTEM VOLUME", 20u);
    if (mvhfs_format(0u) != 0 || mvhfs_mount(&fs, 0u) != 0) return -1;
    render_setup(2u, "WRITING SYSTEM IDENTITY", 55u);
    if (mvhfs_create(&fs, "installed") != 0 ||
        mvhfs_write(&fs, "installed", installed, sizeof(installed) - 1u) != 0 ||
        mvhfs_create(&fs, "edition") != 0 ||
        mvhfs_write(&fs, "edition", edition, sizeof(edition) - 1u) != 0) return -1;
    render_setup(2u, "FINALIZING RECOVERY METADATA", 88u);
    hal_sleep_ms(350u);
    render_setup(2u, "INSTALLATION COMPLETE", 100u);
    hal_sleep_ms(300u);
    return 0;
}

static void render_desktop(uint8_t about)
{
    int32_t width = screen_width();
    int32_t height = screen_height();
    int32_t window_width = width > 980 ? 820 : width - 100;
    int32_t x = (width - window_width) / 2;
    shell_background("DESKTOP");
    panel(x, 100, window_width, height - 230, COLOR_PANEL);
    framebuffer_fill_rect(x, 100, window_width, 44, COLOR_PANEL_2);
    framebuffer_fill_rect(x + 18, 116, 12, 12, COLOR_RED);
    framebuffer_fill_rect(x + 38, 116, 12, 12, 0xFBBF24u);
    framebuffer_fill_rect(x + 58, 116, 12, 12, COLOR_GREEN);
    if (about == 0u) {
        framebuffer_draw_text(x + 48, 185, "WELCOME TO MVH", 4u, COLOR_WHITE);
        framebuffer_draw_text(x + 50, 235, "A FAST X86 64 KERNEL ENVIRONMENT", 2u, COLOR_MUTED);
        panel(x + 50, 290, 210, 105, COLOR_PANEL_2);
        framebuffer_draw_text(x + 72, 312, "SYSTEM", 2u, COLOR_CYAN);
        framebuffer_draw_text(x + 72, 347, "APIC  HPET", 2u, COLOR_WHITE);
        panel(x + 285, 290, 210, 105, COLOR_PANEL_2);
        framebuffer_draw_text(x + 307, 312, "STORAGE", 2u, COLOR_CYAN);
        framebuffer_draw_text(x + 307, 347, "MVHFS  ATA", 2u, COLOR_WHITE);
        panel(x + 520, 290, 210, 105, COLOR_PANEL_2);
        framebuffer_draw_text(x + 542, 312, "NETWORK", 2u, COLOR_CYAN);
        framebuffer_draw_text(x + 542, 347, "IPV4  UDP", 2u, COLOR_WHITE);
    } else {
        framebuffer_draw_text(x + 48, 185, "ABOUT THIS SYSTEM", 4u, COLOR_WHITE);
        framebuffer_draw_text(x + 50, 245, "MVH BETRIEBSYSTEM", 3u, COLOR_CYAN);
        framebuffer_draw_text(x + 50, 295, "KERNEL 1.1.8 2", 2u, COLOR_WHITE);
        framebuffer_draw_text(x + 50, 335, "X86 64  MULTIBOOT  GRAPHICS", 2u, COLOR_MUTED);
        framebuffer_draw_text(x + 50, 375, "PRESS A TO RETURN", 2u, COLOR_GREEN);
    }
    panel(width / 2 - 180, height - 94, 360, 66, 0x121A2Du);
    button(width / 2 - 155, height - 85, 90, "SETUP", COLOR_ACCENT);
    button(width / 2 - 50, height - 85, 90, "ABOUT", 0x334155u);
    button(width / 2 + 55, height - 85, 90, "RESET", 0x334155u);
}

void desktop_run(uint8_t setup_mode)
{
    input_event_t event;
    uint32_t page = setup_mode != 0u ? 0u : 3u;
    uint8_t about = 0u;
    input_init();
    if (setup_mode != 0u) render_setup(0u, "", 0u);
    else render_desktop(0u);
    for (;;) {
        if (input_poll(&event) != 0) continue;
        if (setup_mode != 0u) {
            if (event.type == INPUT_EVENT_BACK && page != 0u) {
                page--;
                render_setup(page, "", 0u);
            } else if (event.type == INPUT_EVENT_ACCEPT) {
                if (page == 0u) { page = 1u; render_setup(page, "", 0u); }
                else if (page == 1u && block_count() != 0u) {
                    page = install_system() == 0 ? 3u : 1u;
                    render_setup(page, page == 3u ? "PRESS ENTER FOR DESKTOP" :
                                                   "INSTALLATION FAILED", 0u);
                } else if (page == 3u) { setup_mode = 0u; render_desktop(0u); }
            }
        } else {
            if (event.text == 's' || event.text == 'S') {
                setup_mode = 1u; page = 0u; render_setup(0u, "", 0u);
            } else if (event.text == 'a' || event.text == 'A') {
                about ^= 1u; render_desktop(about);
            } else if (event.text == 'r' || event.text == 'R') hal_reboot();
        }
    }
}
