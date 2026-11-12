#include "passport_page.h"

#include <cstdio>
#include <cstring>
#include <ctime>
#include <esp_log.h>

#include "board.h"
#include "display.h"
#include "maryam_photo.h"

#define TAG "PassportPage"

void PassportPage::Initialize() {
    ESP_LOGI(TAG, "PassportPage initialized (Retro Pixel UI)");
}

int PassportPage::CalculateAge() const {
    time_t now = time(nullptr);
    struct tm ti;
    localtime_r(&now, &ti);
    int current_year = ti.tm_year + 1900;
    int current_month = ti.tm_mon + 1;
    int current_day = ti.tm_mday;

    if (current_year < 2024) {
        current_year = 2026;
        current_month = 11;
        current_day = 11;
    }

    int age = current_year - birth_year_;
    if (current_month < birth_month_ || (current_month == birth_month_ && current_day < birth_day_)) {
        age--;
    }
    return age;
}

void PassportPage::BuildUi() {
    auto* screen = lv_screen_active();

    root_ = lv_obj_create(screen);
    lv_obj_set_size(root_, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_pos(root_, 0, 0);
    lv_obj_set_style_radius(root_, 0, 0);
    lv_obj_set_style_border_width(root_, 0, 0);
    lv_obj_set_style_pad_all(root_, 0, 0);
    lv_obj_set_style_bg_color(root_, lv_color_hex(0x060b17), 0); // Cyber deep navy
    lv_obj_set_style_bg_opa(root_, LV_OPA_COVER, 0);
    lv_obj_remove_flag(root_, LV_OBJ_FLAG_SCROLLABLE);

    // Top Header: [ MALAYSIA PASSPORT ]
    header_country_ = lv_label_create(root_);
    lv_obj_set_width(header_country_, LV_HOR_RES);
    lv_obj_set_pos(header_country_, 0, 8);
    lv_obj_set_style_text_align(header_country_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(header_country_, &lv_font_unscii_8, 0);
    lv_obj_set_style_text_color(header_country_, lv_color_hex(0xfbbf24), 0); // Gold
    lv_label_set_text(header_country_, "[ MALAYSIA PASSPORT ]");

    // Photo Frame (Photo is 96 x 116)
    photo_frame_ = lv_obj_create(root_);
    lv_obj_set_size(photo_frame_, 100, 120);
    lv_obj_set_pos(photo_frame_, (LV_HOR_RES - 100) / 2, 22);
    lv_obj_set_style_radius(photo_frame_, 4, 0);
    lv_obj_set_style_border_width(photo_frame_, 2, 0);
    lv_obj_set_style_border_color(photo_frame_, lv_color_hex(0xf59e0b), 0); // Amber 500 gold
    lv_obj_set_style_bg_color(photo_frame_, lv_color_hex(0x1e293b), 0);
    lv_obj_set_style_pad_all(photo_frame_, 0, 0);
    lv_obj_remove_flag(photo_frame_, LV_OBJ_FLAG_SCROLLABLE);

    photo_img_ = lv_image_create(photo_frame_);
    lv_image_set_src(photo_img_, &maryam_photo_dsc);
    lv_obj_align(photo_img_, LV_ALIGN_CENTER, 0, 0);

    // Details Card
    card_ = lv_obj_create(root_);
    lv_obj_set_size(card_, 224, 148);
    lv_obj_set_pos(card_, 8, 146);
    lv_obj_set_style_radius(card_, 4, 0);
    lv_obj_set_style_border_width(card_, 1, 0);
    lv_obj_set_style_border_color(card_, lv_color_hex(0x334155), 0);
    lv_obj_set_style_bg_color(card_, lv_color_hex(0x0f172a), 0);
    lv_obj_set_style_pad_all(card_, 4, 0);
    lv_obj_remove_flag(card_, LV_OBJ_FLAG_SCROLLABLE);

    // Field 1: NAME
    name_tag_ = lv_label_create(card_);
    lv_obj_set_pos(name_tag_, 6, 6);
    lv_obj_set_style_text_font(name_tag_, &lv_font_unscii_8, 0);
    lv_obj_set_style_text_color(name_tag_, lv_color_hex(0x64748b), 0);
    lv_label_set_text(name_tag_, "NAME");

    name_val_ = lv_label_create(card_);
    lv_obj_set_pos(name_val_, 6, 18);
    lv_obj_set_style_text_font(name_val_, &lv_font_unscii_8, 0);
    lv_obj_set_style_text_color(name_val_, lv_color_hex(0xffffff), 0);
    lv_label_set_text(name_val_, full_name_.c_str());

    // Field 2: DATE OF BIRTH / AGE
    dob_tag_ = lv_label_create(card_);
    lv_obj_set_pos(dob_tag_, 6, 36);
    lv_obj_set_style_text_font(dob_tag_, &lv_font_unscii_8, 0);
    lv_obj_set_style_text_color(dob_tag_, lv_color_hex(0x64748b), 0);
    lv_label_set_text(dob_tag_, "DOB / AGE");

    dob_val_ = lv_label_create(card_);
    lv_obj_set_pos(dob_val_, 6, 48);
    lv_obj_set_style_text_font(dob_val_, &lv_font_unscii_8, 0);
    lv_obj_set_style_text_color(dob_val_, lv_color_hex(0x38bdf8), 0); // Sky 400
    lv_label_set_text(dob_val_, "--");

    // Field 3: ADDRESS
    addr_tag_ = lv_label_create(card_);
    lv_obj_set_pos(addr_tag_, 6, 66);
    lv_obj_set_style_text_font(addr_tag_, &lv_font_unscii_8, 0);
    lv_obj_set_style_text_color(addr_tag_, lv_color_hex(0x64748b), 0);
    lv_label_set_text(addr_tag_, "ADDRESS");

    addr_val_ = lv_label_create(card_);
    lv_obj_set_pos(addr_val_, 6, 78);
    lv_obj_set_style_text_font(addr_val_, &lv_font_unscii_8, 0);
    lv_obj_set_style_text_color(addr_val_, lv_color_hex(0xe2e8f0), 0);
    lv_label_set_text(addr_val_, address_.c_str());

    // MRZ (Machine Readable Zone)
    mrz_label_1_ = lv_label_create(card_);
    lv_obj_set_pos(mrz_label_1_, 6, 110);
    lv_obj_set_style_text_font(mrz_label_1_, &lv_font_unscii_8, 0);
    lv_obj_set_style_text_color(mrz_label_1_, lv_color_hex(0x475569), 0);
    lv_label_set_text(mrz_label_1_, "P<MYSALAYNA<<MARYAM<LOVELACE");

    mrz_label_2_ = lv_label_create(card_);
    lv_obj_set_pos(mrz_label_2_, 6, 122);
    lv_obj_set_style_text_font(mrz_label_2_, &lv_font_unscii_8, 0);
    lv_obj_set_style_text_color(mrz_label_2_, lv_color_hex(0x475569), 0);
    lv_label_set_text(mrz_label_2_, "MY20191215MYS<<<<<<<<<<<<<<<");

    // Footer Hint
    enter_hint_ = lv_label_create(root_);
    lv_obj_set_width(enter_hint_, LV_HOR_RES);
    lv_obj_set_pos(enter_hint_, 0, 302);
    lv_obj_set_style_text_align(enter_hint_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(enter_hint_, &lv_font_unscii_8, 0);
    lv_obj_set_style_text_color(enter_hint_, lv_color_hex(0xfbbf24), 0); // Amber 400
    lv_label_set_text(enter_hint_, "[ PRESS ANY KEY FOR MENU ]");

    UpdateUi();
    lv_obj_add_flag(root_, LV_OBJ_FLAG_HIDDEN);
}

void PassportPage::UpdateUi() {
    if (dob_val_ == nullptr) return;

    int age = CalculateAge();
    char buf[64];
    snprintf(buf, sizeof(buf), "%s  [AGE: %d]", birthday_str_.c_str(), age);
    lv_label_set_text(dob_val_, buf);
}

void PassportPage::Show(bool show) {
    if (root_ == nullptr) return;
    if (show) {
        UpdateUi();
        lv_obj_clear_flag(root_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(root_);
    } else {
        lv_obj_add_flag(root_, LV_OBJ_FLAG_HIDDEN);
    }
}
