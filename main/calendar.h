#ifndef CALENDAR_H
#define CALENDAR_H

#include <string>
#include <ctime>
#include <lvgl.h>

class Calendar {
public:
    static Calendar& GetInstance() {
        static Calendar instance;
        return instance;
    }
    Calendar(const Calendar&) = delete;
    Calendar& operator=(const Calendar&) = delete;

    void Initialize();
    void BuildUi();
    void Show(bool show);
    void UpdateUi();

    // Navigation
    bool HandleButtonUp();    // Previous month
    bool HandleButtonDown();  // Next month
    bool HandleButtonOk();    // Return

    void ResetToToday();

private:
    Calendar() = default;
    ~Calendar() = default;

    int current_year_ = 2026;
    int current_month_ = 11; // 1-12
    int today_year_ = 2026;
    int today_month_ = 11;
    int today_day_ = 11;

    // LVGL widgets
    lv_obj_t* root_ = nullptr;
    lv_obj_t* header_title_ = nullptr;
    lv_obj_t* month_year_label_ = nullptr;
    lv_obj_t* hijri_label_ = nullptr;

    lv_obj_t* grid_container_ = nullptr;
    lv_obj_t* day_headers_[7]; // Su, Mo, Tu, We, Th, Fr, Sa
    lv_obj_t* today_box_ = nullptr;
    lv_obj_t* day_labels_[42];

    lv_obj_t* info_card_ = nullptr;
    lv_obj_t* date_full_label_ = nullptr;
    lv_obj_t* date_sub_label_ = nullptr;

    lv_obj_t* footer_label_ = nullptr;
};

#endif // CALENDAR_H
