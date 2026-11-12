#ifndef PASSPORT_PAGE_H
#define PASSPORT_PAGE_H

#include <string>
#include <ctime>
#include <lvgl.h>

class PassportPage {
public:
    static PassportPage& GetInstance() {
        static PassportPage instance;
        return instance;
    }
    PassportPage(const PassportPage&) = delete;
    PassportPage& operator=(const PassportPage&) = delete;

    void Initialize();
    void BuildUi();
    void Show(bool show);
    void UpdateUi();

    int CalculateAge() const;

private:
    PassportPage() = default;
    ~PassportPage() = default;

    const std::string full_name_ = "Maryam Lovelace Alayna";
    const std::string address_ = "No.22a Jalan Palma 22,\nPalma Sands, Gamuda Cove";
    const std::string birthday_str_ = "15 DEC 2019";
    const int birth_year_ = 2019;
    const int birth_month_ = 12;
    const int birth_day_ = 15;

    // LVGL widgets
    lv_obj_t* root_ = nullptr;
    lv_obj_t* header_country_ = nullptr;

    // Photo
    lv_obj_t* photo_frame_ = nullptr;
    lv_obj_t* photo_img_ = nullptr;

    // Details Card
    lv_obj_t* card_ = nullptr;
    lv_obj_t* name_tag_ = nullptr;
    lv_obj_t* name_val_ = nullptr;
    lv_obj_t* dob_tag_ = nullptr;
    lv_obj_t* dob_val_ = nullptr;
    lv_obj_t* addr_tag_ = nullptr;
    lv_obj_t* addr_val_ = nullptr;

    // MRZ (Machine Readable Zone)
    lv_obj_t* mrz_label_1_ = nullptr;
    lv_obj_t* mrz_label_2_ = nullptr;

    // Footer
    lv_obj_t* enter_hint_ = nullptr;
};

#endif // PASSPORT_PAGE_H
