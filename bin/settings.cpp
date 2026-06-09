// settings.cpp
// VesperaOS Settings — example app using the stella++ C++ wrapper.
//
// Build:
//   c++ -std=c++17 -o settings settings.cpp stella++.cpp -lstella

#include "stella++.h"
#include <cstdio>

#include "power.h"

// ─── App state ────────────────────────────────────────────────────────────────

struct StorageInfo {
    int32_t used_gb  = 14;
    int32_t total_gb = 64;
};

// ─── Helpers ──────────────────────────────────────────────────────────────────

static void open_dialog(const char* name) {
    // TODO: IPC call to Crepusculum to open a child window
    (void)name;
}

// ─── Main ─────────────────────────────────────────────────────────────────────

extern "C" int main() {
    if (stella_init() != 0) return 1;

    stella::Window win("VesperaOS - Settings", 800, 480);
    win.bg(0x0D0D1A)
       .onClose([] { /* config_save(); */ });

    auto scr = win.screen();
    StorageInfo storage;

    // ── Header bar ────────────────────────────────────────────────────────────

    stella::Container header(scr);
    header.size(stella::Full, 56)
          .bg(0x14142A)
          .flexRow(stella::Flex::Center, stella::Flex::Center)
          .align(stella::Align::TopMid);

    stella::Label(header, "\u2699  Settings")
        .font(STELLA_FONT_24)
        .color(0xC8C8FF)
        .center();

    // ── Storage card ──────────────────────────────────────────────────────────

    stella::Container storage_card(scr);
    storage_card.size(720, 130)
                .bg(0x14142A)
                .radius(14)
                .align(stella::Align::TopMid, 0, 72);

    stella::Label(storage_card, "Storage")
        .font(STELLA_FONT_20)
        .color(0x8888BB)
        .pos(20, 14);

    stella::Label storage_label(storage_card, "Used: 14 GB of 64 GB");
    storage_label.font(STELLA_FONT_16)
                 .color(0xDEDEFF)
                 .pos(20, 46);

    stella::Bar storage_bar(storage_card, 680, 18);
    storage_bar.range(0, storage.total_gb)
               .value(storage.used_gb)
               .trackColor(0x1E1E40)
               .indicatorColor(0x7B5EA7)
               .pos(20, 86);

    // ── Network card ──────────────────────────────────────────────────────────

    stella::Container net_card(scr);
    net_card.size(720, 80)
            .bg(0x14142A)
            .radius(14)
            .align(stella::Align::TopMid, 0, 218);

    stella::Label(net_card, "Network")
        .font(STELLA_FONT_20)
        .color(0x8888BB)
        .pos(20, 14);

    stella::Label net_label(net_card, "Connected: VesperaNet-5G  \xC2\xB7  192.168.1.42");
    net_label.font(STELLA_FONT_16)
             .color(0xDEDEFF)
             .pos(20, 46);

    // ── Action button row ─────────────────────────────────────────────────────

    stella::Container btn_row(scr);
    btn_row.size(720, 52)
           .bgTransp()
           .flexRow(stella::Flex::SpaceBetween, stella::Flex::Center)
           .align(stella::Align::BottomMid, 0, -24);

    stella::Button(btn_row, "About", 160, 44)
        .bg(0x1E1E40).hoverBg(0x7B5EA7).radius(10)
        .onClick([] { open_dialog("about"); });

    stella::Button(btn_row, "Network", 160, 44)
        .bg(0x1E1E40).hoverBg(0x7B5EA7).radius(10)
        .onClick([&] {
            open_dialog("network");
            stella_label_update(net_label, "Connecting...");
        });

    stella::Button(btn_row, "Clear Cache", 160, 44)
        .bg(0x1E1E40).hoverBg(0x7B5EA7).radius(10)
        .onClick([&] {
            storage.used_gb -= 3;
            if (storage.used_gb < 0) storage.used_gb = 0;

            char buf[64];
            snprintf(buf, sizeof(buf), "Used: %d GB of %d GB",
                     storage.used_gb, storage.total_gb);

            stella_label_update(storage_label, buf);
            stella_bar_set_value(storage_bar, storage.used_gb);
        });

    stella::Button(btn_row, "Shutdown", 160, 44)
        .bg(0x3D1010).hoverBg(0xC0392B).radius(10)
        .onClick([] { reboot_poweroff(); });

    win.run();
    return 0;
}